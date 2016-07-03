/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/str_util.h"
#include "v7/src/internal.h"
#include "v7/src/eval.h"
#include "v7/src/string.h"
#include "v7/src/array.h"
#include "v7/src/object.h"
#include "v7/src/gc.h"
#include "v7/src/compiler.h"
#include "v7/src/cyg_profile.h"
#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/util.h"
#include "v7/src/shdata.h"
#include "v7/src/exceptions.h"
#include "v7/src/conversion.h"
#include "v7/src/varint.h"

/*
 * Bcode offsets in "try stack" are stored in JS numbers, i.e.  in `double`s.
 * Apart from the offset itself, we also need some additional data:
 *
 * - type of the block that offset represents (`catch`, `finally`, `switch`,
 *   or some loop)
 * - size of the stack when the block is created (needed when throwing, since
 *   if exception is thrown from the middle of the expression, the stack may
 *   have any arbitrary length)
 *
 * We bake all this data into integer part of the double (53 bits) :
 *
 * - 32 bits: bcode offset
 * - 3 bits: "tag": the type of the block
 * - 16 bits: stack size
 */

/*
 * Widths of data parts
 */
#define LBLOCK_OFFSET_WIDTH 32
#define LBLOCK_TAG_WIDTH 3
#define LBLOCK_STACK_SIZE_WIDTH 16

/*
 * Shifts of data parts
 */
#define LBLOCK_OFFSET_SHIFT (0)
#define LBLOCK_TAG_SHIFT (LBLOCK_OFFSET_SHIFT + LBLOCK_OFFSET_WIDTH)
#define LBLOCK_STACK_SIZE_SHIFT (LBLOCK_TAG_SHIFT + LBLOCK_TAG_WIDTH)
#define LBLOCK_TOTAL_WIDTH (LBLOCK_STACK_SIZE_SHIFT + LBLOCK_STACK_SIZE_WIDTH)

/*
 * Masks of data parts
 */
#define LBLOCK_OFFSET_MASK \
  ((int64_t)(((int64_t) 1 << LBLOCK_OFFSET_WIDTH) - 1) << LBLOCK_OFFSET_SHIFT)
#define LBLOCK_TAG_MASK \
  ((int64_t)(((int64_t) 1 << LBLOCK_TAG_WIDTH) - 1) << LBLOCK_TAG_SHIFT)
#define LBLOCK_STACK_SIZE_MASK                             \
  ((int64_t)(((int64_t) 1 << LBLOCK_STACK_SIZE_WIDTH) - 1) \
   << LBLOCK_STACK_SIZE_SHIFT)

/*
 * Self-check: make sure all the data can fit into double's mantissa
 */
#if (LBLOCK_TOTAL_WIDTH > 53)
#error lblock width is too large, it can't fit into double's mantissa
#endif

/*
 * Tags that are used for bcode offsets in "try stack"
 */
#define LBLOCK_TAG_CATCH ((int64_t) 0x01 << LBLOCK_TAG_SHIFT)
#define LBLOCK_TAG_FINALLY ((int64_t) 0x02 << LBLOCK_TAG_SHIFT)
#define LBLOCK_TAG_LOOP ((int64_t) 0x03 << LBLOCK_TAG_SHIFT)
#define LBLOCK_TAG_SWITCH ((int64_t) 0x04 << LBLOCK_TAG_SHIFT)

/*
 * Yields 32-bit bcode offset value
 */
#define LBLOCK_OFFSET(v) \
  ((bcode_off_t)(((v) &LBLOCK_OFFSET_MASK) >> LBLOCK_OFFSET_SHIFT))

/*
 * Yields tag value (unshifted, to be compared with macros like
 * `LBLOCK_TAG_CATCH`, etc)
 */
#define LBLOCK_TAG(v) ((v) &LBLOCK_TAG_MASK)

/*
 * Yields stack size
 */
#define LBLOCK_STACK_SIZE(v) \
  (((v) &LBLOCK_STACK_SIZE_MASK) >> LBLOCK_STACK_SIZE_SHIFT)

/*
 * Yields `int64_t` value to be stored as a JavaScript number
 */
#define LBLOCK_ITEM_CREATE(offset, tag, stack_size) \
  ((int64_t)(offset) | (tag) |                      \
   (((int64_t)(stack_size)) << LBLOCK_STACK_SIZE_SHIFT))

/*
 * make sure `bcode_off_t` is just 32-bit, so that it can fit in double
 * with 3-bit tag
 */
V7_STATIC_ASSERT((sizeof(bcode_off_t) * 8) == LBLOCK_OFFSET_WIDTH,
                 wrong_size_of_bcode_off_t);

#define PUSH(v) stack_push(&v7->stack, v)
#define POP() stack_pop(&v7->stack)
#define TOS() stack_tos(&v7->stack)
#define SP() stack_sp(&v7->stack)

/*
 * Local-to-function block types that we might want to consider when unwinding
 * stack for whatever reason. see `unwind_local_blocks_stack()`.
 */
enum local_block {
  LOCAL_BLOCK_NONE = (0),
  LOCAL_BLOCK_CATCH = (1 << 0),
  LOCAL_BLOCK_FINALLY = (1 << 1),
  LOCAL_BLOCK_LOOP = (1 << 2),
  LOCAL_BLOCK_SWITCH = (1 << 3),
};

/*
 * Like `V7_TRY()`, but to be used inside `eval_bcode()` only: you should
 * wrap all calls to cfunctions into `BTRY()` instead of `V7_TRY()`.
 *
 * If the provided function returns something other than `V7_OK`, this macro
 * calls `bcode_perform_throw`, which performs bcode stack unwinding.
 */
#define BTRY(call)                                                            \
  do {                                                                        \
    enum v7_err _e = call;                                                    \
    (void) _you_should_use_BTRY_in_eval_bcode_only;                           \
    if (_e != V7_OK) {                                                        \
      V7_TRY(bcode_perform_throw(v7, &r, 0 /*don't take value from stack*/)); \
      goto op_done;                                                           \
    }                                                                         \
  } while (0)

V7_PRIVATE void stack_push(struct mbuf *s, val_t v) {
  mbuf_append(s, &v, sizeof(v));
}

V7_PRIVATE val_t stack_pop(struct mbuf *s) {
  assert(s->len >= sizeof(val_t));
  s->len -= sizeof(val_t);
  return *(val_t *) (s->buf + s->len);
}

V7_PRIVATE val_t stack_tos(struct mbuf *s) {
  assert(s->len >= sizeof(val_t));
  return *(val_t *) (s->buf + s->len - sizeof(val_t));
}

#ifdef V7_BCODE_TRACE_STACK
V7_PRIVATE val_t stack_at(struct mbuf *s, size_t idx) {
  assert(s->len >= sizeof(val_t) * idx);
  return *(val_t *) (s->buf + s->len - sizeof(val_t) - idx * sizeof(val_t));
}
#endif

V7_PRIVATE int stack_sp(struct mbuf *s) {
  return s->len / sizeof(val_t);
}

/*
 * Delete a property with name `name`, `len` from an object `obj`. If the
 * object does not contain own property with the given `name`, moves to `obj`'s
 * prototype, and so on.
 *
 * If the property is eventually found, it is deleted, and `0` is returned.
 * Otherwise, `-1` is returned.
 *
 * If `len` is -1/MAXUINT/~0, then `name` must be 0-terminated.
 *
 * See `v7_del()` as well.
 */
static int del_property_deep(struct v7 *v7, val_t obj, const char *name,
                             size_t len) {
  if (!v7_is_object(obj)) {
    return -1;
  }
  for (; obj != V7_NULL; obj = obj_prototype_v(v7, obj)) {
    int del_res;
    if ((del_res = v7_del(v7, obj, name, len)) != -1) {
      return del_res;
    }
  }
  return -1;
}

/* Visual studio 2012+ has signbit() */
#if defined(_MSC_VER) && _MSC_VER < 1700
static int signbit(double x) {
  double s = _copysign(1, x);
  return s < 0;
}
#endif

static double b_int_bin_op(enum opcode op, double a, double b) {
  int32_t ia = isnan(a) || isinf(a) ? 0 : (int32_t)(int64_t) a;
  int32_t ib = isnan(b) || isinf(b) ? 0 : (int32_t)(int64_t) b;

  switch (op) {
    case OP_LSHIFT:
      return (int32_t)((uint32_t) ia << ((uint32_t) ib & 31));
    case OP_RSHIFT:
      return ia >> ((uint32_t) ib & 31);
    case OP_URSHIFT:
      return (uint32_t) ia >> ((uint32_t) ib & 31);
    case OP_OR:
      return ia | ib;
    case OP_XOR:
      return ia ^ ib;
    case OP_AND:
      return ia & ib;
    default:
      assert(0);
  }
  return 0;
}

static double b_num_bin_op(enum opcode op, double a, double b) {
  /*
   * For certain operations, the result is always NaN if either of arguments
   * is NaN
   */
  switch (op) {
    case OP_ADD:
    case OP_SUB:
    case OP_MUL:
    case OP_DIV:
    case OP_REM:
      if (isnan(a) || isnan(b)) {
        return NAN;
      }
      break;
    default:
      break;
  }

  switch (op) {
    case OP_ADD: /* simple fixed width nodes with no payload */
      return a + b;
    case OP_SUB:
      return a - b;
    case OP_REM:
      if (b == 0 || isnan(b) || isnan(a) || isinf(b) || isinf(a)) {
        return NAN;
      }
      return (int) a % (int) b;
    case OP_MUL:
      return a * b;
    case OP_DIV:
      if (b == 0) {
        if (a == 0) return NAN;
        return (!signbit(a) == !signbit(b)) ? INFINITY : -INFINITY;
      }
      return a / b;
    case OP_LSHIFT:
    case OP_RSHIFT:
    case OP_URSHIFT:
    case OP_OR:
    case OP_XOR:
    case OP_AND:
      return b_int_bin_op(op, a, b);
    default:
      assert(0);
  }
  return 0;
}

static int b_bool_bin_op(enum opcode op, double a, double b) {
#ifdef V7_BROKEN_NAN
  if (isnan(a) || isnan(b)) return op == OP_NE || op == OP_NE_NE;
#endif

  switch (op) {
    case OP_EQ:
    case OP_EQ_EQ:
      return a == b;
    case OP_NE:
    case OP_NE_NE:
      return a != b;
    case OP_LT:
      return a < b;
    case OP_LE:
      return a <= b;
    case OP_GT:
      return a > b;
    case OP_GE:
      return a >= b;
    default:
      assert(0);
  }
  return 0;
}

static bcode_off_t bcode_get_target(char **ops) {
  bcode_off_t target;
  (*ops)++;
  memcpy(&target, *ops, sizeof(target));
  *ops += sizeof(target) - 1;
  return target;
}

struct bcode_registers {
  /*
   * TODO(dfrank): make it contain `struct v7_call_frame_bcode *`
   * and use `bcode_ops` in-place, or probably drop the `bcode_registers`
   * whatsoever
   */
  struct bcode *bcode;
  char *ops;
  char *end;
  unsigned int need_inc_ops : 1;
};

/*
 * If returning from function implicitly, then set return value to `undefined`.
 *
 * And if function was called as a constructor, then make sure returned
 * value is an object.
 */
static void bcode_adjust_retval(struct v7 *v7, uint8_t is_explicit_return) {
  if (!is_explicit_return) {
    /* returning implicitly: set return value to `undefined` */
    POP();
    PUSH(V7_UNDEFINED);
  }

  if (v7->call_stack->is_constructor && !v7_is_object(TOS())) {
    /* constructor is going to return non-object: replace it with `this` */
    POP();
    PUSH(v7_get_this(v7));
  }
}

static void bcode_restore_registers(struct v7 *v7, struct bcode *bcode,
                                    struct bcode_registers *r) {
  r->bcode = bcode;
  r->ops = bcode->ops.p;
  r->end = r->ops + bcode->ops.len;

  (void) v7;
}

V7_PRIVATE struct v7_call_frame_base *find_call_frame(struct v7 *v7,
                                                      uint8_t type_mask) {
  struct v7_call_frame_base *ret = v7->call_stack;

  while (ret != NULL && !(ret->type_mask & type_mask)) {
    ret = ret->prev;
  }

  return ret;
}

static struct v7_call_frame_private *find_call_frame_private(struct v7 *v7) {
  return (struct v7_call_frame_private *) find_call_frame(
      v7, V7_CALL_FRAME_MASK_PRIVATE);
}

static struct v7_call_frame_bcode *find_call_frame_bcode(struct v7 *v7) {
  return (struct v7_call_frame_bcode *) find_call_frame(
      v7, V7_CALL_FRAME_MASK_BCODE);
}

#if 0
static struct v7_call_frame_cfunc *find_call_frame_cfunc(struct v7 *v7) {
  return (struct v7_call_frame_cfunc *) find_call_frame(
      v7, V7_CALL_FRAME_MASK_CFUNC);
}
#endif

static struct v7_call_frame_base *create_call_frame(struct v7 *v7,
                                                    size_t size) {
  struct v7_call_frame_base *call_frame_base = NULL;

  call_frame_base = (struct v7_call_frame_base *) calloc(1, size);

  /* save previous call frame */
  call_frame_base->prev = v7->call_stack;

  /* by default, inherit line_no from the previous frame */
  if (v7->call_stack != NULL) {
    call_frame_base->line_no = v7->call_stack->line_no;
  }

  return call_frame_base;
}

static void init_call_frame_private(struct v7 *v7,
                                    struct v7_call_frame_private *call_frame,
                                    val_t scope) {
  /* make a snapshot of the current state */
  {
    struct v7_call_frame_private *cf = find_call_frame_private(v7);
    if (cf != NULL) {
      cf->stack_size = v7->stack.len;
    }
  }

  /* set a type flag */
  call_frame->base.type_mask |= V7_CALL_FRAME_MASK_PRIVATE;

  /* fill the new frame with data */
  call_frame->vals.scope = scope;
  /* `try_stack` will be lazily created in `eval_try_push()`*/
  call_frame->vals.try_stack = V7_UNDEFINED;
}

static void init_call_frame_bcode(struct v7 *v7,
                                  struct v7_call_frame_bcode *call_frame,
                                  char *prev_bcode_ops, struct bcode *bcode,
                                  val_t this_obj, val_t scope,
                                  uint8_t is_constructor) {
  init_call_frame_private(v7, &call_frame->base, scope);

  /* make a snapshot of the current state */
  {
    struct v7_call_frame_bcode *cf = find_call_frame_bcode(v7);
    if (cf != NULL) {
      cf->bcode_ops = prev_bcode_ops;
    }
  }

  /* set a type flag */
  call_frame->base.base.type_mask |= V7_CALL_FRAME_MASK_BCODE;

  /* fill the new frame with data */
  call_frame->bcode = bcode;
  call_frame->vals.this_obj = this_obj;
  call_frame->base.base.is_constructor = is_constructor;
}

/*
 * Create new bcode call frame object and fill it with data
 */
static void append_call_frame_bcode(struct v7 *v7, char *prev_bcode_ops,
                                    struct bcode *bcode, val_t this_obj,
                                    val_t scope, uint8_t is_constructor) {
  struct v7_call_frame_bcode *call_frame =
      (struct v7_call_frame_bcode *) create_call_frame(v7, sizeof(*call_frame));

  init_call_frame_bcode(v7, call_frame, prev_bcode_ops, bcode, this_obj, scope,
                        is_constructor);

  v7->call_stack = &call_frame->base.base;
}

static void append_call_frame_private(struct v7 *v7, val_t scope) {
  struct v7_call_frame_private *call_frame =
      (struct v7_call_frame_private *) create_call_frame(v7,
                                                         sizeof(*call_frame));
  init_call_frame_private(v7, call_frame, scope);

  v7->call_stack = &call_frame->base;
}

static void append_call_frame_cfunc(struct v7 *v7, val_t this_obj,
                                    v7_cfunction_t *cfunc) {
  struct v7_call_frame_cfunc *call_frame =
      (struct v7_call_frame_cfunc *) create_call_frame(v7, sizeof(*call_frame));

  /* set a type flag */
  call_frame->base.type_mask |= V7_CALL_FRAME_MASK_CFUNC;

  /* fill the new frame with data */
  call_frame->cfunc = cfunc;
  call_frame->vals.this_obj = this_obj;

  v7->call_stack = &call_frame->base;
}

/*
 * The caller's bcode object is needed because we have to restore literals
 * and `end` registers.
 *
 * TODO(mkm): put this state on a return stack
 *
 * Caller of bcode_perform_call is responsible for owning `call_frame`
 */
static enum v7_err bcode_perform_call(struct v7 *v7, v7_val_t scope_frame,
                                      struct v7_js_function *func,
                                      struct bcode_registers *r,
                                      val_t this_object, char *ops,
                                      uint8_t is_constructor) {
  /* new scope_frame will inherit from the function's scope */
  obj_prototype_set(v7, get_object_struct(scope_frame), &func->scope->base);

  /* create new `call_frame` which will replace `v7->call_stack` */
  append_call_frame_bcode(v7, r->ops + 1, func->bcode, this_object, scope_frame,
                          is_constructor);

  bcode_restore_registers(v7, func->bcode, r);

  /* adjust `ops` since names were already read from it */
  r->ops = ops;

  /* `ops` already points to the needed instruction, no need to increment it */
  r->need_inc_ops = 0;

  return V7_OK;
}

/*
 * Apply data from the "private" call frame, typically after some other frame
 * was just unwound.
 *
 * The `call_frame` may actually be `NULL`, if the top frame was unwound.
 */
static void apply_frame_private(struct v7 *v7,
                                struct v7_call_frame_private *call_frame) {
  /*
   * Adjust data stack length (restore saved).
   *
   * If `call_frame` is NULL, it means that the last call frame was just
   * unwound, and hence the data stack size should be 0.
   */
  size_t stack_size = (call_frame != NULL ? call_frame->stack_size : 0);
  assert(stack_size <= v7->stack.len);
  v7->stack.len = stack_size;
}

/*
 * Apply data from the "bcode" call frame, typically after some other frame
 * was just unwound.
 *
 * The `call_frame` may actually be `NULL`, if the top frame was unwound; but
 * in this case, `r` must be `NULL` too, by design. See inline comment below.
 */
static void apply_frame_bcode(struct v7 *v7,
                              struct v7_call_frame_bcode *call_frame,
                              struct bcode_registers *r) {
  if (r != NULL) {
    /*
     * Note: if `r` is non-NULL, then `call_frame` should be non-NULL as well,
     * by design. If this invariant is violated, it means that
     * `unwind_stack_1level()` is misused.
     */
    assert(call_frame != NULL);

    bcode_restore_registers(v7, call_frame->bcode, r);
    r->ops = call_frame->bcode_ops;
  }
}

/*
 * Unwinds `call_stack` by 1 frame.
 *
 * Returns the type of the unwound frame
 */
static v7_call_frame_mask_t unwind_stack_1level(struct v7 *v7,
                                                struct bcode_registers *r) {
  v7_call_frame_mask_t type_mask;
#ifdef V7_BCODE_TRACE
  fprintf(stderr, "unwinding stack by 1 level\n");
#endif

  type_mask = v7->call_stack->type_mask;

  /* drop the top frame */
  {
    struct v7_call_frame_base *tmp = v7->call_stack;
    v7->call_stack = v7->call_stack->prev;
    free(tmp);
  }

  /*
   * depending on the unwound frame type, apply data from the top call frame(s)
   * which are still alive (if any)
   */

  if (type_mask & V7_CALL_FRAME_MASK_PRIVATE) {
    apply_frame_private(v7, find_call_frame_private(v7));
  }

  if (type_mask & V7_CALL_FRAME_MASK_BCODE) {
    apply_frame_bcode(v7, find_call_frame_bcode(v7), r);
  }

  if (type_mask & V7_CALL_FRAME_MASK_CFUNC) {
    /* Nothing to do here at the moment */
  }

  return type_mask;
}

/*
 * Unwinds local "try stack" (i.e. local-to-current-function stack of nested
 * `try` blocks), looking for local-to-function blocks.
 *
 * Block types of interest are specified with `wanted_blocks_mask`: it's a
 * bitmask of `enum local_block` values.
 *
 * Only blocks of specified types will be considered, others will be dropped.
 *
 * If `restore_stack_size` is non-zero, the `v7->stack.len` will be restored
 * to the value saved when the block was created. This is useful when throwing,
 * since if we throw from the middle of the expression, the stack could have
 * any size. But you probably shouldn't set this flag when breaking and
 * returning, since it may hide real bugs in the opcode.
 *
 * Returns id of the block type that control was transferred into, or
 * `LOCAL_BLOCK_NONE` if no appropriate block was found. Note: returned value
 * contains at most 1 block bit; it can't contain multiple bits.
 */
static enum local_block unwind_local_blocks_stack(
    struct v7 *v7, struct bcode_registers *r, unsigned int wanted_blocks_mask,
    uint8_t restore_stack_size) {
  val_t arr = V7_UNDEFINED;
  struct gc_tmp_frame tf = new_tmp_frame(v7);
  enum local_block found_block = LOCAL_BLOCK_NONE;
  unsigned long length;

  tmp_stack_push(&tf, &arr);

  arr = find_call_frame_private(v7)->vals.try_stack;
  if (v7_is_array(v7, arr)) {
    /*
     * pop latest element from "try stack", loop until we need to transfer
     * control there
     */
    while ((length = v7_array_length(v7, arr)) > 0) {
      /* get latest offset from the "try stack" */
      int64_t offset = v7_get_double(v7, v7_array_get(v7, arr, length - 1));
      enum local_block cur_block = LOCAL_BLOCK_NONE;

      /* get id of the current block type */
      switch (LBLOCK_TAG(offset)) {
        case LBLOCK_TAG_CATCH:
          cur_block = LOCAL_BLOCK_CATCH;
          break;
        case LBLOCK_TAG_FINALLY:
          cur_block = LOCAL_BLOCK_FINALLY;
          break;
        case LBLOCK_TAG_LOOP:
          cur_block = LOCAL_BLOCK_LOOP;
          break;
        case LBLOCK_TAG_SWITCH:
          cur_block = LOCAL_BLOCK_SWITCH;
          break;
        default:
          assert(0);
          break;
      }

      if (cur_block & wanted_blocks_mask) {
        /* need to transfer control to this offset */
        r->ops = r->bcode->ops.p + LBLOCK_OFFSET(offset);
#ifdef V7_BCODE_TRACE
        fprintf(stderr, "transferring to block #%d: %u\n", (int) cur_block,
                (unsigned int) LBLOCK_OFFSET(offset));
#endif
        found_block = cur_block;
        /* if needed, restore stack size to the saved value */
        if (restore_stack_size) {
          v7->stack.len = LBLOCK_STACK_SIZE(offset);
        }
        break;
      } else {
#ifdef V7_BCODE_TRACE
        fprintf(stderr, "skipped block #%d: %u\n", (int) cur_block,
                (unsigned int) LBLOCK_OFFSET(offset));
#endif
        /*
         * since we don't need to control transfer there, just pop
         * it from the "try stack"
         */
        v7_array_del(v7, arr, length - 1);
      }
    }
  }

  tmp_frame_cleanup(&tf);
  return found_block;
}

/*
 * Perform break, if there is a `finally` block in effect, transfer
 * control there.
 */
static void bcode_perform_break(struct v7 *v7, struct bcode_registers *r) {
  enum local_block found;
  unsigned int mask;
  v7->is_breaking = 0;
  if (v7->is_continuing) {
    mask = LOCAL_BLOCK_LOOP;
  } else {
    mask = LOCAL_BLOCK_LOOP | LOCAL_BLOCK_SWITCH;
  }

  /*
   * Keep unwinding until we find local block of interest. We should not
   * encounter any "function" frames; only "private" frames are allowed.
   */
  for (;;) {
    /*
     * Try to unwind local "try stack", considering only `finally` and `break`.
     */
    found = unwind_local_blocks_stack(v7, r, mask | LOCAL_BLOCK_FINALLY, 0);
    if (found == LOCAL_BLOCK_NONE) {
      /*
       * no blocks found: this may happen if only the `break` or `continue` has
       * occurred inside "private" frame. So, unwind this frame, make sure it
       * is indeed a "private" frame, and keep unwinding local blocks.
       */
      v7_call_frame_mask_t frame_type_mask = unwind_stack_1level(v7, r);
      assert(frame_type_mask == V7_CALL_FRAME_MASK_PRIVATE);
      (void) frame_type_mask;
    } else {
      /* found some block to transfer control into, stop unwinding */
      break;
    }
  }

  /*
   * upon exit of a finally block we'll reenter here if is_breaking is true.
   * See OP_AFTER_FINALLY.
   */
  if (found == LOCAL_BLOCK_FINALLY) {
    v7->is_breaking = 1;
  }

  /* `ops` already points to the needed instruction, no need to increment it */
  r->need_inc_ops = 0;
}

/*
 * Perform return, but if there is a `finally` block in effect, transfer
 * control there.
 *
 * If `take_retval` is non-zero, value to return will be popped from stack
 * (and saved into `v7->vals.returned_value`), otherwise, it won't ae affected.
 */
static enum v7_err bcode_perform_return(struct v7 *v7,
                                        struct bcode_registers *r,
                                        int take_retval) {
  /*
   * We should either take retval from the stack, or some value should already
   * de pending to return
   */
  assert(take_retval || v7->is_returned);

  if (take_retval) {
    /* taking return value from stack */
    v7->vals.returned_value = POP();
    v7->is_returned = 1;

    /*
     * returning (say, from `finally`) dismisses any errors that are eeing
     * thrown at the moment as well
     */
    v7->is_thrown = 0;
    v7->vals.thrown_error = V7_UNDEFINED;
  }

  /*
   * Keep unwinding until we unwound "function" frame, or found some `finally`
   * block.
   */
  for (;;) {
    /* Try to unwind local "try stack", considering only `finally` blocks */
    if (unwind_local_blocks_stack(v7, r, (LOCAL_BLOCK_FINALLY), 0) ==
        LOCAL_BLOCK_NONE) {
      /*
       * no `finally` blocks were found, so, unwind stack by 1 level, and see
       * if it's a "function" frame. If not, will keep unwinding.
       */
      if (unwind_stack_1level(v7, r) & V7_CALL_FRAME_MASK_BCODE) {
        /*
         * unwound frame is a "function" frame, so, push returned value to
         * stack, and stop unwinding
         */
        PUSH(v7->vals.returned_value);
        v7->is_returned = 0;
        v7->vals.returned_value = V7_UNDEFINED;

        break;
      }
    } else {
      /* found `finally` block, so, stop unwinding */
      break;
    }
  }

  /* `ops` already points to the needed instruction, no need to increment it */
  r->need_inc_ops = 0;

  return V7_OK;
}

/*
 * Perform throw inside `eval_bcode()`.
 *
 * If `take_thrown_value` is non-zero, value to return will be popped from
 * stack (and saved into `v7->vals.thrown_error`), otherwise, it won't be
 * affected.
 *
 * Returns `V7_OK` if thrown exception was caught, `V7_EXEC_EXCEPTION`
 * otherwise (in this case, evaluation of current script must be stopped)
 *
 * When calling this function from `eval_rcode()`, you should wrap this call
 * into the `V7_TRY()` macro.
 */
static enum v7_err bcode_perform_throw(struct v7 *v7, struct bcode_registers *r,
                                       int take_thrown_value) {
  enum v7_err rcode = V7_OK;
  enum local_block found;

  assert(take_thrown_value || v7->is_thrown);

  if (take_thrown_value) {
    v7->vals.thrown_error = POP();
    v7->is_thrown = 1;

    /* Throwing dismisses any pending return values */
    v7->is_returned = 0;
    v7->vals.returned_value = V7_UNDEFINED;
  }

  while ((found = unwind_local_blocks_stack(
              v7, r, (LOCAL_BLOCK_CATCH | LOCAL_BLOCK_FINALLY), 1)) ==
         LOCAL_BLOCK_NONE) {
    if (v7->call_stack != v7->bottom_call_frame) {
#ifdef V7_BCODE_TRACE
      fprintf(stderr, "not at the bottom of the stack, going to unwind..\n");
#endif
      /* not reached bottom of the stack yet, keep unwinding */
      unwind_stack_1level(v7, r);
    } else {
/* reached stack bottom: uncaught exception */
#ifdef V7_BCODE_TRACE
      fprintf(stderr, "reached stack bottom: uncaught exception\n");
#endif
      rcode = V7_EXEC_EXCEPTION;
      break;
    }
  }

  if (found == LOCAL_BLOCK_CATCH) {
    /*
     * we're going to enter `catch` block, so, populate TOS with the thrown
     * value, and clear it in v7 context.
     */
    PUSH(v7->vals.thrown_error);
    v7->is_thrown = 0;
    v7->vals.thrown_error = V7_UNDEFINED;
  }

  /* `ops` already points to the needed instruction, no need to increment it */
  r->need_inc_ops = 0;

  return rcode;
}

/*
 * Throws reference error from `eval_bcode()`. Always wrap a call to this
 * function into `V7_TRY()`.
 */
static enum v7_err bcode_throw_reference_error(struct v7 *v7,
                                               struct bcode_registers *r,
                                               val_t var_name) {
  enum v7_err rcode = V7_OK;
  const char *s;
  size_t name_len;

  assert(v7_is_string(var_name));
  s = v7_get_string(v7, &var_name, &name_len);

  rcode = v7_throwf(v7, REFERENCE_ERROR, "[%.*s] is not defined",
                    (int) name_len, s);
  (void) rcode;
  return bcode_perform_throw(v7, r, 0);
}

/*
 * Takes a half-done function (either from literal table or deserialized from
 * `ops` inlined data), and returns a ready-to-use function.
 *
 * The actual behaviour depends on whether the half-done function has
 * `prototype` defined. If there's no prototype (i.e. it's `undefined`), then
 * the new function is created, with bcode from a given one. If, however,
 * the prototype is defined, it means that the function was just deserialized
 * from `ops`, so we only need to set `scope` on it.
 *
 * Assumes `func` is owned by the caller.
 */
static val_t bcode_instantiate_function(struct v7 *v7, val_t func) {
  val_t res;
  struct v7_generic_object *scope;
  struct v7_js_function *f;
  assert(is_js_function(func));
  f = get_js_function_struct(func);

  scope = get_generic_object_struct(get_scope(v7));

  if (v7_is_undefined(v7_get(v7, func, "prototype", 9))) {
    /*
     * Function's `prototype` is `undefined`: it means that the function is
     * created by the compiler and is stored in the literal table. We have to
     * create completely new function
     */
    struct v7_js_function *rf;

    res = mk_js_function(v7, scope, v7_mk_object(v7));

    /* Copy and retain bcode */
    rf = get_js_function_struct(res);
    rf->bcode = f->bcode;
    retain_bcode(v7, rf->bcode);
  } else {
    /*
     * Function's `prototype` is NOT `undefined`: it means that the function is
     * deserialized from inline `ops` data, and we just need to set scope on
     * it.
     */
    res = func;
    f->scope = scope;
  }

  return res;
}

/**
 * Call C function `func` with given `this_object` and array of arguments
 * `args`. `func` should be a C function pointer, not C function object.
 */
static enum v7_err call_cfunction(struct v7 *v7, val_t func, val_t this_object,
                                  val_t args, uint8_t is_constructor,
                                  val_t *res) {
  enum v7_err rcode = V7_OK;
  uint8_t saved_inhibit_gc = v7->inhibit_gc;
  val_t saved_arguments = v7->vals.arguments;
  struct gc_tmp_frame tf = new_tmp_frame(v7);
  v7_cfunction_t *cfunc = get_cfunction_ptr(v7, func);

  *res = V7_UNDEFINED;

  tmp_stack_push(&tf, &saved_arguments);

  append_call_frame_cfunc(v7, this_object, cfunc);

  /*
   * prepare cfunction environment
   */
  v7->inhibit_gc = 1;
  v7->vals.arguments = args;

  /* call C function */
  rcode = cfunc(v7, res);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (is_constructor && !v7_is_object(*res)) {
    /* constructor returned non-object: replace it with `this` */
    *res = v7_get_this(v7);
  }

clean:
  v7->vals.arguments = saved_arguments;
  v7->inhibit_gc = saved_inhibit_gc;

  unwind_stack_1level(v7, NULL);

  tmp_frame_cleanup(&tf);
  return rcode;
}

/*
 * Evaluate `OP_TRY_PUSH_CATCH` or `OP_TRY_PUSH_FINALLY`: Take an offset (from
 * the parameter of opcode) and push it onto "try stack"
 */
static void eval_try_push(struct v7 *v7, enum opcode op,
                          struct bcode_registers *r) {
  val_t arr = V7_UNDEFINED;
  struct gc_tmp_frame tf = new_tmp_frame(v7);
  bcode_off_t target;
  int64_t offset_tag = 0;

  tmp_stack_push(&tf, &arr);

  /* make sure "try stack" array exists */
  arr = find_call_frame_private(v7)->vals.try_stack;
  if (!v7_is_array(v7, arr)) {
    arr = v7_mk_dense_array(v7);
    find_call_frame_private(v7)->vals.try_stack = arr;
  }

  /*
   * push the target address at the end of the "try stack" array
   */
  switch (op) {
    case OP_TRY_PUSH_CATCH:
      offset_tag = LBLOCK_TAG_CATCH;
      break;
    case OP_TRY_PUSH_FINALLY:
      offset_tag = LBLOCK_TAG_FINALLY;
      break;
    case OP_TRY_PUSH_LOOP:
      offset_tag = LBLOCK_TAG_LOOP;
      break;
    case OP_TRY_PUSH_SWITCH:
      offset_tag = LBLOCK_TAG_SWITCH;
      break;
    default:
      assert(0);
      break;
  }
  target = bcode_get_target(&r->ops);
  v7_array_push(v7, arr, v7_mk_number(v7, LBLOCK_ITEM_CREATE(target, offset_tag,
                                                             v7->stack.len)));

  tmp_frame_cleanup(&tf);
}

/*
 * Evaluate `OP_TRY_POP`: just pop latest item from "try stack", ignoring it
 */
static enum v7_err eval_try_pop(struct v7 *v7) {
  enum v7_err rcode = V7_OK;
  val_t arr = V7_UNDEFINED;
  unsigned long length;
  struct gc_tmp_frame tf = new_tmp_frame(v7);

  tmp_stack_push(&tf, &arr);

  /* get "try stack" array, which must be defined and must not be emtpy */
  arr = find_call_frame_private(v7)->vals.try_stack;
  if (!v7_is_array(v7, arr)) {
    rcode = v7_throwf(v7, "Error", "TRY_POP when try_stack is not an array");
    V7_TRY(V7_INTERNAL_ERROR);
  }

  length = v7_array_length(v7, arr);
  if (length == 0) {
    rcode = v7_throwf(v7, "Error", "TRY_POP when try_stack is empty");
    V7_TRY(V7_INTERNAL_ERROR);
  }

  /* delete the latest element of this array */
  v7_array_del(v7, arr, length - 1);

clean:
  tmp_frame_cleanup(&tf);
  return rcode;
}

static void own_bcode(struct v7 *v7, struct bcode *p) {
  mbuf_append(&v7->act_bcodes, &p, sizeof(p));
}

static void disown_bcode(struct v7 *v7, struct bcode *p) {
#ifndef NDEBUG
  struct bcode **vp =
      (struct bcode **) (v7->act_bcodes.buf + v7->act_bcodes.len - sizeof(p));

  /* given `p` should be the last item */
  assert(*vp == p);
#endif
  v7->act_bcodes.len -= sizeof(p);
}

/* Keeps track of last evaluated bcodes in order to improve error reporting */
static void push_bcode_history(struct v7 *v7, enum opcode op) {
  size_t i;

  if (op == OP_CHECK_CALL || op == OP_CALL || op == OP_NEW) return;

  for (i = ARRAY_SIZE(v7->last_ops) - 1; i > 0; i--) {
    v7->last_ops[i] = v7->last_ops[i - 1];
  }
  v7->last_ops[0] = op;
}

#ifndef V7_DISABLE_CALL_ERROR_CONTEXT
static void reset_last_name(struct v7 *v7) {
  v7->vals.last_name[0] = V7_UNDEFINED;
  v7->vals.last_name[1] = V7_UNDEFINED;
}
#else
static void reset_last_name(struct v7 *v7) {
  /* should be inlined out */
  (void) v7;
}
#endif

/*
 * Evaluates given `bcode`. If `reset_line_no` is non-zero, the line number
 * is initially reset to 1; otherwise, it is inherited from the previous call
 * frame.
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err eval_bcode(struct v7 *v7, struct bcode *bcode,
                                  val_t this_object, uint8_t reset_line_no,
                                  val_t *_res) {
  struct bcode_registers r;
  enum v7_err rcode = V7_OK;
  struct v7_call_frame_base *saved_bottom_call_frame = v7->bottom_call_frame;

  /*
   * Dummy variable just to enforce that `BTRY()` macro is used only inside the
   * `eval_bcode()` function
   */
  uint8_t _you_should_use_BTRY_in_eval_bcode_only = 0;

  char buf[512];

  val_t res = V7_UNDEFINED, v1 = V7_UNDEFINED, v2 = V7_UNDEFINED,
        v3 = V7_UNDEFINED, v4 = V7_UNDEFINED, scope_frame = V7_UNDEFINED;
  struct gc_tmp_frame tf = new_tmp_frame(v7);

  append_call_frame_bcode(v7, NULL, bcode, this_object, get_scope(v7), 0);

  if (reset_line_no) {
    v7->call_stack->line_no = 1;
  }

  /*
   * Set current call stack as the "bottom" call stack, so that bcode evaluator
   * will exit when it reaches this "bottom"
   */
  v7->bottom_call_frame = v7->call_stack;

  bcode_restore_registers(v7, bcode, &r);

  tmp_stack_push(&tf, &res);
  tmp_stack_push(&tf, &v1);
  tmp_stack_push(&tf, &v2);
  tmp_stack_push(&tf, &v3);
  tmp_stack_push(&tf, &v4);
  tmp_stack_push(&tf, &scope_frame);

  /*
   * populate local variables on current scope, making them undeletable
   * (since they're defined with `var`)
   */
  {
    size_t i;
    for (i = 0; i < bcode->names_cnt; ++i) {
      r.ops = bcode_next_name_v(v7, bcode, r.ops, &v1);

      /* set undeletable property on current scope */
      V7_TRY(def_property_v(v7, get_scope(v7), v1, V7_DESC_CONFIGURABLE(0),
                            V7_UNDEFINED, 1 /*as_assign*/, NULL));
    }
  }

restart:
  while (r.ops < r.end && rcode == V7_OK) {
    enum opcode op = (enum opcode) * r.ops;

#ifndef V7_DISABLE_LINE_NUMBERS
    if ((uint8_t) op >= _OP_LINE_NO) {
      unsigned char buf[sizeof(size_t)];
      int len;
      size_t max_llen = sizeof(buf);

      /* ASAN doesn't like out of bound reads */
      if (r.ops + max_llen > r.end) {
        max_llen = r.end - r.ops;
      }

      /*
       * before we decode varint, we'll have to swap MSB and LSB, but we can't
       * do it in place since we're decoding from constant memory, so, we also
       * have to copy the data to the temp buffer first. 4 bytes should be
       * enough for everyone's line number.
       */
      memcpy(buf, r.ops, max_llen);
      buf[0] = msb_lsb_swap(buf[0]);

      v7->call_stack->line_no = decode_varint(buf, &len) >> 1;
      assert((size_t) len <= sizeof(buf));
      r.ops += len;

      continue;
    }
#endif

    push_bcode_history(v7, op);

    if (v7->need_gc) {
      maybe_gc(v7);
      v7->need_gc = 0;
    }

    r.need_inc_ops = 1;
#ifdef V7_BCODE_TRACE
    {
      char *dops = r.ops;
      fprintf(stderr, "eval ");
      dump_op(v7, stderr, r.bcode, &dops);
    }
#endif

    switch (op) {
      case OP_DROP:
        POP();
        break;
      case OP_DUP:
        v1 = POP();
        PUSH(v1);
        PUSH(v1);
        break;
      case OP_2DUP:
        v2 = POP();
        v1 = POP();
        PUSH(v1);
        PUSH(v2);
        PUSH(v1);
        PUSH(v2);
        break;
      case OP_SWAP:
        v1 = POP();
        v2 = POP();
        PUSH(v1);
        PUSH(v2);
        break;
      case OP_STASH:
        assert(!v7->is_stashed);
        v7->vals.stash = TOS();
        v7->is_stashed = 1;
        break;
      case OP_UNSTASH:
        assert(v7->is_stashed);
        POP();
        PUSH(v7->vals.stash);
        v7->vals.stash = V7_UNDEFINED;
        v7->is_stashed = 0;
        break;

      case OP_SWAP_DROP:
        v1 = POP();
        POP();
        PUSH(v1);
        break;

      case OP_PUSH_UNDEFINED:
        PUSH(V7_UNDEFINED);
        break;
      case OP_PUSH_NULL:
        PUSH(V7_NULL);
        break;
      case OP_PUSH_THIS:
        PUSH(v7_get_this(v7));
        reset_last_name(v7);
        break;
      case OP_PUSH_TRUE:
        PUSH(v7_mk_boolean(v7, 1));
        reset_last_name(v7);
        break;
      case OP_PUSH_FALSE:
        PUSH(v7_mk_boolean(v7, 0));
        reset_last_name(v7);
        break;
      case OP_PUSH_ZERO:
        PUSH(v7_mk_number(v7, 0));
        reset_last_name(v7);
        break;
      case OP_PUSH_ONE:
        PUSH(v7_mk_number(v7, 1));
        reset_last_name(v7);
        break;
      case OP_PUSH_LIT: {
        PUSH(bcode_decode_lit(v7, r.bcode, &r.ops));
#ifndef V7_DISABLE_CALL_ERROR_CONTEXT
        /* name tracking */
        if (!v7_is_string(TOS())) {
          reset_last_name(v7);
        }
#endif
        break;
      }
      case OP_LOGICAL_NOT:
        v1 = POP();
        PUSH(v7_mk_boolean(v7, !v7_is_truthy(v7, v1)));
        break;
      case OP_NOT: {
        v1 = POP();
        BTRY(to_number_v(v7, v1, &v1));
        PUSH(v7_mk_number(v7, ~(int32_t) v7_get_double(v7, v1)));
        break;
      }
      case OP_NEG: {
        v1 = POP();
        BTRY(to_number_v(v7, v1, &v1));
        PUSH(v7_mk_number(v7, -v7_get_double(v7, v1)));
        break;
      }
      case OP_POS: {
        v1 = POP();
        BTRY(to_number_v(v7, v1, &v1));
        PUSH(v1);
        break;
      }
      case OP_ADD: {
        v2 = POP();
        v1 = POP();

        /*
         * If either operand is an object, convert both of them to primitives
         */
        if (v7_is_object(v1) || v7_is_object(v2)) {
          BTRY(to_primitive(v7, v1, V7_TO_PRIMITIVE_HINT_AUTO, &v1));
          BTRY(to_primitive(v7, v2, V7_TO_PRIMITIVE_HINT_AUTO, &v2));
        }

        if (v7_is_string(v1) || v7_is_string(v2)) {
          /* Convert both operands to strings, and concatenate */

          BTRY(primitive_to_str(v7, v1, &v1, NULL, 0, NULL));
          BTRY(primitive_to_str(v7, v2, &v2, NULL, 0, NULL));

          PUSH(s_concat(v7, v1, v2));
        } else {
          /* Convert both operands to numbers, and sum */

          BTRY(primitive_to_number(v7, v1, &v1));
          BTRY(primitive_to_number(v7, v2, &v2));

          PUSH(v7_mk_number(v7, b_num_bin_op(op, v7_get_double(v7, v1),
                                             v7_get_double(v7, v2))));
        }
        break;
      }
      case OP_SUB:
      case OP_REM:
      case OP_MUL:
      case OP_DIV:
      case OP_LSHIFT:
      case OP_RSHIFT:
      case OP_URSHIFT:
      case OP_OR:
      case OP_XOR:
      case OP_AND: {
        v2 = POP();
        v1 = POP();

        BTRY(to_number_v(v7, v1, &v1));
        BTRY(to_number_v(v7, v2, &v2));

        PUSH(v7_mk_number(v7, b_num_bin_op(op, v7_get_double(v7, v1),
                                           v7_get_double(v7, v2))));
        break;
      }
      case OP_EQ_EQ: {
        v2 = POP();
        v1 = POP();
        if (v7_is_string(v1) && v7_is_string(v2)) {
          res = v7_mk_boolean(v7, s_cmp(v7, v1, v2) == 0);
        } else if (v1 == v2 && v1 == V7_TAG_NAN) {
          res = v7_mk_boolean(v7, 0);
        } else {
          res = v7_mk_boolean(v7, v1 == v2);
        }
        PUSH(res);
        break;
      }
      case OP_NE_NE: {
        v2 = POP();
        v1 = POP();
        if (v7_is_string(v1) && v7_is_string(v2)) {
          res = v7_mk_boolean(v7, s_cmp(v7, v1, v2) != 0);
        } else if (v1 == v2 && v1 == V7_TAG_NAN) {
          res = v7_mk_boolean(v7, 1);
        } else {
          res = v7_mk_boolean(v7, v1 != v2);
        }
        PUSH(res);
        break;
      }
      case OP_EQ:
      case OP_NE: {
        v2 = POP();
        v1 = POP();
        /*
         * TODO(dfrank) : it's not really correct. Fix it accordingly to
         * the p. 4.9 of The Definitive Guide (page 71)
         */
        if (((v7_is_object(v1) || v7_is_object(v2)) && v1 == v2)) {
          res = v7_mk_boolean(v7, op == OP_EQ);
          PUSH(res);
          break;
        } else if (v7_is_undefined(v1) || v7_is_null(v1)) {
          res = v7_mk_boolean(
              v7, (op != OP_EQ) ^ (v7_is_undefined(v2) || v7_is_null(v2)));
          PUSH(res);
          break;
        } else if (v7_is_undefined(v2) || v7_is_null(v2)) {
          res = v7_mk_boolean(
              v7, (op != OP_EQ) ^ (v7_is_undefined(v1) || v7_is_null(v1)));
          PUSH(res);
          break;
        }

        if (v7_is_string(v1) && v7_is_string(v2)) {
          int cmp = s_cmp(v7, v1, v2);
          switch (op) {
            case OP_EQ:
              res = v7_mk_boolean(v7, cmp == 0);
              break;
            case OP_NE:
              res = v7_mk_boolean(v7, cmp != 0);
              break;
            default:
              /* should never be here */
              assert(0);
          }
        } else {
          /* Convert both operands to numbers */

          BTRY(to_number_v(v7, v1, &v1));
          BTRY(to_number_v(v7, v2, &v2));

          res = v7_mk_boolean(v7, b_bool_bin_op(op, v7_get_double(v7, v1),
                                                v7_get_double(v7, v2)));
        }
        PUSH(res);
        break;
      }
      case OP_LT:
      case OP_LE:
      case OP_GT:
      case OP_GE: {
        v2 = POP();
        v1 = POP();
        BTRY(to_primitive(v7, v1, V7_TO_PRIMITIVE_HINT_NUMBER, &v1));
        BTRY(to_primitive(v7, v2, V7_TO_PRIMITIVE_HINT_NUMBER, &v2));

        if (v7_is_string(v1) && v7_is_string(v2)) {
          int cmp = s_cmp(v7, v1, v2);
          switch (op) {
            case OP_LT:
              res = v7_mk_boolean(v7, cmp < 0);
              break;
            case OP_LE:
              res = v7_mk_boolean(v7, cmp <= 0);
              break;
            case OP_GT:
              res = v7_mk_boolean(v7, cmp > 0);
              break;
            case OP_GE:
              res = v7_mk_boolean(v7, cmp >= 0);
              break;
            default:
              /* should never be here */
              assert(0);
          }
        } else {
          /* Convert both operands to numbers */

          BTRY(to_number_v(v7, v1, &v1));
          BTRY(to_number_v(v7, v2, &v2));

          res = v7_mk_boolean(v7, b_bool_bin_op(op, v7_get_double(v7, v1),
                                                v7_get_double(v7, v2)));
        }
        PUSH(res);
        break;
      }
      case OP_INSTANCEOF: {
        v2 = POP();
        v1 = POP();
        if (!v7_is_callable(v7, v2)) {
          BTRY(v7_throwf(v7, TYPE_ERROR,
                         "Expecting a function in instanceof check"));
          goto op_done;
        } else {
          PUSH(v7_mk_boolean(
              v7, is_prototype_of(v7, v1, v7_get(v7, v2, "prototype", 9))));
        }
        break;
      }
      case OP_TYPEOF:
        v1 = POP();
        switch (val_type(v7, v1)) {
          case V7_TYPE_NUMBER:
            res = v7_mk_string(v7, "number", 6, 1);
            break;
          case V7_TYPE_STRING:
            res = v7_mk_string(v7, "string", 6, 1);
            break;
          case V7_TYPE_BOOLEAN:
            res = v7_mk_string(v7, "boolean", 7, 1);
            break;
          case V7_TYPE_FUNCTION_OBJECT:
          case V7_TYPE_CFUNCTION_OBJECT:
          case V7_TYPE_CFUNCTION:
            res = v7_mk_string(v7, "function", 8, 1);
            break;
          case V7_TYPE_UNDEFINED:
            res = v7_mk_string(v7, "undefined", 9, 1);
            break;
          default:
            res = v7_mk_string(v7, "object", 6, 1);
            break;
        }
        PUSH(res);
        break;
      case OP_IN: {
        struct v7_property *prop = NULL;
        v2 = POP();
        v1 = POP();
        BTRY(to_string(v7, v1, NULL, buf, sizeof(buf), NULL));
        prop = v7_get_property(v7, v2, buf, ~0);
        PUSH(v7_mk_boolean(v7, prop != NULL));
      } break;
      case OP_GET:
        v2 = POP();
        v1 = POP();
        BTRY(v7_get_throwing_v(v7, v1, v2, &v3));
        PUSH(v3);
#ifndef V7_DISABLE_CALL_ERROR_CONTEXT
        v7->vals.last_name[1] = v7->vals.last_name[0];
        v7->vals.last_name[0] = v2;
#endif
        break;
      case OP_SET: {
        v3 = POP();
        v2 = POP();
        v1 = POP();

        /* convert name to string, if it's not already */
        BTRY(to_string(v7, v2, &v2, NULL, 0, NULL));

        /* set value */
        BTRY(set_property_v(v7, v1, v2, v3, NULL));

        PUSH(v3);
        break;
      }
      case OP_GET_VAR:
      case OP_SAFE_GET_VAR: {
        struct v7_property *p = NULL;
        assert(r.ops < r.end - 1);
        v1 = bcode_decode_lit(v7, r.bcode, &r.ops);
        BTRY(v7_get_property_v(v7, get_scope(v7), v1, &p));
        if (p == NULL) {
          if (op == OP_SAFE_GET_VAR) {
            PUSH(V7_UNDEFINED);
          } else {
            /* variable does not exist: Reference Error */
            V7_TRY(bcode_throw_reference_error(v7, &r, v1));
            goto op_done;
          }
          break;
        } else {
          BTRY(v7_property_value(v7, get_scope(v7), p, &v2));
          PUSH(v2);
        }
#ifndef V7_DISABLE_CALL_ERROR_CONTEXT
        v7->vals.last_name[0] = v1;
        v7->vals.last_name[1] = V7_UNDEFINED;
#endif
        break;
      }
      case OP_SET_VAR: {
        struct v7_property *prop;
        v3 = POP();
        v2 = bcode_decode_lit(v7, r.bcode, &r.ops);
        v1 = get_scope(v7);

        BTRY(to_string(v7, v2, NULL, buf, sizeof(buf), NULL));
        prop = v7_get_property(v7, v1, buf, strlen(buf));
        if (prop != NULL) {
          /* Property already exists: update its value */
          /*
           * TODO(dfrank): currently we can't use `def_property_v()` here,
           * because if the property was already found somewhere in the
           * prototype chain, then it should be updated, instead of creating a
           * new one on the top of the scope.
           *
           * Probably we need to make `def_property_v()` more generic and
           * use it here; or split `def_property_v()` into smaller pieces and
           * use one of them here.
           */
          if (!(prop->attributes & V7_PROPERTY_NON_WRITABLE)) {
            prop->value = v3;
          }
        } else if (!r.bcode->strict_mode) {
          /*
           * Property does not exist: since we're not in strict mode, let's
           * create new property at Global Object
           */
          BTRY(set_property_v(v7, v7_get_global(v7), v2, v3, NULL));
        } else {
          /*
           * In strict mode, throw reference error instead of polluting Global
           * Object
           */
          V7_TRY(bcode_throw_reference_error(v7, &r, v2));
          goto op_done;
        }
        PUSH(v3);
        break;
      }
      case OP_JMP: {
        bcode_off_t target = bcode_get_target(&r.ops);
        r.ops = r.bcode->ops.p + target - 1;
        break;
      }
      case OP_JMP_FALSE: {
        bcode_off_t target = bcode_get_target(&r.ops);
        v1 = POP();
        if (!v7_is_truthy(v7, v1)) {
          r.ops = r.bcode->ops.p + target - 1;
        }
        break;
      }
      case OP_JMP_TRUE: {
        bcode_off_t target = bcode_get_target(&r.ops);
        v1 = POP();
        if (v7_is_truthy(v7, v1)) {
          r.ops = r.bcode->ops.p + target - 1;
        }
        break;
      }
      case OP_JMP_TRUE_DROP: {
        bcode_off_t target = bcode_get_target(&r.ops);
        v1 = POP();
        if (v7_is_truthy(v7, v1)) {
          r.ops = r.bcode->ops.p + target - 1;
          v1 = POP();
          POP();
          PUSH(v1);
        }
        break;
      }
      case OP_JMP_IF_CONTINUE: {
        bcode_off_t target = bcode_get_target(&r.ops);
        if (v7->is_continuing) {
          r.ops = r.bcode->ops.p + target - 1;
        }
        v7->is_continuing = 0;
        break;
      }
      case OP_CREATE_OBJ:
        PUSH(v7_mk_object(v7));
        break;
      case OP_CREATE_ARR:
        PUSH(v7_mk_array(v7));
        break;
      case OP_NEXT_PROP: {
        void *h = NULL;
        v1 = POP(); /* handle */
        v2 = POP(); /* object */

        if (!v7_is_null(v1)) {
          h = v7_get_ptr(v7, v1);
        }

        if (v7_is_object(v2)) {
          v7_prop_attr_t attrs;
          do {
            /* iterate properties until we find a non-hidden enumerable one */
            do {
              h = v7_next_prop(h, v2, &res, NULL, &attrs);
            } while (h != NULL && (attrs & (_V7_PROPERTY_HIDDEN |
                                            V7_PROPERTY_NON_ENUMERABLE)));

            if (h == NULL) {
              /* no more properties in this object: proceed to the prototype */
              v2 = obj_prototype_v(v7, v2);
            }
          } while (h == NULL && get_generic_object_struct(v2) != NULL);
        }

        if (h == NULL) {
          PUSH(v7_mk_boolean(v7, 0));
        } else {
          PUSH(v2);
          PUSH(v7_mk_foreign(v7, h));
          PUSH(res);
          PUSH(v7_mk_boolean(v7, 1));
        }
        break;
      }
      case OP_FUNC_LIT: {
        v1 = POP();
        v2 = bcode_instantiate_function(v7, v1);
        PUSH(v2);
        break;
      }
      case OP_CHECK_CALL:
        v1 = TOS();
        if (!v7_is_callable(v7, v1)) {
          int arity = 0;
          enum v7_err ignore;
/* tried to call non-function object: throw a TypeError */

#ifndef V7_DISABLE_CALL_ERROR_CONTEXT
          /*
           * try to provide some useful context for the error message
           * using a good-enough heuristics
           * but defer actual throw when process the incriminated call
           * in order to evaluate the arguments as required by the spec.
           */
          if (v7->last_ops[0] == OP_GET_VAR) {
            arity = 1;
          } else if (v7->last_ops[0] == OP_GET &&
                     v7->last_ops[1] == OP_PUSH_LIT) {
            /*
             * OP_PUSH_LIT is used to both push property names for OP_GET
             * and for pushing actual literals. During PUSH_LIT push lit
             * evaluation we reset the last name variable in case the literal
             * is not a string, such as in `[].foo()`.
             * Unfortunately it doesn't handle `"foo".bar()`; could be
             * solved by adding another bytecode for property literals but
             * probably it doesn't matter much.
             */
            if (v7_is_undefined(v7->vals.last_name[1])) {
              arity = 1;
            } else {
              arity = 2;
            }
          }
#endif

          switch (arity) {
            case 0:
              ignore = v7_throwf(v7, TYPE_ERROR, "value is not a function");
              break;
#ifndef V7_DISABLE_CALL_ERROR_CONTEXT

            case 1:
              ignore = v7_throwf(v7, TYPE_ERROR, "%s is not a function",
                                 v7_get_cstring(v7, &v7->vals.last_name[0]));
              break;
            case 2:
              ignore = v7_throwf(v7, TYPE_ERROR, "%s.%s is not a function",
                                 v7_get_cstring(v7, &v7->vals.last_name[1]),
                                 v7_get_cstring(v7, &v7->vals.last_name[0]));
              break;
#endif
          };

          v7->vals.call_check_ex = v7->vals.thrown_error;
          v7_clear_thrown_value(v7);
          (void) ignore;
        }
        break;
      case OP_CALL:
      case OP_NEW: {
        /* Naive implementation pending stack frame redesign */
        int args = (int) *(++r.ops);
        uint8_t is_constructor = (op == OP_NEW);

        if (SP() < (args + 1 /*func*/ + 1 /*this*/)) {
          BTRY(v7_throwf(v7, INTERNAL_ERROR, "stack underflow"));
          goto op_done;
        } else {
          v2 = v7_mk_dense_array(v7);
          while (args > 0) {
            BTRY(v7_array_set_throwing(v7, v2, --args, POP(), NULL));
          }
          /* pop function to call */
          v1 = POP();

          /* pop `this` */
          v3 = POP();

          /*
           * adjust `this` if the function is called with the constructor
           * invocation pattern
           */
          if (is_constructor) {
            /*
             * The function is invoked as a constructor: we ignore `this`
             * value popped from stack, create new object and set prototype.
             */

            /*
             * get "prototype" property from the constructor function,
             * and make sure it's an object
             */
            v4 = v7_get(v7, v1 /*func*/, "prototype", 9);
            if (!v7_is_object(v4)) {
              /* TODO(dfrank): box primitive value */
              BTRY(v7_throwf(
                  v7, TYPE_ERROR,
                  "Cannot set a primitive value as object prototype"));
              goto op_done;
            } else if (is_cfunction_lite(v4)) {
              /*
               * TODO(dfrank): maybe add support for a cfunction pointer to be
               * a prototype
               */
              BTRY(v7_throwf(v7, TYPE_ERROR,
                             "Not implemented: cfunction as a prototype"));
              goto op_done;
            }

            /* create an object with given prototype */
            v3 = mk_object(v7, v4 /*prototype*/);
            v4 = V7_UNDEFINED;
          }

          if (!v7_is_callable(v7, v1)) {
            /* tried to call non-function object: throw a TypeError */
            BTRY(v7_throw(v7, v7->vals.call_check_ex));
            goto op_done;
          } else if (is_cfunction_lite(v1) || is_cfunction_obj(v7, v1)) {
            /* call cfunction */

            /*
             * In "function invocation pattern", the `this` value popped from
             * stack is an `undefined`. And in non-strict mode, we should change
             * it to global object.
             */
            if (!is_constructor && !r.bcode->strict_mode &&
                v7_is_undefined(v3)) {
              v3 = v7->vals.global_object;
            }

            BTRY(call_cfunction(v7, v1 /*func*/, v3 /*this*/, v2 /*args*/,
                                is_constructor, &v4));

            /* push value returned from C function to bcode stack */
            PUSH(v4);

          } else {
            char *ops;
            struct v7_js_function *func = get_js_function_struct(v1);

            /*
             * In "function invocation pattern", the `this` value popped from
             * stack is an `undefined`. And in non-strict mode, we should change
             * it to global object.
             */
            if (!is_constructor && !func->bcode->strict_mode &&
                v7_is_undefined(v3)) {
              v3 = v7->vals.global_object;
            }

            scope_frame = v7_mk_object(v7);

            /*
             * Before actual opcodes, `ops` contains one or more
             * null-terminated strings: first of all, the function name (if the
             * function is anonymous, it's an empty string).
             *
             * Then, argument names follow. We know number of arguments, so, we
             * know how many names to take.
             *
             * And then, local variable names follow. We know total number of
             * strings (`names_cnt`), so, again, we know how many names to
             * take.
             */

            ops = func->bcode->ops.p;

            /* populate function itself */
            ops = bcode_next_name_v(v7, func->bcode, ops, &v4);
            BTRY(def_property_v(v7, scope_frame, v4, V7_DESC_CONFIGURABLE(0),
                                v1, 0 /*not assign*/, NULL));

            /* populate arguments */
            {
              int arg_num;
              for (arg_num = 0; arg_num < func->bcode->args_cnt; ++arg_num) {
                ops = bcode_next_name_v(v7, func->bcode, ops, &v4);
                BTRY(def_property_v(
                    v7, scope_frame, v4, V7_DESC_CONFIGURABLE(0),
                    v7_array_get(v7, v2, arg_num), 0 /*not assign*/, NULL));
              }
            }

            /* populate `arguments` object */

            /*
             * TODO(dfrank): it's actually much more complicated than that:
             * it's not an array, it's an array-like object. More, in
             * non-strict mode, elements of `arguments` object are just aliases
             * for actual arguments, so this one:
             *
             *   `(function(a){arguments[0]=2; return a;})(1);`
             *
             * should yield 2. Currently, it yields 1.
             */
            v7_def(v7, scope_frame, "arguments", 9, V7_DESC_CONFIGURABLE(0),
                   v2);

            /* populate local variables */
            {
              uint8_t loc_num;
              uint8_t loc_cnt = func->bcode->names_cnt - func->bcode->args_cnt -
                                1 /*func name*/;
              for (loc_num = 0; loc_num < loc_cnt; ++loc_num) {
                ops = bcode_next_name_v(v7, func->bcode, ops, &v4);
                BTRY(def_property_v(v7, scope_frame, v4,
                                    V7_DESC_CONFIGURABLE(0), V7_UNDEFINED,
                                    0 /*not assign*/, NULL));
              }
            }

            /* transfer control to the function */
            V7_TRY(bcode_perform_call(v7, scope_frame, func, &r, v3 /*this*/,
                                      ops, is_constructor));

            scope_frame = V7_UNDEFINED;
          }
        }
        break;
      }
      case OP_RET:
        bcode_adjust_retval(v7, 1 /*explicit return*/);
        V7_TRY(bcode_perform_return(v7, &r, 1 /*take value from stack*/));
        break;
      case OP_DELETE:
      case OP_DELETE_VAR: {
        size_t name_len;
        struct v7_property *prop;

        res = v7_mk_boolean(v7, 1);

        /* pop property name to delete */
        v2 = POP();

        if (op == OP_DELETE) {
          /* pop object to delete the property from */
          v1 = POP();
        } else {
          /* use scope as an object to delete the property from */
          v1 = get_scope(v7);
        }

        if (!v7_is_object(v1)) {
          /*
           * the "object" to delete a property from is not actually an object
           * (at least this can happen with cfunction pointers), will just
           * return `true`
           */
          goto delete_clean;
        }

        BTRY(to_string(v7, v2, NULL, buf, sizeof(buf), &name_len));

        prop = v7_get_property(v7, v1, buf, name_len);
        if (prop == NULL) {
          /* not found a property; will just return `true` */
          goto delete_clean;
        }

        /* found needed property */

        if (prop->attributes & V7_PROPERTY_NON_CONFIGURABLE) {
          /*
           * this property is undeletable. In non-strict mode, we just
           * return `false`; otherwise, we throw.
           */
          if (!r.bcode->strict_mode) {
            res = v7_mk_boolean(v7, 0);
          } else {
            BTRY(v7_throwf(v7, TYPE_ERROR, "Cannot delete property '%s'", buf));
            goto op_done;
          }
        } else {
          /*
           * delete property: when we operate on the current scope, we should
           * walk the prototype chain when deleting property.
           *
           * But when we operate on a "real" object, we should delete own
           * properties only.
           */
          if (op == OP_DELETE) {
            v7_del(v7, v1, buf, name_len);
          } else {
            del_property_deep(v7, v1, buf, name_len);
          }
        }

      delete_clean:
        PUSH(res);
        break;
      }
      case OP_TRY_PUSH_CATCH:
      case OP_TRY_PUSH_FINALLY:
      case OP_TRY_PUSH_LOOP:
      case OP_TRY_PUSH_SWITCH:
        eval_try_push(v7, op, &r);
        break;
      case OP_TRY_POP:
        V7_TRY(eval_try_pop(v7));
        break;
      case OP_AFTER_FINALLY:
        /*
         * exited from `finally` block: if some value is currently being
         * returned, continue returning it.
         *
         * Likewise, if some value is currently being thrown, continue
         * unwinding stack.
         */
        if (v7->is_thrown) {
          V7_TRY(
              bcode_perform_throw(v7, &r, 0 /*don't take value from stack*/));
          goto op_done;
        } else if (v7->is_returned) {
          V7_TRY(
              bcode_perform_return(v7, &r, 0 /*don't take value from stack*/));
          break;
        } else if (v7->is_breaking) {
          bcode_perform_break(v7, &r);
        }
        break;
      case OP_THROW:
        V7_TRY(bcode_perform_throw(v7, &r, 1 /*take thrown value*/));
        goto op_done;
      case OP_BREAK:
        bcode_perform_break(v7, &r);
        break;
      case OP_CONTINUE:
        v7->is_continuing = 1;
        bcode_perform_break(v7, &r);
        break;
      case OP_ENTER_CATCH: {
        /* pop thrown value from stack */
        v1 = POP();
        /* get the name of the thrown value */
        v2 = bcode_decode_lit(v7, r.bcode, &r.ops);

        /*
         * create a new stack frame (a "private" one), and set exception
         * property on it
         */
        scope_frame = v7_mk_object(v7);
        BTRY(set_property_v(v7, scope_frame, v2, v1, NULL));

        /* Push this "private" frame on the call stack */

        /* new scope_frame will inherit from the current scope */

        obj_prototype_set(v7, get_object_struct(scope_frame),
                          get_object_struct(get_scope(v7)));

        /*
         * Create new `call_frame` which will replace `v7->call_stack`.
         */
        append_call_frame_private(v7, scope_frame);

        break;
      }
      case OP_EXIT_CATCH: {
        v7_call_frame_mask_t frame_type_mask;
        /* unwind 1 frame */
        frame_type_mask = unwind_stack_1level(v7, &r);
        /* make sure the unwound frame is a "private" frame */
        assert(frame_type_mask == V7_CALL_FRAME_MASK_PRIVATE);
#if defined(NDEBUG)
        (void) frame_type_mask;
#endif
        break;
      }
      default:
        BTRY(v7_throwf(v7, INTERNAL_ERROR, "Unknown opcode: %d", (int) op));
        goto op_done;
    }

  op_done:
#ifdef V7_BCODE_TRACE
    /* print current stack state */
    {
      char buf[40];
      char *str = v7_stringify(v7, TOS(), buf, sizeof(buf), V7_STRINGIFY_DEBUG);
      fprintf(stderr, "        stack size: %u, TOS: '%s'\n",
              (unsigned int) (v7->stack.len / sizeof(val_t)), str);
      if (str != buf) {
        free(str);
      }

#ifdef V7_BCODE_TRACE_STACK
      {
        size_t i;
        for (i = 0; i < (v7->stack.len / sizeof(val_t)); i++) {
          char *str = v7_stringify(v7, stack_at(&v7->stack, i), buf,
                                   sizeof(buf), V7_STRINGIFY_DEBUG);

          fprintf(stderr, "        #: '%s'\n", str);

          if (str != buf) {
            free(str);
          }
        }
      }
#endif
    }
#endif
    if (r.need_inc_ops) {
      r.ops++;
    }
  }

  /* implicit return */
  if (v7->call_stack != v7->bottom_call_frame) {
#ifdef V7_BCODE_TRACE
    fprintf(stderr, "return implicitly\n");
#endif
    bcode_adjust_retval(v7, 0 /*implicit return*/);
    V7_TRY(bcode_perform_return(v7, &r, 1));
    goto restart;
  } else {
#ifdef V7_BCODE_TRACE
    const char *s = (get_scope(v7) != v7->vals.global_object)
                        ? "not global object"
                        : "global object";
    fprintf(stderr, "reached bottom_call_frame (%s)\n", s);
#endif
  }

clean:

  if (rcode == V7_OK) {
/*
 * bcode evaluated successfully. Make sure try stack is empty.
 * (data stack will be checked below, in `clean`)
 */
#ifndef NDEBUG
    {
      unsigned long try_stack_len =
          v7_array_length(v7, find_call_frame_private(v7)->vals.try_stack);
      if (try_stack_len != 0) {
        fprintf(stderr, "try_stack_len=%lu, should be 0\n", try_stack_len);
      }
      assert(try_stack_len == 0);
    }
#endif

    /* get the value returned from the evaluated script */
    *_res = POP();
  }

  assert(v7->bottom_call_frame == v7->call_stack);
  unwind_stack_1level(v7, NULL);

  v7->bottom_call_frame = saved_bottom_call_frame;

  tmp_frame_cleanup(&tf);
  return rcode;
}

/*
 * TODO(dfrank) this function is probably too overloaded: it handles both
 * `v7_exec` and `v7_apply`. Read below why it's written this way, but it's
 * probably a good idea to factor out common functionality in some other
 * function.
 *
 * If `src` is not `NULL`, then we behave in favour of `v7_exec`: parse,
 * compile, and evaluate the script. The `func` and `args` are ignored.
 *
 * If, however, `src` is `NULL`, then we behave in favour of `v7_apply`: we
 * call the provided `func` with `args`. But unlike interpreter, we can't just
 * call the provided function: we need to setup environment for this call.
 *
 * Currently, we just quickly generate the "wrapper" bcode for the function.
 * This wrapper bcode looks like this:
 *
 *    OP_PUSH_UNDEFINED
 *    OP_PUSH_LIT       # push this
 *    OP_PUSH_LIT       # push function
 *    OP_PUSH_LIT       # push arg1
 *    OP_PUSH_LIT       # push arg2
 *    ...
 *    OP_PUSH_LIT       # push argN
 *    OP_CALL(N)        # call function with N arguments
 *    OP_SWAP_DROP
 *
 * and then, bcode evaluator proceeds with this code.
 *
 * In fact, both cases (eval or apply) are quite similar: we should prepare
 * environment for the bcode evaluation in exactly the same way, and the only
 * different part is where we get the bcode from. This is why that
 * functionality is baked in the single function, but it would be good to make
 * it suck less.
 */
V7_PRIVATE enum v7_err b_exec(struct v7 *v7, const char *src, size_t src_len,
                              const char *filename, val_t func, val_t args,
                              val_t this_object, int is_json, int fr,
                              uint8_t is_constructor, val_t *res) {
#if defined(V7_BCODE_TRACE_SRC)
  fprintf(stderr, "src:'%s'\n", src);
#endif

  /* TODO(mkm): use GC pool */
  struct ast *a = (struct ast *) malloc(sizeof(struct ast));
  size_t saved_stack_len = v7->stack.len;
  enum v7_err rcode = V7_OK;
  val_t _res = V7_UNDEFINED;
  struct gc_tmp_frame tf = new_tmp_frame(v7);
  struct bcode *bcode = NULL;
#if defined(V7_ENABLE_STACK_TRACKING)
  struct stack_track_ctx stack_track_ctx;
#endif
  struct {
    unsigned noopt : 1;
    unsigned line_no_reset : 1;
  } flags = {0, 0};

  (void) filename;

#if defined(V7_ENABLE_STACK_TRACKING)
  v7_stack_track_start(v7, &stack_track_ctx);
#endif

  tmp_stack_push(&tf, &func);
  tmp_stack_push(&tf, &args);
  tmp_stack_push(&tf, &this_object);
  tmp_stack_push(&tf, &_res);

  /* init new bcode */
  bcode = (struct bcode *) calloc(1, sizeof(*bcode));

  bcode_init(bcode,
#ifndef V7_FORCE_STRICT_MODE
             0,
#else
             1,
#endif
#ifndef V7_DISABLE_FILENAMES
             filename ? shdata_create_from_string(filename) : NULL,
#else
             NULL,
#endif
             0 /*filename not in ROM*/
             );

  retain_bcode(v7, bcode);
  own_bcode(v7, bcode);

  ast_init(a, 0);
  a->refcnt = 1;

  if (src != NULL) {
    /* Caller provided some source code, so, handle it somehow */

    flags.line_no_reset = 1;

    if (src_len >= sizeof(BIN_BCODE_SIGNATURE) &&
        strncmp(BIN_BCODE_SIGNATURE, src, sizeof(BIN_BCODE_SIGNATURE)) == 0) {
      /* we have a serialized bcode */

      bcode_deserialize(v7, bcode, src + sizeof(BIN_BCODE_SIGNATURE));

      /*
       * Currently, we only support serialized bcode that is stored in some
       * mmapped memory. Otherwise, we don't yet have any mechanism to free
       * this memory at the appropriate time.
       */
      assert(fr == 0);
    } else {
      /* Maybe regular JavaScript source or binary AST data */

      if (src_len >= sizeof(BIN_AST_SIGNATURE) &&
          strncmp(BIN_AST_SIGNATURE, src, sizeof(BIN_AST_SIGNATURE)) == 0) {
        /* we have binary AST data */

        if (fr == 0) {
          /* Unmanaged memory, usually rom or mmapped flash */
          mbuf_free(&a->mbuf);
          a->mbuf.buf = (char *) (src + sizeof(BIN_AST_SIGNATURE));
          a->mbuf.size = a->mbuf.len = src_len - sizeof(BIN_AST_SIGNATURE);
          a->refcnt++; /* prevent freeing */
          flags.noopt = 1;
        } else {
          mbuf_append(&a->mbuf, src + sizeof(BIN_AST_SIGNATURE),
                      src_len - sizeof(BIN_AST_SIGNATURE));
        }
      } else {
        /* we have regular JavaScript source, so, parse it */
        V7_TRY(parse(v7, a, src, src_len, is_json));
      }

      /* we now have binary AST, let's compile it */

      if (!flags.noopt) {
        ast_optimize(a);
      }
#if V7_ENABLE__Memory__stats
      v7->function_arena_ast_size += a->mbuf.size;
#endif

      if (v7_is_undefined(this_object)) {
        this_object = v7->vals.global_object;
      }

      if (!is_json) {
        V7_TRY(compile_script(v7, a, bcode));
      } else {
        ast_off_t pos = 0;
        V7_TRY(compile_expr(v7, a, &pos, bcode));
      }
    }

  } else if (is_js_function(func)) {
    /*
     * Caller did not provide source code, so, assume we should call
     * provided function. Here, we prepare "wrapper" bcode.
     */

    struct bcode_builder bbuilder;
    lit_t lit;
    int args_cnt = v7_array_length(v7, args);

    bcode_builder_init(v7, &bbuilder, bcode);

    bcode_op(&bbuilder, OP_PUSH_UNDEFINED);

    /* push `this` */
    lit = bcode_add_lit(&bbuilder, this_object);
    bcode_push_lit(&bbuilder, lit);

    /* push func literal */
    lit = bcode_add_lit(&bbuilder, func);
    bcode_push_lit(&bbuilder, lit);

    /* push args */
    {
      int i;
      for (i = 0; i < args_cnt; i++) {
        lit = bcode_add_lit(&bbuilder, v7_array_get(v7, args, i));
        bcode_push_lit(&bbuilder, lit);
      }
    }

    bcode_op(&bbuilder, OP_CALL);
    /* TODO(dfrank): check if args <= 0x7f */
    bcode_op(&bbuilder, (uint8_t) args_cnt);

    bcode_op(&bbuilder, OP_SWAP_DROP);

    bcode_builder_finalize(&bbuilder);
  } else if (is_cfunction_lite(func) || is_cfunction_obj(v7, func)) {
    /* call cfunction */

    V7_TRY(call_cfunction(v7, func, this_object, args, is_constructor, &_res));

    goto clean;
  } else {
    /* value is not a function */
    V7_TRY(v7_throwf(v7, TYPE_ERROR, "value is not a function"));
  }

  /* We now have bcode to evaluate; proceed to it */

  /*
   * Before we evaluate bcode, we can safely release AST since it's not needed
   * anymore. Note that there's no leak here: if we `goto clean` from somewhere
   * above, we'll anyway release the AST under `clean` as well.
   */
  release_ast(v7, a);
  a = NULL;

  /* Evaluate bcode */
  V7_TRY(eval_bcode(v7, bcode, this_object, flags.line_no_reset, &_res));

clean:

  /* free `src` if needed */
  /*
   * TODO(dfrank) : free it above, just after parsing, and make sure you use
   * V7_TRY2() with custom label instead of V7_TRY()
   */
  if (src != NULL && fr) {
    free((void *) src);
  }

  /* disown and release current bcode */
  disown_bcode(v7, bcode);
  release_bcode(v7, bcode);
  bcode = NULL;

  if (rcode != V7_OK) {
    /* some exception happened. */
    _res = v7->vals.thrown_error;

    /*
     * if this is a top-level bcode, clear thrown error from the v7 context
     *
     * TODO(dfrank): do we really need to do this?
     *
     * If we don't clear the error, then we should clear it manually after each
     * call to v7_exec() or friends; otherwise, all the following calls will
     * see this error.
     *
     * On the other hand, user would still need to clear the error if he calls
     * v7_exec() from some cfunction. So, currently, sometimes we don't need
     * to clear the error, and sometimes we do, which is confusing.
     */
    if (v7->act_bcodes.len == 0) {
      v7->vals.thrown_error = V7_UNDEFINED;
      v7->is_thrown = 0;
    }
  }

  /*
   * Data stack should have the same length as it was before evaluating script.
   */
  if (v7->stack.len != saved_stack_len) {
    fprintf(stderr, "len=%d, saved=%d\n", (int) v7->stack.len,
            (int) saved_stack_len);
  }
  assert(v7->stack.len == saved_stack_len);

  /*
   * release AST if needed (normally, it's already released above, before
   * bcode evaluation)
   */
  if (a != NULL) {
    release_ast(v7, a);
    a = NULL;
  }

  if (is_constructor && !v7_is_object(_res)) {
    /* constructor returned non-object: replace it with `this` */
    _res = v7_get_this(v7);
  }

  /* Provide the caller with the result, if asked to do so */
  if (res != NULL) {
    *res = _res;
  }

#if defined(V7_ENABLE_STACK_TRACKING)
  {
    int diff = v7_stack_track_end(v7, &stack_track_ctx);
    if (diff > v7->stack_stat[V7_STACK_STAT_EXEC]) {
      v7->stack_stat[V7_STACK_STAT_EXEC] = diff;
    }
  }
#endif

  tmp_frame_cleanup(&tf);
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err b_apply(struct v7 *v7, v7_val_t func, v7_val_t this_obj,
                               v7_val_t args, uint8_t is_constructor,
                               v7_val_t *res) {
  return b_exec(v7, NULL, 0, NULL, func, args, this_obj, 0, 0, is_constructor,
                res);
}
