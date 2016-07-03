/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_AST_H_
#define CS_V7_SRC_AST_H_

#include <stdio.h>
#include "common/mbuf.h"
#include "v7/src/internal.h"
#include "v7/src/core.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#define BIN_AST_SIGNATURE "V\007ASTV10"

enum ast_tag {
  AST_NOP,
  AST_SCRIPT,
  AST_VAR,
  AST_VAR_DECL,
  AST_FUNC_DECL,
  AST_IF,
  AST_FUNC,

  AST_ASSIGN,
  AST_REM_ASSIGN,
  AST_MUL_ASSIGN,
  AST_DIV_ASSIGN,
  AST_XOR_ASSIGN,
  AST_PLUS_ASSIGN,
  AST_MINUS_ASSIGN,
  AST_OR_ASSIGN,
  AST_AND_ASSIGN,
  AST_LSHIFT_ASSIGN,
  AST_RSHIFT_ASSIGN,
  AST_URSHIFT_ASSIGN,

  AST_NUM,
  AST_IDENT,
  AST_STRING,
  AST_REGEX,
  AST_LABEL,

  AST_SEQ,
  AST_WHILE,
  AST_DOWHILE,
  AST_FOR,
  AST_FOR_IN,
  AST_COND,

  AST_DEBUGGER,
  AST_BREAK,
  AST_LABELED_BREAK,
  AST_CONTINUE,
  AST_LABELED_CONTINUE,
  AST_RETURN,
  AST_VALUE_RETURN,
  AST_THROW,

  AST_TRY,
  AST_SWITCH,
  AST_CASE,
  AST_DEFAULT,
  AST_WITH,

  AST_LOGICAL_OR,
  AST_LOGICAL_AND,
  AST_OR,
  AST_XOR,
  AST_AND,

  AST_EQ,
  AST_EQ_EQ,
  AST_NE,
  AST_NE_NE,

  AST_LE,
  AST_LT,
  AST_GE,
  AST_GT,
  AST_IN,
  AST_INSTANCEOF,

  AST_LSHIFT,
  AST_RSHIFT,
  AST_URSHIFT,

  AST_ADD,
  AST_SUB,

  AST_REM,
  AST_MUL,
  AST_DIV,

  AST_POSITIVE,
  AST_NEGATIVE,
  AST_NOT,
  AST_LOGICAL_NOT,
  AST_VOID,
  AST_DELETE,
  AST_TYPEOF,
  AST_PREINC,
  AST_PREDEC,

  AST_POSTINC,
  AST_POSTDEC,

  AST_MEMBER,
  AST_INDEX,
  AST_CALL,

  AST_NEW,

  AST_ARRAY,
  AST_OBJECT,
  AST_PROP,
  AST_GETTER,
  AST_SETTER,

  AST_THIS,
  AST_TRUE,
  AST_FALSE,
  AST_NULL,
  AST_UNDEFINED,

  AST_USE_STRICT,

  AST_MAX_TAG
};

struct ast {
  struct mbuf mbuf;
  int refcnt;
  int has_overflow;
};

typedef unsigned long ast_off_t;

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 8
#define GCC_HAS_PRAGMA_DIAGNOSTIC
#endif

#ifdef GCC_HAS_PRAGMA_DIAGNOSTIC
/*
 * TODO(mkm): GCC complains that bitfields on char are not standard
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
struct ast_node_def {
#ifndef V7_DISABLE_AST_TAG_NAMES
  const char *name; /* tag name, for debugging and serialization */
#endif
  unsigned char has_varint : 1;   /* has a varint body */
  unsigned char has_inlined : 1;  /* inlined data whose size is in varint fld */
  unsigned char num_skips : 3;    /* number of skips */
  unsigned char num_subtrees : 3; /* number of fixed subtrees */
};
extern const struct ast_node_def ast_node_defs[];
#if V7_ENABLE_FOOTPRINT_REPORT
extern const size_t ast_node_defs_size;
extern const size_t ast_node_defs_count;
#endif
#ifdef GCC_HAS_PRAGMA_DIAGNOSTIC
#pragma GCC diagnostic pop
#endif

enum ast_which_skip {
  AST_END_SKIP = 0,
  AST_VAR_NEXT_SKIP = 1,
  AST_SCRIPT_FIRST_VAR_SKIP = AST_VAR_NEXT_SKIP,
  AST_FOR_BODY_SKIP = 1,
  AST_DO_WHILE_COND_SKIP = 1,
  AST_END_IF_TRUE_SKIP = 1,
  AST_TRY_CATCH_SKIP = 1,
  AST_TRY_FINALLY_SKIP = 2,
  AST_FUNC_FIRST_VAR_SKIP = AST_VAR_NEXT_SKIP,
  AST_FUNC_BODY_SKIP = 2,
  AST_SWITCH_DEFAULT_SKIP = 1
};

V7_PRIVATE void ast_init(struct ast *, size_t);
V7_PRIVATE void ast_optimize(struct ast *);
V7_PRIVATE void ast_free(struct ast *);

/*
 * Begins an AST node by inserting a tag to the AST at the given offset.
 *
 * It also allocates space for the fixed_size payload and the space for
 * the skips.
 *
 * The caller is responsible for appending children.
 *
 * Returns the offset of the node payload (one byte after the tag).
 * This offset can be passed to `ast_set_skip`.
 */
V7_PRIVATE ast_off_t
ast_insert_node(struct ast *a, ast_off_t pos, enum ast_tag tag);

/*
 * Modify tag which is already added to buffer. Keeps `AST_TAG_LINENO_PRESENT`
 * flag.
 */
V7_PRIVATE void ast_modify_tag(struct ast *a, ast_off_t tag_off,
                               enum ast_tag tag);

#ifndef V7_DISABLE_LINE_NUMBERS
/*
 * Add line_no varint after all skips of the tag at the offset `tag_off`, and
 * marks the tag byte.
 *
 * Byte at the offset `tag_off` should be a valid tag.
 */
V7_PRIVATE void ast_add_line_no(struct ast *a, ast_off_t tag_off, int line_no);
#endif

/*
 * Patches a given skip slot for an already emitted node with the
 * current write cursor position (e.g. AST length).
 *
 * This is intended to be invoked when a node with a variable number
 * of child subtrees is closed, or when the consumers need a shortcut
 * to the next sibling.
 *
 * Each node type has a different number and semantic for skips,
 * all of them defined in the `ast_which_skip` enum.
 * All nodes having a variable number of child subtrees must define
 * at least the `AST_END_SKIP` skip, which effectively skips a node
 * boundary.
 *
 * Every tree reader can assume this and safely skip unknown nodes.
 *
 * `pos` should be an offset of the byte right after a tag.
 */
V7_PRIVATE ast_off_t
ast_set_skip(struct ast *a, ast_off_t pos, enum ast_which_skip skip);

/*
 * Patches a given skip slot for an already emitted node with the value
 * (stored as delta relative to the `pos` node) of the `where` argument.
 */
V7_PRIVATE ast_off_t ast_modify_skip(struct ast *a, ast_off_t pos,
                                     ast_off_t where, enum ast_which_skip skip);

/*
 * Returns the offset in AST to which the given `skip` points.
 *
 * `pos` should be an offset of the byte right after a tag.
 */
V7_PRIVATE ast_off_t
ast_get_skip(struct ast *a, ast_off_t pos, enum ast_which_skip skip);

/*
 * Returns the tag from the offset `ppos`, and shifts `ppos` by 1.
 */
V7_PRIVATE enum ast_tag ast_fetch_tag(struct ast *a, ast_off_t *ppos);

/*
 * Moves the cursor to the tag's varint and inlined data (if there are any, see
 * `struct ast_node_def::has_varint` and `struct ast_node_def::has_inlined`).
 *
 * Technically, it skips node's "skips" and line number data, if either is
 * present.
 *
 * Assumes a cursor (`ppos`) positioned right after a tag.
 */
V7_PRIVATE void ast_move_to_inlined_data(struct ast *a, ast_off_t *ppos);

/*
 * Moves the cursor to the tag's subtrees (if there are any,
 * see `struct ast_node_def::num_subtrees`), or to the next node in case the
 * current node has no subtrees.
 *
 * Technically, it skips node's "skips", line number data, and inlined data, if
 * either is present.
 *
 * Assumes a cursor (`ppos`) positioned right after a tag.
 */
V7_PRIVATE void ast_move_to_children(struct ast *a, ast_off_t *ppos);

/* Helper to add a node with inlined data. */
V7_PRIVATE ast_off_t ast_insert_inlined_node(struct ast *a, ast_off_t pos,
                                             enum ast_tag tag, const char *name,
                                             size_t len);

/*
 * Returns the line number encoded in the node, or `0` in case of none is
 * encoded.
 *
 * `pos` should be an offset of the byte right after a tag.
 */
V7_PRIVATE int ast_get_line_no(struct ast *a, ast_off_t pos);

/*
 * `pos` should be an offset of the byte right after a tag
 */
V7_PRIVATE char *ast_get_inlined_data(struct ast *a, ast_off_t pos, size_t *n);

/*
 * Returns the `double` number inlined in the node
 */
V7_PRIVATE double ast_get_num(struct ast *a, ast_off_t pos);

/*
 * Skips the node and all its subnodes.
 *
 * Cursor (`ppos`) should be at the tag byte
 */
V7_PRIVATE void ast_skip_tree(struct ast *a, ast_off_t *ppos);

V7_PRIVATE void ast_dump_tree(FILE *fp, struct ast *a, ast_off_t *ppos,
                              int depth);

V7_PRIVATE void release_ast(struct v7 *v7, struct ast *a);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_AST_H_ */
