/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_BCODE_H_
#define CS_V7_SRC_BCODE_H_

#define BIN_BCODE_SIGNATURE "V\007BCODE:"

#if !defined(V7_NAMES_CNT_WIDTH)
#define V7_NAMES_CNT_WIDTH 10
#endif

#if !defined(V7_ARGS_CNT_WIDTH)
#define V7_ARGS_CNT_WIDTH 8
#endif

#define V7_NAMES_CNT_MAX ((1 << V7_NAMES_CNT_WIDTH) - 1)
#define V7_ARGS_CNT_MAX ((1 << V7_ARGS_CNT_WIDTH) - 1)

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/opcodes.h"
#include "v7/src/string.h"
#include "v7/src/object.h"
#include "v7/src/primitive.h"
#include "common/mbuf.h"

enum bcode_inline_lit_type_tag {
  BCODE_INLINE_STRING_TYPE_TAG = 0,
  BCODE_INLINE_NUMBER_TYPE_TAG,
  BCODE_INLINE_FUNC_TYPE_TAG,
  BCODE_INLINE_REGEXP_TYPE_TAG,

  BCODE_MAX_INLINE_TYPE_TAG
};

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef uint32_t bcode_off_t;

/*
 * Each JS function will have one bcode structure
 * containing the instruction stream, a literal table, and function
 * metadata.
 * Instructions contain references to literals (strings, constants, etc)
 *
 * The bcode struct can be shared between function instances
 * and keeps a reference count used to free it in the function destructor.
 */
struct bcode {
  /*
   * Names + instruction opcode.
   * Names are null-terminates strings. For function's bcode, there are:
   *  - function name (for anonymous function, the name is still present: an
   *    empty string);
   *  - arg names (a number of args is determined by `args_cnt`, see below);
   *  - local names (a number or locals can be calculated:
   *    `(names_cnt - args_cnt - 1)`).
   *
   * For script's bcode, there are just local names.
   */
  struct v7_vec ops;

  /* Literal table */
  struct v7_vec lit;

#ifndef V7_DISABLE_FILENAMES
  /* Name of the file from which this bcode was generated (used for debug) */
  void *filename;
#endif

  /* Reference count */
  uint8_t refcnt;

  /* Total number of null-terminated strings in the beginning of `ops` */
  unsigned int names_cnt : V7_NAMES_CNT_WIDTH;

  /* Number of args (should be <= `(names_cnt + 1)`) */
  unsigned int args_cnt : V7_ARGS_CNT_WIDTH;

  unsigned int strict_mode : 1;
  /*
   * If true this structure lives on read only memory, either
   * mmapped or constant data section.
   */
  unsigned int frozen : 1;

  /* If set, `ops.buf` points to ROM, so we shouldn't free it */
  unsigned int ops_in_rom : 1;
  /* Set for deserialized bcode. Used for metrics only */
  unsigned int deserialized : 1;

  /* Set when `ops` contains function name as the first `name` */
  unsigned int func_name_present : 1;

#ifndef V7_DISABLE_FILENAMES
  /* If set, `filename` points to ROM, so we shouldn't free it */
  unsigned int filename_in_rom : 1;
#endif
};

/*
 * Bcode builder context: it contains mutable mbufs for opcodes and literals,
 * whereas the bcode itself contains just vectors (`struct v7_vec`).
 */
struct bcode_builder {
  struct v7 *v7;
  struct bcode *bcode;

  struct mbuf ops; /* names + instruction opcode */
  struct mbuf lit; /* literal table */
};

V7_PRIVATE void bcode_builder_init(struct v7 *v7,
                                   struct bcode_builder *bbuilder,
                                   struct bcode *bcode);
V7_PRIVATE void bcode_builder_finalize(struct bcode_builder *bbuilder);

/*
 * Note: `filename` must be either:
 *
 * - `NULL`. In this case, `filename_in_rom` is ignored.
 * - A pointer to ROM (or to any other unmanaged memory). `filename_in_rom`
 *   must be set to 1.
 * - A pointer returned by `shdata_create()`, i.e. a pointer to shared data.
 *
 * If you need to copy filename from another bcode, just pass NULL initially,
 * and after bcode is initialized, call `bcode_copy_filename_from()`.
 *
 * To get later a proper null-terminated filename string from bcode, use
 * `bcode_get_filename()`.
 */
V7_PRIVATE void bcode_init(struct bcode *bcode, uint8_t strict_mode,
                           void *filename, uint8_t filename_in_rom);
V7_PRIVATE void bcode_free(struct v7 *v7, struct bcode *bcode);
V7_PRIVATE void release_bcode(struct v7 *v7, struct bcode *bcode);
V7_PRIVATE void retain_bcode(struct v7 *v7, struct bcode *bcode);

#ifndef V7_DISABLE_FILENAMES
/*
 * Return a pointer to null-terminated filename string
 */
V7_PRIVATE const char *bcode_get_filename(struct bcode *bcode);
#endif

/*
 * Copy filename from `src` to `dst`. If source filename is a pointer to ROM,
 * then just the pointer is copied; otherwise, appropriate shdata pointer is
 * retained.
 */
V7_PRIVATE void bcode_copy_filename_from(struct bcode *dst, struct bcode *src);

#ifndef V7_NO_FS
/*
 * Serialize a bcode structure.
 *
 * All literals, including functions, are inlined into `ops` data; see
 * the serialization logic in `bcode_op_lit()`.
 *
 * The root bcode looks just like a regular function.
 */
V7_PRIVATE void bcode_serialize(struct v7 *v7, struct bcode *bcode, FILE *f);
V7_PRIVATE void bcode_deserialize(struct v7 *v7, struct bcode *bcode,
                                  const char *data);
#endif

#ifdef V7_BCODE_DUMP
V7_PRIVATE void dump_bcode(struct v7 *v7, FILE *, struct bcode *);
#endif

/* mode of literal storage: in literal table or inlined in `ops` */
enum lit_mode {
  /* literal stored in table, index is in `lit_t::lit_idx` */
  LIT_MODE__TABLE,
  /* literal should be inlined in `ops`, value is in `lit_t::inline_val` */
  LIT_MODE__INLINED,
};

/*
 * Result of the addition of literal value to bcode (see `bcode_add_lit()`).
 * There are two possible cases:
 *
 * - Literal is added to the literal table. In this case, `mode ==
 *   LIT_MODE__TABLE`, and the index of the literal is stored in `lit_idx`
 * - Literal is not added anywhere, and should be inlined into `ops`. In this
 *   case, `mode == LIT_MODE__INLINED`, and the value to inline is stored in
 *   `inline_val`.
 *
 * It's `bcode_op_lit()` who handles both of these cases.
 */
typedef struct {
  union {
    /*
     * index in literal table;
     * NOTE: valid if only `mode == LIT_MODE__TABLE`
     */
    size_t lit_idx;

    /*
     * value to be inlined into `ops`;
     * NOTE: valid if only `mode == LIT_MODE__INLINED`
     */
    v7_val_t inline_val;
  } v; /* anonymous unions are a c11 feature */

  /*
   * mode of literal storage (see `enum lit_mode`)
   * NOTE: we need one more bit, because enum can be signed
   * on some compilers (e.g. msvc) and thus will get signextended
   * when moved to a `enum lit_mode` variable basically corrupting
   * the value. See https://github.com/cesanta/v7/issues/551
   */
  enum lit_mode mode : 2;
} lit_t;

V7_PRIVATE void bcode_op(struct bcode_builder *bbuilder, uint8_t op);

#ifndef V7_DISABLE_LINE_NUMBERS
V7_PRIVATE void bcode_append_lineno(struct bcode_builder *bbuilder,
                                    int line_no);
#endif

/*
 * Add a literal to the bcode literal table, or just decide that the literal
 * should be inlined into `ops`. See `lit_t` for details.
 */
V7_PRIVATE
lit_t bcode_add_lit(struct bcode_builder *bbuilder, v7_val_t val);

/* disabled because of short lits */
#if 0
V7_PRIVATE v7_val_t bcode_get_lit(struct bcode *bcode, size_t idx);
#endif

/*
 * Emit an opcode `op`, and handle the literal `lit` (see `bcode_add_lit()`,
 * `lit_t`). Depending on the literal storage mode (see `enum lit_mode`), this
 * function either emits literal table index or inlines the literal directly
 * into `ops.`
 */
V7_PRIVATE void bcode_op_lit(struct bcode_builder *bbuilder, enum opcode op,
                             lit_t lit);

/* Helper function, equivalent of `bcode_op_lit(bbuilder, OP_PUSH_LIT, lit)` */
V7_PRIVATE void bcode_push_lit(struct bcode_builder *bbuilder, lit_t lit);

/*
 * Add name to bcode. If `idx` is null, a name is appended to the end of the
 * `bcode->ops.buf`. If `idx` is provided, it should point to the index at
 * which new name should be inserted; and it is updated by the
 * `bcode_add_name()` to point right after newly added name.
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err bcode_add_name(struct bcode_builder *bbuilder,
                                      const char *p, size_t len, size_t *idx);

/*
 * Takes a pointer to the beginning of `ops` buffer and names count, returns
 * a pointer where actual opcodes begin (i.e. skips names).
 *
 * It takes two distinct arguments instead of just `struct bcode` pointer,
 * because during bcode building `ops` is stored in builder.
 */
V7_PRIVATE char *bcode_end_names(char *ops, size_t names_cnt);

/*
 * Given a pointer to `ops` (which should be `bcode->ops` or a pointer returned
 * from previous invocation of `bcode_next_name()`), yields a name string via
 * arguments `pname`, `plen`.
 *
 * Returns a pointer that should be given to `bcode_next_name()` to get a next
 * string (Whether there is a next string should be determined via the
 * `names_cnt`; since if there are no more names, this function will return an
 * invalid non-null pointer as next name pointer)
 */
V7_PRIVATE char *bcode_next_name(char *ops, char **pname, size_t *plen);

/*
 * Like `bcode_next_name()`, but instead of yielding a C string, it yields a
 * `val_t` value (via `res`).
 */
V7_PRIVATE char *bcode_next_name_v(struct v7 *v7, struct bcode *bcode,
                                   char *ops, val_t *res);
V7_PRIVATE bcode_off_t bcode_pos(struct bcode_builder *bbuilder);
V7_PRIVATE bcode_off_t bcode_add_target(struct bcode_builder *bbuilder);
V7_PRIVATE bcode_off_t
bcode_op_target(struct bcode_builder *bbuilder, uint8_t op);
V7_PRIVATE void bcode_patch_target(struct bcode_builder *bbuilder,
                                   bcode_off_t label, bcode_off_t target);

V7_PRIVATE void bcode_add_varint(struct bcode_builder *bbuilder, size_t value);
/*
 * Reads varint-encoded integer from the provided pointer, and adjusts
 * the pointer appropriately
 */
V7_PRIVATE size_t bcode_get_varint(char **ops);

/*
 * Decode a literal value from a string of opcodes and update the cursor to
 * point past it
 */
V7_PRIVATE
v7_val_t bcode_decode_lit(struct v7 *v7, struct bcode *bcode, char **ops);

#if defined(V7_BCODE_DUMP) || defined(V7_BCODE_TRACE)
V7_PRIVATE void dump_op(struct v7 *v7, FILE *f, struct bcode *bcode,
                        char **ops);
#endif

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_BCODE_H_ */
