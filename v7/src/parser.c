/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/coroutine.h"
#include "v7/src/internal.h"
#include "v7/src/parser.h"
#include "v7/src/tokenizer.h"
#include "v7/src/core.h"
#include "v7/src/exceptions.h"
#include "v7/src/ast.h"
#include "v7/src/primitive.h"
#include "v7/src/cyg_profile.h"

#define ACCEPT(t) (((v7)->cur_tok == (t)) ? next_tok((v7)), 1 : 0)

#define EXPECT(t)                            \
  do {                                       \
    if ((v7)->cur_tok != (t)) {              \
      CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR); \
    }                                        \
    next_tok(v7);                            \
  } while (0)

#define PARSE_WITH_OPT_ARG(tag, arg_tag, arg_parser, label) \
  do {                                                      \
    if (end_of_statement(v7) == V7_OK) {                    \
      add_node(v7, a, (tag));                               \
    } else {                                                \
      add_node(v7, a, (arg_tag));                           \
      arg_parser(label);                                    \
    }                                                       \
  } while (0)

#define N (CR_ARG_RET_PT()->arg)

/*
 * User functions
 * (as well as other in-function entry points)
 */
enum my_fid {
  fid_none = CR_FID__NONE,

  /* parse_script function */
  fid_parse_script = CR_FID__USER,
  fid_p_script_1,
  fid_p_script_2,
  fid_p_script_3,
  fid_p_script_4,

  /* parse_use_strict function */
  fid_parse_use_strict,

  /* parse_body function */
  fid_parse_body,
  fid_p_body_1,
  fid_p_body_2,

  /* parse_statement function */
  fid_parse_statement,
  fid_p_stat_1,
  fid_p_stat_2,
  fid_p_stat_3,
  fid_p_stat_4,
  fid_p_stat_5,
  fid_p_stat_6,
  fid_p_stat_7,
  fid_p_stat_8,
  fid_p_stat_9,
  fid_p_stat_10,
  fid_p_stat_11,
  fid_p_stat_12,
  fid_p_stat_13,
  fid_p_stat_14,

  /* parse_expression function */
  fid_parse_expression,
  fid_p_expr_1,

  /* parse_assign function */
  fid_parse_assign,
  fid_p_assign_1,

  /* parse_binary function */
  fid_parse_binary,
  fid_p_binary_1,
  fid_p_binary_2,
  fid_p_binary_3,
  fid_p_binary_4,
  fid_p_binary_5,
  fid_p_binary_6,

  /* parse_prefix function */
  fid_parse_prefix,
  fid_p_prefix_1,

  /* parse_postfix function */
  fid_parse_postfix,
  fid_p_postfix_1,

  /* parse_callexpr function */
  fid_parse_callexpr,
  fid_p_callexpr_1,
  fid_p_callexpr_2,
  fid_p_callexpr_3,

  /* parse_newexpr function */
  fid_parse_newexpr,
  fid_p_newexpr_1,
  fid_p_newexpr_2,
  fid_p_newexpr_3,
  fid_p_newexpr_4,

  /* parse_terminal function */
  fid_parse_terminal,
  fid_p_terminal_1,
  fid_p_terminal_2,
  fid_p_terminal_3,
  fid_p_terminal_4,

  /* parse_block function */
  fid_parse_block,
  fid_p_block_1,

  /* parse_if function */
  fid_parse_if,
  fid_p_if_1,
  fid_p_if_2,
  fid_p_if_3,

  /* parse_while function */
  fid_parse_while,
  fid_p_while_1,
  fid_p_while_2,

  /* parse_ident function */
  fid_parse_ident,

  /* parse_ident_allow_reserved_words function */
  fid_parse_ident_allow_reserved_words,
  fid_p_ident_arw_1,

  /* parse_funcdecl function */
  fid_parse_funcdecl,
  fid_p_funcdecl_1,
  fid_p_funcdecl_2,
  fid_p_funcdecl_3,
  fid_p_funcdecl_4,
  fid_p_funcdecl_5,
  fid_p_funcdecl_6,
  fid_p_funcdecl_7,
  fid_p_funcdecl_8,
  fid_p_funcdecl_9,

  /* parse_arglist function */
  fid_parse_arglist,
  fid_p_arglist_1,

  /* parse_member function */
  fid_parse_member,
  fid_p_member_1,

  /* parse_memberexpr function */
  fid_parse_memberexpr,
  fid_p_memberexpr_1,
  fid_p_memberexpr_2,

  /* parse_var function */
  fid_parse_var,
  fid_p_var_1,

  /* parse_prop function */
  fid_parse_prop,
#ifdef V7_ENABLE_JS_GETTERS
  fid_p_prop_1_getter,
#endif
  fid_p_prop_2,
#ifdef V7_ENABLE_JS_SETTERS
  fid_p_prop_3_setter,
#endif
  fid_p_prop_4,

  /* parse_dowhile function */
  fid_parse_dowhile,
  fid_p_dowhile_1,
  fid_p_dowhile_2,

  /* parse_for function */
  fid_parse_for,
  fid_p_for_1,
  fid_p_for_2,
  fid_p_for_3,
  fid_p_for_4,
  fid_p_for_5,
  fid_p_for_6,

  /* parse_try function */
  fid_parse_try,
  fid_p_try_1,
  fid_p_try_2,
  fid_p_try_3,
  fid_p_try_4,

  /* parse_switch function */
  fid_parse_switch,
  fid_p_switch_1,
  fid_p_switch_2,
  fid_p_switch_3,
  fid_p_switch_4,

  /* parse_with function */
  fid_parse_with,
  fid_p_with_1,
  fid_p_with_2,

  MY_FID_CNT
};

/*
 * User exception IDs. The first one should have value `CR_EXC_ID__USER`
 */
enum parser_exc_id {
  PARSER_EXC_ID__NONE = CR_EXC_ID__NONE,
  PARSER_EXC_ID__SYNTAX_ERROR = CR_EXC_ID__USER,
};

/* structures with locals and args {{{ */

/* parse_script {{{ */

/* parse_script's arguments */
#if 0
typedef struct fid_parse_script_arg {
} fid_parse_script_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_script_arg_t;
#endif

/* parse_script's data on stack */
typedef struct fid_parse_script_locals {
#if 0
  struct fid_parse_script_arg arg;
#endif

  ast_off_t start;
  ast_off_t outer_last_var_node;
  int saved_in_strict;
} fid_parse_script_locals_t;

/* }}} */

/* parse_use_strict {{{ */
/* parse_use_strict's arguments */
#if 0
typedef struct fid_parse_use_strict_arg {
} fid_parse_use_strict_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_use_strict_arg_t;
#endif

/* parse_use_strict's data on stack */
#if 0
typedef struct fid_parse_use_strict_locals {
  struct fid_parse_use_strict_arg arg;
} fid_parse_use_strict_locals_t;
#else
typedef cr_zero_size_type_t fid_parse_use_strict_locals_t;
#endif

#define CALL_PARSE_USE_STRICT(_label)      \
  do {                                     \
    CR_CALL(fid_parse_use_strict, _label); \
  } while (0)

/* }}} */

/* parse_body {{{ */
/* parse_body's arguments */
typedef struct fid_parse_body_arg { enum v7_tok end; } fid_parse_body_arg_t;

/* parse_body's data on stack */
typedef struct fid_parse_body_locals {
  struct fid_parse_body_arg arg;

  ast_off_t start;
} fid_parse_body_locals_t;

#define CALL_PARSE_BODY(_end, _label) \
  do {                                \
    N.fid_parse_body.end = (_end);    \
    CR_CALL(fid_parse_body, _label);  \
  } while (0)
/* }}} */

/* parse_statement {{{ */
/* parse_statement's arguments */
#if 0
typedef struct fid_parse_statement_arg {
} fid_parse_statement_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_statement_arg_t;
#endif

/* parse_statement's data on stack */
#if 0
typedef struct fid_parse_statement_locals {
  struct fid_parse_statement_arg arg;
} fid_parse_statement_locals_t;
#else
typedef cr_zero_size_type_t fid_parse_statement_locals_t;
#endif

#define CALL_PARSE_STATEMENT(_label)      \
  do {                                    \
    CR_CALL(fid_parse_statement, _label); \
  } while (0)
/* }}} */

/* parse_expression {{{ */
/* parse_expression's arguments */
#if 0
typedef struct fid_parse_expression_arg {
} fid_parse_expression_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_expression_arg_t;
#endif

/* parse_expression's data on stack */
typedef struct fid_parse_expression_locals {
#if 0
  struct fid_parse_expression_arg arg;
#endif

  ast_off_t pos;
  int group;
} fid_parse_expression_locals_t;

#define CALL_PARSE_EXPRESSION(_label)      \
  do {                                     \
    CR_CALL(fid_parse_expression, _label); \
  } while (0)
/* }}} */

/* parse_assign {{{ */
/* parse_assign's arguments */
#if 0
typedef struct fid_parse_assign_arg {
} fid_parse_assign_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_assign_arg_t;
#endif

/* parse_assign's data on stack */
#if 0
typedef struct fid_parse_assign_locals {
  struct fid_parse_assign_arg arg;
} fid_parse_assign_locals_t;
#else
typedef cr_zero_size_type_t fid_parse_assign_locals_t;
#endif

#define CALL_PARSE_ASSIGN(_label)      \
  do {                                 \
    CR_CALL(fid_parse_assign, _label); \
  } while (0)
/* }}} */

/* parse_binary {{{ */
/* parse_binary's arguments */
typedef struct fid_parse_binary_arg {
  ast_off_t pos;
  uint8_t min_level;
} fid_parse_binary_arg_t;

/* parse_binary's data on stack */
typedef struct fid_parse_binary_locals {
  struct fid_parse_binary_arg arg;

  uint8_t i;
  /* during iteration, it becomes negative, so should be signed */
  int8_t level;
  uint8_t /*enum v7_tok*/ tok;
  uint8_t /*enum ast_tag*/ ast;
  ast_off_t saved_mbuf_len;
} fid_parse_binary_locals_t;

#define CALL_PARSE_BINARY(_level, _pos, _label) \
  do {                                          \
    N.fid_parse_binary.min_level = (_level);    \
    N.fid_parse_binary.pos = (_pos);            \
    CR_CALL(fid_parse_binary, _label);          \
  } while (0)
/* }}} */

/* parse_prefix {{{ */
/* parse_prefix's arguments */
#if 0
typedef struct fid_parse_prefix_arg {
} fid_parse_prefix_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_prefix_arg_t;
#endif

/* parse_prefix's data on stack */
#if 0
typedef struct fid_parse_prefix_locals {
  struct fid_parse_prefix_arg arg;
} fid_parse_prefix_locals_t;
#else
typedef cr_zero_size_type_t fid_parse_prefix_locals_t;
#endif

#define CALL_PARSE_PREFIX(_label)      \
  do {                                 \
    CR_CALL(fid_parse_prefix, _label); \
  } while (0)
/* }}} */

/* parse_postfix {{{ */
/* parse_postfix's arguments */
#if 0
typedef struct fid_parse_postfix_arg {
} fid_parse_postfix_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_postfix_arg_t;
#endif

/* parse_postfix's data on stack */
typedef struct fid_parse_postfix_locals {
#if 0
  struct fid_parse_postfix_arg arg;
#endif

  ast_off_t pos;
} fid_parse_postfix_locals_t;

#define CALL_PARSE_POSTFIX(_label)      \
  do {                                  \
    CR_CALL(fid_parse_postfix, _label); \
  } while (0)
/* }}} */

/* parse_callexpr {{{ */
/* parse_callexpr's arguments */
#if 0
typedef struct fid_parse_callexpr_arg {
} fid_parse_callexpr_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_callexpr_arg_t;
#endif

/* parse_callexpr's data on stack */
typedef struct fid_parse_callexpr_locals {
#if 0
  struct fid_parse_callexpr_arg arg;
#endif

  ast_off_t pos;
} fid_parse_callexpr_locals_t;

#define CALL_PARSE_CALLEXPR(_label)      \
  do {                                   \
    CR_CALL(fid_parse_callexpr, _label); \
  } while (0)
/* }}} */

/* parse_newexpr {{{ */
/* parse_newexpr's arguments */
#if 0
typedef struct fid_parse_newexpr_arg {
} fid_parse_newexpr_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_newexpr_arg_t;
#endif

/* parse_newexpr's data on stack */
typedef struct fid_parse_newexpr_locals {
#if 0
  struct fid_parse_newexpr_arg arg;
#endif

  ast_off_t start;
} fid_parse_newexpr_locals_t;

#define CALL_PARSE_NEWEXPR(_label)      \
  do {                                  \
    CR_CALL(fid_parse_newexpr, _label); \
  } while (0)
/* }}} */

/* parse_terminal {{{ */
/* parse_terminal's arguments */
#if 0
typedef struct fid_parse_terminal_arg {
} fid_parse_terminal_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_terminal_arg_t;
#endif

/* parse_terminal's data on stack */
typedef struct fid_parse_terminal_locals {
#if 0
  struct fid_parse_terminal_arg arg;
#endif

  ast_off_t start;
} fid_parse_terminal_locals_t;

#define CALL_PARSE_TERMINAL(_label)      \
  do {                                   \
    CR_CALL(fid_parse_terminal, _label); \
  } while (0)
/* }}} */

/* parse_block {{{ */
/* parse_block's arguments */
#if 0
typedef struct fid_parse_block_arg {
} fid_parse_block_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_block_arg_t;
#endif

/* parse_block's data on stack */
#if 0
typedef struct fid_parse_block_locals {
  struct fid_parse_block_arg arg;
} fid_parse_block_locals_t;
#else
typedef cr_zero_size_type_t fid_parse_block_locals_t;
#endif

#define CALL_PARSE_BLOCK(_label)      \
  do {                                \
    CR_CALL(fid_parse_block, _label); \
  } while (0)
/* }}} */

/* parse_if {{{ */
/* parse_if's arguments */
#if 0
typedef struct fid_parse_if_arg {
} fid_parse_if_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_if_arg_t;
#endif

/* parse_if's data on stack */
typedef struct fid_parse_if_locals {
#if 0
  struct fid_parse_if_arg arg;
#endif

  ast_off_t start;
} fid_parse_if_locals_t;

#define CALL_PARSE_IF(_label)      \
  do {                             \
    CR_CALL(fid_parse_if, _label); \
  } while (0)
/* }}} */

/* parse_while {{{ */
/* parse_while's arguments */
#if 0
typedef struct fid_parse_while_arg {
} fid_parse_while_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_while_arg_t;
#endif

/* parse_while's data on stack */
typedef struct fid_parse_while_locals {
#if 0
  struct fid_parse_while_arg arg;
#endif

  ast_off_t start;
  uint8_t saved_in_loop;
} fid_parse_while_locals_t;

#define CALL_PARSE_WHILE(_label)      \
  do {                                \
    CR_CALL(fid_parse_while, _label); \
  } while (0)
/* }}} */

/* parse_ident {{{ */
/* parse_ident's arguments */
#if 0
typedef struct fid_parse_ident_arg {
} fid_parse_ident_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_ident_arg_t;
#endif

/* parse_ident's data on stack */
#if 0
typedef struct fid_parse_ident_locals {
  struct fid_parse_ident_arg arg;
} fid_parse_ident_locals_t;
#else
typedef cr_zero_size_type_t fid_parse_ident_locals_t;
#endif

#define CALL_PARSE_IDENT(_label)      \
  do {                                \
    CR_CALL(fid_parse_ident, _label); \
  } while (0)
/* }}} */

/* parse_ident_allow_reserved_words {{{ */
/* parse_ident_allow_reserved_words's arguments */
#if 0
typedef struct fid_parse_ident_allow_reserved_words_arg {
} fid_parse_ident_allow_reserved_words_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_ident_allow_reserved_words_arg_t;
#endif

/* parse_ident_allow_reserved_words's data on stack */
#if 0
typedef struct fid_parse_ident_allow_reserved_words_locals {
  struct fid_parse_ident_allow_reserved_words_arg arg;
} fid_parse_ident_allow_reserved_words_locals_t;
#else
typedef cr_zero_size_type_t fid_parse_ident_allow_reserved_words_locals_t;
#endif

#define CALL_PARSE_IDENT_ALLOW_RESERVED_WORDS(_label)      \
  do {                                                     \
    CR_CALL(fid_parse_ident_allow_reserved_words, _label); \
  } while (0)
/* }}} */

/* parse_funcdecl {{{ */
/* parse_funcdecl's arguments */
typedef struct fid_parse_funcdecl_arg {
  uint8_t require_named;
  uint8_t reserved_name;
} fid_parse_funcdecl_arg_t;

/* parse_funcdecl's data on stack */
typedef struct fid_parse_funcdecl_locals {
  struct fid_parse_funcdecl_arg arg;

  ast_off_t start;
  ast_off_t outer_last_var_node;
  uint8_t saved_in_function;
  uint8_t saved_in_strict;
} fid_parse_funcdecl_locals_t;

#define CALL_PARSE_FUNCDECL(_require_named, _reserved_name, _label) \
  do {                                                              \
    N.fid_parse_funcdecl.require_named = (_require_named);          \
    N.fid_parse_funcdecl.reserved_name = (_reserved_name);          \
    CR_CALL(fid_parse_funcdecl, _label);                            \
  } while (0)
/* }}} */

/* parse_arglist {{{ */
/* parse_arglist's arguments */
#if 0
typedef struct fid_parse_arglist_arg {
} fid_parse_arglist_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_arglist_arg_t;
#endif

/* parse_arglist's data on stack */
#if 0
typedef struct fid_parse_arglist_locals {
  struct fid_parse_arglist_arg arg;
} fid_parse_arglist_locals_t;
#else
typedef cr_zero_size_type_t fid_parse_arglist_locals_t;
#endif

#define CALL_PARSE_ARGLIST(_label)      \
  do {                                  \
    CR_CALL(fid_parse_arglist, _label); \
  } while (0)
/* }}} */

/* parse_member {{{ */
/* parse_member's arguments */
typedef struct fid_parse_member_arg { ast_off_t pos; } fid_parse_member_arg_t;

/* parse_member's data on stack */
typedef struct fid_parse_member_locals {
  struct fid_parse_member_arg arg;
} fid_parse_member_locals_t;

#define CALL_PARSE_MEMBER(_pos, _label) \
  do {                                  \
    N.fid_parse_member.pos = (_pos);    \
    CR_CALL(fid_parse_member, _label);  \
  } while (0)
/* }}} */

/* parse_memberexpr {{{ */
/* parse_memberexpr's arguments */
#if 0
typedef struct fid_parse_memberexpr_arg {
} fid_parse_memberexpr_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_memberexpr_arg_t;
#endif

/* parse_memberexpr's data on stack */
typedef struct fid_parse_memberexpr_locals {
#if 0
  struct fid_parse_memberexpr_arg arg;
#endif

  ast_off_t pos;
} fid_parse_memberexpr_locals_t;

#define CALL_PARSE_MEMBEREXPR(_label)      \
  do {                                     \
    CR_CALL(fid_parse_memberexpr, _label); \
  } while (0)
/* }}} */

/* parse_var {{{ */
/* parse_var's arguments */
#if 0
typedef struct fid_parse_var_arg {
} fid_parse_var_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_var_arg_t;
#endif

/* parse_var's data on stack */
typedef struct fid_parse_var_locals {
#if 0
  struct fid_parse_var_arg arg;
#endif

  ast_off_t start;
} fid_parse_var_locals_t;

#define CALL_PARSE_VAR(_label)      \
  do {                              \
    CR_CALL(fid_parse_var, _label); \
  } while (0)
/* }}} */

/* parse_prop {{{ */
/* parse_prop's arguments */
#if 0
typedef struct fid_parse_prop_arg {
} fid_parse_prop_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_prop_arg_t;
#endif

/* parse_prop's data on stack */
#if 0
typedef struct fid_parse_prop_locals {
  struct fid_parse_prop_arg arg;
} fid_parse_prop_locals_t;
#else
typedef cr_zero_size_type_t fid_parse_prop_locals_t;
#endif

#define CALL_PARSE_PROP(_label)      \
  do {                               \
    CR_CALL(fid_parse_prop, _label); \
  } while (0)
/* }}} */

/* parse_dowhile {{{ */
/* parse_dowhile's arguments */
#if 0
typedef struct fid_parse_dowhile_arg {
} fid_parse_dowhile_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_dowhile_arg_t;
#endif

/* parse_dowhile's data on stack */
typedef struct fid_parse_dowhile_locals {
#if 0
  struct fid_parse_dowhile_arg arg;
#endif

  ast_off_t start;
  uint8_t saved_in_loop;
} fid_parse_dowhile_locals_t;

#define CALL_PARSE_DOWHILE(_label)      \
  do {                                  \
    CR_CALL(fid_parse_dowhile, _label); \
  } while (0)
/* }}} */

/* parse_for {{{ */
/* parse_for's arguments */
#if 0
typedef struct fid_parse_for_arg {
} fid_parse_for_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_for_arg_t;
#endif

/* parse_for's data on stack */
typedef struct fid_parse_for_locals {
#if 0
  struct fid_parse_for_arg arg;
#endif

  ast_off_t start;
  uint8_t saved_in_loop;
} fid_parse_for_locals_t;

#define CALL_PARSE_FOR(_label)      \
  do {                              \
    CR_CALL(fid_parse_for, _label); \
  } while (0)
/* }}} */

/* parse_try {{{ */
/* parse_try's arguments */
#if 0
typedef struct fid_parse_try_arg {
} fid_parse_try_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_try_arg_t;
#endif

/* parse_try's data on stack */
typedef struct fid_parse_try_locals {
#if 0
  struct fid_parse_try_arg arg;
#endif

  ast_off_t start;
  uint8_t catch_or_finally;
} fid_parse_try_locals_t;

#define CALL_PARSE_TRY(_label)      \
  do {                              \
    CR_CALL(fid_parse_try, _label); \
  } while (0)
/* }}} */

/* parse_switch {{{ */
/* parse_switch's arguments */
#if 0
typedef struct fid_parse_switch_arg {
} fid_parse_switch_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_switch_arg_t;
#endif

/* parse_switch's data on stack */
typedef struct fid_parse_switch_locals {
#if 0
  struct fid_parse_switch_arg arg;
#endif

  ast_off_t start;
  int saved_in_switch;
  ast_off_t case_start;
} fid_parse_switch_locals_t;

#define CALL_PARSE_SWITCH(_label)      \
  do {                                 \
    CR_CALL(fid_parse_switch, _label); \
  } while (0)
/* }}} */

/* parse_with {{{ */
/* parse_with's arguments */
#if 0
typedef struct fid_parse_with_arg {
} fid_parse_with_arg_t;
#else
typedef cr_zero_size_type_t fid_parse_with_arg_t;
#endif

/* parse_with's data on stack */
typedef struct fid_parse_with_locals {
#if 0
  struct fid_parse_with_arg arg;
#endif

  ast_off_t start;
} fid_parse_with_locals_t;

#define CALL_PARSE_WITH(_label)      \
  do {                               \
    CR_CALL(fid_parse_with, _label); \
  } while (0)
/* }}} */

/* }}} */

/*
 * Array of "function" descriptors. Each descriptor contains just a size
 * of "function"'s locals.
 */
static const struct cr_func_desc _fid_descrs[MY_FID_CNT] = {

    /* fid_none */
    {0},

    /* fid_parse_script ----------------------------------------- */
    /* fid_parse_script */
    {CR_LOCALS_SIZEOF(fid_parse_script_locals_t)},
    /* fid_p_script_1 */
    {CR_LOCALS_SIZEOF(fid_parse_script_locals_t)},
    /* fid_p_script_2 */
    {CR_LOCALS_SIZEOF(fid_parse_script_locals_t)},
    /* fid_p_script_3 */
    {CR_LOCALS_SIZEOF(fid_parse_script_locals_t)},
    /* fid_p_script_4 */
    {CR_LOCALS_SIZEOF(fid_parse_script_locals_t)},

    /* fid_parse_use_strict ----------------------------------------- */
    /* fid_parse_use_strict */
    {CR_LOCALS_SIZEOF(fid_parse_use_strict_locals_t)},

    /* fid_parse_body ----------------------------------------- */
    /* fid_parse_body */
    {CR_LOCALS_SIZEOF(fid_parse_body_locals_t)},
    /* fid_p_body_1 */
    {CR_LOCALS_SIZEOF(fid_parse_body_locals_t)},
    /* fid_p_body_2 */
    {CR_LOCALS_SIZEOF(fid_parse_body_locals_t)},

    /* fid_parse_statement ----------------------------------------- */
    /* fid_parse_statement */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_1 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_2 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_3 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_4 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_5 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_6 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_7 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_8 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_9 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_10 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_11 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_12 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_13 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},
    /* fid_p_stat_14 */
    {CR_LOCALS_SIZEOF(fid_parse_statement_locals_t)},

    /* fid_parse_expression ----------------------------------------- */
    /* fid_parse_expression */
    {CR_LOCALS_SIZEOF(fid_parse_expression_locals_t)},
    /* fid_p_expr_1 */
    {CR_LOCALS_SIZEOF(fid_parse_expression_locals_t)},

    /* fid_parse_assign ----------------------------------------- */
    /* fid_parse_assign */
    {CR_LOCALS_SIZEOF(fid_parse_assign_locals_t)},
    /* fid_p_assign_1 */
    {CR_LOCALS_SIZEOF(fid_parse_assign_locals_t)},

    /* fid_parse_binary ----------------------------------------- */
    /* fid_parse_binary */
    {CR_LOCALS_SIZEOF(fid_parse_binary_locals_t)},
    /* fid_p_binary_1 */
    {CR_LOCALS_SIZEOF(fid_parse_binary_locals_t)},
    /* fid_p_binary_2 */
    {CR_LOCALS_SIZEOF(fid_parse_binary_locals_t)},
    /* fid_p_binary_3 */
    {CR_LOCALS_SIZEOF(fid_parse_binary_locals_t)},
    /* fid_p_binary_4 */
    {CR_LOCALS_SIZEOF(fid_parse_binary_locals_t)},
    /* fid_p_binary_5 */
    {CR_LOCALS_SIZEOF(fid_parse_binary_locals_t)},
    /* fid_p_binary_6 */
    {CR_LOCALS_SIZEOF(fid_parse_binary_locals_t)},

    /* fid_parse_prefix ----------------------------------------- */
    /* fid_parse_prefix */
    {CR_LOCALS_SIZEOF(fid_parse_prefix_locals_t)},
    /* fid_p_prefix_1 */
    {CR_LOCALS_SIZEOF(fid_parse_prefix_locals_t)},

    /* fid_parse_postfix ----------------------------------------- */
    /* fid_parse_postfix */
    {CR_LOCALS_SIZEOF(fid_parse_postfix_locals_t)},
    /* fid_p_postfix_1 */
    {CR_LOCALS_SIZEOF(fid_parse_postfix_locals_t)},

    /* fid_parse_callexpr ----------------------------------------- */
    /* fid_parse_callexpr */
    {CR_LOCALS_SIZEOF(fid_parse_callexpr_locals_t)},
    /* fid_p_callexpr_1 */
    {CR_LOCALS_SIZEOF(fid_parse_callexpr_locals_t)},
    /* fid_p_callexpr_2 */
    {CR_LOCALS_SIZEOF(fid_parse_callexpr_locals_t)},
    /* fid_p_callexpr_3 */
    {CR_LOCALS_SIZEOF(fid_parse_callexpr_locals_t)},

    /* fid_parse_newexpr ----------------------------------------- */
    /* fid_parse_newexpr */
    {CR_LOCALS_SIZEOF(fid_parse_newexpr_locals_t)},
    /* fid_p_newexpr_1 */
    {CR_LOCALS_SIZEOF(fid_parse_newexpr_locals_t)},
    /* fid_p_newexpr_2 */
    {CR_LOCALS_SIZEOF(fid_parse_newexpr_locals_t)},
    /* fid_p_newexpr_3 */
    {CR_LOCALS_SIZEOF(fid_parse_newexpr_locals_t)},
    /* fid_p_newexpr_4 */
    {CR_LOCALS_SIZEOF(fid_parse_newexpr_locals_t)},

    /* fid_parse_terminal ----------------------------------------- */
    /* fid_parse_terminal */
    {CR_LOCALS_SIZEOF(fid_parse_terminal_locals_t)},
    /* fid_p_terminal_1 */
    {CR_LOCALS_SIZEOF(fid_parse_terminal_locals_t)},
    /* fid_p_terminal_2 */
    {CR_LOCALS_SIZEOF(fid_parse_terminal_locals_t)},
    /* fid_p_terminal_3 */
    {CR_LOCALS_SIZEOF(fid_parse_terminal_locals_t)},
    /* fid_p_terminal_4 */
    {CR_LOCALS_SIZEOF(fid_parse_terminal_locals_t)},

    /* fid_parse_block ----------------------------------------- */
    /* fid_parse_block */
    {CR_LOCALS_SIZEOF(fid_parse_block_locals_t)},
    /* fid_p_block_1 */
    {CR_LOCALS_SIZEOF(fid_parse_block_locals_t)},

    /* fid_parse_if ----------------------------------------- */
    /* fid_parse_if */
    {CR_LOCALS_SIZEOF(fid_parse_if_locals_t)},
    /* fid_p_if_1 */
    {CR_LOCALS_SIZEOF(fid_parse_if_locals_t)},
    /* fid_p_if_2 */
    {CR_LOCALS_SIZEOF(fid_parse_if_locals_t)},
    /* fid_p_if_3 */
    {CR_LOCALS_SIZEOF(fid_parse_if_locals_t)},

    /* fid_parse_while ----------------------------------------- */
    /* fid_parse_while */
    {CR_LOCALS_SIZEOF(fid_parse_while_locals_t)},
    /* fid_p_while_1 */
    {CR_LOCALS_SIZEOF(fid_parse_while_locals_t)},
    /* fid_p_while_2 */
    {CR_LOCALS_SIZEOF(fid_parse_while_locals_t)},

    /* fid_parse_ident ----------------------------------------- */
    /* fid_parse_ident */
    {CR_LOCALS_SIZEOF(fid_parse_ident_locals_t)},

    /* fid_parse_ident_allow_reserved_words -------------------- */
    /* fid_parse_ident_allow_reserved_words */
    {CR_LOCALS_SIZEOF(fid_parse_ident_allow_reserved_words_locals_t)},
    /* fid_p_ident_allow_reserved_words_1 */
    {CR_LOCALS_SIZEOF(fid_parse_ident_allow_reserved_words_locals_t)},

    /* fid_parse_funcdecl ----------------------------------------- */
    /* fid_parse_funcdecl */
    {CR_LOCALS_SIZEOF(fid_parse_funcdecl_locals_t)},
    /* fid_p_funcdecl_1 */
    {CR_LOCALS_SIZEOF(fid_parse_funcdecl_locals_t)},
    /* fid_p_funcdecl_2 */
    {CR_LOCALS_SIZEOF(fid_parse_funcdecl_locals_t)},
    /* fid_p_funcdecl_3 */
    {CR_LOCALS_SIZEOF(fid_parse_funcdecl_locals_t)},
    /* fid_p_funcdecl_4 */
    {CR_LOCALS_SIZEOF(fid_parse_funcdecl_locals_t)},
    /* fid_p_funcdecl_5 */
    {CR_LOCALS_SIZEOF(fid_parse_funcdecl_locals_t)},
    /* fid_p_funcdecl_6 */
    {CR_LOCALS_SIZEOF(fid_parse_funcdecl_locals_t)},
    /* fid_p_funcdecl_7 */
    {CR_LOCALS_SIZEOF(fid_parse_funcdecl_locals_t)},
    /* fid_p_funcdecl_8 */
    {CR_LOCALS_SIZEOF(fid_parse_funcdecl_locals_t)},
    /* fid_p_funcdecl_9 */
    {CR_LOCALS_SIZEOF(fid_parse_funcdecl_locals_t)},

    /* fid_parse_arglist ----------------------------------------- */
    /* fid_parse_arglist */
    {CR_LOCALS_SIZEOF(fid_parse_arglist_locals_t)},
    /* fid_p_arglist_1 */
    {CR_LOCALS_SIZEOF(fid_parse_arglist_locals_t)},

    /* fid_parse_member ----------------------------------------- */
    /* fid_parse_member */
    {CR_LOCALS_SIZEOF(fid_parse_member_locals_t)},
    /* fid_p_member_1 */
    {CR_LOCALS_SIZEOF(fid_parse_member_locals_t)},

    /* fid_parse_memberexpr ----------------------------------------- */
    /* fid_parse_memberexpr */
    {CR_LOCALS_SIZEOF(fid_parse_memberexpr_locals_t)},
    /* fid_p_memberexpr_1 */
    {CR_LOCALS_SIZEOF(fid_parse_memberexpr_locals_t)},
    /* fid_p_memberexpr_2 */
    {CR_LOCALS_SIZEOF(fid_parse_memberexpr_locals_t)},

    /* fid_parse_var ----------------------------------------- */
    /* fid_parse_var */
    {CR_LOCALS_SIZEOF(fid_parse_var_locals_t)},
    /* fid_p_var_1 */
    {CR_LOCALS_SIZEOF(fid_parse_var_locals_t)},

    /* fid_parse_prop ----------------------------------------- */
    /* fid_parse_prop */
    {CR_LOCALS_SIZEOF(fid_parse_prop_locals_t)},
#ifdef V7_ENABLE_JS_GETTERS
    /* fid_p_prop_1_getter */
    {CR_LOCALS_SIZEOF(fid_parse_prop_locals_t)},
#endif
    /* fid_p_prop_2 */
    {CR_LOCALS_SIZEOF(fid_parse_prop_locals_t)},
#ifdef V7_ENABLE_JS_SETTERS
    /* fid_p_prop_3_setter */
    {CR_LOCALS_SIZEOF(fid_parse_prop_locals_t)},
#endif
    /* fid_p_prop_4 */
    {CR_LOCALS_SIZEOF(fid_parse_prop_locals_t)},

    /* fid_parse_dowhile ----------------------------------------- */
    /* fid_parse_dowhile */
    {CR_LOCALS_SIZEOF(fid_parse_dowhile_locals_t)},
    /* fid_p_dowhile_1 */
    {CR_LOCALS_SIZEOF(fid_parse_dowhile_locals_t)},
    /* fid_p_dowhile_2 */
    {CR_LOCALS_SIZEOF(fid_parse_dowhile_locals_t)},

    /* fid_parse_for ----------------------------------------- */
    /* fid_parse_for */
    {CR_LOCALS_SIZEOF(fid_parse_for_locals_t)},
    /* fid_p_for_1 */
    {CR_LOCALS_SIZEOF(fid_parse_for_locals_t)},
    /* fid_p_for_2 */
    {CR_LOCALS_SIZEOF(fid_parse_for_locals_t)},
    /* fid_p_for_3 */
    {CR_LOCALS_SIZEOF(fid_parse_for_locals_t)},
    /* fid_p_for_4 */
    {CR_LOCALS_SIZEOF(fid_parse_for_locals_t)},
    /* fid_p_for_5 */
    {CR_LOCALS_SIZEOF(fid_parse_for_locals_t)},
    /* fid_p_for_6 */
    {CR_LOCALS_SIZEOF(fid_parse_for_locals_t)},

    /* fid_parse_try ----------------------------------------- */
    /* fid_parse_try */
    {CR_LOCALS_SIZEOF(fid_parse_try_locals_t)},
    /* fid_p_try_1 */
    {CR_LOCALS_SIZEOF(fid_parse_try_locals_t)},
    /* fid_p_try_2 */
    {CR_LOCALS_SIZEOF(fid_parse_try_locals_t)},
    /* fid_p_try_3 */
    {CR_LOCALS_SIZEOF(fid_parse_try_locals_t)},
    /* fid_p_try_4 */
    {CR_LOCALS_SIZEOF(fid_parse_try_locals_t)},

    /* fid_parse_switch ----------------------------------------- */
    /* fid_parse_switch */
    {CR_LOCALS_SIZEOF(fid_parse_switch_locals_t)},
    /* fid_p_switch_1 */
    {CR_LOCALS_SIZEOF(fid_parse_switch_locals_t)},
    /* fid_p_switch_2 */
    {CR_LOCALS_SIZEOF(fid_parse_switch_locals_t)},
    /* fid_p_switch_3 */
    {CR_LOCALS_SIZEOF(fid_parse_switch_locals_t)},
    /* fid_p_switch_4 */
    {CR_LOCALS_SIZEOF(fid_parse_switch_locals_t)},

    /* fid_parse_with ----------------------------------------- */
    /* fid_parse_with */
    {CR_LOCALS_SIZEOF(fid_parse_with_locals_t)},
    /* fid_p_with_1 */
    {CR_LOCALS_SIZEOF(fid_parse_with_locals_t)},
    /* fid_p_with_2 */
    {CR_LOCALS_SIZEOF(fid_parse_with_locals_t)},

};

/*
 * Union of arguments and return values for all existing "functions".
 *
 * Used as an accumulator when we call function, return from function,
 * yield, or resume.
 */
union user_arg_ret {
  /* arguments to the next function */
  union {
#if 0
    fid_parse_script_arg_t fid_parse_script;
    fid_parse_use_strict_arg_t fid_parse_use_strict;
    fid_parse_statement_arg_t fid_parse_statement;
    fid_parse_expression_arg_t fid_parse_expression;
    fid_parse_assign_arg_t fid_parse_assign;
    fid_parse_prefix_arg_t fid_parse_prefix;
    fid_parse_postfix_arg_t fid_parse_postfix;
    fid_parse_callexpr_arg_t fid_parse_callexpr;
    fid_parse_newexpr_arg_t fid_parse_newexpr;
    fid_parse_terminal_arg_t fid_parse_terminal;
    fid_parse_block_arg_t fid_parse_block;
    fid_parse_if_arg_t fid_parse_if;
    fid_parse_while_arg_t fid_parse_while;
    fid_parse_ident_arg_t fid_parse_ident;
    fid_parse_ident_allow_reserved_words_arg_t
      fid_parse_ident_allow_reserved_words;
    fid_parse_arglist_arg_t fid_parse_arglist;
    fid_parse_memberexpr_arg_t fid_parse_memberexpr;
    fid_parse_var_arg_t fid_parse_var;
    fid_parse_prop_arg_t fid_parse_prop;
    fid_parse_dowhile_arg_t fid_parse_dowhile;
    fid_parse_for_arg_t fid_parse_for;
    fid_parse_try_arg_t fid_parse_try;
    fid_parse_switch_arg_t fid_parse_switch;
    fid_parse_with_arg_t fid_parse_with;
#endif
    fid_parse_body_arg_t fid_parse_body;
    fid_parse_binary_arg_t fid_parse_binary;
    fid_parse_funcdecl_arg_t fid_parse_funcdecl;
    fid_parse_member_arg_t fid_parse_member;
  } arg;

  /* value returned from function */
  /*
     union {
     } ret;
     */
};

static enum v7_tok next_tok(struct v7 *v7) {
  int prev_line_no = v7->pstate.prev_line_no;
  v7->pstate.prev_line_no = v7->pstate.line_no;
  v7->pstate.line_no += skip_to_next_tok(&v7->pstate.pc, v7->pstate.src_end);
  v7->after_newline = prev_line_no != v7->pstate.line_no;
  v7->tok = v7->pstate.pc;
  v7->cur_tok = get_tok(&v7->pstate.pc, v7->pstate.src_end, &v7->cur_tok_dbl,
                        v7->cur_tok);
  v7->tok_len = v7->pstate.pc - v7->tok;
  v7->pstate.line_no += skip_to_next_tok(&v7->pstate.pc, v7->pstate.src_end);
  return v7->cur_tok;
}

#ifndef V7_DISABLE_LINE_NUMBERS
/*
 * Assumes `offset` points to the byte right after a tag
 */
static void insert_line_no_if_changed(struct v7 *v7, struct ast *a,
                                      ast_off_t offset) {
  if (v7->pstate.prev_line_no != v7->line_no) {
    v7->line_no = v7->pstate.prev_line_no;
    ast_add_line_no(a, offset - 1, v7->line_no);
  } else {
#if V7_AST_FORCE_LINE_NUMBERS
    /*
     * This mode is needed for debug only: to make sure AST consumers correctly
     * consume all nodes with line numbers data encoded
     */
    ast_add_line_no(a, offset - 1, 0);
#endif
  }
}
#else
static void insert_line_no_if_changed(struct v7 *v7, struct ast *a,
                                      ast_off_t offset) {
  (void) v7;
  (void) a;
  (void) offset;
}
#endif

static ast_off_t insert_node(struct v7 *v7, struct ast *a, ast_off_t start,
                             enum ast_tag tag) {
  ast_off_t ret = ast_insert_node(a, start, tag);
  insert_line_no_if_changed(v7, a, ret);
  return ret;
}

static ast_off_t add_node(struct v7 *v7, struct ast *a, enum ast_tag tag) {
  return insert_node(v7, a, a->mbuf.len, tag);
}

static ast_off_t insert_inlined_node(struct v7 *v7, struct ast *a,
                                     ast_off_t start, enum ast_tag tag,
                                     const char *name, size_t len) {
  ast_off_t ret = ast_insert_inlined_node(a, start, tag, name, len);
  insert_line_no_if_changed(v7, a, ret);
  return ret;
}

static ast_off_t add_inlined_node(struct v7 *v7, struct ast *a,
                                  enum ast_tag tag, const char *name,
                                  size_t len) {
  return insert_inlined_node(v7, a, a->mbuf.len, tag, name, len);
}

static unsigned long get_column(const char *code, const char *pos) {
  const char *p = pos;
  while (p > code && *p != '\n') {
    p--;
  }
  return p == code ? pos - p : pos - (p + 1);
}

static enum v7_err end_of_statement(struct v7 *v7) {
  if (v7->cur_tok == TOK_SEMICOLON || v7->cur_tok == TOK_END_OF_INPUT ||
      v7->cur_tok == TOK_CLOSE_CURLY || v7->after_newline) {
    return V7_OK;
  }
  return V7_SYNTAX_ERROR;
}

static enum v7_tok lookahead(const struct v7 *v7) {
  const char *s = v7->pstate.pc;
  double d;
  return get_tok(&s, v7->pstate.src_end, &d, v7->cur_tok);
}

static int parse_optional(struct v7 *v7, struct ast *a,
                          enum v7_tok terminator) {
  if (v7->cur_tok != terminator) {
    return 1;
  }
  add_node(v7, a, AST_NOP);
  return 0;
}

/*
 * On ESP8266 'levels' declaration have to be outside of 'parse_binary'
 * in order to prevent reboot on return from this function
 * TODO(alashkin): understand why
 */
#define NONE \
  { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }

static const struct {
  int len, left_to_right;
  struct {
    enum v7_tok start_tok, end_tok;
    enum ast_tag start_ast;
  } parts[2];
} levels[] = {
    {1, 0, {{TOK_ASSIGN, TOK_URSHIFT_ASSIGN, AST_ASSIGN}, NONE}},
    {1, 0, {{TOK_QUESTION, TOK_QUESTION, AST_COND}, NONE}},
    {1, 1, {{TOK_LOGICAL_OR, TOK_LOGICAL_OR, AST_LOGICAL_OR}, NONE}},
    {1, 1, {{TOK_LOGICAL_AND, TOK_LOGICAL_AND, AST_LOGICAL_AND}, NONE}},
    {1, 1, {{TOK_OR, TOK_OR, AST_OR}, NONE}},
    {1, 1, {{TOK_XOR, TOK_XOR, AST_XOR}, NONE}},
    {1, 1, {{TOK_AND, TOK_AND, AST_AND}, NONE}},
    {1, 1, {{TOK_EQ, TOK_NE_NE, AST_EQ}, NONE}},
    {2, 1, {{TOK_LE, TOK_GT, AST_LE}, {TOK_IN, TOK_INSTANCEOF, AST_IN}}},
    {1, 1, {{TOK_LSHIFT, TOK_URSHIFT, AST_LSHIFT}, NONE}},
    {1, 1, {{TOK_PLUS, TOK_MINUS, AST_ADD}, NONE}},
    {1, 1, {{TOK_REM, TOK_DIV, AST_REM}, NONE}}};

enum cr_status parser_cr_exec(struct cr_ctx *p_ctx, struct v7 *v7,
                              struct ast *a) {
  enum cr_status rc = CR_RES__OK;

_cr_iter_begin:

  rc = cr_on_iter_begin(p_ctx);
  if (rc != CR_RES__OK) {
    return rc;
  }

  /*
   * dispatcher switch: depending on the fid, jump to the corresponding label
   */
  switch ((enum my_fid) CR_CURR_FUNC()) {
    CR_DEFINE_ENTRY_POINT(fid_none);

    CR_DEFINE_ENTRY_POINT(fid_parse_script);
    CR_DEFINE_ENTRY_POINT(fid_p_script_1);
    CR_DEFINE_ENTRY_POINT(fid_p_script_2);
    CR_DEFINE_ENTRY_POINT(fid_p_script_3);
    CR_DEFINE_ENTRY_POINT(fid_p_script_4);

    CR_DEFINE_ENTRY_POINT(fid_parse_use_strict);

    CR_DEFINE_ENTRY_POINT(fid_parse_body);
    CR_DEFINE_ENTRY_POINT(fid_p_body_1);
    CR_DEFINE_ENTRY_POINT(fid_p_body_2);

    CR_DEFINE_ENTRY_POINT(fid_parse_statement);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_1);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_2);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_3);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_4);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_5);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_6);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_7);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_8);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_9);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_10);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_11);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_12);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_13);
    CR_DEFINE_ENTRY_POINT(fid_p_stat_14);

    CR_DEFINE_ENTRY_POINT(fid_parse_expression);
    CR_DEFINE_ENTRY_POINT(fid_p_expr_1);

    CR_DEFINE_ENTRY_POINT(fid_parse_assign);
    CR_DEFINE_ENTRY_POINT(fid_p_assign_1);

    CR_DEFINE_ENTRY_POINT(fid_parse_binary);
    CR_DEFINE_ENTRY_POINT(fid_p_binary_2);
    CR_DEFINE_ENTRY_POINT(fid_p_binary_3);
    CR_DEFINE_ENTRY_POINT(fid_p_binary_4);
    CR_DEFINE_ENTRY_POINT(fid_p_binary_5);
    CR_DEFINE_ENTRY_POINT(fid_p_binary_6);

    CR_DEFINE_ENTRY_POINT(fid_parse_prefix);
    CR_DEFINE_ENTRY_POINT(fid_p_prefix_1);

    CR_DEFINE_ENTRY_POINT(fid_parse_postfix);
    CR_DEFINE_ENTRY_POINT(fid_p_postfix_1);

    CR_DEFINE_ENTRY_POINT(fid_parse_callexpr);
    CR_DEFINE_ENTRY_POINT(fid_p_callexpr_1);
    CR_DEFINE_ENTRY_POINT(fid_p_callexpr_2);
    CR_DEFINE_ENTRY_POINT(fid_p_callexpr_3);

    CR_DEFINE_ENTRY_POINT(fid_parse_newexpr);
    CR_DEFINE_ENTRY_POINT(fid_p_newexpr_1);
    CR_DEFINE_ENTRY_POINT(fid_p_newexpr_2);
    CR_DEFINE_ENTRY_POINT(fid_p_newexpr_3);
    CR_DEFINE_ENTRY_POINT(fid_p_newexpr_4);

    CR_DEFINE_ENTRY_POINT(fid_parse_terminal);
    CR_DEFINE_ENTRY_POINT(fid_p_terminal_1);
    CR_DEFINE_ENTRY_POINT(fid_p_terminal_2);
    CR_DEFINE_ENTRY_POINT(fid_p_terminal_3);
    CR_DEFINE_ENTRY_POINT(fid_p_terminal_4);

    CR_DEFINE_ENTRY_POINT(fid_parse_block);
    CR_DEFINE_ENTRY_POINT(fid_p_block_1);

    CR_DEFINE_ENTRY_POINT(fid_parse_if);
    CR_DEFINE_ENTRY_POINT(fid_p_if_1);
    CR_DEFINE_ENTRY_POINT(fid_p_if_2);
    CR_DEFINE_ENTRY_POINT(fid_p_if_3);

    CR_DEFINE_ENTRY_POINT(fid_parse_while);
    CR_DEFINE_ENTRY_POINT(fid_p_while_1);
    CR_DEFINE_ENTRY_POINT(fid_p_while_2);

    CR_DEFINE_ENTRY_POINT(fid_parse_ident);

    CR_DEFINE_ENTRY_POINT(fid_parse_ident_allow_reserved_words);
    CR_DEFINE_ENTRY_POINT(fid_p_ident_arw_1);

    CR_DEFINE_ENTRY_POINT(fid_parse_funcdecl);
    CR_DEFINE_ENTRY_POINT(fid_p_funcdecl_1);
    CR_DEFINE_ENTRY_POINT(fid_p_funcdecl_2);
    CR_DEFINE_ENTRY_POINT(fid_p_funcdecl_3);
    CR_DEFINE_ENTRY_POINT(fid_p_funcdecl_4);
    CR_DEFINE_ENTRY_POINT(fid_p_funcdecl_5);
    CR_DEFINE_ENTRY_POINT(fid_p_funcdecl_6);
    CR_DEFINE_ENTRY_POINT(fid_p_funcdecl_7);
    CR_DEFINE_ENTRY_POINT(fid_p_funcdecl_8);
    CR_DEFINE_ENTRY_POINT(fid_p_funcdecl_9);

    CR_DEFINE_ENTRY_POINT(fid_parse_arglist);
    CR_DEFINE_ENTRY_POINT(fid_p_arglist_1);

    CR_DEFINE_ENTRY_POINT(fid_parse_member);
    CR_DEFINE_ENTRY_POINT(fid_p_member_1);

    CR_DEFINE_ENTRY_POINT(fid_parse_memberexpr);
    CR_DEFINE_ENTRY_POINT(fid_p_memberexpr_1);
    CR_DEFINE_ENTRY_POINT(fid_p_memberexpr_2);

    CR_DEFINE_ENTRY_POINT(fid_parse_var);
    CR_DEFINE_ENTRY_POINT(fid_p_var_1);

    CR_DEFINE_ENTRY_POINT(fid_parse_prop);
#ifdef V7_ENABLE_JS_GETTERS
    CR_DEFINE_ENTRY_POINT(fid_p_prop_1_getter);
#endif
    CR_DEFINE_ENTRY_POINT(fid_p_prop_2);
#ifdef V7_ENABLE_JS_SETTERS
    CR_DEFINE_ENTRY_POINT(fid_p_prop_3_setter);
#endif
    CR_DEFINE_ENTRY_POINT(fid_p_prop_4);

    CR_DEFINE_ENTRY_POINT(fid_parse_dowhile);
    CR_DEFINE_ENTRY_POINT(fid_p_dowhile_1);
    CR_DEFINE_ENTRY_POINT(fid_p_dowhile_2);

    CR_DEFINE_ENTRY_POINT(fid_parse_for);
    CR_DEFINE_ENTRY_POINT(fid_p_for_1);
    CR_DEFINE_ENTRY_POINT(fid_p_for_2);
    CR_DEFINE_ENTRY_POINT(fid_p_for_3);
    CR_DEFINE_ENTRY_POINT(fid_p_for_4);
    CR_DEFINE_ENTRY_POINT(fid_p_for_5);
    CR_DEFINE_ENTRY_POINT(fid_p_for_6);

    CR_DEFINE_ENTRY_POINT(fid_parse_try);
    CR_DEFINE_ENTRY_POINT(fid_p_try_1);
    CR_DEFINE_ENTRY_POINT(fid_p_try_2);
    CR_DEFINE_ENTRY_POINT(fid_p_try_3);
    CR_DEFINE_ENTRY_POINT(fid_p_try_4);

    CR_DEFINE_ENTRY_POINT(fid_parse_switch);
    CR_DEFINE_ENTRY_POINT(fid_p_switch_1);
    CR_DEFINE_ENTRY_POINT(fid_p_switch_2);
    CR_DEFINE_ENTRY_POINT(fid_p_switch_3);
    CR_DEFINE_ENTRY_POINT(fid_p_switch_4);

    CR_DEFINE_ENTRY_POINT(fid_parse_with);
    CR_DEFINE_ENTRY_POINT(fid_p_with_1);
    CR_DEFINE_ENTRY_POINT(fid_p_with_2);

    default:
      /* should never be here */
      printf("fatal: wrong func id: %d", CR_CURR_FUNC());
      break;
  };

/* static enum v7_err parse_script(struct v7 *v7, struct ast *a) */
fid_parse_script :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_script_locals_t)
{
  L->start = add_node(v7, a, AST_SCRIPT);
  L->outer_last_var_node = v7->last_var_node;
  L->saved_in_strict = v7->pstate.in_strict;

  v7->last_var_node = L->start;
  ast_modify_skip(a, L->start, L->start, AST_FUNC_FIRST_VAR_SKIP);

  CR_TRY(fid_p_script_1);
  {
    CALL_PARSE_USE_STRICT(fid_p_script_3);
    v7->pstate.in_strict = 1;
  }
  CR_CATCH(PARSER_EXC_ID__SYNTAX_ERROR, fid_p_script_1, fid_p_script_2);
  CR_ENDCATCH(fid_p_script_2);

  CALL_PARSE_BODY(TOK_END_OF_INPUT, fid_p_script_4);
  ast_set_skip(a, L->start, AST_END_SKIP);
  v7->pstate.in_strict = L->saved_in_strict;
  v7->last_var_node = L->outer_last_var_node;

  CR_RETURN_VOID();
}

/* static enum v7_err parse_use_strict(struct v7 *v7, struct ast *a) */
fid_parse_use_strict :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_use_strict_locals_t)
{
  if (v7->cur_tok == TOK_STRING_LITERAL &&
      (strncmp(v7->tok, "\"use strict\"", v7->tok_len) == 0 ||
       strncmp(v7->tok, "'use strict'", v7->tok_len) == 0)) {
    next_tok(v7);
    add_node(v7, a, AST_USE_STRICT);
    CR_RETURN_VOID();
  } else {
    CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
  }
}

/*
 * static enum v7_err parse_body(struct v7 *v7, struct ast *a,
 *                               enum v7_tok end)
 */
fid_parse_body :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_body_locals_t)
{
  while (v7->cur_tok != L->arg.end) {
    if (ACCEPT(TOK_FUNCTION)) {
      if (v7->cur_tok != TOK_IDENTIFIER) {
        CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
      }
      L->start = add_node(v7, a, AST_VAR);
      ast_modify_skip(a, v7->last_var_node, L->start, AST_FUNC_FIRST_VAR_SKIP);
      /* zero out var node pointer */
      ast_modify_skip(a, L->start, L->start, AST_FUNC_FIRST_VAR_SKIP);
      v7->last_var_node = L->start;
      add_inlined_node(v7, a, AST_FUNC_DECL, v7->tok, v7->tok_len);

      CALL_PARSE_FUNCDECL(1, 0, fid_p_body_1);
      ast_set_skip(a, L->start, AST_END_SKIP);
    } else {
      CALL_PARSE_STATEMENT(fid_p_body_2);
    }
  }
  CR_RETURN_VOID();
}

/* static enum v7_err parse_statement(struct v7 *v7, struct ast *a) */
fid_parse_statement :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_statement_locals_t)
{
  switch (v7->cur_tok) {
    case TOK_SEMICOLON:
      next_tok(v7);
      /* empty statement */
      CR_RETURN_VOID();
    case TOK_OPEN_CURLY: /* block */
      CALL_PARSE_BLOCK(fid_p_stat_3);
      /* returning because no semicolon required */
      CR_RETURN_VOID();
    case TOK_IF:
      next_tok(v7);
      CALL_PARSE_IF(fid_p_stat_4);
      CR_RETURN_VOID();
    case TOK_WHILE:
      next_tok(v7);
      CALL_PARSE_WHILE(fid_p_stat_5);
      CR_RETURN_VOID();
    case TOK_DO:
      next_tok(v7);
      CALL_PARSE_DOWHILE(fid_p_stat_10);
      CR_RETURN_VOID();
    case TOK_FOR:
      next_tok(v7);
      CALL_PARSE_FOR(fid_p_stat_11);
      CR_RETURN_VOID();
    case TOK_TRY:
      next_tok(v7);
      CALL_PARSE_TRY(fid_p_stat_12);
      CR_RETURN_VOID();
    case TOK_SWITCH:
      next_tok(v7);
      CALL_PARSE_SWITCH(fid_p_stat_13);
      CR_RETURN_VOID();
    case TOK_WITH:
      next_tok(v7);
      CALL_PARSE_WITH(fid_p_stat_14);
      CR_RETURN_VOID();
    case TOK_BREAK:
      if (!(v7->pstate.in_loop || v7->pstate.in_switch)) {
        CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
      }
      next_tok(v7);
      PARSE_WITH_OPT_ARG(AST_BREAK, AST_LABELED_BREAK, CALL_PARSE_IDENT,
                         fid_p_stat_7);
      break;
    case TOK_CONTINUE:
      if (!v7->pstate.in_loop) {
        CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
      }
      next_tok(v7);
      PARSE_WITH_OPT_ARG(AST_CONTINUE, AST_LABELED_CONTINUE, CALL_PARSE_IDENT,
                         fid_p_stat_8);
      break;
    case TOK_RETURN:
      if (!v7->pstate.in_function) {
        CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
      }
      next_tok(v7);
      PARSE_WITH_OPT_ARG(AST_RETURN, AST_VALUE_RETURN, CALL_PARSE_EXPRESSION,
                         fid_p_stat_6);
      break;
    case TOK_THROW:
      next_tok(v7);
      add_node(v7, a, AST_THROW);
      CALL_PARSE_EXPRESSION(fid_p_stat_2);
      break;
    case TOK_DEBUGGER:
      next_tok(v7);
      add_node(v7, a, AST_DEBUGGER);
      break;
    case TOK_VAR:
      next_tok(v7);
      CALL_PARSE_VAR(fid_p_stat_9);
      break;
    case TOK_IDENTIFIER:
      if (lookahead(v7) == TOK_COLON) {
        add_inlined_node(v7, a, AST_LABEL, v7->tok, v7->tok_len);
        next_tok(v7);
        EXPECT(TOK_COLON);
        CR_RETURN_VOID();
      }
    /* fall through */
    default:
      CALL_PARSE_EXPRESSION(fid_p_stat_1);
      break;
  }

  if (end_of_statement(v7) != V7_OK) {
    CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
  }
  ACCEPT(TOK_SEMICOLON); /* swallow optional semicolon */
  CR_RETURN_VOID();
}

/* static enum v7_err parse_expression(struct v7 *v7, struct ast *a) */
fid_parse_expression :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_expression_locals_t)
{
  L->pos = a->mbuf.len;
  L->group = 0;
  while (1) {
    CALL_PARSE_ASSIGN(fid_p_expr_1);
    if (ACCEPT(TOK_COMMA)) {
      L->group = 1;
    } else {
      break;
    }
  }
  if (L->group) {
    insert_node(v7, a, L->pos, AST_SEQ);
  }
  CR_RETURN_VOID();
}

/* static enum v7_err parse_assign(struct v7 *v7, struct ast *a) */
fid_parse_assign :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_assign_locals_t)
{
  CALL_PARSE_BINARY(0, a->mbuf.len, fid_p_assign_1);
  CR_RETURN_VOID();
}

/*
 * static enum v7_err parse_binary(struct v7 *v7, struct ast *a, int level,
 *                                 ast_off_t pos)
 */
#if 1
fid_parse_binary :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_binary_locals_t)
{
/*
 * Note: we use macro CUR_POS instead of a local variable, since this
 * function is called really a lot, so, each byte on stack frame counts.
 *
 * It will work a bit slower of course, but slowness is not a problem
 */
#define CUR_POS ((L->level > L->arg.min_level) ? L->saved_mbuf_len : L->arg.pos)
  L->saved_mbuf_len = a->mbuf.len;

  CALL_PARSE_PREFIX(fid_p_binary_6);

  for (L->level = (int) ARRAY_SIZE(levels) - 1; L->level >= L->arg.min_level;
       L->level--) {
    for (L->i = 0; L->i < levels[L->level].len; L->i++) {
      L->tok = levels[L->level].parts[L->i].start_tok;
      L->ast = levels[L->level].parts[L->i].start_ast;
      do {
        if (v7->pstate.inhibit_in && L->tok == TOK_IN) {
          continue;
        }

        /*
         * Ternary operator sits in the middle of the binary operator
         * precedence chain. Deal with it as an exception and don't break
         * the chain.
         */
        if (L->tok == TOK_QUESTION && v7->cur_tok == TOK_QUESTION) {
          next_tok(v7);
          CALL_PARSE_ASSIGN(fid_p_binary_2);
          EXPECT(TOK_COLON);
          CALL_PARSE_ASSIGN(fid_p_binary_3);
          insert_node(v7, a, CUR_POS, AST_COND);
          CR_RETURN_VOID();
        } else if (ACCEPT(L->tok)) {
          if (levels[L->level].left_to_right) {
            insert_node(v7, a, CUR_POS, (enum ast_tag) L->ast);
            CALL_PARSE_BINARY(L->level, CUR_POS, fid_p_binary_4);
          } else {
            CALL_PARSE_BINARY(L->level, a->mbuf.len, fid_p_binary_5);
            insert_node(v7, a, CUR_POS, (enum ast_tag) L->ast);
          }
        }
      } while (L->ast = (enum ast_tag)(L->ast + 1),
               L->tok < levels[L->level].parts[L->i].end_tok &&
                   (L->tok = (enum v7_tok)(L->tok + 1)));
    }
  }

  CR_RETURN_VOID();
#undef CUR_POS
}
#endif

/* enum v7_err parse_prefix(struct v7 *v7, struct ast *a) */
fid_parse_prefix :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_prefix_locals_t)
{
  for (;;) {
    switch (v7->cur_tok) {
      case TOK_PLUS:
        next_tok(v7);
        add_node(v7, a, AST_POSITIVE);
        break;
      case TOK_MINUS:
        next_tok(v7);
        add_node(v7, a, AST_NEGATIVE);
        break;
      case TOK_PLUS_PLUS:
        next_tok(v7);
        add_node(v7, a, AST_PREINC);
        break;
      case TOK_MINUS_MINUS:
        next_tok(v7);
        add_node(v7, a, AST_PREDEC);
        break;
      case TOK_TILDA:
        next_tok(v7);
        add_node(v7, a, AST_NOT);
        break;
      case TOK_NOT:
        next_tok(v7);
        add_node(v7, a, AST_LOGICAL_NOT);
        break;
      case TOK_VOID:
        next_tok(v7);
        add_node(v7, a, AST_VOID);
        break;
      case TOK_DELETE:
        next_tok(v7);
        add_node(v7, a, AST_DELETE);
        break;
      case TOK_TYPEOF:
        next_tok(v7);
        add_node(v7, a, AST_TYPEOF);
        break;
      default:
        CALL_PARSE_POSTFIX(fid_p_prefix_1);
        CR_RETURN_VOID();
    }
  }
}

/* static enum v7_err parse_postfix(struct v7 *v7, struct ast *a) */
fid_parse_postfix :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_postfix_locals_t)
{
  L->pos = a->mbuf.len;
  CALL_PARSE_CALLEXPR(fid_p_postfix_1);

  if (v7->after_newline) {
    CR_RETURN_VOID();
  }
  switch (v7->cur_tok) {
    case TOK_PLUS_PLUS:
      next_tok(v7);
      insert_node(v7, a, L->pos, AST_POSTINC);
      break;
    case TOK_MINUS_MINUS:
      next_tok(v7);
      insert_node(v7, a, L->pos, AST_POSTDEC);
      break;
    default:
      break; /* nothing */
  }
  CR_RETURN_VOID();
}

/* static enum v7_err parse_callexpr(struct v7 *v7, struct ast *a) */
fid_parse_callexpr :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_callexpr_locals_t)
{
  L->pos = a->mbuf.len;
  CALL_PARSE_NEWEXPR(fid_p_callexpr_1);

  for (;;) {
    switch (v7->cur_tok) {
      case TOK_DOT:
      case TOK_OPEN_BRACKET:
        CALL_PARSE_MEMBER(L->pos, fid_p_callexpr_3);
        break;
      case TOK_OPEN_PAREN:
        next_tok(v7);
        CALL_PARSE_ARGLIST(fid_p_callexpr_2);
        EXPECT(TOK_CLOSE_PAREN);
        insert_node(v7, a, L->pos, AST_CALL);
        break;
      default:
        CR_RETURN_VOID();
    }
  }
}

/* static enum v7_err parse_newexpr(struct v7 *v7, struct ast *a) */
fid_parse_newexpr :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_newexpr_locals_t)
{
  switch (v7->cur_tok) {
    case TOK_NEW:
      next_tok(v7);
      L->start = add_node(v7, a, AST_NEW);
      CALL_PARSE_MEMBEREXPR(fid_p_newexpr_3);
      if (ACCEPT(TOK_OPEN_PAREN)) {
        CALL_PARSE_ARGLIST(fid_p_newexpr_4);
        EXPECT(TOK_CLOSE_PAREN);
      }
      ast_set_skip(a, L->start, AST_END_SKIP);
      break;
    case TOK_FUNCTION:
      next_tok(v7);
      CALL_PARSE_FUNCDECL(0, 0, fid_p_newexpr_2);
      break;
    default:
      CALL_PARSE_TERMINAL(fid_p_newexpr_1);
      break;
  }
  CR_RETURN_VOID();
}

/* static enum v7_err parse_terminal(struct v7 *v7, struct ast *a) */
fid_parse_terminal :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_terminal_locals_t)
{
  switch (v7->cur_tok) {
    case TOK_OPEN_PAREN:
      next_tok(v7);
      CALL_PARSE_EXPRESSION(fid_p_terminal_1);
      EXPECT(TOK_CLOSE_PAREN);
      break;
    case TOK_OPEN_BRACKET:
      next_tok(v7);
      L->start = add_node(v7, a, AST_ARRAY);
      while (v7->cur_tok != TOK_CLOSE_BRACKET) {
        if (v7->cur_tok == TOK_COMMA) {
          /* Array literals allow missing elements, e.g. [,,1,] */
          add_node(v7, a, AST_NOP);
        } else {
          CALL_PARSE_ASSIGN(fid_p_terminal_2);
        }
        ACCEPT(TOK_COMMA);
      }
      EXPECT(TOK_CLOSE_BRACKET);
      ast_set_skip(a, L->start, AST_END_SKIP);
      break;
    case TOK_OPEN_CURLY:
      next_tok(v7);
      L->start = add_node(v7, a, AST_OBJECT);
      if (v7->cur_tok != TOK_CLOSE_CURLY) {
        do {
          if (v7->cur_tok == TOK_CLOSE_CURLY) {
            break;
          }
          CALL_PARSE_PROP(fid_p_terminal_3);
        } while (ACCEPT(TOK_COMMA));
      }
      EXPECT(TOK_CLOSE_CURLY);
      ast_set_skip(a, L->start, AST_END_SKIP);
      break;
    case TOK_THIS:
      next_tok(v7);
      add_node(v7, a, AST_THIS);
      break;
    case TOK_TRUE:
      next_tok(v7);
      add_node(v7, a, AST_TRUE);
      break;
    case TOK_FALSE:
      next_tok(v7);
      add_node(v7, a, AST_FALSE);
      break;
    case TOK_NULL:
      next_tok(v7);
      add_node(v7, a, AST_NULL);
      break;
    case TOK_STRING_LITERAL:
      add_inlined_node(v7, a, AST_STRING, v7->tok + 1, v7->tok_len - 2);
      next_tok(v7);
      break;
    case TOK_NUMBER:
      add_inlined_node(v7, a, AST_NUM, v7->tok, v7->tok_len);
      next_tok(v7);
      break;
    case TOK_REGEX_LITERAL:
      add_inlined_node(v7, a, AST_REGEX, v7->tok, v7->tok_len);
      next_tok(v7);
      break;
    case TOK_IDENTIFIER:
      if (v7->tok_len == 9 && strncmp(v7->tok, "undefined", v7->tok_len) == 0) {
        add_node(v7, a, AST_UNDEFINED);
        next_tok(v7);
        break;
      }
    /* fall through */
    default:
      CALL_PARSE_IDENT(fid_p_terminal_4);
  }
  CR_RETURN_VOID();
}

/* static enum v7_err parse_block(struct v7 *v7, struct ast *a) */
fid_parse_block :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_block_locals_t)
{
  EXPECT(TOK_OPEN_CURLY);
  CALL_PARSE_BODY(TOK_CLOSE_CURLY, fid_p_block_1);
  EXPECT(TOK_CLOSE_CURLY);
  CR_RETURN_VOID();
}

/* static enum v7_err parse_if(struct v7 *v7, struct ast *a) */
fid_parse_if :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_if_locals_t)
{
  L->start = add_node(v7, a, AST_IF);
  EXPECT(TOK_OPEN_PAREN);
  CALL_PARSE_EXPRESSION(fid_p_if_1);
  EXPECT(TOK_CLOSE_PAREN);
  CALL_PARSE_STATEMENT(fid_p_if_2);
  ast_set_skip(a, L->start, AST_END_IF_TRUE_SKIP);
  if (ACCEPT(TOK_ELSE)) {
    CALL_PARSE_STATEMENT(fid_p_if_3);
  }
  ast_set_skip(a, L->start, AST_END_SKIP);
  CR_RETURN_VOID();
}

/* static enum v7_err parse_while(struct v7 *v7, struct ast *a) */
fid_parse_while :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_while_locals_t)
{
  L->start = add_node(v7, a, AST_WHILE);
  L->saved_in_loop = v7->pstate.in_loop;
  EXPECT(TOK_OPEN_PAREN);
  CALL_PARSE_EXPRESSION(fid_p_while_1);
  EXPECT(TOK_CLOSE_PAREN);
  v7->pstate.in_loop = 1;
  CALL_PARSE_STATEMENT(fid_p_while_2);
  ast_set_skip(a, L->start, AST_END_SKIP);
  v7->pstate.in_loop = L->saved_in_loop;
  CR_RETURN_VOID();
}

/* static enum v7_err parse_ident(struct v7 *v7, struct ast *a) */
fid_parse_ident :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_ident_locals_t)
{
  if (v7->cur_tok == TOK_IDENTIFIER) {
    add_inlined_node(v7, a, AST_IDENT, v7->tok, v7->tok_len);
    next_tok(v7);
    CR_RETURN_VOID();
  }
  CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
}

/*
 * static enum v7_err parse_ident_allow_reserved_words(struct v7 *v7,
 *                                                     struct ast *a)
 *
 */
fid_parse_ident_allow_reserved_words :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_ident_allow_reserved_words_locals_t)
{
  /* Allow reserved words as property names. */
  if (is_reserved_word_token(v7->cur_tok)) {
    add_inlined_node(v7, a, AST_IDENT, v7->tok, v7->tok_len);
    next_tok(v7);
  } else {
    CALL_PARSE_IDENT(fid_p_ident_arw_1);
  }
  CR_RETURN_VOID();
}

/* static enum v7_err parse_funcdecl(struct v7 *, struct ast *, int, int) */
fid_parse_funcdecl :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_funcdecl_locals_t)
{
  L->start = add_node(v7, a, AST_FUNC);
  L->outer_last_var_node = v7->last_var_node;
  L->saved_in_function = v7->pstate.in_function;
  L->saved_in_strict = v7->pstate.in_strict;

  v7->last_var_node = L->start;
  ast_modify_skip(a, L->start, L->start, AST_FUNC_FIRST_VAR_SKIP);

  CR_TRY(fid_p_funcdecl_2);
  {
    if (L->arg.reserved_name) {
      CALL_PARSE_IDENT_ALLOW_RESERVED_WORDS(fid_p_funcdecl_1);
    } else {
      CALL_PARSE_IDENT(fid_p_funcdecl_9);
    }
  }
  CR_CATCH(PARSER_EXC_ID__SYNTAX_ERROR, fid_p_funcdecl_2, fid_p_funcdecl_3);
  {
    if (L->arg.require_named) {
      /* function name is required, so, rethrow SYNTAX ERROR */
      CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
    } else {
      /* it's ok not to have a function name, just insert NOP */
      add_node(v7, a, AST_NOP);
    }
  }
  CR_ENDCATCH(fid_p_funcdecl_3);

  EXPECT(TOK_OPEN_PAREN);
  CALL_PARSE_ARGLIST(fid_p_funcdecl_4);
  EXPECT(TOK_CLOSE_PAREN);
  ast_set_skip(a, L->start, AST_FUNC_BODY_SKIP);
  v7->pstate.in_function = 1;
  EXPECT(TOK_OPEN_CURLY);

  CR_TRY(fid_p_funcdecl_5);
  {
    CALL_PARSE_USE_STRICT(fid_p_funcdecl_7);
    v7->pstate.in_strict = 1;
  }
  CR_CATCH(PARSER_EXC_ID__SYNTAX_ERROR, fid_p_funcdecl_5, fid_p_funcdecl_6);
  CR_ENDCATCH(fid_p_funcdecl_6);

  CALL_PARSE_BODY(TOK_CLOSE_CURLY, fid_p_funcdecl_8);
  EXPECT(TOK_CLOSE_CURLY);
  v7->pstate.in_strict = L->saved_in_strict;
  v7->pstate.in_function = L->saved_in_function;
  ast_set_skip(a, L->start, AST_END_SKIP);
  v7->last_var_node = L->outer_last_var_node;

  CR_RETURN_VOID();
}

/* static enum v7_err parse_arglist(struct v7 *v7, struct ast *a) */
fid_parse_arglist :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_arglist_locals_t)
{
  if (v7->cur_tok != TOK_CLOSE_PAREN) {
    do {
      CALL_PARSE_ASSIGN(fid_p_arglist_1);
    } while (ACCEPT(TOK_COMMA));
  }
  CR_RETURN_VOID();
}

/*
 * static enum v7_err parse_member(struct v7 *v7, struct ast *a, ast_off_t pos)
 */
fid_parse_member :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_member_locals_t)
{
  switch (v7->cur_tok) {
    case TOK_DOT:
      next_tok(v7);
      /* Allow reserved words as member identifiers */
      if (is_reserved_word_token(v7->cur_tok) ||
          v7->cur_tok == TOK_IDENTIFIER) {
        insert_inlined_node(v7, a, L->arg.pos, AST_MEMBER, v7->tok,
                            v7->tok_len);
        next_tok(v7);
      } else {
        CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
      }
      break;
    case TOK_OPEN_BRACKET:
      next_tok(v7);
      CALL_PARSE_EXPRESSION(fid_p_member_1);
      EXPECT(TOK_CLOSE_BRACKET);
      insert_node(v7, a, L->arg.pos, AST_INDEX);
      break;
    default:
      CR_RETURN_VOID();
  }
  /* not necessary, but let it be anyway */
  CR_RETURN_VOID();
}

/* static enum v7_err parse_memberexpr(struct v7 *v7, struct ast *a) */
fid_parse_memberexpr :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_memberexpr_locals_t)
{
  L->pos = a->mbuf.len;
  CALL_PARSE_NEWEXPR(fid_p_memberexpr_1);

  for (;;) {
    switch (v7->cur_tok) {
      case TOK_DOT:
      case TOK_OPEN_BRACKET:
        CALL_PARSE_MEMBER(L->pos, fid_p_memberexpr_2);
        break;
      default:
        CR_RETURN_VOID();
    }
  }
}

/* static enum v7_err parse_var(struct v7 *v7, struct ast *a) */
fid_parse_var :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_var_locals_t)
{
  L->start = add_node(v7, a, AST_VAR);
  ast_modify_skip(a, v7->last_var_node, L->start, AST_FUNC_FIRST_VAR_SKIP);
  /* zero out var node pointer */
  ast_modify_skip(a, L->start, L->start, AST_FUNC_FIRST_VAR_SKIP);
  v7->last_var_node = L->start;
  do {
    add_inlined_node(v7, a, AST_VAR_DECL, v7->tok, v7->tok_len);
    EXPECT(TOK_IDENTIFIER);
    if (ACCEPT(TOK_ASSIGN)) {
      CALL_PARSE_ASSIGN(fid_p_var_1);
    } else {
      add_node(v7, a, AST_NOP);
    }
  } while (ACCEPT(TOK_COMMA));
  ast_set_skip(a, L->start, AST_END_SKIP);
  CR_RETURN_VOID();
}

/* static enum v7_err parse_prop(struct v7 *v7, struct ast *a) */
fid_parse_prop :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_prop_locals_t)
{
#ifdef V7_ENABLE_JS_GETTERS
  if (v7->cur_tok == TOK_IDENTIFIER && v7->tok_len == 3 &&
      strncmp(v7->tok, "get", v7->tok_len) == 0 && lookahead(v7) != TOK_COLON) {
    next_tok(v7);
    add_node(v7, a, AST_GETTER);
    CALL_PARSE_FUNCDECL(1, 1, fid_p_prop_1_getter);
  } else
#endif
      if (v7->cur_tok == TOK_IDENTIFIER && lookahead(v7) == TOK_OPEN_PAREN) {
    /* ecmascript 6 feature */
    CALL_PARSE_FUNCDECL(1, 1, fid_p_prop_2);
#ifdef V7_ENABLE_JS_SETTERS
  } else if (v7->cur_tok == TOK_IDENTIFIER && v7->tok_len == 3 &&
             strncmp(v7->tok, "set", v7->tok_len) == 0 &&
             lookahead(v7) != TOK_COLON) {
    next_tok(v7);
    add_node(v7, a, AST_SETTER);
    CALL_PARSE_FUNCDECL(1, 1, fid_p_prop_3_setter);
#endif
  } else {
    /* Allow reserved words as property names. */
    if (is_reserved_word_token(v7->cur_tok) || v7->cur_tok == TOK_IDENTIFIER ||
        v7->cur_tok == TOK_NUMBER) {
      add_inlined_node(v7, a, AST_PROP, v7->tok, v7->tok_len);
    } else if (v7->cur_tok == TOK_STRING_LITERAL) {
      add_inlined_node(v7, a, AST_PROP, v7->tok + 1, v7->tok_len - 2);
    } else {
      CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
    }
    next_tok(v7);
    EXPECT(TOK_COLON);
    CALL_PARSE_ASSIGN(fid_p_prop_4);
  }
  CR_RETURN_VOID();
}

/* static enum v7_err parse_dowhile(struct v7 *v7, struct ast *a) */
fid_parse_dowhile :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_dowhile_locals_t)
{
  L->start = add_node(v7, a, AST_DOWHILE);
  L->saved_in_loop = v7->pstate.in_loop;

  v7->pstate.in_loop = 1;
  CALL_PARSE_STATEMENT(fid_p_dowhile_1);
  v7->pstate.in_loop = L->saved_in_loop;
  ast_set_skip(a, L->start, AST_DO_WHILE_COND_SKIP);
  EXPECT(TOK_WHILE);
  EXPECT(TOK_OPEN_PAREN);
  CALL_PARSE_EXPRESSION(fid_p_dowhile_2);
  EXPECT(TOK_CLOSE_PAREN);
  ast_set_skip(a, L->start, AST_END_SKIP);
  CR_RETURN_VOID();
}

/* static enum v7_err parse_for(struct v7 *v7, struct ast *a) */
fid_parse_for :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_for_locals_t)
{
  /* TODO(mkm): for of, for each in */
  /*
  ast_off_t start;
  int saved_in_loop;
  */

  L->start = add_node(v7, a, AST_FOR);
  L->saved_in_loop = v7->pstate.in_loop;

  EXPECT(TOK_OPEN_PAREN);

  if (parse_optional(v7, a, TOK_SEMICOLON)) {
    /*
     * TODO(mkm): make this reentrant otherwise this pearl won't parse:
     * for((function(){return 1 in o.a ? o : x})().a in [1,2,3])
     */
    v7->pstate.inhibit_in = 1;
    if (ACCEPT(TOK_VAR)) {
      CALL_PARSE_VAR(fid_p_for_1);
    } else {
      CALL_PARSE_EXPRESSION(fid_p_for_2);
    }
    v7->pstate.inhibit_in = 0;

    if (ACCEPT(TOK_IN)) {
      CALL_PARSE_EXPRESSION(fid_p_for_3);
      add_node(v7, a, AST_NOP);
      /*
       * Assumes that for and for in have the same AST format which is
       * suboptimal but avoids the need of fixing up the var offset chain.
       * TODO(mkm) improve this
       */
      ast_modify_tag(a, L->start - 1, AST_FOR_IN);
      goto for_loop_body;
    }
  }

  EXPECT(TOK_SEMICOLON);
  if (parse_optional(v7, a, TOK_SEMICOLON)) {
    CALL_PARSE_EXPRESSION(fid_p_for_4);
  }
  EXPECT(TOK_SEMICOLON);
  if (parse_optional(v7, a, TOK_CLOSE_PAREN)) {
    CALL_PARSE_EXPRESSION(fid_p_for_5);
  }

for_loop_body:
  EXPECT(TOK_CLOSE_PAREN);
  ast_set_skip(a, L->start, AST_FOR_BODY_SKIP);
  v7->pstate.in_loop = 1;
  CALL_PARSE_STATEMENT(fid_p_for_6);
  v7->pstate.in_loop = L->saved_in_loop;
  ast_set_skip(a, L->start, AST_END_SKIP);
  CR_RETURN_VOID();
}

/* static enum v7_err parse_try(struct v7 *v7, struct ast *a) */
fid_parse_try :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_try_locals_t)
{
  L->start = add_node(v7, a, AST_TRY);
  L->catch_or_finally = 0;
  CALL_PARSE_BLOCK(fid_p_try_1);
  ast_set_skip(a, L->start, AST_TRY_CATCH_SKIP);
  if (ACCEPT(TOK_CATCH)) {
    L->catch_or_finally = 1;
    EXPECT(TOK_OPEN_PAREN);
    CALL_PARSE_IDENT(fid_p_try_2);
    EXPECT(TOK_CLOSE_PAREN);
    CALL_PARSE_BLOCK(fid_p_try_3);
  }
  ast_set_skip(a, L->start, AST_TRY_FINALLY_SKIP);
  if (ACCEPT(TOK_FINALLY)) {
    L->catch_or_finally = 1;
    CALL_PARSE_BLOCK(fid_p_try_4);
  }
  ast_set_skip(a, L->start, AST_END_SKIP);

  /* make sure `catch` and `finally` aren't both missing */
  if (!L->catch_or_finally) {
    CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
  }

  CR_RETURN_VOID();
}

/* static enum v7_err parse_switch(struct v7 *v7, struct ast *a) */
fid_parse_switch :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_switch_locals_t)
{
  L->start = add_node(v7, a, AST_SWITCH);
  L->saved_in_switch = v7->pstate.in_switch;

  ast_set_skip(a, L->start, AST_SWITCH_DEFAULT_SKIP); /* clear out */
  EXPECT(TOK_OPEN_PAREN);
  CALL_PARSE_EXPRESSION(fid_p_switch_1);
  EXPECT(TOK_CLOSE_PAREN);
  EXPECT(TOK_OPEN_CURLY);
  v7->pstate.in_switch = 1;
  while (v7->cur_tok != TOK_CLOSE_CURLY) {
    switch (v7->cur_tok) {
      case TOK_CASE:
        next_tok(v7);
        L->case_start = add_node(v7, a, AST_CASE);
        CALL_PARSE_EXPRESSION(fid_p_switch_2);
        EXPECT(TOK_COLON);
        while (v7->cur_tok != TOK_CASE && v7->cur_tok != TOK_DEFAULT &&
               v7->cur_tok != TOK_CLOSE_CURLY) {
          CALL_PARSE_STATEMENT(fid_p_switch_3);
        }
        ast_set_skip(a, L->case_start, AST_END_SKIP);
        break;
      case TOK_DEFAULT:
        next_tok(v7);
        EXPECT(TOK_COLON);
        ast_set_skip(a, L->start, AST_SWITCH_DEFAULT_SKIP);
        L->case_start = add_node(v7, a, AST_DEFAULT);
        while (v7->cur_tok != TOK_CASE && v7->cur_tok != TOK_DEFAULT &&
               v7->cur_tok != TOK_CLOSE_CURLY) {
          CALL_PARSE_STATEMENT(fid_p_switch_4);
        }
        ast_set_skip(a, L->case_start, AST_END_SKIP);
        break;
      default:
        CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
    }
  }
  EXPECT(TOK_CLOSE_CURLY);
  ast_set_skip(a, L->start, AST_END_SKIP);
  v7->pstate.in_switch = L->saved_in_switch;
  CR_RETURN_VOID();
}

/* static enum v7_err parse_with(struct v7 *v7, struct ast *a) */
fid_parse_with :
#undef L
#define L CR_CUR_LOCALS_PT(fid_parse_with_locals_t)
{
  L->start = add_node(v7, a, AST_WITH);
  if (v7->pstate.in_strict) {
    CR_THROW(PARSER_EXC_ID__SYNTAX_ERROR);
  }
  EXPECT(TOK_OPEN_PAREN);
  CALL_PARSE_EXPRESSION(fid_p_with_1);
  EXPECT(TOK_CLOSE_PAREN);
  CALL_PARSE_STATEMENT(fid_p_with_2);
  ast_set_skip(a, L->start, AST_END_SKIP);
  CR_RETURN_VOID();
}

fid_none:
  /* stack is empty, so we're done; return */
  return rc;
}

V7_PRIVATE enum v7_err parse(struct v7 *v7, struct ast *a, const char *src,
                             size_t src_len, int is_json) {
  enum v7_err rcode;
  const char *error_msg = NULL;
  const char *p;
  struct cr_ctx cr_ctx;
  union user_arg_ret arg_retval;
  enum cr_status rc;
#if defined(V7_ENABLE_STACK_TRACKING)
  struct stack_track_ctx stack_track_ctx;
#endif
  int saved_line_no = v7->line_no;

#if defined(V7_ENABLE_STACK_TRACKING)
  v7_stack_track_start(v7, &stack_track_ctx);
#endif

  v7->pstate.source_code = v7->pstate.pc = src;
  v7->pstate.src_end = src + src_len;
  v7->pstate.file_name = "<stdin>";
  v7->pstate.line_no = 1;
  v7->pstate.in_function = 0;
  v7->pstate.in_loop = 0;
  v7->pstate.in_switch = 0;

  /*
   * TODO(dfrank): `v7->parser.line_no` vs `v7->line_no` is confusing.  probaby
   * we need to refactor it.
   *
   * See comment for v7->line_no in core.h for some details.
   */
  v7->line_no = 1;

  next_tok(v7);
  /*
   * setup initial state for "after newline" tracking.
   * next_tok will consume our token and position the current line
   * position at the beginning of the next token.
   * While processing the first token, both the leading and the
   * trailing newlines will be counted and thus it will create a spurious
   * "after newline" condition at the end of the first token
   * regardless if there is actually a newline after it.
   */
  for (p = src; isspace((int) *p); p++) {
    if (*p == '\n') {
      v7->pstate.prev_line_no++;
    }
  }

  /* init cr context */
  cr_context_init(&cr_ctx, &arg_retval, sizeof(arg_retval), _fid_descrs);

  /* prepare first function call: fid_mul_sum */
  if (is_json) {
    CR_FIRST_CALL_PREPARE_C(&cr_ctx, fid_parse_terminal);
  } else {
    CR_FIRST_CALL_PREPARE_C(&cr_ctx, fid_parse_script);
  }

  /* proceed to coroutine execution */
  rc = parser_cr_exec(&cr_ctx, v7, a);

  /* set `rcode` depending on coroutine state */
  switch (rc) {
    case CR_RES__OK:
      rcode = V7_OK;
      break;
    case CR_RES__ERR_UNCAUGHT_EXCEPTION:
      switch ((enum parser_exc_id) CR_THROWN_C(&cr_ctx)) {
        case PARSER_EXC_ID__SYNTAX_ERROR:
          rcode = V7_SYNTAX_ERROR;
          error_msg = "Syntax error";
          break;

        default:
          rcode = V7_INTERNAL_ERROR;
          error_msg = "Internal error: no exception id set";
          break;
      }
      break;
    default:
      rcode = V7_INTERNAL_ERROR;
      error_msg = "Internal error: unexpected parser coroutine return code";
      break;
  }

#if defined(V7_TRACK_MAX_PARSER_STACK_SIZE)
  /* remember how much stack space was consumed */

  if (v7->parser_stack_data_max_size < cr_ctx.stack_data.size) {
    v7->parser_stack_data_max_size = cr_ctx.stack_data.size;
#ifndef NO_LIBC
    printf("parser_stack_data_max_size=%u\n",
           (unsigned int) v7->parser_stack_data_max_size);
#endif
  }

  if (v7->parser_stack_ret_max_size < cr_ctx.stack_ret.size) {
    v7->parser_stack_ret_max_size = cr_ctx.stack_ret.size;
#ifndef NO_LIBC
    printf("parser_stack_ret_max_size=%u\n",
           (unsigned int) v7->parser_stack_ret_max_size);
#endif
  }

#if defined(CR_TRACK_MAX_STACK_LEN)
  if (v7->parser_stack_data_max_len < cr_ctx.stack_data_max_len) {
    v7->parser_stack_data_max_len = cr_ctx.stack_data_max_len;
#ifndef NO_LIBC
    printf("parser_stack_data_max_len=%u\n",
           (unsigned int) v7->parser_stack_data_max_len);
#endif
  }

  if (v7->parser_stack_ret_max_len < cr_ctx.stack_ret_max_len) {
    v7->parser_stack_ret_max_len = cr_ctx.stack_ret_max_len;
#ifndef NO_LIBC
    printf("parser_stack_ret_max_len=%u\n",
           (unsigned int) v7->parser_stack_ret_max_len);
#endif
  }
#endif

#endif

  /* free resources occupied by context (at least, "stack" arrays) */
  cr_context_free(&cr_ctx);

#if defined(V7_ENABLE_STACK_TRACKING)
  {
    int diff = v7_stack_track_end(v7, &stack_track_ctx);
    if (diff > v7->stack_stat[V7_STACK_STAT_PARSER]) {
      v7->stack_stat[V7_STACK_STAT_PARSER] = diff;
    }
  }
#endif

  /* Check if AST was overflown */
  if (a->has_overflow) {
    rcode = v7_throwf(v7, SYNTAX_ERROR,
                      "Script too large (try V7_LARGE_AST build option)");
    V7_THROW(V7_AST_TOO_LARGE);
  }

  if (rcode == V7_OK && v7->cur_tok != TOK_END_OF_INPUT) {
    rcode = V7_SYNTAX_ERROR;
    error_msg = "Syntax error";
  }

  if (rcode != V7_OK) {
    unsigned long col = get_column(v7->pstate.source_code, v7->tok);
    int line_len = 0;

    assert(error_msg != NULL);

    for (p = v7->tok - col; p < v7->pstate.src_end && *p != '\0' && *p != '\n';
         p++) {
      line_len++;
    }

    /* fixup line number: line_no points to the beginning of the next token */
    for (; p < v7->pstate.pc; p++) {
      if (*p == '\n') {
        v7->pstate.line_no--;
      }
    }

    /*
     * We already have a proper `rcode`, that's why we discard returned value
     * of `v7_throwf()`, which is always `V7_EXEC_EXCEPTION`.
     *
     * TODO(dfrank): probably get rid of distinct error types, and use just
     * `V7_JS_EXCEPTION`. However it would be good to have a way to get exact
     * error type, so probably error object should contain some property with
     * error code, but it would make exceptions even more expensive, etc, etc.
     */
    {
      enum v7_err _tmp;
      _tmp = v7_throwf(v7, SYNTAX_ERROR, "%s at line %d col %lu:\n%.*s\n%*s^",
                       error_msg, v7->pstate.line_no, col, line_len,
                       v7->tok - col, (int) col - 1, "");
      (void) _tmp;
    }
    V7_THROW(rcode);
  }

clean:
  v7->line_no = saved_line_no;
  return rcode;
}
