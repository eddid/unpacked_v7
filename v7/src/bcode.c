/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/bcode.h"
#include "v7/src/varint.h"
#include "v7/src/exceptions.h"
#include "v7/src/gc.h"
#include "v7/src/core.h"
#include "v7/src/regexp.h"
#include "v7/src/function.h"
#include "v7/src/util.h"
#include "v7/src/shdata.h"

/*
 * TODO(dfrank): implement `bcode_serialize_*` more generically, so that they
 * can write to buffer instead of a `FILE`. Then, remove a need for mmap here.
 */
#if CS_PLATFORM == CS_P_UNIX
#include <sys/mman.h>
#endif

#if defined(V7_BCODE_DUMP) || defined(V7_BCODE_TRACE)
/* clang-format off */
static const char *op_names[] = {
  "DROP",
  "DUP",
  "2DUP",
  "SWAP",
  "STASH",
  "UNSTASH",
  "SWAP_DROP",
  "PUSH_UNDEFINED",
  "PUSH_NULL",
  "PUSH_THIS",
  "PUSH_TRUE",
  "PUSH_FALSE",
  "PUSH_ZERO",
  "PUSH_ONE",
  "PUSH_LIT",
  "NOT",
  "LOGICAL_NOT",
  "NEG",
  "POS",
  "ADD",
  "SUB",
  "REM",
  "MUL",
  "DIV",
  "LSHIFT",
  "RSHIFT",
  "URSHIFT",
  "OR",
  "XOR",
  "AND",
  "EQ_EQ",
  "EQ",
  "NE",
  "NE_NE",
  "LT",
  "LE",
  "GT",
  "GE",
  "INSTANCEOF",
  "TYPEOF",
  "IN",
  "GET",
  "SET",
  "SET_VAR",
  "GET_VAR",
  "SAFE_GET_VAR",
  "JMP",
  "JMP_TRUE",
  "JMP_FALSE",
  "JMP_TRUE_DROP",
  "JMP_IF_CONTINUE",
  "CREATE_OBJ",
  "CREATE_ARR",
  "NEXT_PROP",
  "FUNC_LIT",
  "CALL",
  "NEW",
  "CHECK_CALL",
  "RET",
  "DELETE",
  "DELETE_VAR",
  "TRY_PUSH_CATCH",
  "TRY_PUSH_FINALLY",
  "TRY_PUSH_LOOP",
  "TRY_PUSH_SWITCH",
  "TRY_POP",
  "AFTER_FINALLY",
  "THROW",
  "BREAK",
  "CONTINUE",
  "ENTER_CATCH",
  "EXIT_CATCH",
};
/* clang-format on */

V7_STATIC_ASSERT(OP_MAX == ARRAY_SIZE(op_names), bad_op_names);
V7_STATIC_ASSERT(OP_MAX <= _OP_LINE_NO, bad_OP_LINE_NO);
#endif

static void bcode_serialize_func(struct v7 *v7, struct bcode *bcode, FILE *out);

static size_t bcode_ops_append(struct bcode_builder *bbuilder, const void *buf,
                               size_t len) {
  size_t ret;
#if V7_ENABLE__Memory__stats
  bbuilder->v7->bcode_ops_size -= bbuilder->ops.len;
#endif
  ret = mbuf_append(&bbuilder->ops, buf, len);
#if V7_ENABLE__Memory__stats
  bbuilder->v7->bcode_ops_size += bbuilder->ops.len;
#endif
  return ret;
}

/*
 * Initialize bcode builder. The `bcode` should be already initialized by the
 * caller, and should be empty (i.e. should not own any ops, literals, etc)
 *
 * TODO(dfrank) : probably make `bcode_builder_init()` to initialize `bcode`
 * as well
 */
V7_PRIVATE void bcode_builder_init(struct v7 *v7,
                                   struct bcode_builder *bbuilder,
                                   struct bcode *bcode) {
  memset(bbuilder, 0x00, sizeof(*bbuilder));
  bbuilder->v7 = v7;
  bbuilder->bcode = bcode;

  mbuf_init(&bbuilder->ops, 0);
  mbuf_init(&bbuilder->lit, 0);
}

/*
 * Finalize bcode builder: propagate data to the bcode and transfer the
 * ownership from builder to bcode
 */
V7_PRIVATE void bcode_builder_finalize(struct bcode_builder *bbuilder) {
  mbuf_trim(&bbuilder->ops);
  bbuilder->bcode->ops.p = bbuilder->ops.buf;
  bbuilder->bcode->ops.len = bbuilder->ops.len;
  mbuf_init(&bbuilder->ops, 0);

  mbuf_trim(&bbuilder->lit);
  bbuilder->bcode->lit.p = bbuilder->lit.buf;
  bbuilder->bcode->lit.len = bbuilder->lit.len;
  mbuf_init(&bbuilder->lit, 0);

  memset(bbuilder, 0x00, sizeof(*bbuilder));
}

#if defined(V7_BCODE_DUMP) || defined(V7_BCODE_TRACE)
V7_PRIVATE void dump_op(struct v7 *v7, FILE *f, struct bcode *bcode,
                        char **ops) {
  char *p = *ops;

  assert(*p < OP_MAX);
  fprintf(f, "%zu: %s", (size_t)(p - bcode->ops.p), op_names[(uint8_t) *p]);
  switch (*p) {
    case OP_PUSH_LIT:
    case OP_SAFE_GET_VAR:
    case OP_GET_VAR:
    case OP_SET_VAR: {
      size_t idx = bcode_get_varint(&p);
      fprintf(f, "(%lu): ", (unsigned long) idx);
      v7_fprint(f, v7, ((val_t *) bcode->lit.p)[idx]);
      break;
    }
    case OP_CALL:
    case OP_NEW:
      p++;
      fprintf(f, "(%d)", *p);
      break;
    case OP_JMP:
    case OP_JMP_FALSE:
    case OP_JMP_TRUE:
    case OP_JMP_TRUE_DROP:
    case OP_JMP_IF_CONTINUE:
    case OP_TRY_PUSH_CATCH:
    case OP_TRY_PUSH_FINALLY:
    case OP_TRY_PUSH_LOOP:
    case OP_TRY_PUSH_SWITCH: {
      bcode_off_t target;
      p++;
      memcpy(&target, p, sizeof(target));
      fprintf(f, "(%lu)", (unsigned long) target);
      p += sizeof(target) - 1;
      break;
    }
    default:
      break;
  }
  fprintf(f, "\n");
  *ops = p;
}
#endif

#ifdef V7_BCODE_DUMP
V7_PRIVATE void dump_bcode(struct v7 *v7, FILE *f, struct bcode *bcode) {
  char *p = bcode_end_names(bcode->ops.p, bcode->names_cnt);
  char *end = bcode->ops.p + bcode->ops.len;
  for (; p < end; p++) {
    dump_op(v7, f, bcode, &p);
  }
}
#endif

V7_PRIVATE void bcode_init(struct bcode *bcode, uint8_t strict_mode,
                           void *filename, uint8_t filename_in_rom) {
  memset(bcode, 0x00, sizeof(*bcode));
  bcode->refcnt = 0;
  bcode->args_cnt = 0;
  bcode->strict_mode = strict_mode;
#ifndef V7_DISABLE_FILENAMES
  bcode->filename = filename;
  bcode->filename_in_rom = filename_in_rom;
#else
  (void) filename;
  (void) filename_in_rom;
#endif
}

V7_PRIVATE void bcode_free(struct v7 *v7, struct bcode *bcode) {
  (void) v7;
#if V7_ENABLE__Memory__stats
  if (!bcode->ops_in_rom) {
    v7->bcode_ops_size -= bcode->ops.len;
  }

  v7->bcode_lit_total_size -= bcode->lit.len;
  if (bcode->deserialized) {
    v7->bcode_lit_deser_size -= bcode->lit.len;
  }
#endif

  if (!bcode->ops_in_rom) {
    free(bcode->ops.p);
  }
  memset(&bcode->ops, 0x00, sizeof(bcode->ops));

  free(bcode->lit.p);
  memset(&bcode->lit, 0x00, sizeof(bcode->lit));

#ifndef V7_DISABLE_FILENAMES
  if (!bcode->filename_in_rom && bcode->filename != NULL) {
    shdata_release((struct shdata *) bcode->filename);
    bcode->filename = NULL;
  }
#endif

  bcode->refcnt = 0;
}

V7_PRIVATE void retain_bcode(struct v7 *v7, struct bcode *b) {
  (void) v7;
  if (!b->frozen) {
    b->refcnt++;
  }
}

V7_PRIVATE void release_bcode(struct v7 *v7, struct bcode *b) {
  (void) v7;
  if (b->frozen) return;

  assert(b->refcnt > 0);
  if (b->refcnt != 0) b->refcnt--;

  if (b->refcnt == 0) {
    bcode_free(v7, b);
    free(b);
  }
}

#ifndef V7_DISABLE_FILENAMES
V7_PRIVATE const char *bcode_get_filename(struct bcode *bcode) {
  const char *ret = NULL;
  if (bcode->filename_in_rom) {
    ret = (const char *) bcode->filename;
  } else if (bcode->filename != NULL) {
    ret = (const char *) shdata_get_payload((struct shdata *) bcode->filename);
  }
  return ret;
}
#endif

V7_PRIVATE void bcode_copy_filename_from(struct bcode *dst, struct bcode *src) {
#ifndef V7_DISABLE_FILENAMES
  dst->filename_in_rom = src->filename_in_rom;
  dst->filename = src->filename;

  if (src->filename != NULL && !src->filename_in_rom) {
    shdata_retain((struct shdata *) dst->filename);
  }
#else
  (void) dst;
  (void) src;
#endif
}

V7_PRIVATE void bcode_op(struct bcode_builder *bbuilder, uint8_t op) {
  bcode_ops_append(bbuilder, &op, 1);
}

#ifndef V7_DISABLE_LINE_NUMBERS
V7_PRIVATE void bcode_append_lineno(struct bcode_builder *bbuilder,
                                    int line_no) {
  int offset = bbuilder->ops.len;
  bcode_add_varint(bbuilder, (line_no << 1) | 1);
  bbuilder->ops.buf[offset] = msb_lsb_swap(bbuilder->ops.buf[offset]);
  assert(bbuilder->ops.buf[offset] & _OP_LINE_NO);
}
#endif

/*
 * Appends varint-encoded integer to the `ops` mbuf
 */
V7_PRIVATE void bcode_add_varint(struct bcode_builder *bbuilder, size_t value) {
  int k = calc_llen(value); /* Calculate how many bytes length takes */
  int offset = bbuilder->ops.len;

  /* Allocate buffer */
  bcode_ops_append(bbuilder, NULL, k);

  /* Write value */
  encode_varint(value, (unsigned char *) bbuilder->ops.buf + offset);
}

V7_PRIVATE size_t bcode_get_varint(char **ops) {
  size_t ret = 0;
  int len = 0;
  (*ops)++;
  ret = decode_varint((unsigned char *) *ops, &len);
  *ops += len - 1;
  return ret;
}

static int bcode_is_inline_string(struct v7 *v7, val_t val) {
  uint64_t tag = val & V7_TAG_MASK;
  if (v7->is_precompiling && v7_is_string(val)) {
    return 1;
  }
  return tag == V7_TAG_STRING_I || tag == V7_TAG_STRING_5;
}

static int bcode_is_inline_func(struct v7 *v7, val_t val) {
  return (v7->is_precompiling && is_js_function(val));
}

static int bcode_is_inline_regexp(struct v7 *v7, val_t val) {
  return (v7->is_precompiling && v7_is_regexp(v7, val));
}

V7_PRIVATE lit_t bcode_add_lit(struct bcode_builder *bbuilder, val_t val) {
  lit_t lit;
  memset(&lit, 0, sizeof(lit));

  if (bcode_is_inline_string(bbuilder->v7, val) ||
      bcode_is_inline_func(bbuilder->v7, val) || v7_is_number(val) ||
      bcode_is_inline_regexp(bbuilder->v7, val)) {
    /* literal should be inlined (it's `bcode_op_lit()` who does this) */
    lit.mode = LIT_MODE__INLINED;
    lit.v.inline_val = val;
  } else {
    /* literal will now be added to the literal table */
    lit.mode = LIT_MODE__TABLE;
    lit.v.lit_idx = bbuilder->lit.len / sizeof(val);

#if V7_ENABLE__Memory__stats
    bbuilder->v7->bcode_lit_total_size -= bbuilder->lit.len;
    if (bbuilder->bcode->deserialized) {
      bbuilder->v7->bcode_lit_deser_size -= bbuilder->lit.len;
    }
#endif

    mbuf_append(&bbuilder->lit, &val, sizeof(val));

    /*
     * immediately propagate current lit buffer to the bcode, so that GC will
     * be aware of it
     */
    bbuilder->bcode->lit.p = bbuilder->lit.buf;
    bbuilder->bcode->lit.len = bbuilder->lit.len;

#if V7_ENABLE__Memory__stats
    bbuilder->v7->bcode_lit_total_size += bbuilder->lit.len;
    if (bbuilder->bcode->deserialized) {
      bbuilder->v7->bcode_lit_deser_size += bbuilder->lit.len;
    }
#endif
  }
  return lit;
}

#if 0
V7_PRIVATE v7_val_t bcode_get_lit(struct bcode *bcode, size_t idx) {
  val_t ret;
  memcpy(&ret, bcode->lit.p + (size_t) idx * sizeof(ret), sizeof(ret));
  return ret;
}
#endif

static const char *bcode_deserialize_func(struct v7 *v7, struct bcode *bcode,
                                          const char *data);

V7_PRIVATE v7_val_t
bcode_decode_lit(struct v7 *v7, struct bcode *bcode, char **ops) {
  struct v7_vec *vec = &bcode->lit;
  size_t idx = bcode_get_varint(ops);
  switch (idx) {
    case BCODE_INLINE_STRING_TYPE_TAG: {
      val_t res;
      size_t len = bcode_get_varint(ops);
      res = v7_mk_string(
          v7, (const char *) *ops + 1 /*skip BCODE_INLINE_STRING_TYPE_TAG*/,
          len, !bcode->ops_in_rom);
      *ops += len + 1;
      return res;
    }
    case BCODE_INLINE_NUMBER_TYPE_TAG: {
      val_t res;
      memcpy(&res, *ops + 1 /*skip BCODE_INLINE_NUMBER_TYPE_TAG*/, sizeof(res));
      *ops += sizeof(res);
      return res;
    }
    case BCODE_INLINE_FUNC_TYPE_TAG: {
      /*
       * Create half-done function: without scope but _with_ prototype. Scope
       * will be set by `bcode_instantiate_function()`.
       *
       * The fact that the prototype is already set will make
       * `bcode_instantiate_function()` just set scope on this function,
       * instead of creating a new one.
       */
      val_t res = mk_js_function(v7, NULL, v7_mk_object(v7));

      /* Create bcode in this half-done function */
      struct v7_js_function *func = get_js_function_struct(res);

      func->bcode = (struct bcode *) calloc(1, sizeof(*func->bcode));
      bcode_init(func->bcode, bcode->strict_mode, NULL /* will be set below */,
                 0);
      bcode_copy_filename_from(func->bcode, bcode);
      retain_bcode(v7, func->bcode);

      /* deserialize the function's bcode from `ops` */
      *ops = (char *) bcode_deserialize_func(
          v7, func->bcode, *ops + 1 /*skip BCODE_INLINE_FUNC_TYPE_TAG*/);

      /* decrement *ops, because it will be incremented by `eval_bcode` soon */
      *ops -= 1;

      return res;
    }
    case BCODE_INLINE_REGEXP_TYPE_TAG: {
#if V7_ENABLE__RegExp
      enum v7_err rcode = V7_OK;
      val_t res;
      size_t len_src, len_flags;
      char *buf_src, *buf_flags;

      len_src = bcode_get_varint(ops);
      buf_src = *ops + 1;
      *ops += len_src + 1 /* nul term */;

      len_flags = bcode_get_varint(ops);
      buf_flags = *ops + 1;
      *ops += len_flags + 1 /* nul term */;

      rcode = v7_mk_regexp(v7, buf_src, len_src, buf_flags, len_flags, &res);
      assert(rcode == V7_OK);
      (void) rcode;

      return res;
#else
      fprintf(stderr, "Firmware is built without -DV7_ENABLE__RegExp\n");
      abort();
#endif
    }
    default:
      return ((val_t *) vec->p)[idx - BCODE_MAX_INLINE_TYPE_TAG];
  }
}

V7_PRIVATE void bcode_op_lit(struct bcode_builder *bbuilder, enum opcode op,
                             lit_t lit) {
  bcode_op(bbuilder, op);

  switch (lit.mode) {
    case LIT_MODE__TABLE:
      bcode_add_varint(bbuilder, lit.v.lit_idx + BCODE_MAX_INLINE_TYPE_TAG);
      break;

    case LIT_MODE__INLINED:
      if (v7_is_string(lit.v.inline_val)) {
        size_t len;
        const char *s = v7_get_string(bbuilder->v7, &lit.v.inline_val, &len);
        bcode_add_varint(bbuilder, BCODE_INLINE_STRING_TYPE_TAG);
        bcode_add_varint(bbuilder, len);
        bcode_ops_append(bbuilder, s, len + 1 /* nul term */);
      } else if (v7_is_number(lit.v.inline_val)) {
        bcode_add_varint(bbuilder, BCODE_INLINE_NUMBER_TYPE_TAG);
        /*
         * TODO(dfrank): we can save some memory by storing string
         * representation of a number here, instead of wasting 8 bytes for each
         * number.
         *
         * Alternatively, we can add more tags for integers, like
         * `BCODE_INLINE_S08_TYPE_TAG`, `BCODE_INLINE_S16_TYPE_TAG`, etc, since
         * integers are the most common numbers for sure.
         */
        bcode_ops_append(bbuilder, &lit.v.inline_val, sizeof(lit.v.inline_val));
      } else if (is_js_function(lit.v.inline_val)) {
/*
 * TODO(dfrank): implement `bcode_serialize_*` more generically, so
 * that they can write to buffer instead of a `FILE`. Then, remove this
 * workaround with `CS_PLATFORM == CS_P_UNIX`, `tmpfile()`, etc.
 */
#if CS_PLATFORM == CS_P_UNIX
        struct v7_js_function *func;
        FILE *fp = tmpfile();
        long len = 0;
        char *p;

        func = get_js_function_struct(lit.v.inline_val);

        /* we inline functions if only we're precompiling */
        assert(bbuilder->v7->is_precompiling);

        bcode_add_varint(bbuilder, BCODE_INLINE_FUNC_TYPE_TAG);
        bcode_serialize_func(bbuilder->v7, func->bcode, fp);

        fflush(fp);

        len = ftell(fp);

        p = (char *) mmap(NULL, len, PROT_WRITE, MAP_PRIVATE, fileno(fp), 0);

        bcode_ops_append(bbuilder, p, len);

        fclose(fp);
#endif
      } else if (v7_is_regexp(bbuilder->v7, lit.v.inline_val)) {
#if V7_ENABLE__RegExp
        struct v7_regexp *rp =
            v7_get_regexp_struct(bbuilder->v7, lit.v.inline_val);
        bcode_add_varint(bbuilder, BCODE_INLINE_REGEXP_TYPE_TAG);

        /* append regexp source */
        {
          size_t len;
          const char *buf =
              v7_get_string(bbuilder->v7, &rp->regexp_string, &len);
          bcode_add_varint(bbuilder, len);
          bcode_ops_append(bbuilder, buf, len + 1 /* nul term */);
        }

        /* append regexp flags */
        {
          char buf[_V7_REGEXP_MAX_FLAGS_LEN + 1 /* nul term */];
          size_t len = get_regexp_flags_str(bbuilder->v7, rp, buf);
          bcode_add_varint(bbuilder, len);
          bcode_ops_append(bbuilder, buf, len + 1 /* nul term */);
        }
#else
        fprintf(stderr, "Firmware is built without -DV7_ENABLE__RegExp\n");
        abort();
#endif
      } else {
        /* invalid type of inlined value */
        abort();
      }
      break;

    default:
      /* invalid literal mode */
      abort();
      break;
  }
}

V7_PRIVATE void bcode_push_lit(struct bcode_builder *bbuilder, lit_t lit) {
  bcode_op_lit(bbuilder, OP_PUSH_LIT, lit);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err bcode_add_name(struct bcode_builder *bbuilder,
                                      const char *p, size_t len, size_t *idx) {
  enum v7_err rcode = V7_OK;
  int llen;
  size_t ops_index;

  /*
   * if name length is not provided, assume it's null-terminated and calculate
   * it
   */
  if (len == ~((size_t) 0)) {
    len = strlen(p);
  }

  /* index at which to put name. If not provided, we'll append at the end */
  if (idx != NULL) {
    ops_index = *idx;
  } else {
    ops_index = bbuilder->ops.len;
  }

  /* calculate how much varint len will take */
  llen = calc_llen(len);

  /* reserve space in `ops` buffer */
  mbuf_insert(&bbuilder->ops, ops_index, NULL, llen + len + 1 /*null-term*/);

  {
    char *ops = bbuilder->ops.buf + ops_index;

    /* put varint len */
    ops += encode_varint(len, (unsigned char *) ops);

    /* put string */
    memcpy(ops, p, len);
    ops += len;

    /* null-terminate */
    *ops++ = 0x00;

    if (idx != NULL) {
      *idx = ops - bbuilder->ops.buf;
    }
  }

  /* maintain total number of names */
  if (bbuilder->bcode->names_cnt < V7_NAMES_CNT_MAX) {
    bbuilder->bcode->names_cnt++;
  } else {
    rcode = v7_throwf(bbuilder->v7, SYNTAX_ERROR, "Too many local variables");
  }

  return rcode;
}

V7_PRIVATE char *bcode_end_names(char *ops, size_t names_cnt) {
  while (names_cnt--) {
    ops = bcode_next_name(ops, NULL, NULL);
  }
  return ops;
}

V7_PRIVATE char *bcode_next_name(char *ops, char **pname, size_t *plen) {
  size_t len;
  int llen;

  len = decode_varint((unsigned char *) ops, &llen);

  ops += llen;

  if (pname != NULL) {
    *pname = ops;
  }

  if (plen != NULL) {
    *plen = len;
  }

  ops += len + 1 /*null-terminator*/;
  return ops;
}

V7_PRIVATE char *bcode_next_name_v(struct v7 *v7, struct bcode *bcode,
                                   char *ops, val_t *res) {
  char *name;
  size_t len;

  ops = bcode_next_name(ops, &name, &len);

  /*
   * If `ops` is in RAM, we create owned string, since the string may outlive
   * bcode. Otherwise (`ops` is in ROM), we create foreign string.
   */
  *res = v7_mk_string(v7, name, len, !bcode->ops_in_rom);

  return ops;
}

V7_PRIVATE bcode_off_t bcode_pos(struct bcode_builder *bbuilder) {
  return bbuilder->ops.len;
}

/*
 * Appends a branch target and returns its location.
 * This location can be updated with bcode_patch_target.
 * To be issued following a JMP_* bytecode
 */
V7_PRIVATE bcode_off_t bcode_add_target(struct bcode_builder *bbuilder) {
  bcode_off_t pos = bcode_pos(bbuilder);
  bcode_off_t zero = 0;
  bcode_ops_append(bbuilder, &zero, sizeof(bcode_off_t));
  return pos;
}

/* Appends an op requiring a branch target. See bcode_add_target. */
V7_PRIVATE bcode_off_t
bcode_op_target(struct bcode_builder *bbuilder, uint8_t op) {
  bcode_op(bbuilder, op);
  return bcode_add_target(bbuilder);
}

V7_PRIVATE void bcode_patch_target(struct bcode_builder *bbuilder,
                                   bcode_off_t label, bcode_off_t target) {
  memcpy(bbuilder->ops.buf + label, &target, sizeof(target));
}

#ifndef V7_NO_FS

static void bcode_serialize_varint(int n, FILE *out) {
  unsigned char buf[8];
  int k = calc_llen(n);
  encode_varint(n, buf);
  fwrite(buf, k, 1, out);
}

static void bcode_serialize_func(struct v7 *v7, struct bcode *bcode,
                                 FILE *out) {
  struct v7_vec *vec;
  (void) v7;

  /*
   * All literals should be inlined into `ops`, so we expect literals table
   * to be empty here
   */
  assert(bcode->lit.len == 0);

  /* args_cnt */
  bcode_serialize_varint(bcode->args_cnt, out);

  /* names_cnt */
  bcode_serialize_varint(bcode->names_cnt, out);

  /* func_name_present */
  bcode_serialize_varint(bcode->func_name_present, out);

  /*
   * bcode:
   * <varint> // opcodes length
   * <opcode>*
   */
  vec = &bcode->ops;
  bcode_serialize_varint(vec->len, out);
  fwrite(vec->p, vec->len, 1, out);
}

V7_PRIVATE void bcode_serialize(struct v7 *v7, struct bcode *bcode, FILE *out) {
  (void) v7;
  (void) bcode;

  fwrite(BIN_BCODE_SIGNATURE, sizeof(BIN_BCODE_SIGNATURE), 1, out);
  bcode_serialize_func(v7, bcode, out);
}

static size_t bcode_deserialize_varint(const char **data) {
  size_t ret = 0;
  int len = 0;
  ret = decode_varint((const unsigned char *) (*data), &len);
  *data += len;
  return ret;
}

static const char *bcode_deserialize_func(struct v7 *v7, struct bcode *bcode,
                                          const char *data) {
  size_t size;
  struct bcode_builder bbuilder;

  bcode_builder_init(v7, &bbuilder, bcode);

  /*
   * before deserializing, set the corresponding flag, so that metrics will be
   * updated accordingly
   */
  bcode->deserialized = 1;

  /*
   * In serialized functions, all literals are inlined into `ops`, so we don't
   * deserialize them here in any way
   */

  /* get number of args */
  bcode->args_cnt = bcode_deserialize_varint(&data);

  /* get number of names */
  bcode->names_cnt = bcode_deserialize_varint(&data);

  /* get whether the function name is present in `names` */
  bcode->func_name_present = bcode_deserialize_varint(&data);

  /* get opcode size */
  size = bcode_deserialize_varint(&data);

  bbuilder.ops.buf = (char *) data;
  bbuilder.ops.size = size;
  bbuilder.ops.len = size;

  bcode->ops_in_rom = 1;

  data += size;

  bcode_builder_finalize(&bbuilder);
  return data;
}

V7_PRIVATE void bcode_deserialize(struct v7 *v7, struct bcode *bcode,
                                  const char *data) {
  data = bcode_deserialize_func(v7, bcode, data);
}

#endif
