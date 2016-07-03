/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/compiler.h"
#include "v7/src/std_error.h"
#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/exceptions.h"
#include "v7/src/conversion.h"
#include "v7/src/regexp.h"

/*
 * The bytecode compiler takes an AST as input and produces one or more
 * bcode structure as output.
 *
 * Each script or function body is compiled into it's own bcode structure.
 *
 * Each bcode stream produces a new value on the stack, i.e. its overall
 * stack diagram is: `( -- a)`
 *
 * This value will be then popped by the function caller or by v7_exec in case
 * of scripts.
 *
 * In JS, the value of a script is the value of the last statement.
 * A script with no statement has an `undefined` value.
 * Functions instead require an explicit return value, so this matters only
 * for `v7_exec` and JS `eval`.
 *
 * Since an empty script has an undefined value, and each script has to
 * yield a value, the script/function prologue consists of a PUSH_UNDEFINED.
 *
 * Each statement will be compiled to push a value on the stack.
 * When a statement begins evaluating, the current TOS is thus either
 * the value of the previous statement or `undefined` in case of the first
 * statement.
 *
 * Every statement of a given script/function body always evaluates at the same
 * stack depth.
 *
 * In order to achieve that, after a statement is compiled out, a SWAP_DROP
 * opcode is emitted, that drops the value of the previous statement (or the
 * initial `undefined`). Dropping the value after the next statement is
 * evaluated and not before has allows us to correctly implement exception
 * behaviour and the break statement.
 *
 * Compound statements are constructs such as `if`/`while`/`for`/`try`. These
 * constructs contain a body consisting of a possibly empty statement list.
 *
 * Unlike normal statements, compound statements don't produce a value
 * themselves. Their value is either the value of their last executed statement
 * in their body, or the previous statement in case their body is empty or not
 * evaluated at all.
 *
 * An example is:
 *
 * [source,js]
 * ----
 * try {
 *   42;
 *   someUnexistingVariable;
 * } catch(e) {
 *   while(true) {}
 *     if(true) {
 *     }
 *     if(false) {
 *       2;
 *     }
 *     break;
 *   }
 * }
 * ----
 */

static const enum ast_tag assign_ast_map[] = {
    AST_REM, AST_MUL, AST_DIV,    AST_XOR,    AST_ADD,    AST_SUB,
    AST_OR,  AST_AND, AST_LSHIFT, AST_RSHIFT, AST_URSHIFT};

#ifdef V7_BCODE_DUMP
extern void dump_bcode(struct v7 *v7, FILE *f, struct bcode *bcode);
#endif

V7_PRIVATE enum v7_err compile_expr_builder(struct bcode_builder *bbuilder,
                                            struct ast *a, ast_off_t *ppos);

V7_PRIVATE enum v7_err compile_function(struct v7 *v7, struct ast *a,
                                        ast_off_t *ppos, struct bcode *bcode);

V7_PRIVATE enum v7_err binary_op(struct bcode_builder *bbuilder,
                                 enum ast_tag tag) {
  uint8_t op;
  enum v7_err rcode = V7_OK;
  struct v7 *v7 = bbuilder->v7;

  switch (tag) {
    case AST_ADD:
      op = OP_ADD;
      break;
    case AST_SUB:
      op = OP_SUB;
      break;
    case AST_REM:
      op = OP_REM;
      break;
    case AST_MUL:
      op = OP_MUL;
      break;
    case AST_DIV:
      op = OP_DIV;
      break;
    case AST_LSHIFT:
      op = OP_LSHIFT;
      break;
    case AST_RSHIFT:
      op = OP_RSHIFT;
      break;
    case AST_URSHIFT:
      op = OP_URSHIFT;
      break;
    case AST_OR:
      op = OP_OR;
      break;
    case AST_XOR:
      op = OP_XOR;
      break;
    case AST_AND:
      op = OP_AND;
      break;
    case AST_EQ_EQ:
      op = OP_EQ_EQ;
      break;
    case AST_EQ:
      op = OP_EQ;
      break;
    case AST_NE:
      op = OP_NE;
      break;
    case AST_NE_NE:
      op = OP_NE_NE;
      break;
    case AST_LT:
      op = OP_LT;
      break;
    case AST_LE:
      op = OP_LE;
      break;
    case AST_GT:
      op = OP_GT;
      break;
    case AST_GE:
      op = OP_GE;
      break;
    case AST_INSTANCEOF:
      op = OP_INSTANCEOF;
      break;
    default:
      rcode = v7_throwf(bbuilder->v7, SYNTAX_ERROR, "unknown binary ast node");
      V7_THROW(V7_SYNTAX_ERROR);
  }
  bcode_op(bbuilder, op);
clean:
  return rcode;
}

V7_PRIVATE enum v7_err compile_binary(struct bcode_builder *bbuilder,
                                      struct ast *a, ast_off_t *ppos,
                                      enum ast_tag tag) {
  enum v7_err rcode = V7_OK;
  struct v7 *v7 = bbuilder->v7;
  V7_TRY(compile_expr_builder(bbuilder, a, ppos));
  V7_TRY(compile_expr_builder(bbuilder, a, ppos));

  V7_TRY(binary_op(bbuilder, tag));
clean:
  return rcode;
}

/*
 * `pos` should be an offset of the byte right after a tag
 */
static lit_t string_lit(struct bcode_builder *bbuilder, struct ast *a,
                        ast_off_t pos) {
  size_t i = 0, name_len;
  val_t v = V7_UNDEFINED;
  struct mbuf *m = &bbuilder->lit;
  char *name = ast_get_inlined_data(a, pos, &name_len);

/* temp disabled because of short lits */
#if 0
  for (i = 0; i < m->len / sizeof(val_t); i++) {
    v = ((val_t *) m->buf)[i];
    if (v7_is_string(v)) {
      size_t l;
      const char *s = v7_get_string(bbuilder->v7, &v, &l);
      if (name_len == l && memcmp(name, s, name_len) == 0) {
        lit_t res;
        memset(&res, 0, sizeof(res));
        res.idx = i + bcode_max_inline_type_tag;
        return res;
      }
    }
  }
#else
  (void) i;
  (void) v;
  (void) m;
#endif
  return bcode_add_lit(bbuilder, v7_mk_string(bbuilder->v7, name, name_len, 1));
}

#if V7_ENABLE__RegExp
WARN_UNUSED_RESULT
static enum v7_err regexp_lit(struct bcode_builder *bbuilder, struct ast *a,
                              ast_off_t pos, lit_t *res) {
  enum v7_err rcode = V7_OK;
  size_t name_len;
  char *p;
  char *name = ast_get_inlined_data(a, pos, &name_len);
  val_t tmp = V7_UNDEFINED;
  struct v7 *v7 = bbuilder->v7;

  for (p = name + name_len - 1; *p != '/';) p--;

  V7_TRY(v7_mk_regexp(bbuilder->v7, name + 1, p - (name + 1), p + 1,
                      (name + name_len) - p - 1, &tmp));

  *res = bcode_add_lit(bbuilder, tmp);

clean:
  return rcode;
}
#endif

#ifndef V7_DISABLE_LINE_NUMBERS
static void append_lineno_if_changed(struct v7 *v7,
                                     struct bcode_builder *bbuilder,
                                     int line_no) {
  (void) v7;
  if (line_no != 0 && line_no != v7->line_no) {
    v7->line_no = line_no;
    bcode_append_lineno(bbuilder, line_no);
  }
}
#else
static void append_lineno_if_changed(struct v7 *v7,
                                     struct bcode_builder *bbuilder,
                                     int line_no) {
  (void) v7;
  (void) bbuilder;
  (void) line_no;
}
#endif

static enum ast_tag fetch_tag(struct v7 *v7, struct bcode_builder *bbuilder,
                              struct ast *a, ast_off_t *ppos,
                              ast_off_t *ppos_after_tag) {
  enum ast_tag ret = ast_fetch_tag(a, ppos);
  int line_no = ast_get_line_no(a, *ppos);
  append_lineno_if_changed(v7, bbuilder, line_no);
  if (ppos_after_tag != NULL) {
    *ppos_after_tag = *ppos;
  }
  ast_move_to_children(a, ppos);
  return ret;
}

/*
 * a++ and a-- need to ignore the updated value.
 *
 * Call this before updating the lhs.
 */
static void fixup_post_op(struct bcode_builder *bbuilder, enum ast_tag tag) {
  if (tag == AST_POSTINC || tag == AST_POSTDEC) {
    bcode_op(bbuilder, OP_UNSTASH);
  }
}

/*
 * evaluate rhs expression.
 * ++a and a++ are equivalent to a+=1
 */
static enum v7_err eval_assign_rhs(struct bcode_builder *bbuilder,
                                   struct ast *a, ast_off_t *ppos,
                                   enum ast_tag tag) {
  enum v7_err rcode = V7_OK;
  struct v7 *v7 = bbuilder->v7;

  /* a++ and a-- need to preserve initial value. */
  if (tag == AST_POSTINC || tag == AST_POSTDEC) {
    bcode_op(bbuilder, OP_STASH);
  }
  if (tag >= AST_PREINC && tag <= AST_POSTDEC) {
    bcode_op(bbuilder, OP_PUSH_ONE);
  } else {
    V7_TRY(compile_expr_builder(bbuilder, a, ppos));
  }

  switch (tag) {
    case AST_PREINC:
    case AST_POSTINC:
      bcode_op(bbuilder, OP_ADD);
      break;
    case AST_PREDEC:
    case AST_POSTDEC:
      bcode_op(bbuilder, OP_SUB);
      break;
    case AST_ASSIGN:
      /* no operation */
      break;
    default:
      binary_op(bbuilder, assign_ast_map[tag - AST_ASSIGN - 1]);
  }

clean:
  return rcode;
}

static enum v7_err compile_assign(struct bcode_builder *bbuilder, struct ast *a,
                                  ast_off_t *ppos, enum ast_tag tag) {
  lit_t lit;
  enum ast_tag ntag;
  enum v7_err rcode = V7_OK;
  struct v7 *v7 = bbuilder->v7;
  ast_off_t pos_after_tag;

  ntag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);

  switch (ntag) {
    case AST_IDENT:
      lit = string_lit(bbuilder, a, pos_after_tag);
      if (tag != AST_ASSIGN) {
        bcode_op_lit(bbuilder, OP_GET_VAR, lit);
      }

      V7_TRY(eval_assign_rhs(bbuilder, a, ppos, tag));
      bcode_op_lit(bbuilder, OP_SET_VAR, lit);

      fixup_post_op(bbuilder, tag);
      break;
    case AST_MEMBER:
    case AST_INDEX:
      switch (ntag) {
        case AST_MEMBER:
          lit = string_lit(bbuilder, a, pos_after_tag);
          V7_TRY(compile_expr_builder(bbuilder, a, ppos));
          bcode_push_lit(bbuilder, lit);
          break;
        case AST_INDEX:
          V7_TRY(compile_expr_builder(bbuilder, a, ppos));
          V7_TRY(compile_expr_builder(bbuilder, a, ppos));
          break;
        default:
          /* unreachable, compilers are dumb */
          V7_THROW(V7_SYNTAX_ERROR);
      }
      if (tag != AST_ASSIGN) {
        bcode_op(bbuilder, OP_2DUP);
        bcode_op(bbuilder, OP_GET);
      }

      V7_TRY(eval_assign_rhs(bbuilder, a, ppos, tag));
      bcode_op(bbuilder, OP_SET);

      fixup_post_op(bbuilder, tag);
      break;
    default:
      /* We end up here on expressions like `1 = 2;`, it's a ReferenceError */
      rcode = v7_throwf(bbuilder->v7, REFERENCE_ERROR, "unexpected ast node");
      V7_THROW(V7_SYNTAX_ERROR);
  }
clean:
  return rcode;
}

/*
 * Walks through all declarations (`var` and `function`) in the current scope,
 * and adds names of all of them to `bcode->ops`. Additionally, `function`
 * declarations are compiled right here, since they're hoisted in JS.
 */
static enum v7_err compile_local_vars(struct bcode_builder *bbuilder,
                                      struct ast *a, ast_off_t start,
                                      ast_off_t fvar) {
  ast_off_t next, fvar_end;
  char *name;
  size_t name_len;
  lit_t lit;
  enum v7_err rcode = V7_OK;
  struct v7 *v7 = bbuilder->v7;
  size_t names_end = 0;
  ast_off_t pos_after_tag;

  /* calculate `names_end`: offset at which names in `bcode->ops` end */
  names_end =
      (size_t)(bcode_end_names(bbuilder->ops.buf, bbuilder->bcode->names_cnt) -
               bbuilder->ops.buf);

  if (fvar != start) {
    /* iterate all `AST_VAR`s in the current scope */
    do {
      V7_CHECK_INTERNAL(fetch_tag(v7, bbuilder, a, &fvar, &pos_after_tag) ==
                        AST_VAR);

      next = ast_get_skip(a, pos_after_tag, AST_VAR_NEXT_SKIP);
      if (next == pos_after_tag) {
        next = 0;
      }

      fvar_end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);

      /*
       * iterate all `AST_VAR_DECL`s and `AST_FUNC_DECL`s in the current
       * `AST_VAR`
       */
      while (fvar < fvar_end) {
        enum ast_tag tag = fetch_tag(v7, bbuilder, a, &fvar, &pos_after_tag);
        V7_CHECK_INTERNAL(tag == AST_VAR_DECL || tag == AST_FUNC_DECL);
        name = ast_get_inlined_data(a, pos_after_tag, &name_len);
        if (tag == AST_VAR_DECL) {
          /*
           * it's a `var` declaration, so, skip the value for now, it'll be set
           * to `undefined` initially
           */
          ast_skip_tree(a, &fvar);
        } else {
          /*
           * tag is an AST_FUNC_DECL: since functions in JS are hoisted,
           * we compile it and put `OP_SET_VAR` directly here
           */
          lit = string_lit(bbuilder, a, pos_after_tag);
          V7_TRY(compile_expr_builder(bbuilder, a, &fvar));
          bcode_op_lit(bbuilder, OP_SET_VAR, lit);

          /* function declarations are stack-neutral */
          bcode_op(bbuilder, OP_DROP);
          /*
           * Note: the `is_stack_neutral` flag will be set by `compile_stmt`
           * later, when it encounters `AST_FUNC_DECL` again.
           */
        }
        V7_TRY(bcode_add_name(bbuilder, name, name_len, &names_end));
      }

      if (next > 0) {
        fvar = next - 1;
      }

    } while (next != 0);
  }

clean:
  return rcode;
}

/*
 * Just like `compile_expr_builder`, but it takes additional argument:
 *`for_call`.
 * If it's non-zero, the stack is additionally populated with `this` value
 * for call.
 *
 * If there is a refinement (a dot, or a subscript), then it'll be the
 * appropriate object. Otherwise, it's `undefined`.
 *
 * In non-strict mode, `undefined` will be changed to Global Object at runtime,
 * see `OP_CALL` handling in `eval_bcode()`.
 */
V7_PRIVATE enum v7_err compile_expr_ext(struct bcode_builder *bbuilder,
                                        struct ast *a, ast_off_t *ppos,
                                        uint8_t for_call) {
  enum v7_err rcode = V7_OK;
  struct v7 *v7 = bbuilder->v7;
  ast_off_t pos_after_tag;
  enum ast_tag tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);

  switch (tag) {
    case AST_MEMBER: {
      lit_t lit = string_lit(bbuilder, a, pos_after_tag);
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      if (for_call) {
        /* current TOS will be used as `this` */
        bcode_op(bbuilder, OP_DUP);
      }
      bcode_push_lit(bbuilder, lit);
      bcode_op(bbuilder, OP_GET);
      break;
    }

    case AST_INDEX:
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      if (for_call) {
        /* current TOS will be used as `this` */
        bcode_op(bbuilder, OP_DUP);
      }
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_GET);
      break;

    default:
      if (for_call) {
        /*
         * `this` will be an `undefined` (but it may change to Global Object
         * at runtime)
         */
        bcode_op(bbuilder, OP_PUSH_UNDEFINED);
      }
      *ppos = pos_after_tag - 1;
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      break;
  }

clean:
  return rcode;
}

V7_PRIVATE enum v7_err compile_delete(struct bcode_builder *bbuilder,
                                      struct ast *a, ast_off_t *ppos) {
  enum v7_err rcode = V7_OK;
  struct v7 *v7 = bbuilder->v7;
  ast_off_t pos_after_tag;
  enum ast_tag tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);

  switch (tag) {
    case AST_MEMBER: {
      /* Delete a specified property of an object */
      lit_t lit = string_lit(bbuilder, a, pos_after_tag);
      /* put an object to delete property from */
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      /* put a property name */
      bcode_push_lit(bbuilder, lit);
      bcode_op(bbuilder, OP_DELETE);
      break;
    }

    case AST_INDEX:
      /* Delete a specified property of an object */
      /* put an object to delete property from */
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      /* put a property name */
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_DELETE);
      break;

    case AST_IDENT:
      /* Delete the scope variable (or throw an error if strict mode) */
      if (!bbuilder->bcode->strict_mode) {
        /* put a property name */
        bcode_push_lit(bbuilder, string_lit(bbuilder, a, pos_after_tag));
        bcode_op(bbuilder, OP_DELETE_VAR);
      } else {
        rcode =
            v7_throwf(bbuilder->v7, SYNTAX_ERROR,
                      "Delete of an unqualified identifier in strict mode.");
        V7_THROW(V7_SYNTAX_ERROR);
      }
      break;

    case AST_UNDEFINED:
      /*
       * `undefined` should actually be an undeletable property of the Global
       * Object, so, trying to delete it just yields `false`
       */
      bcode_op(bbuilder, OP_PUSH_FALSE);
      break;

    default:
      /*
       * For all other cases, we evaluate the expression given to `delete`
       * for side effects, then drop the result, and yield `true`
       */
      *ppos = pos_after_tag - 1;
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_DROP);
      bcode_op(bbuilder, OP_PUSH_TRUE);
      break;
  }

clean:
  return rcode;
}

V7_PRIVATE enum v7_err compile_expr_builder(struct bcode_builder *bbuilder,
                                            struct ast *a, ast_off_t *ppos) {
  enum v7_err rcode = V7_OK;
  struct v7 *v7 = bbuilder->v7;
  ast_off_t pos_after_tag;
  enum ast_tag tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);

  switch (tag) {
    case AST_ADD:
    case AST_SUB:
    case AST_REM:
    case AST_MUL:
    case AST_DIV:
    case AST_LSHIFT:
    case AST_RSHIFT:
    case AST_URSHIFT:
    case AST_OR:
    case AST_XOR:
    case AST_AND:
    case AST_EQ_EQ:
    case AST_EQ:
    case AST_NE:
    case AST_NE_NE:
    case AST_LT:
    case AST_LE:
    case AST_GT:
    case AST_GE:
    case AST_INSTANCEOF:
      V7_TRY(compile_binary(bbuilder, a, ppos, tag));
      break;
    case AST_LOGICAL_NOT:
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_LOGICAL_NOT);
      break;
    case AST_NOT:
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_NOT);
      break;
    case AST_POSITIVE:
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_POS);
      break;
    case AST_NEGATIVE:
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_NEG);
      break;
    case AST_IDENT:
      bcode_op_lit(bbuilder, OP_GET_VAR,
                   string_lit(bbuilder, a, pos_after_tag));
      break;
    case AST_MEMBER:
    case AST_INDEX:
      /*
       * These two are handled by the "extended" version of this function,
       * since we may need to put `this` value on stack, for "method pattern
       * invocation".
       *
       * First of all, restore starting AST position, and then call extended
       * function.
       */
      *ppos = pos_after_tag - 1;
      V7_TRY(compile_expr_ext(bbuilder, a, ppos, 0 /*not for call*/));
      break;
    case AST_IN:
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_IN);
      break;
    case AST_TYPEOF: {
      ast_off_t lookahead = *ppos;
      tag = fetch_tag(v7, bbuilder, a, &lookahead, &pos_after_tag);
      if (tag == AST_IDENT) {
        *ppos = lookahead;
        bcode_op_lit(bbuilder, OP_SAFE_GET_VAR,
                     string_lit(bbuilder, a, pos_after_tag));
      } else {
        V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      }
      bcode_op(bbuilder, OP_TYPEOF);
      break;
    }
    case AST_ASSIGN:
    case AST_PREINC:
    case AST_PREDEC:
    case AST_POSTINC:
    case AST_POSTDEC:
    case AST_REM_ASSIGN:
    case AST_MUL_ASSIGN:
    case AST_DIV_ASSIGN:
    case AST_XOR_ASSIGN:
    case AST_PLUS_ASSIGN:
    case AST_MINUS_ASSIGN:
    case AST_OR_ASSIGN:
    case AST_AND_ASSIGN:
    case AST_LSHIFT_ASSIGN:
    case AST_RSHIFT_ASSIGN:
    case AST_URSHIFT_ASSIGN:
      V7_TRY(compile_assign(bbuilder, a, ppos, tag));
      break;
    case AST_COND: {
      /*
      * A ? B : C
      *
      * ->
      *
      *   <A>
      *   JMP_FALSE false
      *   <B>
      *   JMP end
      * false:
      *   <C>
      * end:
      *
      */
      bcode_off_t false_label, end_label;
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      false_label = bcode_op_target(bbuilder, OP_JMP_FALSE);
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      end_label = bcode_op_target(bbuilder, OP_JMP);
      bcode_patch_target(bbuilder, false_label, bcode_pos(bbuilder));
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_patch_target(bbuilder, end_label, bcode_pos(bbuilder));
      break;
    }
    case AST_LOGICAL_OR:
    case AST_LOGICAL_AND: {
      /*
       * A && B
       *
       * ->
       *
       *   <A>
       *   JMP_FALSE end
       *   POP
       *   <B>
       * end:
       *
       */
      bcode_off_t end_label;
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_DUP);
      end_label = bcode_op_target(
          bbuilder, tag == AST_LOGICAL_AND ? OP_JMP_FALSE : OP_JMP_TRUE);
      bcode_op(bbuilder, OP_DROP);
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_patch_target(bbuilder, end_label, bcode_pos(bbuilder));
      break;
    }
    /*
     * A, B, C
     *
     * ->
     *
     * <A>
     * DROP
     * <B>
     * DROP
     * <C>
     */
    case AST_SEQ: {
      ast_off_t end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);
      while (*ppos < end) {
        V7_TRY(compile_expr_builder(bbuilder, a, ppos));
        if (*ppos < end) {
          bcode_op(bbuilder, OP_DROP);
        }
      }
      break;
    }
    case AST_CALL:
    case AST_NEW: {
      /*
       * f()
       *
       * ->
       *
       *  PUSH_UNDEFINED (value for `this`)
       *  GET_VAR "f"
       *  CHECK_CALL
       *  CALL 0 args
       *
       * ---------------
       *
       * f(a, b)
       *
       * ->
       *
       *  PUSH_UNDEFINED (value for `this`)
       *  GET_VAR "f"
       *  CHECK_CALL
       *  GET_VAR "a"
       *  GET_VAR "b"
       *  CALL 2 args
       *
       * ---------------
       *
       * o.f(a, b)
       *
       * ->
       *
       *  GET_VAR "o" (so that `this` will be an `o`)
       *  DUP         (we'll also need `o` for GET below, so, duplicate it)
       *  PUSH_LIT "f"
       *  GET         (get property "f" of the object "o")
       *  CHECK_CALL
       *  GET_VAR "a"
       *  GET_VAR "b"
       *  CALL 2 args
       *
       */
      int args;
      ast_off_t end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);

      V7_TRY(compile_expr_ext(bbuilder, a, ppos, 1 /*for call*/));
      bcode_op(bbuilder, OP_CHECK_CALL);
      for (args = 0; *ppos < end; args++) {
        V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      }
      bcode_op(bbuilder, (tag == AST_CALL ? OP_CALL : OP_NEW));
      if (args > 0x7f) {
        rcode = v7_throwf(bbuilder->v7, SYNTAX_ERROR, "too many arguments");
        V7_THROW(V7_SYNTAX_ERROR);
      }
      bcode_op(bbuilder, (uint8_t) args);
      break;
    }
    case AST_DELETE: {
      V7_TRY(compile_delete(bbuilder, a, ppos));
      break;
    }
    case AST_OBJECT: {
      /*
       * {a:<B>, ...}
       *
       * ->
       *
       *   CREATE_OBJ
       *   DUP
       *   PUSH_LIT "a"
       *   <B>
       *   SET
       *   POP
       *   ...
       */

      /*
       * Literal indices of property names of current object literal.
       * Needed for strict mode: we need to keep track of the added
       * properties, since duplicates are not allowed
       */
      struct mbuf cur_literals;
      lit_t lit;
      ast_off_t end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);
      mbuf_init(&cur_literals, 0);

      bcode_op(bbuilder, OP_CREATE_OBJ);
      while (*ppos < end) {
        tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);
        switch (tag) {
          case AST_PROP:
            bcode_op(bbuilder, OP_DUP);
            lit = string_lit(bbuilder, a, pos_after_tag);

/* disabled because we broke get_lit */
#if 0
            if (bbuilder->bcode->strict_mode) {
              /*
               * In strict mode, check for duplicate property names in
               * object literals
               */
              char *plit;
              for (plit = (char *) cur_literals.buf;
                   (char *) plit < cur_literals.buf + cur_literals.len;
                   plit++) {
                const char *str1, *str2;
                size_t size1, size2;
                v7_val_t val1, val2;

                val1 = bcode_get_lit(bbuilder->bcode, lit);
                str1 = v7_get_string(bbuilder->v7, &val1, &size1);

                val2 = bcode_get_lit(bbuilder->bcode, *plit);
                str2 = v7_get_string(bbuilder->v7, &val2, &size2);

                if (size1 == size2 && memcmp(str1, str2, size1) == 0) {
                  /* found already existing property of the same name */
                  rcode = v7_throwf(bbuilder->v7, SYNTAX_ERROR,
                                    "duplicate data property in object literal "
                                    "is not allowed in strict mode");
                  V7_THROW2(V7_SYNTAX_ERROR, ast_object_clean);
                }
              }
              mbuf_append(&cur_literals, &lit, sizeof(lit));
            }
#endif
            bcode_push_lit(bbuilder, lit);
            V7_TRY(compile_expr_builder(bbuilder, a, ppos));
            bcode_op(bbuilder, OP_SET);
            bcode_op(bbuilder, OP_DROP);
            break;
          default:
            rcode = v7_throwf(bbuilder->v7, SYNTAX_ERROR, "not implemented");
            V7_THROW2(V7_SYNTAX_ERROR, ast_object_clean);
        }
      }

    ast_object_clean:
      mbuf_free(&cur_literals);
      if (rcode != V7_OK) {
        V7_THROW(rcode);
      }
      break;
    }
    case AST_ARRAY: {
      /*
       * [<A>,,<B>,...]
       *
       * ->
       *
       *   CREATE_ARR
       *   PUSH_ZERO
       *
       *   2DUP
       *   <A>
       *   SET
       *   POP
       *   PUSH_ONE
       *   ADD
       *
       *   PUSH_ONE
       *   ADD
       *
       *   2DUP
       *   <B>
       *   ...
       *   POP // tmp index
       *
       * TODO(mkm): optimize this out. we can have more compact array push
       * that uses a special marker value for missing array elements
       * (which are not the same as undefined btw)
       */
      ast_off_t end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);
      bcode_op(bbuilder, OP_CREATE_ARR);
      bcode_op(bbuilder, OP_PUSH_ZERO);
      while (*ppos < end) {
        ast_off_t lookahead = *ppos;
        tag = fetch_tag(v7, bbuilder, a, &lookahead, &pos_after_tag);
        if (tag != AST_NOP) {
          bcode_op(bbuilder, OP_2DUP);
          V7_TRY(compile_expr_builder(bbuilder, a, ppos));
          bcode_op(bbuilder, OP_SET);
          bcode_op(bbuilder, OP_DROP);
        } else {
          *ppos = lookahead; /* skip nop */
        }
        bcode_op(bbuilder, OP_PUSH_ONE);
        bcode_op(bbuilder, OP_ADD);
      }
      bcode_op(bbuilder, OP_DROP);
      break;
    }
    case AST_FUNC: {
      lit_t flit;

      /*
       * Create half-done function: without scope and prototype. The real
       * function will be created from this one during bcode evaluation: see
       * `bcode_instantiate_function()`.
       */
      val_t funv = mk_js_function(bbuilder->v7, NULL, V7_UNDEFINED);

      /* Create bcode in this half-done function */
      struct v7_js_function *func = get_js_function_struct(funv);
      func->bcode = (struct bcode *) calloc(1, sizeof(*bbuilder->bcode));
      bcode_init(func->bcode, bbuilder->bcode->strict_mode,
                 NULL /* will be set below */, 0);
      bcode_copy_filename_from(func->bcode, bbuilder->bcode);
      retain_bcode(bbuilder->v7, func->bcode);
      flit = bcode_add_lit(bbuilder, funv);

      *ppos = pos_after_tag - 1;
      V7_TRY(compile_function(v7, a, ppos, func->bcode));
      bcode_push_lit(bbuilder, flit);
      bcode_op(bbuilder, OP_FUNC_LIT);
      break;
    }
    case AST_THIS:
      bcode_op(bbuilder, OP_PUSH_THIS);
      break;
    case AST_VOID:
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_DROP);
      bcode_op(bbuilder, OP_PUSH_UNDEFINED);
      break;
    case AST_NULL:
      bcode_op(bbuilder, OP_PUSH_NULL);
      break;
    case AST_NOP:
    case AST_UNDEFINED:
      bcode_op(bbuilder, OP_PUSH_UNDEFINED);
      break;
    case AST_TRUE:
      bcode_op(bbuilder, OP_PUSH_TRUE);
      break;
    case AST_FALSE:
      bcode_op(bbuilder, OP_PUSH_FALSE);
      break;
    case AST_NUM: {
      double dv = ast_get_num(a, pos_after_tag);
      if (dv == 0) {
        bcode_op(bbuilder, OP_PUSH_ZERO);
      } else if (dv == 1) {
        bcode_op(bbuilder, OP_PUSH_ONE);
      } else {
        bcode_push_lit(bbuilder, bcode_add_lit(bbuilder, v7_mk_number(v7, dv)));
      }
      break;
    }
    case AST_STRING:
      bcode_push_lit(bbuilder, string_lit(bbuilder, a, pos_after_tag));
      break;
    case AST_REGEX:
#if V7_ENABLE__RegExp
    {
      lit_t tmp;
      rcode = regexp_lit(bbuilder, a, pos_after_tag, &tmp);
      if (rcode != V7_OK) {
        rcode = V7_SYNTAX_ERROR;
        goto clean;
      }

      bcode_push_lit(bbuilder, tmp);
      break;
    }
#else
      rcode =
          v7_throwf(bbuilder->v7, SYNTAX_ERROR, "Regexp support is disabled");
      V7_THROW(V7_SYNTAX_ERROR);
#endif
    case AST_LABEL:
    case AST_LABELED_BREAK:
    case AST_LABELED_CONTINUE:
      /* TODO(dfrank): implement */
      rcode = v7_throwf(bbuilder->v7, SYNTAX_ERROR, "not implemented");
      V7_THROW(V7_SYNTAX_ERROR);
    case AST_WITH:
      rcode = v7_throwf(bbuilder->v7, SYNTAX_ERROR, "not implemented");
      V7_THROW(V7_SYNTAX_ERROR);
    default:
      /*
       * We end up here if the AST is broken.
       *
       * It might be appropriate to return `V7_INTERNAL_ERROR` here, but since
       * we might receive AST from network or something, we just interpret
       * it as SyntaxError.
       */
      rcode = v7_throwf(bbuilder->v7, SYNTAX_ERROR, "unknown ast node %d",
                        (int) tag);
      V7_THROW(V7_SYNTAX_ERROR);
  }
clean:
  return rcode;
}

V7_PRIVATE enum v7_err compile_stmt(struct bcode_builder *bbuilder,
                                    struct ast *a, ast_off_t *ppos);

V7_PRIVATE enum v7_err compile_stmts(struct bcode_builder *bbuilder,
                                     struct ast *a, ast_off_t *ppos,
                                     ast_off_t end) {
  enum v7_err rcode = V7_OK;
  struct v7 *v7 = bbuilder->v7;

  while (*ppos < end) {
    V7_TRY(compile_stmt(bbuilder, a, ppos));
    if (!bbuilder->v7->is_stack_neutral) {
      bcode_op(bbuilder, OP_SWAP_DROP);
    } else {
      bbuilder->v7->is_stack_neutral = 0;
    }
  }
clean:
  return rcode;
}

V7_PRIVATE enum v7_err compile_stmt(struct bcode_builder *bbuilder,
                                    struct ast *a, ast_off_t *ppos) {
  ast_off_t end;
  enum ast_tag tag;
  ast_off_t cond, pos_after_tag;
  bcode_off_t body_target, body_label, cond_label;
  struct mbuf case_labels;
  enum v7_err rcode = V7_OK;
  struct v7 *v7 = bbuilder->v7;

  tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);

  mbuf_init(&case_labels, 0);

  switch (tag) {
    /*
     * if(E) {
     *   BT...
     * } else {
     *   BF...
     * }
     *
     * ->
     *
     *   <E>
     *   JMP_FALSE body
     *   <BT>
     *   JMP end
     * body:
     *   <BF>
     * end:
     *
     * If else clause is omitted, it will emit output equivalent to:
     *
     * if(E) {BT} else undefined;
     */
    case AST_IF: {
      ast_off_t if_false;
      bcode_off_t end_label, if_false_label;
      end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);
      if_false = ast_get_skip(a, pos_after_tag, AST_END_IF_TRUE_SKIP);

      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      if_false_label = bcode_op_target(bbuilder, OP_JMP_FALSE);

      /* body if true */
      V7_TRY(compile_stmts(bbuilder, a, ppos, if_false));

      if (if_false != end) {
        /* `else` branch is present */
        end_label = bcode_op_target(bbuilder, OP_JMP);

        /* will jump here if `false` */
        bcode_patch_target(bbuilder, if_false_label, bcode_pos(bbuilder));

        /* body if false */
        V7_TRY(compile_stmts(bbuilder, a, ppos, end));

        bcode_patch_target(bbuilder, end_label, bcode_pos(bbuilder));
      } else {
        /*
         * `else` branch is not present: just remember where we should
         * jump in case of `false`
         */
        bcode_patch_target(bbuilder, if_false_label, bcode_pos(bbuilder));
      }

      bbuilder->v7->is_stack_neutral = 1;
      break;
    }
    /*
     * while(C) {
     *   B...
     * }
     *
     * ->
     *
     *   TRY_PUSH_LOOP end
     *   JMP cond
     * body:
     *   <B>
     * cond:
     *   <C>
     *   JMP_TRUE body
     * end:
     *   JMP_IF_CONTINUE cond
     *   TRY_POP
     *
     */
    case AST_WHILE: {
      bcode_off_t end_label, continue_label, continue_target;

      end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);
      cond = *ppos;
      ast_skip_tree(a, ppos);

      end_label = bcode_op_target(bbuilder, OP_TRY_PUSH_LOOP);

      /*
       * Condition check is at the end of the loop, this layout
       * reduces the number of jumps in the steady state.
       */
      cond_label = bcode_op_target(bbuilder, OP_JMP);
      body_target = bcode_pos(bbuilder);

      V7_TRY(compile_stmts(bbuilder, a, ppos, end));

      continue_target = bcode_pos(bbuilder);
      bcode_patch_target(bbuilder, cond_label, continue_target);

      V7_TRY(compile_expr_builder(bbuilder, a, &cond));
      body_label = bcode_op_target(bbuilder, OP_JMP_TRUE);
      bcode_patch_target(bbuilder, body_label, body_target);

      bcode_patch_target(bbuilder, end_label, bcode_pos(bbuilder));
      continue_label = bcode_op_target(bbuilder, OP_JMP_IF_CONTINUE);
      bcode_patch_target(bbuilder, continue_label, continue_target);
      bcode_op(bbuilder, OP_TRY_POP);

      bbuilder->v7->is_stack_neutral = 1;
      break;
    }
    case AST_BREAK:
      bcode_op(bbuilder, OP_BREAK);
      break;
    case AST_CONTINUE:
      bcode_op(bbuilder, OP_CONTINUE);
      break;
    /*
     * Frame objects (`v7->vals.call_stack`) contain one more hidden property:
     * `____t`, which is an array of offsets in bcode. Each element of the array
     * is an offset of either `catch` or `finally` block (distinguished by the
     * tag: `OFFSET_TAG_CATCH` or `OFFSET_TAG_FINALLY`). Let's call this array
     * as a "try stack". When evaluator enters new `try` block, it adds
     * appropriate offset(s) at the top of "try stack", and when we unwind the
     * stack, we can "pop" offsets from "try stack" at each level.
     *
     * try {
     *   TRY_B
     * } catch (e) {
     *   CATCH_B
     * } finally {
     *   FIN_B
     * }
     *
     * ->
     *    OP_TRY_PUSH_FINALLY finally
     *    OP_TRY_PUSH_CATCH catch
     *    <TRY_B>
     *    OP_TRY_POP
     *    JMP finally
     *  catch:
     *    OP_TRY_POP
     *    OP_ENTER_CATCH <e>
     *    <CATCH_B>
     *    OP_EXIT_CATCH
     *  finally:
     *    OP_TRY_POP
     *    <FIN_B>
     *    OP_AFTER_FINALLY
     *
     * ---------------
     *
     * try {
     *   TRY_B
     * } catch (e) {
     *   CATCH_B
     * }
     *
     * ->
     *    OP_TRY_PUSH_CATCH catch
     *    <TRY_B>
     *    OP_TRY_POP
     *    JMP end
     *  catch:
     *    OP_TRY_POP
     *    OP_ENTER_CATCH <e>
     *    <CATCH_B>
     *    OP_EXIT_CATCH
     *  end:
     *
     * ---------------
     *
     * try {
     *   TRY_B
     * } finally {
     *   FIN_B
     * }
     *
     * ->
     *    OP_TRY_PUSH_FINALLY finally
     *    <TRY_B>
     *  finally:
     *    OP_TRY_POP
     *    <FIN_B>
     *    OP_AFTER_FINALLY
     */
    case AST_TRY: {
      ast_off_t acatch, afinally;
      bcode_off_t finally_label, catch_label;

      end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);
      acatch = ast_get_skip(a, pos_after_tag, AST_TRY_CATCH_SKIP);
      afinally = ast_get_skip(a, pos_after_tag, AST_TRY_FINALLY_SKIP);

      if (afinally != end) {
        /* `finally` clause is present: push its offset */
        finally_label = bcode_op_target(bbuilder, OP_TRY_PUSH_FINALLY);
      }

      if (acatch != afinally) {
        /* `catch` clause is present: push its offset */
        catch_label = bcode_op_target(bbuilder, OP_TRY_PUSH_CATCH);
      }

      /* compile statements of `try` block */
      V7_TRY(compile_stmts(bbuilder, a, ppos, acatch));

      if (acatch != afinally) {
        /* `catch` clause is present: compile it */
        bcode_off_t after_catch_label;

        /*
         * pop offset pushed by OP_TRY_PUSH_CATCH, and jump over the `catch`
         * block
         */
        bcode_op(bbuilder, OP_TRY_POP);
        after_catch_label = bcode_op_target(bbuilder, OP_JMP);

        /* --- catch --- */

        /* in case of exception in the `try` block above, we'll get here */
        bcode_patch_target(bbuilder, catch_label, bcode_pos(bbuilder));

        /* pop offset pushed by OP_TRY_PUSH_CATCH */
        bcode_op(bbuilder, OP_TRY_POP);

        /*
         * retrieve identifier where to store thrown error, and make sure
         * it is actually an indentifier (AST_IDENT)
         */
        tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);
        V7_CHECK(tag == AST_IDENT, V7_SYNTAX_ERROR);

        /*
         * when we enter `catch` block, the TOS contains thrown value.
         * We should create private frame for the `catch` clause, and populate
         * a variable with the thrown value there.
         * The `OP_ENTER_CATCH` opcode does just that.
         */
        bcode_op_lit(bbuilder, OP_ENTER_CATCH,
                     string_lit(bbuilder, a, pos_after_tag));

        /*
         * compile statements until the end of `catch` clause
         * (`afinally` points to the end of the `catch` clause independently
         * of whether the `finally` clause is present)
         */
        V7_TRY(compile_stmts(bbuilder, a, ppos, afinally));

        /* pop private frame */
        bcode_op(bbuilder, OP_EXIT_CATCH);

        bcode_patch_target(bbuilder, after_catch_label, bcode_pos(bbuilder));
      }

      if (afinally != end) {
        /* `finally` clause is present: compile it */

        /* --- finally --- */

        /* after the `try` block above executes, we'll get here */
        bcode_patch_target(bbuilder, finally_label, bcode_pos(bbuilder));

        /* pop offset pushed by OP_TRY_PUSH_FINALLY */
        bcode_op(bbuilder, OP_TRY_POP);

        /* compile statements until the end of `finally` clause */
        V7_TRY(compile_stmts(bbuilder, a, ppos, end));

        bcode_op(bbuilder, OP_AFTER_FINALLY);
      }

      bbuilder->v7->is_stack_neutral = 1;
      break;
    }

    case AST_THROW: {
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_THROW);
      break;
    }

    /*
     * switch(E) {
     * default:
     *   D...
     * case C1:
     *   B1...
     * case C2:
     *   B2...
     * }
     *
     * ->
     *
     *   TRY_PUSH_SWITCH end
     *   <E>
     *   DUP
     *   <C1>
     *   EQ
     *   JMP_TRUE_DROP l1
     *   DUP
     *   <C2>
     *   EQ
     *   JMP_TRUE_DROP l2
     *   DROP
     *   JMP dfl
     *
     * dfl:
     *   <D>
     *
     * l1:
     *   <B1>
     *
     * l2:
     *   <B2>
     *
     * end:
     *   TRY_POP
     *
     * If the default case is missing we treat it as if had an empty body and
     * placed in last position (i.e. `dfl` label is replaced with `end`).
     *
     * Before emitting a case/default block (except the first one) we have to
     * drop the TOS resulting from evaluating the last expression
     */
    case AST_SWITCH: {
      bcode_off_t dfl_label, end_label;
      ast_off_t case_end, case_start;
      enum ast_tag case_tag;
      int i, has_default = 0, cases = 0;

      end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);

      end_label = bcode_op_target(bbuilder, OP_TRY_PUSH_SWITCH);

      V7_TRY(compile_expr_builder(bbuilder, a, ppos));

      case_start = *ppos;
      /* first pass: evaluate case expression and generate jump table */
      while (*ppos < end) {
        case_tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);
        assert(case_tag == AST_DEFAULT || case_tag == AST_CASE);

        case_end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);

        switch (case_tag) {
          case AST_DEFAULT:
            /* default jump table entry must be the last one */
            break;
          case AST_CASE: {
            bcode_off_t case_label;
            bcode_op(bbuilder, OP_DUP);
            V7_TRY(compile_expr_builder(bbuilder, a, ppos));
            bcode_op(bbuilder, OP_EQ);
            case_label = bcode_op_target(bbuilder, OP_JMP_TRUE_DROP);
            cases++;
            mbuf_append(&case_labels, &case_label, sizeof(case_label));
            break;
          }
          default:
            assert(case_tag == AST_DEFAULT || case_tag == AST_CASE);
        }
        *ppos = case_end;
      }

      /* jmp table epilogue: unconditional jump to default case */
      bcode_op(bbuilder, OP_DROP);
      dfl_label = bcode_op_target(bbuilder, OP_JMP);

      *ppos = case_start;
      /* second pass: emit case bodies and patch jump table */

      for (i = 0; *ppos < end;) {
        case_tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);
        assert(case_tag == AST_DEFAULT || case_tag == AST_CASE);
        assert(i <= cases);

        case_end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);

        switch (case_tag) {
          case AST_DEFAULT:
            has_default = 1;
            bcode_patch_target(bbuilder, dfl_label, bcode_pos(bbuilder));
            V7_TRY(compile_stmts(bbuilder, a, ppos, case_end));
            break;
          case AST_CASE: {
            bcode_off_t case_label = ((bcode_off_t *) case_labels.buf)[i++];
            bcode_patch_target(bbuilder, case_label, bcode_pos(bbuilder));
            ast_skip_tree(a, ppos);
            V7_TRY(compile_stmts(bbuilder, a, ppos, case_end));
            break;
          }
          default:
            assert(case_tag == AST_DEFAULT || case_tag == AST_CASE);
        }

        *ppos = case_end;
      }
      mbuf_free(&case_labels);

      if (!has_default) {
        bcode_patch_target(bbuilder, dfl_label, bcode_pos(bbuilder));
      }

      bcode_patch_target(bbuilder, end_label, bcode_pos(bbuilder));
      bcode_op(bbuilder, OP_TRY_POP);

      bbuilder->v7->is_stack_neutral = 1;
      break;
    }
    /*
     * for(INIT,COND,IT) {
     *   B...
     * }
     *
     * ->
     *
     *   <INIT>
     *   DROP
     *   TRY_PUSH_LOOP end
     *   JMP cond
     * body:
     *   <B>
     * next:
     *   <IT>
     *   DROP
     * cond:
     *   <COND>
     *   JMP_TRUE body
     * end:
     *   JMP_IF_CONTINUE next
     *   TRY_POP
     *
     */
    case AST_FOR: {
      ast_off_t iter, body, lookahead;
      bcode_off_t end_label, continue_label, continue_target;
      end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);
      body = ast_get_skip(a, pos_after_tag, AST_FOR_BODY_SKIP);

      lookahead = *ppos;
      tag = fetch_tag(v7, bbuilder, a, &lookahead, &pos_after_tag);
      /*
       * Support for `var` declaration in INIT
       */
      if (tag == AST_VAR) {
        ast_off_t fvar_end;
        lit_t lit;

        *ppos = lookahead;
        fvar_end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);

        /*
         * Iterate through all vars in the given `var` declaration: they are
         * just like assigments here
         */
        while (*ppos < fvar_end) {
          tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);
          /* Only var declarations are allowed (not function declarations) */
          V7_CHECK_INTERNAL(tag == AST_VAR_DECL);
          lit = string_lit(bbuilder, a, pos_after_tag);
          V7_TRY(compile_expr_builder(bbuilder, a, ppos));

          /* Just like an assigment */
          bcode_op_lit(bbuilder, OP_SET_VAR, lit);

          /* INIT is stack-neutral */
          bcode_op(bbuilder, OP_DROP);
        }
      } else {
        /* normal expression in INIT (not `var` declaration) */
        V7_TRY(compile_expr_builder(bbuilder, a, ppos));
        /* INIT is stack-neutral */
        bcode_op(bbuilder, OP_DROP);
      }
      cond = *ppos;
      ast_skip_tree(a, ppos);
      iter = *ppos;
      *ppos = body;

      end_label = bcode_op_target(bbuilder, OP_TRY_PUSH_LOOP);
      cond_label = bcode_op_target(bbuilder, OP_JMP);
      body_target = bcode_pos(bbuilder);
      V7_TRY(compile_stmts(bbuilder, a, ppos, end));

      continue_target = bcode_pos(bbuilder);

      V7_TRY(compile_expr_builder(bbuilder, a, &iter));
      bcode_op(bbuilder, OP_DROP);

      bcode_patch_target(bbuilder, cond_label, bcode_pos(bbuilder));

      /*
       * Handle for(INIT;;ITER)
       */
      lookahead = cond;
      tag = fetch_tag(v7, bbuilder, a, &lookahead, &pos_after_tag);
      if (tag == AST_NOP) {
        bcode_op(bbuilder, OP_JMP);
      } else {
        V7_TRY(compile_expr_builder(bbuilder, a, &cond));
        bcode_op(bbuilder, OP_JMP_TRUE);
      }
      body_label = bcode_add_target(bbuilder);
      bcode_patch_target(bbuilder, body_label, body_target);
      bcode_patch_target(bbuilder, end_label, bcode_pos(bbuilder));

      continue_label = bcode_op_target(bbuilder, OP_JMP_IF_CONTINUE);
      bcode_patch_target(bbuilder, continue_label, continue_target);

      bcode_op(bbuilder, OP_TRY_POP);

      bbuilder->v7->is_stack_neutral = 1;
      break;
    }
    /*
     * for(I in O) {
     *   B...
     * }
     *
     * ->
     *
     *   DUP
     *   <O>
     *   SWAP
     *   STASH
     *   DROP
     *   PUSH_NULL
     *   TRY_PUSH_LOOP brend
     * loop:
     *   NEXT_PROP
     *   JMP_FALSE end
     *   SET_VAR <I>
     *   UNSTASH
     *   <B>
     * next:
     *   STASH
     *   DROP
     *   JMP loop
     * end:
     *   UNSTASH
     *   JMP try_pop:
     * brend:
     *              # got here after a `break` or `continue` from a loop body:
     *   JMP_IF_CONTINUE next
     *
     *              # we're not going to `continue`, so, need to remove an
     *              # extra stuff that was needed for the NEXT_PROP
     *
     *   SWAP_DROP  # drop handle
     *   SWAP_DROP  # drop <O>
     *   SWAP_DROP  # drop the value preceding the loop
     * try_pop:
     *   TRY_POP
     *
     */
    case AST_FOR_IN: {
      lit_t lit;
      bcode_off_t loop_label, loop_target, end_label, brend_label,
          continue_label, pop_label, continue_target;
      ast_off_t end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);

      tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);
      /* TODO(mkm) accept any l-value */
      if (tag == AST_VAR) {
        tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);
        V7_CHECK_INTERNAL(tag == AST_VAR_DECL);
        lit = string_lit(bbuilder, a, pos_after_tag);
        ast_skip_tree(a, ppos);
      } else {
        V7_CHECK_INTERNAL(tag == AST_IDENT);
        lit = string_lit(bbuilder, a, pos_after_tag);
      }

      /*
       * preserve previous statement value.
       * We need to feed the previous value into the stash
       * because it's required for the loop steady state.
       *
       * The stash register is required to simplify the steady state stack
       * management, in particular the removal of value in 3rd position in case
       * a of not taken exit.
       *
       * TODO(mkm): consider having a stash OP that moves a value to the stash
       * register instead of copying it. The current behaviour has been
       * optimized for the `assign` use case which seems more common.
       */
      bcode_op(bbuilder, OP_DUP);
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_SWAP);
      bcode_op(bbuilder, OP_STASH);
      bcode_op(bbuilder, OP_DROP);

      /*
       * OP_NEXT_PROP keeps the current position in an opaque handler.
       * Feeding a null as initial value.
       */
      bcode_op(bbuilder, OP_PUSH_NULL);

      brend_label = bcode_op_target(bbuilder, OP_TRY_PUSH_LOOP);

      /* loop: */
      loop_target = bcode_pos(bbuilder);

      /*
       * The loop stead state begins with the following stack layout:
       * `( S:v o h )`
       */

      bcode_op(bbuilder, OP_NEXT_PROP);
      end_label = bcode_op_target(bbuilder, OP_JMP_FALSE);
      bcode_op_lit(bbuilder, OP_SET_VAR, lit);

      /*
       * The stash register contains the value of the previous statement,
       * being it the statement before the for..in statement or
       * the previous iteration. We move it to the data stack. It will
       * be replaced by the values of the body statements as usual.
       */
      bcode_op(bbuilder, OP_UNSTASH);

      /*
       * This node is always a NOP, for compatibility
       * with the layout of the AST_FOR node.
       */
      ast_skip_tree(a, ppos);

      V7_TRY(compile_stmts(bbuilder, a, ppos, end));

      continue_target = bcode_pos(bbuilder);

      /*
       * Save the last body statement. If next evaluation of NEXT_PROP returns
       * false, we'll unstash it.
       */
      bcode_op(bbuilder, OP_STASH);
      bcode_op(bbuilder, OP_DROP);

      loop_label = bcode_op_target(bbuilder, OP_JMP);
      bcode_patch_target(bbuilder, loop_label, loop_target);

      /* end: */
      bcode_patch_target(bbuilder, end_label, bcode_pos(bbuilder));
      bcode_op(bbuilder, OP_UNSTASH);

      pop_label = bcode_op_target(bbuilder, OP_JMP);

      /* brend: */
      bcode_patch_target(bbuilder, brend_label, bcode_pos(bbuilder));

      continue_label = bcode_op_target(bbuilder, OP_JMP_IF_CONTINUE);
      bcode_patch_target(bbuilder, continue_label, continue_target);

      bcode_op(bbuilder, OP_SWAP_DROP);
      bcode_op(bbuilder, OP_SWAP_DROP);
      bcode_op(bbuilder, OP_SWAP_DROP);

      /* try_pop: */
      bcode_patch_target(bbuilder, pop_label, bcode_pos(bbuilder));

      bcode_op(bbuilder, OP_TRY_POP);

      bbuilder->v7->is_stack_neutral = 1;
      break;
    }
    /*
     * do {
     *   B...
     * } while(COND);
     *
     * ->
     *
     *   TRY_PUSH_LOOP end
     * body:
     *   <B>
     * cond:
     *   <COND>
     *   JMP_TRUE body
     * end:
     *   JMP_IF_CONTINUE cond
     *   TRY_POP
     *
     */
    case AST_DOWHILE: {
      bcode_off_t end_label, continue_label, continue_target;
      end = ast_get_skip(a, pos_after_tag, AST_DO_WHILE_COND_SKIP);
      end_label = bcode_op_target(bbuilder, OP_TRY_PUSH_LOOP);
      body_target = bcode_pos(bbuilder);
      V7_TRY(compile_stmts(bbuilder, a, ppos, end));

      continue_target = bcode_pos(bbuilder);
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      body_label = bcode_op_target(bbuilder, OP_JMP_TRUE);
      bcode_patch_target(bbuilder, body_label, body_target);

      bcode_patch_target(bbuilder, end_label, bcode_pos(bbuilder));
      continue_label = bcode_op_target(bbuilder, OP_JMP_IF_CONTINUE);
      bcode_patch_target(bbuilder, continue_label, continue_target);
      bcode_op(bbuilder, OP_TRY_POP);

      bbuilder->v7->is_stack_neutral = 1;
      break;
    }
    case AST_VAR: {
      /*
       * Var decls are hoisted when the function frame is created. Vars
       * declared inside a `with` or `catch` block belong to the function
       * lexical scope, and although those clauses create an inner frame
       * no new variables should be created in it. A var decl thus
       * behaves as a normal assignment at runtime.
       */
      lit_t lit;
      end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);
      while (*ppos < end) {
        tag = fetch_tag(v7, bbuilder, a, ppos, &pos_after_tag);
        if (tag == AST_FUNC_DECL) {
          /*
           * function declarations are already set during hoisting (see
           * `compile_local_vars()`), so, skip it.
           *
           * Plus, they are stack-neutral, so don't forget to set
           * `is_stack_neutral`.
           */
          ast_skip_tree(a, ppos);
          bbuilder->v7->is_stack_neutral = 1;
        } else {
          /*
           * compile `var` declaration: basically it looks similar to an
           * assignment, but it differs from an assignment is that it's
           * stack-neutral: `1; var a = 5;` yields `1`, not `5`.
           */
          V7_CHECK_INTERNAL(tag == AST_VAR_DECL);
          lit = string_lit(bbuilder, a, pos_after_tag);
          V7_TRY(compile_expr_builder(bbuilder, a, ppos));
          bcode_op_lit(bbuilder, OP_SET_VAR, lit);

          /* `var` declaration is stack-neutral */
          bcode_op(bbuilder, OP_DROP);
          bbuilder->v7->is_stack_neutral = 1;
        }
      }
      break;
    }
    case AST_RETURN:
      bcode_op(bbuilder, OP_PUSH_UNDEFINED);
      bcode_op(bbuilder, OP_RET);
      break;
    case AST_VALUE_RETURN:
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
      bcode_op(bbuilder, OP_RET);
      break;
    default:
      *ppos = pos_after_tag - 1;
      V7_TRY(compile_expr_builder(bbuilder, a, ppos));
  }

clean:
  mbuf_free(&case_labels);
  return rcode;
}

static enum v7_err compile_body(struct bcode_builder *bbuilder, struct ast *a,
                                ast_off_t start, ast_off_t end, ast_off_t body,
                                ast_off_t fvar, ast_off_t *ppos) {
  enum v7_err rcode = V7_OK;
  struct v7 *v7 = bbuilder->v7;

#ifndef V7_FORCE_STRICT_MODE
  /* check 'use strict' */
  if (*ppos < end) {
    ast_off_t tmp_pos = body;
    if (fetch_tag(v7, bbuilder, a, &tmp_pos, NULL) == AST_USE_STRICT) {
      bbuilder->bcode->strict_mode = 1;
      /* move `body` offset, effectively removing `AST_USE_STRICT` from it */
      body = tmp_pos;
    }
  }
#endif

  /* put initial value for the function body execution */
  bcode_op(bbuilder, OP_PUSH_UNDEFINED);

  /*
   * populate `bcode->ops` with function's local variable names. Note that we
   * should do this *after* `OP_PUSH_UNDEFINED`, since `compile_local_vars`
   * emits code that assigns the hoisted functions to local variables, and
   * those statements assume that the stack contains `undefined`.
   */
  V7_TRY(compile_local_vars(bbuilder, a, start, fvar));

  /* compile body */
  *ppos = body;
  V7_TRY(compile_stmts(bbuilder, a, ppos, end));

clean:
  return rcode;
}

/*
 * Compiles a given script and populates a bcode structure.
 * The AST must start with an AST_SCRIPT node.
 */
V7_PRIVATE enum v7_err compile_script(struct v7 *v7, struct ast *a,
                                      struct bcode *bcode) {
  ast_off_t pos_after_tag, end, fvar, pos = 0;
  int saved_line_no = v7->line_no;
  enum v7_err rcode = V7_OK;
  struct bcode_builder bbuilder;
  enum ast_tag tag;

  bcode_builder_init(v7, &bbuilder, bcode);
  v7->line_no = 1;

  tag = fetch_tag(v7, &bbuilder, a, &pos, &pos_after_tag);

  /* first tag should always be AST_SCRIPT */
  assert(tag == AST_SCRIPT);
  (void) tag;

  end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);
  fvar = ast_get_skip(a, pos_after_tag, AST_FUNC_FIRST_VAR_SKIP) - 1;

  V7_TRY(compile_body(&bbuilder, a, pos_after_tag - 1, end, pos /* body */,
                      fvar, &pos));

clean:

  bcode_builder_finalize(&bbuilder);

#ifdef V7_BCODE_DUMP
  if (rcode == V7_OK) {
    fprintf(stderr, "--- script ---\n");
    dump_bcode(v7, stderr, bcode);
  }
#endif

  v7->line_no = saved_line_no;

  return rcode;
}

/*
 * Compiles a given function and populates a bcode structure.
 * The AST must contain an AST_FUNC node at offset ast_off.
 */
V7_PRIVATE enum v7_err compile_function(struct v7 *v7, struct ast *a,
                                        ast_off_t *ppos, struct bcode *bcode) {
  ast_off_t pos_after_tag, start, end, body, fvar;
  const char *name;
  size_t name_len;
  size_t args_cnt;
  enum v7_err rcode = V7_OK;
  struct bcode_builder bbuilder;
  enum ast_tag tag;
  size_t names_end = 0;
  bcode_builder_init(v7, &bbuilder, bcode);
  tag = fetch_tag(v7, &bbuilder, a, ppos, &pos_after_tag);
  start = pos_after_tag - 1;

  (void) tag;
  assert(tag == AST_FUNC);
  end = ast_get_skip(a, pos_after_tag, AST_END_SKIP);
  body = ast_get_skip(a, pos_after_tag, AST_FUNC_BODY_SKIP);
  fvar = ast_get_skip(a, pos_after_tag, AST_FUNC_FIRST_VAR_SKIP) - 1;

  /* retrieve function name */
  tag = fetch_tag(v7, &bbuilder, a, ppos, &pos_after_tag);
  if (tag == AST_IDENT) {
    /* function name is provided */
    name = ast_get_inlined_data(a, pos_after_tag, &name_len);
    V7_TRY(bcode_add_name(&bbuilder, name, name_len, &names_end));
  } else {
    /* no name: anonymous function */
    V7_TRY(bcode_add_name(&bbuilder, "", 0, &names_end));
  }

  /* retrieve function's argument names */
  for (args_cnt = 0; *ppos < body; args_cnt++) {
    if (args_cnt > V7_ARGS_CNT_MAX) {
      /* too many arguments */
      rcode = v7_throwf(v7, SYNTAX_ERROR, "Too many arguments");
      V7_THROW(V7_SYNTAX_ERROR);
    }

    tag = fetch_tag(v7, &bbuilder, a, ppos, &pos_after_tag);
    /*
     * TODO(dfrank): it's not actually an internal error, we get here if
     * we compile e.g. the following: (function(1){})
     */
    V7_CHECK_INTERNAL(tag == AST_IDENT);
    name = ast_get_inlined_data(a, pos_after_tag, &name_len);
    V7_TRY(bcode_add_name(&bbuilder, name, name_len, &names_end));
  }

  bcode->args_cnt = args_cnt;
  bcode->func_name_present = 1;

  V7_TRY(compile_body(&bbuilder, a, start, end, body, fvar, ppos));

clean:
  bcode_builder_finalize(&bbuilder);

#ifdef V7_BCODE_DUMP
  if (rcode == V7_OK) {
    fprintf(stderr, "--- function ---\n");
    dump_bcode(v7, stderr, bcode);
  }
#endif

  return rcode;
}

V7_PRIVATE enum v7_err compile_expr(struct v7 *v7, struct ast *a,
                                    ast_off_t *ppos, struct bcode *bcode) {
  enum v7_err rcode = V7_OK;
  struct bcode_builder bbuilder;
  int saved_line_no = v7->line_no;

  bcode_builder_init(v7, &bbuilder, bcode);
  v7->line_no = 1;

  rcode = compile_expr_builder(&bbuilder, a, ppos);

  bcode_builder_finalize(&bbuilder);
  v7->line_no = saved_line_no;
  return rcode;
}
