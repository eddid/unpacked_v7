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
























/*
 * User functions
 * (as well as other in-function entry points)
 */
enum my_fid {
  fid_none = CR_FID__NONE,

  /* parse_script function */
  fid_parse_script = CR_FID__USER,
  fid_p_script_1_catched_error_from_parse_use_strict,
  fid_p_script_2_uncatched_error,
  fid_p_script_3_after_parse_use_strict,
  fid_p_script_4_after_parse_body,

  /* parse_use_strict function */
  fid_parse_use_strict,

  /* parse_body function */
  fid_parse_body,
  fid_p_body_1_after_parse_funcdecl,
  fid_p_body_2_after_parse_statement,

  /* parse_statement function */
  fid_parse_statement,
  fid_p_stat_1_after_parse_expression,
  fid_p_stat_2_after_parse_expression,
  fid_p_stat_3_after_parse_block,
  fid_p_stat_4_after_parse_if,
  fid_p_stat_5_after_parse_while,
  fid_p_stat_6_after_parse_expression,
  fid_p_stat_7_after_parse_ident,
  fid_p_stat_8_after_parse_ident,
  fid_p_stat_9_after_parse_var,
  fid_p_stat_10_after_parse_dowhile,
  fid_p_stat_11_after_parse_for,
  fid_p_stat_12_after_parse_try,
  fid_p_stat_13_after_parse_switch,
  fid_p_stat_14_after_parse_with,

  /* parse_expression function */
  fid_parse_expression,
  fid_p_expr_1_after_parse_assign,

  /* parse_assign function */
  fid_parse_assign,
  fid_p_assign_1_after_parse_binary,

  /* parse_binary function */
  fid_parse_binary,
  fid_p_binary_1,
  fid_p_binary_2_after_parse_assign,
  fid_p_binary_3_after_parse_assign,
  fid_p_binary_4_after_parse_binary,
  fid_p_binary_5_after_parse_binary,
  fid_p_binary_6_after_parse_prefix,

  /* parse_prefix function */
  fid_parse_prefix,
  fid_p_prefix_1_after_parse_postfix,

  /* parse_postfix function */
  fid_parse_postfix,
  fid_p_postfix_1_after_parse_callexpr,

  /* parse_callexpr function */
  fid_parse_callexpr,
  fid_p_callexpr_1_after_parse_newexpr,
  fid_p_callexpr_2_after_parse_arglist,
  fid_p_callexpr_3_after_parse_member,

  /* parse_newexpr function */
  fid_parse_newexpr,
  fid_p_newexpr_1_after_parse_terminal,
  fid_p_newexpr_2_after_parse_funcdecl,
  fid_p_newexpr_3_after_parse_memberexpr,
  fid_p_newexpr_4_after_parse_arglist,

  /* parse_terminal function */
  fid_parse_terminal,
  fid_p_terminal_1_after_parse_expression,
  fid_p_terminal_2_after_parse_assign,
  fid_p_terminal_3_after_parse_prop,
  fid_p_terminal_4_after_parse_ident,

  /* parse_block function */
  fid_parse_block,
  fid_p_block_1_after_parse_body,

  /* parse_if function */
  fid_parse_if,
  fid_p_if_1_after_parse_expression,
  fid_p_if_2_after_parse_statement,
  fid_p_if_3_after_parse_statement,

  /* parse_while function */
  fid_parse_while,
  fid_p_while_1_after_parse_expression,
  fid_p_while_2_after_parse_statement,

  /* parse_ident function */
  fid_parse_ident,

  /* parse_ident_allow_reserved_words function */
  fid_parse_ident_allow_reserved_words,
  fid_p_ident_arw_1_after_parse_ident,

  /* parse_funcdecl function */
  fid_parse_funcdecl,
  fid_p_funcdecl_1_after_parse_ident_allow_reserved_words,
  fid_p_funcdecl_2_catched_error_from_parse_ident_allow_reserved_words,
  fid_p_funcdecl_3_uncatched_error,
  fid_p_funcdecl_4_after_parse_arglist,
  fid_p_funcdecl_5_catched_error_from_parse_use_strict,
  fid_p_funcdecl_6_uncatched_error,
  fid_p_funcdecl_7_after_parse_use_strict,
  fid_p_funcdecl_8_after_parse_body,
  fid_p_funcdecl_9_after_parse_ident,

  /* parse_arglist function */
  fid_parse_arglist,
  fid_p_arglist_1_after_parse_assign,

  /* parse_member function */
  fid_parse_member,
  fid_p_member_1_after_parse_expression,

  /* parse_memberexpr function */
  fid_parse_memberexpr,
  fid_p_memberexpr_1_after_parse_newexpr,
  fid_p_memberexpr_2_after_parse_member,

  /* parse_var function */
  fid_parse_var,
  fid_p_var_1_after_parse_assign,

  /* parse_prop function */
  fid_parse_prop,



  fid_p_prop_2_after_parse_funcdecl,



  fid_p_prop_4_after_parse_assign,

  /* parse_dowhile function */
  fid_parse_dowhile,
  fid_p_dowhile_1_after_parse_statement,
  fid_p_dowhile_2_after_parse_expression,

  /* parse_for function */
  fid_parse_for,
  fid_p_for_1_after_parse_var,
  fid_p_for_2_after_parse_expression,
  fid_p_for_3_after_parse_expression,
  fid_p_for_4_after_parse_expression,
  fid_p_for_5_after_parse_expression,
  fid_p_for_6_after_parse_statement,

  /* parse_try function */
  fid_parse_try,
  fid_p_try_1_after_parse_block,
  fid_p_try_2_after_parse_ident,
  fid_p_try_3_after_parse_block,
  fid_p_try_4_after_parse_block,

  /* parse_switch function */
  fid_parse_switch,
  fid_p_switch_1_after_parse_expression,
  fid_p_switch_2_after_parse_expression,
  fid_p_switch_3_after_parse_statement,
  fid_p_switch_4_after_parse_statement,

  /* parse_with function */
  fid_parse_with,
  fid_p_with_1_after_parse_expression,
  fid_p_with_2_after_parse_statement,

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



typedef cr_zero_size_type_t fid_parse_script_arg_t;

/* parse_script's data on stack */
typedef struct fid_parse_script_locals {



  ast_off_t start;
  ast_off_t outer_last_var_node;
  int saved_in_strict;
} fid_parse_script_locals_t;

/* }}} */

/* parse_use_strict {{{ */
/* parse_use_strict's arguments */



typedef cr_zero_size_type_t fid_parse_use_strict_arg_t;

/* parse_use_strict's data on stack */




typedef cr_zero_size_type_t fid_parse_use_strict_locals_t;






/* }}} */

/* parse_body {{{ */
/* parse_body's arguments */
typedef struct fid_parse_body_arg { enum v7_tok end; } fid_parse_body_arg_t;

/* parse_body's data on stack */
typedef struct fid_parse_body_locals {
  struct fid_parse_body_arg arg;

  ast_off_t start;
} fid_parse_body_locals_t;






/* }}} */

/* parse_statement {{{ */
/* parse_statement's arguments */



typedef cr_zero_size_type_t fid_parse_statement_arg_t;

/* parse_statement's data on stack */




typedef cr_zero_size_type_t fid_parse_statement_locals_t;





/* }}} */

/* parse_expression {{{ */
/* parse_expression's arguments */



typedef cr_zero_size_type_t fid_parse_expression_arg_t;

/* parse_expression's data on stack */
typedef struct fid_parse_expression_locals {



  ast_off_t pos;
  int group;
} fid_parse_expression_locals_t;





/* }}} */

/* parse_assign {{{ */
/* parse_assign's arguments */



typedef cr_zero_size_type_t fid_parse_assign_arg_t;

/* parse_assign's data on stack */




typedef cr_zero_size_type_t fid_parse_assign_locals_t;





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







/* }}} */

/* parse_prefix {{{ */
/* parse_prefix's arguments */



typedef cr_zero_size_type_t fid_parse_prefix_arg_t;

/* parse_prefix's data on stack */




typedef cr_zero_size_type_t fid_parse_prefix_locals_t;





/* }}} */

/* parse_postfix {{{ */
/* parse_postfix's arguments */



typedef cr_zero_size_type_t fid_parse_postfix_arg_t;

/* parse_postfix's data on stack */
typedef struct fid_parse_postfix_locals {



  ast_off_t pos;
} fid_parse_postfix_locals_t;





/* }}} */

/* parse_callexpr {{{ */
/* parse_callexpr's arguments */



typedef cr_zero_size_type_t fid_parse_callexpr_arg_t;

/* parse_callexpr's data on stack */
typedef struct fid_parse_callexpr_locals {



  ast_off_t pos;
} fid_parse_callexpr_locals_t;





/* }}} */

/* parse_newexpr {{{ */
/* parse_newexpr's arguments */



typedef cr_zero_size_type_t fid_parse_newexpr_arg_t;

/* parse_newexpr's data on stack */
typedef struct fid_parse_newexpr_locals {



  ast_off_t start;
} fid_parse_newexpr_locals_t;





/* }}} */

/* parse_terminal {{{ */
/* parse_terminal's arguments */



typedef cr_zero_size_type_t fid_parse_terminal_arg_t;

/* parse_terminal's data on stack */
typedef struct fid_parse_terminal_locals {



  ast_off_t start;
} fid_parse_terminal_locals_t;





/* }}} */

/* parse_block {{{ */
/* parse_block's arguments */



typedef cr_zero_size_type_t fid_parse_block_arg_t;

/* parse_block's data on stack */




typedef cr_zero_size_type_t fid_parse_block_locals_t;





/* }}} */

/* parse_if {{{ */
/* parse_if's arguments */



typedef cr_zero_size_type_t fid_parse_if_arg_t;

/* parse_if's data on stack */
typedef struct fid_parse_if_locals {



  ast_off_t start;
} fid_parse_if_locals_t;





/* }}} */

/* parse_while {{{ */
/* parse_while's arguments */



typedef cr_zero_size_type_t fid_parse_while_arg_t;

/* parse_while's data on stack */
typedef struct fid_parse_while_locals {



  ast_off_t start;
  uint8_t saved_in_loop;
} fid_parse_while_locals_t;





/* }}} */

/* parse_ident {{{ */
/* parse_ident's arguments */



typedef cr_zero_size_type_t fid_parse_ident_arg_t;

/* parse_ident's data on stack */




typedef cr_zero_size_type_t fid_parse_ident_locals_t;





/* }}} */

/* parse_ident_allow_reserved_words {{{ */
/* parse_ident_allow_reserved_words's arguments */



typedef cr_zero_size_type_t fid_parse_ident_allow_reserved_words_arg_t;

/* parse_ident_allow_reserved_words's data on stack */




typedef cr_zero_size_type_t fid_parse_ident_allow_reserved_words_locals_t;





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







/* }}} */

/* parse_arglist {{{ */
/* parse_arglist's arguments */



typedef cr_zero_size_type_t fid_parse_arglist_arg_t;

/* parse_arglist's data on stack */




typedef cr_zero_size_type_t fid_parse_arglist_locals_t;





/* }}} */

/* parse_member {{{ */
/* parse_member's arguments */
typedef struct fid_parse_member_arg { ast_off_t pos; } fid_parse_member_arg_t;

/* parse_member's data on stack */
typedef struct fid_parse_member_locals {
  struct fid_parse_member_arg arg;
} fid_parse_member_locals_t;






/* }}} */

/* parse_memberexpr {{{ */
/* parse_memberexpr's arguments */



typedef cr_zero_size_type_t fid_parse_memberexpr_arg_t;

/* parse_memberexpr's data on stack */
typedef struct fid_parse_memberexpr_locals {



  ast_off_t pos;
} fid_parse_memberexpr_locals_t;





/* }}} */

/* parse_var {{{ */
/* parse_var's arguments */



typedef cr_zero_size_type_t fid_parse_var_arg_t;

/* parse_var's data on stack */
typedef struct fid_parse_var_locals {



  ast_off_t start;
} fid_parse_var_locals_t;





/* }}} */

/* parse_prop {{{ */
/* parse_prop's arguments */



typedef cr_zero_size_type_t fid_parse_prop_arg_t;

/* parse_prop's data on stack */




typedef cr_zero_size_type_t fid_parse_prop_locals_t;





/* }}} */

/* parse_dowhile {{{ */
/* parse_dowhile's arguments */



typedef cr_zero_size_type_t fid_parse_dowhile_arg_t;

/* parse_dowhile's data on stack */
typedef struct fid_parse_dowhile_locals {



  ast_off_t start;
  uint8_t saved_in_loop;
} fid_parse_dowhile_locals_t;





/* }}} */

/* parse_for {{{ */
/* parse_for's arguments */



typedef cr_zero_size_type_t fid_parse_for_arg_t;

/* parse_for's data on stack */
typedef struct fid_parse_for_locals {



  ast_off_t start;
  uint8_t saved_in_loop;
} fid_parse_for_locals_t;





/* }}} */

/* parse_try {{{ */
/* parse_try's arguments */



typedef cr_zero_size_type_t fid_parse_try_arg_t;

/* parse_try's data on stack */
typedef struct fid_parse_try_locals {



  ast_off_t start;
  uint8_t catch_or_finally;
} fid_parse_try_locals_t;





/* }}} */

/* parse_switch {{{ */
/* parse_switch's arguments */



typedef cr_zero_size_type_t fid_parse_switch_arg_t;

/* parse_switch's data on stack */
typedef struct fid_parse_switch_locals {



  ast_off_t start;
  int saved_in_switch;
  ast_off_t case_start;
} fid_parse_switch_locals_t;





/* }}} */

/* parse_with {{{ */
/* parse_with's arguments */



typedef cr_zero_size_type_t fid_parse_with_arg_t;

/* parse_with's data on stack */
typedef struct fid_parse_with_locals {



  ast_off_t start;
} fid_parse_with_locals_t;





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
    {((sizeof(fid_parse_script_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_script_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_script_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_script_1_catched_error_from_parse_use_strict */
    {((sizeof(fid_parse_script_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_script_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_script_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_script_2_uncatched_error */
    {((sizeof(fid_parse_script_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_script_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_script_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_script_3_after_parse_use_strict */
    {((sizeof(fid_parse_script_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_script_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_script_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_script_4_after_parse_body */
    {((sizeof(fid_parse_script_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_script_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_script_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_use_strict ----------------------------------------- */
    /* fid_parse_use_strict */
    {((sizeof(fid_parse_use_strict_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_use_strict_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_use_strict_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_body ----------------------------------------- */
    /* fid_parse_body */
    {((sizeof(fid_parse_body_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_body_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_body_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_body_1_after_parse_funcdecl */
    {((sizeof(fid_parse_body_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_body_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_body_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_body_2_after_parse_statement */
    {((sizeof(fid_parse_body_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_body_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_body_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_statement ----------------------------------------- */
    /* fid_parse_statement */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_1_after_parse_expression */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_2_after_parse_expression */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_3_after_parse_block */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_4_after_parse_if */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_5_after_parse_while */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_6_after_parse_expression */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_7_after_parse_ident */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_8_after_parse_ident */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_9_after_parse_var */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_10_after_parse_dowhile */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_11_after_parse_for */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_12_after_parse_try */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_13_after_parse_switch */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_stat_14_after_parse_with */
    {((sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_statement_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_expression ----------------------------------------- */
    /* fid_parse_expression */
    {((sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_expression_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_expr_1_after_parse_assign */
    {((sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_expression_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_assign ----------------------------------------- */
    /* fid_parse_assign */
    {((sizeof(fid_parse_assign_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_assign_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_assign_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_assign_1_after_parse_binary */
    {((sizeof(fid_parse_assign_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_assign_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_assign_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_binary ----------------------------------------- */
    /* fid_parse_binary */
    {((sizeof(fid_parse_binary_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_binary_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_binary_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_binary_1 */
    {((sizeof(fid_parse_binary_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_binary_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_binary_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_binary_2_after_parse_assign */
    {((sizeof(fid_parse_binary_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_binary_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_binary_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_binary_3_after_parse_assign */
    {((sizeof(fid_parse_binary_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_binary_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_binary_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_binary_4_after_parse_binary */
    {((sizeof(fid_parse_binary_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_binary_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_binary_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_binary_5_after_parse_binary */
    {((sizeof(fid_parse_binary_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_binary_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_binary_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_binary_6_after_parse_prefix */
    {((sizeof(fid_parse_binary_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_binary_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_binary_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_prefix ----------------------------------------- */
    /* fid_parse_prefix */
    {((sizeof(fid_parse_prefix_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_prefix_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_prefix_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_prefix_1_after_parse_postfix */
    {((sizeof(fid_parse_prefix_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_prefix_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_prefix_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_postfix ----------------------------------------- */
    /* fid_parse_postfix */
    {((sizeof(fid_parse_postfix_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_postfix_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_postfix_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_postfix_1_after_parse_callexpr */
    {((sizeof(fid_parse_postfix_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_postfix_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_postfix_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_callexpr ----------------------------------------- */
    /* fid_parse_callexpr */
    {((sizeof(fid_parse_callexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_callexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_callexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_callexpr_1_after_parse_newexpr */
    {((sizeof(fid_parse_callexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_callexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_callexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_callexpr_2_after_parse_arglist */
    {((sizeof(fid_parse_callexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_callexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_callexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_callexpr_3_after_parse_member */
    {((sizeof(fid_parse_callexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_callexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_callexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_newexpr ----------------------------------------- */
    /* fid_parse_newexpr */
    {((sizeof(fid_parse_newexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_newexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_newexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_newexpr_1_after_parse_terminal */
    {((sizeof(fid_parse_newexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_newexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_newexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_newexpr_2_after_parse_funcdecl */
    {((sizeof(fid_parse_newexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_newexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_newexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_newexpr_3_after_parse_memberexpr */
    {((sizeof(fid_parse_newexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_newexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_newexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_newexpr_4_after_parse_arglist */
    {((sizeof(fid_parse_newexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_newexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_newexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_terminal ----------------------------------------- */
    /* fid_parse_terminal */
    {((sizeof(fid_parse_terminal_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_terminal_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_terminal_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_terminal_1_after_parse_expression */
    {((sizeof(fid_parse_terminal_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_terminal_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_terminal_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_terminal_2_after_parse_assign */
    {((sizeof(fid_parse_terminal_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_terminal_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_terminal_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_terminal_3_after_parse_prop */
    {((sizeof(fid_parse_terminal_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_terminal_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_terminal_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_terminal_4_after_parse_ident */
    {((sizeof(fid_parse_terminal_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_terminal_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_terminal_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_block ----------------------------------------- */
    /* fid_parse_block */
    {((sizeof(fid_parse_block_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_block_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_block_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_block_1_after_parse_body */
    {((sizeof(fid_parse_block_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_block_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_block_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_if ----------------------------------------- */
    /* fid_parse_if */
    {((sizeof(fid_parse_if_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_if_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_if_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_if_1_after_parse_expression */
    {((sizeof(fid_parse_if_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_if_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_if_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_if_2_after_parse_statement */
    {((sizeof(fid_parse_if_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_if_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_if_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_if_3_after_parse_statement */
    {((sizeof(fid_parse_if_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_if_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_if_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_while ----------------------------------------- */
    /* fid_parse_while */
    {((sizeof(fid_parse_while_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_while_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_while_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_while_1_after_parse_expression */
    {((sizeof(fid_parse_while_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_while_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_while_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_while_2_after_parse_statement */
    {((sizeof(fid_parse_while_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_while_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_while_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_ident ----------------------------------------- */
    /* fid_parse_ident */
    {((sizeof(fid_parse_ident_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_ident_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_ident_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_ident_allow_reserved_words -------------------- */
    /* fid_parse_ident_allow_reserved_words */
    {((sizeof(fid_parse_ident_allow_reserved_words_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_ident_allow_reserved_words_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_ident_allow_reserved_words_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_ident_allow_reserved_words_1 */
    {((sizeof(fid_parse_ident_allow_reserved_words_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_ident_allow_reserved_words_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_ident_allow_reserved_words_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_funcdecl ----------------------------------------- */
    /* fid_parse_funcdecl */
    {((sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_funcdecl_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_funcdecl_1_after_parse_ident_allow_reserved_words */
    {((sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_funcdecl_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_funcdecl_2_catched_error_from_parse_ident_allow_reserved_words */
    {((sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_funcdecl_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_funcdecl_3_uncatched_error */
    {((sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_funcdecl_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_funcdecl_4_after_parse_arglist */
    {((sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_funcdecl_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_funcdecl_5_catched_error_from_parse_use_strict */
    {((sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_funcdecl_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_funcdecl_6_uncatched_error */
    {((sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_funcdecl_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_funcdecl_7_after_parse_use_strict */
    {((sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_funcdecl_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_funcdecl_8_after_parse_body */
    {((sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_funcdecl_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_funcdecl_9_after_parse_ident */
    {((sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_funcdecl_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_arglist ----------------------------------------- */
    /* fid_parse_arglist */
    {((sizeof(fid_parse_arglist_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_arglist_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_arglist_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_arglist_1_after_parse_assign */
    {((sizeof(fid_parse_arglist_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_arglist_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_arglist_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_member ----------------------------------------- */
    /* fid_parse_member */
    {((sizeof(fid_parse_member_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_member_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_member_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_member_1_after_parse_expression */
    {((sizeof(fid_parse_member_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_member_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_member_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_memberexpr ----------------------------------------- */
    /* fid_parse_memberexpr */
    {((sizeof(fid_parse_memberexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_memberexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_memberexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_memberexpr_1_after_parse_newexpr */
    {((sizeof(fid_parse_memberexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_memberexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_memberexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_memberexpr_2_after_parse_member */
    {((sizeof(fid_parse_memberexpr_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_memberexpr_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_memberexpr_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_var ----------------------------------------- */
    /* fid_parse_var */
    {((sizeof(fid_parse_var_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_var_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_var_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_var_1_after_parse_assign */
    {((sizeof(fid_parse_var_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_var_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_var_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_prop ----------------------------------------- */
    /* fid_parse_prop */
    {((sizeof(fid_parse_prop_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_prop_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_prop_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},




    /* fid_p_prop_2_after_parse_funcdecl */
    {((sizeof(fid_parse_prop_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_prop_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_prop_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},




    /* fid_p_prop_4_after_parse_assign */
    {((sizeof(fid_parse_prop_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_prop_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_prop_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_dowhile ----------------------------------------- */
    /* fid_parse_dowhile */
    {((sizeof(fid_parse_dowhile_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_dowhile_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_dowhile_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_dowhile_1_after_parse_statement */
    {((sizeof(fid_parse_dowhile_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_dowhile_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_dowhile_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_dowhile_2_after_parse_expression */
    {((sizeof(fid_parse_dowhile_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_dowhile_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_dowhile_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_for ----------------------------------------- */
    /* fid_parse_for */
    {((sizeof(fid_parse_for_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_for_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_for_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_for_1_after_parse_var */
    {((sizeof(fid_parse_for_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_for_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_for_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_for_2_after_parse_expression */
    {((sizeof(fid_parse_for_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_for_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_for_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_for_3_after_parse_expression */
    {((sizeof(fid_parse_for_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_for_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_for_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_for_4_after_parse_expression */
    {((sizeof(fid_parse_for_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_for_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_for_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_for_5_after_parse_expression */
    {((sizeof(fid_parse_for_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_for_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_for_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_for_6_after_parse_statement */
    {((sizeof(fid_parse_for_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_for_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_for_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_try ----------------------------------------- */
    /* fid_parse_try */
    {((sizeof(fid_parse_try_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_try_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_try_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_try_1_after_parse_block */
    {((sizeof(fid_parse_try_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_try_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_try_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_try_2_after_parse_ident */
    {((sizeof(fid_parse_try_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_try_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_try_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_try_3_after_parse_block */
    {((sizeof(fid_parse_try_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_try_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_try_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_try_4_after_parse_block */
    {((sizeof(fid_parse_try_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_try_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_try_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_switch ----------------------------------------- */
    /* fid_parse_switch */
    {((sizeof(fid_parse_switch_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_switch_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_switch_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_switch_1_after_parse_expression */
    {((sizeof(fid_parse_switch_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_switch_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_switch_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_switch_2_after_parse_expression */
    {((sizeof(fid_parse_switch_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_switch_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_switch_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_switch_3_after_parse_statement */
    {((sizeof(fid_parse_switch_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_switch_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_switch_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_switch_4_after_parse_statement */
    {((sizeof(fid_parse_switch_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_switch_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_switch_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

    /* fid_parse_with ----------------------------------------- */
    /* fid_parse_with */
    {((sizeof(fid_parse_with_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_with_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_with_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_with_1_after_parse_expression */
    {((sizeof(fid_parse_with_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_with_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_with_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},
    /* fid_p_with_2_after_parse_statement */
    {((sizeof(fid_parse_with_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_with_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_with_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))},

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


/*
 * Assumes `offset` points to the byte right after a tag
 */
static void insert_line_no_if_changed(struct v7 *v7, struct ast *a,
                                      ast_off_t offset) {
  if (v7->pstate.prev_line_no != v7->line_no) {
    v7->line_no = v7->pstate.prev_line_no;
    ast_add_line_no(a, offset - 1, v7->line_no);
  } else {






  }
}








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



static const struct {
  int len, left_to_right;
  struct {
    enum v7_tok start_tok, end_tok;
    enum ast_tag start_ast;
  } parts[2];
} levels[] = {
    {1, 0, {{TOK_ASSIGN, TOK_URSHIFT_ASSIGN, AST_ASSIGN}, { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }}},
    {1, 0, {{TOK_QUESTION, TOK_QUESTION, AST_COND}, { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }}},
    {1, 1, {{TOK_LOGICAL_OR, TOK_LOGICAL_OR, AST_LOGICAL_OR}, { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }}},
    {1, 1, {{TOK_LOGICAL_AND, TOK_LOGICAL_AND, AST_LOGICAL_AND}, { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }}},
    {1, 1, {{TOK_OR, TOK_OR, AST_OR}, { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }}},
    {1, 1, {{TOK_XOR, TOK_XOR, AST_XOR}, { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }}},
    {1, 1, {{TOK_AND, TOK_AND, AST_AND}, { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }}},
    {1, 1, {{TOK_EQ, TOK_NE_NE, AST_EQ}, { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }}},
    {2, 1, {{TOK_LE, TOK_GT, AST_LE}, {TOK_IN, TOK_INSTANCEOF, AST_IN}}},
    {1, 1, {{TOK_LSHIFT, TOK_URSHIFT, AST_LSHIFT}, { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }}},
    {1, 1, {{TOK_PLUS, TOK_MINUS, AST_ADD}, { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }}},
    {1, 1, {{TOK_REM, TOK_DIV, AST_REM}, { (enum v7_tok) 0, (enum v7_tok) 0, (enum ast_tag) 0 }}}};

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
  switch ((enum my_fid) *(((cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1)) {
    case fid_none: goto fid_none;

    case fid_parse_script: goto fid_parse_script;
    case fid_p_script_1_catched_error_from_parse_use_strict: goto fid_p_script_1_catched_error_from_parse_use_strict;
    case fid_p_script_2_uncatched_error: goto fid_p_script_2_uncatched_error;
    case fid_p_script_3_after_parse_use_strict: goto fid_p_script_3_after_parse_use_strict;
    case fid_p_script_4_after_parse_body: goto fid_p_script_4_after_parse_body;

    case fid_parse_use_strict: goto fid_parse_use_strict;

    case fid_parse_body: goto fid_parse_body;
    case fid_p_body_1_after_parse_funcdecl: goto fid_p_body_1_after_parse_funcdecl;
    case fid_p_body_2_after_parse_statement: goto fid_p_body_2_after_parse_statement;

    case fid_parse_statement: goto fid_parse_statement;
    case fid_p_stat_1_after_parse_expression: goto fid_p_stat_1_after_parse_expression;
    case fid_p_stat_2_after_parse_expression: goto fid_p_stat_2_after_parse_expression;
    case fid_p_stat_3_after_parse_block: goto fid_p_stat_3_after_parse_block;
    case fid_p_stat_4_after_parse_if: goto fid_p_stat_4_after_parse_if;
    case fid_p_stat_5_after_parse_while: goto fid_p_stat_5_after_parse_while;
    case fid_p_stat_6_after_parse_expression: goto fid_p_stat_6_after_parse_expression;
    case fid_p_stat_7_after_parse_ident: goto fid_p_stat_7_after_parse_ident;
    case fid_p_stat_8_after_parse_ident: goto fid_p_stat_8_after_parse_ident;
    case fid_p_stat_9_after_parse_var: goto fid_p_stat_9_after_parse_var;
    case fid_p_stat_10_after_parse_dowhile: goto fid_p_stat_10_after_parse_dowhile;
    case fid_p_stat_11_after_parse_for: goto fid_p_stat_11_after_parse_for;
    case fid_p_stat_12_after_parse_try: goto fid_p_stat_12_after_parse_try;
    case fid_p_stat_13_after_parse_switch: goto fid_p_stat_13_after_parse_switch;
    case fid_p_stat_14_after_parse_with: goto fid_p_stat_14_after_parse_with;

    case fid_parse_expression: goto fid_parse_expression;
    case fid_p_expr_1_after_parse_assign: goto fid_p_expr_1_after_parse_assign;

    case fid_parse_assign: goto fid_parse_assign;
    case fid_p_assign_1_after_parse_binary: goto fid_p_assign_1_after_parse_binary;

    case fid_parse_binary: goto fid_parse_binary;
    case fid_p_binary_2_after_parse_assign: goto fid_p_binary_2_after_parse_assign;
    case fid_p_binary_3_after_parse_assign: goto fid_p_binary_3_after_parse_assign;
    case fid_p_binary_4_after_parse_binary: goto fid_p_binary_4_after_parse_binary;
    case fid_p_binary_5_after_parse_binary: goto fid_p_binary_5_after_parse_binary;
    case fid_p_binary_6_after_parse_prefix: goto fid_p_binary_6_after_parse_prefix;

    case fid_parse_prefix: goto fid_parse_prefix;
    case fid_p_prefix_1_after_parse_postfix: goto fid_p_prefix_1_after_parse_postfix;

    case fid_parse_postfix: goto fid_parse_postfix;
    case fid_p_postfix_1_after_parse_callexpr: goto fid_p_postfix_1_after_parse_callexpr;

    case fid_parse_callexpr: goto fid_parse_callexpr;
    case fid_p_callexpr_1_after_parse_newexpr: goto fid_p_callexpr_1_after_parse_newexpr;
    case fid_p_callexpr_2_after_parse_arglist: goto fid_p_callexpr_2_after_parse_arglist;
    case fid_p_callexpr_3_after_parse_member: goto fid_p_callexpr_3_after_parse_member;

    case fid_parse_newexpr: goto fid_parse_newexpr;
    case fid_p_newexpr_1_after_parse_terminal: goto fid_p_newexpr_1_after_parse_terminal;
    case fid_p_newexpr_2_after_parse_funcdecl: goto fid_p_newexpr_2_after_parse_funcdecl;
    case fid_p_newexpr_3_after_parse_memberexpr: goto fid_p_newexpr_3_after_parse_memberexpr;
    case fid_p_newexpr_4_after_parse_arglist: goto fid_p_newexpr_4_after_parse_arglist;

    case fid_parse_terminal: goto fid_parse_terminal;
    case fid_p_terminal_1_after_parse_expression: goto fid_p_terminal_1_after_parse_expression;
    case fid_p_terminal_2_after_parse_assign: goto fid_p_terminal_2_after_parse_assign;
    case fid_p_terminal_3_after_parse_prop: goto fid_p_terminal_3_after_parse_prop;
    case fid_p_terminal_4_after_parse_ident: goto fid_p_terminal_4_after_parse_ident;

    case fid_parse_block: goto fid_parse_block;
    case fid_p_block_1_after_parse_body: goto fid_p_block_1_after_parse_body;

    case fid_parse_if: goto fid_parse_if;
    case fid_p_if_1_after_parse_expression: goto fid_p_if_1_after_parse_expression;
    case fid_p_if_2_after_parse_statement: goto fid_p_if_2_after_parse_statement;
    case fid_p_if_3_after_parse_statement: goto fid_p_if_3_after_parse_statement;

    case fid_parse_while: goto fid_parse_while;
    case fid_p_while_1_after_parse_expression: goto fid_p_while_1_after_parse_expression;
    case fid_p_while_2_after_parse_statement: goto fid_p_while_2_after_parse_statement;

    case fid_parse_ident: goto fid_parse_ident;

    case fid_parse_ident_allow_reserved_words: goto fid_parse_ident_allow_reserved_words;
    case fid_p_ident_arw_1_after_parse_ident: goto fid_p_ident_arw_1_after_parse_ident;

    case fid_parse_funcdecl: goto fid_parse_funcdecl;
    case fid_p_funcdecl_1_after_parse_ident_allow_reserved_words: goto fid_p_funcdecl_1_after_parse_ident_allow_reserved_words;
    case fid_p_funcdecl_2_catched_error_from_parse_ident_allow_reserved_words: goto fid_p_funcdecl_2_catched_error_from_parse_ident_allow_reserved_words;
    case fid_p_funcdecl_3_uncatched_error: goto fid_p_funcdecl_3_uncatched_error;
    case fid_p_funcdecl_4_after_parse_arglist: goto fid_p_funcdecl_4_after_parse_arglist;
    case fid_p_funcdecl_5_catched_error_from_parse_use_strict: goto fid_p_funcdecl_5_catched_error_from_parse_use_strict;
    case fid_p_funcdecl_6_uncatched_error: goto fid_p_funcdecl_6_uncatched_error;
    case fid_p_funcdecl_7_after_parse_use_strict: goto fid_p_funcdecl_7_after_parse_use_strict;
    case fid_p_funcdecl_8_after_parse_body: goto fid_p_funcdecl_8_after_parse_body;
    case fid_p_funcdecl_9_after_parse_ident: goto fid_p_funcdecl_9_after_parse_ident;

    case fid_parse_arglist: goto fid_parse_arglist;
    case fid_p_arglist_1_after_parse_assign: goto fid_p_arglist_1_after_parse_assign;

    case fid_parse_member: goto fid_parse_member;
    case fid_p_member_1_after_parse_expression: goto fid_p_member_1_after_parse_expression;

    case fid_parse_memberexpr: goto fid_parse_memberexpr;
    case fid_p_memberexpr_1_after_parse_newexpr: goto fid_p_memberexpr_1_after_parse_newexpr;
    case fid_p_memberexpr_2_after_parse_member: goto fid_p_memberexpr_2_after_parse_member;

    case fid_parse_var: goto fid_parse_var;
    case fid_p_var_1_after_parse_assign: goto fid_p_var_1_after_parse_assign;

    case fid_parse_prop: goto fid_parse_prop;



    case fid_p_prop_2_after_parse_funcdecl: goto fid_p_prop_2_after_parse_funcdecl;



    case fid_p_prop_4_after_parse_assign: goto fid_p_prop_4_after_parse_assign;

    case fid_parse_dowhile: goto fid_parse_dowhile;
    case fid_p_dowhile_1_after_parse_statement: goto fid_p_dowhile_1_after_parse_statement;
    case fid_p_dowhile_2_after_parse_expression: goto fid_p_dowhile_2_after_parse_expression;

    case fid_parse_for: goto fid_parse_for;
    case fid_p_for_1_after_parse_var: goto fid_p_for_1_after_parse_var;
    case fid_p_for_2_after_parse_expression: goto fid_p_for_2_after_parse_expression;
    case fid_p_for_3_after_parse_expression: goto fid_p_for_3_after_parse_expression;
    case fid_p_for_4_after_parse_expression: goto fid_p_for_4_after_parse_expression;
    case fid_p_for_5_after_parse_expression: goto fid_p_for_5_after_parse_expression;
    case fid_p_for_6_after_parse_statement: goto fid_p_for_6_after_parse_statement;

    case fid_parse_try: goto fid_parse_try;
    case fid_p_try_1_after_parse_block: goto fid_p_try_1_after_parse_block;
    case fid_p_try_2_after_parse_ident: goto fid_p_try_2_after_parse_ident;
    case fid_p_try_3_after_parse_block: goto fid_p_try_3_after_parse_block;
    case fid_p_try_4_after_parse_block: goto fid_p_try_4_after_parse_block;

    case fid_parse_switch: goto fid_parse_switch;
    case fid_p_switch_1_after_parse_expression: goto fid_p_switch_1_after_parse_expression;
    case fid_p_switch_2_after_parse_expression: goto fid_p_switch_2_after_parse_expression;
    case fid_p_switch_3_after_parse_statement: goto fid_p_switch_3_after_parse_statement;
    case fid_p_switch_4_after_parse_statement: goto fid_p_switch_4_after_parse_statement;

    case fid_parse_with: goto fid_parse_with;
    case fid_p_with_1_after_parse_expression: goto fid_p_with_1_after_parse_expression;
    case fid_p_with_2_after_parse_statement: goto fid_p_with_2_after_parse_statement;

    default:
      /* should never be here */
      printf("fatal: wrong func id: %d", *(((cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1));
      break;
  };

  /* static enum v7_err parse_script(struct v7 *v7, struct ast *a) */
fid_parse_script:


  {
    ( (fid_parse_script_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start = add_node( v7, a, AST_SCRIPT );
    ( (fid_parse_script_locals_t *) ( (p_ctx)->p_cur_func_locals) )->outer_last_var_node = v7->last_var_node;
    ( (fid_parse_script_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_strict = v7->pstate.in_strict;

    v7->last_var_node = ( (fid_parse_script_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start;
    ast_modify_skip( a, ( (fid_parse_script_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, ( (fid_parse_script_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_FUNC_FIRST_VAR_SKIP );

    do {
      do {
        mbuf_append( &( ( (p_ctx) )->stack_ret), ( (void *) 0), (2) );
      } while ( 0 );
      do {
        (p_ctx)->p_cur_func_locals = (uint8_t *) (p_ctx)->stack_data.buf + (p_ctx)->stack_data.len - ( (p_ctx)->p_func_descrs[*( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1)].locals_size);
      } while ( 0 );
      *( ( (uint8_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->stack_ret.len - 1) = CR_FID__TRY_MARKER;
      *( ( (uint8_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->stack_ret.len - 2) = (fid_p_script_1_catched_error_from_parse_use_strict);
    } while ( 0 );
    {
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1)	= (fid_p_script_3_after_parse_use_strict);
            (p_ctx)->called_fid = (fid_parse_use_strict);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_use_strict_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_use_strict_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_use_strict_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_use_strict_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_use_strict_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_script_3_after_parse_use_strict:
          ;
        } while ( 0 );
      } while ( 0 );
      v7->pstate.in_strict = 1;
    }
fid_p_script_1_catched_error_from_parse_use_strict:
    do {
      if ( (p_ctx)->thrown_exc != (PARSER_EXC_ID__SYNTAX_ERROR) ) {
        goto fid_p_script_2_uncatched_error;
      }
      (p_ctx)->thrown_exc = CR_EXC_ID__NONE;
    } while ( 0 );
fid_p_script_2_uncatched_error:
    do {
      (p_ctx)->stack_ret.len -= 2;
      if ( (p_ctx)->thrown_exc != CR_EXC_ID__NONE ) {
        goto _cr_iter_begin;
      }
    } while ( 0 );

    do {
      ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_body.end = (TOK_END_OF_INPUT);
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1)	= (fid_p_script_4_after_parse_body);
          (p_ctx)->called_fid = (fid_parse_body);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_body_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_body_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_body_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_body_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_body_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_script_4_after_parse_body:
        ;
      } while ( 0 );
    } while ( 0 );
    ast_set_skip( a, ( (fid_parse_script_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
    v7->pstate.in_strict = ( (fid_parse_script_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_strict;
    v7->last_var_node = ( (fid_parse_script_locals_t *) ( (p_ctx)->p_cur_func_locals) )->outer_last_var_node;

    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_use_strict(struct v7 *v7, struct ast *a) */
fid_parse_use_strict:


  {
    if ( v7->cur_tok == TOK_STRING_LITERAL &&
         (strncmp( v7->tok, "\"use strict\"", v7->tok_len ) == 0 ||
          strncmp( v7->tok, "'use strict'", v7->tok_len ) == 0) ) {
      next_tok( v7 );
      add_node( v7, a, AST_USE_STRICT );
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    } else {
      do {
        assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
        (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
        goto _cr_iter_begin;
      } while ( 0 );
    }
  }


  /*
   * static enum v7_err parse_body(struct v7 *v7, struct ast *a,
   *                               enum v7_tok end)
   */
fid_parse_body:


  {
    while ( v7->cur_tok != ( (fid_parse_body_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.end ) {
      if ( ( ( (v7)->cur_tok == (TOK_FUNCTION) ) ? next_tok( (v7) ), 1 : 0) ) {
        if ( v7->cur_tok != TOK_IDENTIFIER ) {
          do {
            assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
            (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
            goto _cr_iter_begin;
          } while ( 0 );
        }
        ( (fid_parse_body_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start = add_node( v7, a, AST_VAR );
        ast_modify_skip( a, v7->last_var_node, ( (fid_parse_body_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_FUNC_FIRST_VAR_SKIP );
        /* zero out var node pointer */
        ast_modify_skip( a, ( (fid_parse_body_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, ( (fid_parse_body_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_FUNC_FIRST_VAR_SKIP );
        v7->last_var_node = ( (fid_parse_body_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start;
        add_inlined_node( v7, a, AST_FUNC_DECL, v7->tok, v7->tok_len );

        do {
          ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_funcdecl.require_named	= (1);
          ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_funcdecl.reserved_name	= (0);
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1)	= (fid_p_body_1_after_parse_funcdecl);
              (p_ctx)->called_fid = (fid_parse_funcdecl);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_funcdecl_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_funcdecl_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_funcdecl_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_body_1_after_parse_funcdecl:
            ;
          } while ( 0 );
        } while ( 0 );
        ast_set_skip( a, ( (fid_parse_body_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
      } else {
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1)	= (fid_p_body_2_after_parse_statement);
              (p_ctx)->called_fid = (fid_parse_statement);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_statement_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_statement_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_statement_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_body_2_after_parse_statement :
            ;
          } while ( 0 );
        } while ( 0 );
      }
    }
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_statement(struct v7 *v7, struct ast *a) */
fid_parse_statement:


  {
    switch ( v7->cur_tok ) {
    case TOK_SEMICOLON:
      next_tok( v7 );
      /* empty statement */
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    case TOK_OPEN_CURLY: /* block */
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_3_after_parse_block);
            (p_ctx)->called_fid = (fid_parse_block);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_block_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_block_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_block_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_block_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_block_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_stat_3_after_parse_block :
          ;
        } while ( 0 );
      } while ( 0 );
      /* returning because no semicolon required */
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    case TOK_IF:
      next_tok( v7 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_4_after_parse_if);
            (p_ctx)->called_fid = (fid_parse_if);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_if_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_if_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_if_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_if_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_if_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_stat_4_after_parse_if :
          ;
        } while ( 0 );
      } while ( 0 );
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    case TOK_WHILE:
      next_tok( v7 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_5_after_parse_while);
            (p_ctx)->called_fid = (fid_parse_while);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_while_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_while_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_while_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_while_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_while_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_stat_5_after_parse_while :
          ;
        } while ( 0 );
      } while ( 0 );
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    case TOK_DO:
      next_tok( v7 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_10_after_parse_dowhile);
            (p_ctx)->called_fid = (fid_parse_dowhile);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_dowhile_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_dowhile_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_dowhile_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_dowhile_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_dowhile_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_stat_10_after_parse_dowhile :
          ;
        } while ( 0 );
      } while ( 0 );
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    case TOK_FOR:
      next_tok( v7 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_11_after_parse_for);
            (p_ctx)->called_fid = (fid_parse_for);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_for_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_for_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_for_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_for_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_for_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_stat_11_after_parse_for :
          ;
        } while ( 0 );
      } while ( 0 );
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    case TOK_TRY:
      next_tok( v7 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_12_after_parse_try);
            (p_ctx)->called_fid = (fid_parse_try);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_try_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_try_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_try_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_try_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_try_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_stat_12_after_parse_try :
          ;
        } while ( 0 );
      } while ( 0 );
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    case TOK_SWITCH:
      next_tok( v7 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_13_after_parse_switch);
            (p_ctx)->called_fid = (fid_parse_switch);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_switch_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_switch_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_switch_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_switch_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_switch_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_stat_13_after_parse_switch :
          ;
        } while ( 0 );
      } while ( 0 );
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    case TOK_WITH:
      next_tok( v7 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_14_after_parse_with);
            (p_ctx)->called_fid = (fid_parse_with);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_with_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_with_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_with_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_with_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_with_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_stat_14_after_parse_with :
          ;
        } while ( 0 );
      } while ( 0 );
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    case TOK_BREAK:
      if ( !(v7->pstate.in_loop || v7->pstate.in_switch) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
      do {
        if ( end_of_statement( v7 ) == V7_OK ) {
          add_node( v7, a, (AST_BREAK) );
        } else {
          add_node( v7, a, (AST_LABELED_BREAK) );
          do {
            do {
              do {
                *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_7_after_parse_ident);
                (p_ctx)->called_fid = (fid_parse_ident);
                (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_ident_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_ident_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_ident_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
                (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_ident_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_ident_arg_t) ) );
              } while ( 0 );
              goto _cr_iter_begin;
fid_p_stat_7_after_parse_ident :
              ;
            } while ( 0 );
          } while ( 0 );
        }
      } while ( 0 );
      break;
    case TOK_CONTINUE:
      if ( !v7->pstate.in_loop ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
      do {
        if ( end_of_statement( v7 ) == V7_OK ) {
          add_node( v7, a, (AST_CONTINUE) );
        } else {
          add_node( v7, a, (AST_LABELED_CONTINUE) );
          do {
            do {
              do {
                *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_8_after_parse_ident);
                (p_ctx)->called_fid = (fid_parse_ident);
                (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_ident_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_ident_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_ident_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
                (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_ident_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_ident_arg_t) ) );
              } while ( 0 );
              goto _cr_iter_begin;
fid_p_stat_8_after_parse_ident :
              ;
            } while ( 0 );
          } while ( 0 );
        }
      } while ( 0 );
      break;
    case TOK_RETURN:
      if ( !v7->pstate.in_function ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
      do {
        if ( end_of_statement( v7 ) == V7_OK ) {
          add_node( v7, a, (AST_RETURN) );
        } else {
          add_node( v7, a, (AST_VALUE_RETURN) );
          do {
            do {
              do {
                *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_6_after_parse_expression);
                (p_ctx)->called_fid = (fid_parse_expression);
                (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
                (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
              } while ( 0 );
              goto _cr_iter_begin;
fid_p_stat_6_after_parse_expression :
              ;
            } while ( 0 );
          } while ( 0 );
        }
      } while ( 0 );
      break;
    case TOK_THROW:
      next_tok( v7 );
      add_node( v7, a, AST_THROW );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_2_after_parse_expression);
            (p_ctx)->called_fid = (fid_parse_expression);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_stat_2_after_parse_expression :
          ;
        } while ( 0 );
      } while ( 0 );
      break;
    case TOK_DEBUGGER:
      next_tok( v7 );
      add_node( v7, a, AST_DEBUGGER );
      break;
    case TOK_VAR:
      next_tok( v7 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_9_after_parse_var);
            (p_ctx)->called_fid = (fid_parse_var);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_var_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_var_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_var_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_var_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_var_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_stat_9_after_parse_var :
          ;
        } while ( 0 );
      } while ( 0 );
      break;
    case TOK_IDENTIFIER:
      if ( lookahead( v7 ) == TOK_COLON ) {
        add_inlined_node( v7, a, AST_LABEL, v7->tok, v7->tok_len );
        next_tok( v7 );
        do {
          if ( (v7)->cur_tok != (TOK_COLON) ) {
            do {
              assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
              (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
              goto _cr_iter_begin;
            } while ( 0 );
          }
          next_tok( v7 );
        } while ( 0 );
        do {
          (p_ctx)->need_return = 1;
          goto _cr_iter_begin;
        } while ( 0 );
      }
      /* fall through */
    default:
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_stat_1_after_parse_expression);
            (p_ctx)->called_fid = (fid_parse_expression);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_stat_1_after_parse_expression :
          ;
        } while ( 0 );
      } while ( 0 );
      break;
    }

    if ( end_of_statement( v7 ) != V7_OK ) {
      do {
        assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
        (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
        goto _cr_iter_begin;
      } while ( 0 );
    }
    ( ( (v7)->cur_tok == (TOK_SEMICOLON) ) ? next_tok( (v7) ), 1 : 0); /* swallow optional semicolon */
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_expression(struct v7 *v7, struct ast *a) */
fid_parse_expression:


  {
    ( (fid_parse_expression_locals_t *) ( (p_ctx)->p_cur_func_locals) )->pos	= a->mbuf.len;
    ( (fid_parse_expression_locals_t *) ( (p_ctx)->p_cur_func_locals) )->group	= 0;
    while ( 1 ) {
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_expr_1_after_parse_assign);
            (p_ctx)->called_fid = (fid_parse_assign);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_assign_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_assign_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_assign_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_assign_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_assign_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_expr_1_after_parse_assign :
          ;
        } while ( 0 );
      } while ( 0 );
      if ( ( ( (v7)->cur_tok == (TOK_COMMA) ) ? next_tok( (v7) ), 1 : 0) ) {
        ( (fid_parse_expression_locals_t *) ( (p_ctx)->p_cur_func_locals) )->group = 1;
      } else {
        break;
      }
    }
    if ( ( (fid_parse_expression_locals_t *) ( (p_ctx)->p_cur_func_locals) )->group ) {
      insert_node( v7, a, ( (fid_parse_expression_locals_t *) ( (p_ctx)->p_cur_func_locals) )->pos, AST_SEQ );
    }
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_assign(struct v7 *v7, struct ast *a) */
fid_parse_assign:


  {
    do {
      ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_binary.min_level = (0);
      ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_binary.pos = (a->mbuf.len);
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_assign_1_after_parse_binary);
          (p_ctx)->called_fid = (fid_parse_binary);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_binary_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_binary_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_binary_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_binary_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_binary_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_assign_1_after_parse_binary :
        ;
      } while ( 0 );
    } while ( 0 );
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }


  /*
   * static enum v7_err parse_binary(struct v7 *v7, struct ast *a, int level,
   *                                 ast_off_t pos)
   */

fid_parse_binary:


  {
    /*
     * Note: we use macro CUR_POS instead of a local variable, since this
     * function is called really a lot, so, each byte on stack frame counts.
     *
     * It will work a bit slower of course, but slowness is not a problem
     */

    ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_mbuf_len = a->mbuf.len;

    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_binary_6_after_parse_prefix);
          (p_ctx)->called_fid = (fid_parse_prefix);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_prefix_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_prefix_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_prefix_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_prefix_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_prefix_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_binary_6_after_parse_prefix :
        ;
      } while ( 0 );
    } while ( 0 );

    for ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level = (int) (sizeof(levels) / sizeof(levels[0]) ) - 1; ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level >= ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.min_level;
          ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level-- ) {
      for ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->i = 0; ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->i < levels[( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level].len; ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->i++ ) {
        ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->tok	= levels[( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level].parts[( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->i].start_tok;
        ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->ast	= levels[( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level].parts[( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->i].start_ast;
        do {
          if ( v7->pstate.inhibit_in && ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->tok == TOK_IN ) {
            continue;
          }


          /*
           * Ternary operator sits in the middle of the binary operator
           * precedence chain. Deal with it as an exception and don't break
           * the chain.
           */
          if ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->tok == TOK_QUESTION && v7->cur_tok == TOK_QUESTION ) {
            next_tok( v7 );
            do {
              do {
                do {
                  *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_binary_2_after_parse_assign);
                  (p_ctx)->called_fid = (fid_parse_assign);
                  (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_assign_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_assign_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_assign_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
                  (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_assign_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_assign_arg_t) ) );
                } while ( 0 );
                goto _cr_iter_begin;
fid_p_binary_2_after_parse_assign :
                ;
              } while ( 0 );
            } while ( 0 );
            do {
              if ( (v7)->cur_tok != (TOK_COLON) ) {
                do {
                  assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
                  (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
                  goto _cr_iter_begin;
                } while ( 0 );
              }
              next_tok( v7 );
            } while ( 0 );
            do {
              do {
                do {
                  *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_binary_3_after_parse_assign);
                  (p_ctx)->called_fid = (fid_parse_assign);
                  (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_assign_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_assign_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_assign_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
                  (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_assign_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_assign_arg_t) ) );
                } while ( 0 );
                goto _cr_iter_begin;
fid_p_binary_3_after_parse_assign :
                ;
              } while ( 0 );
            } while ( 0 );
            insert_node( v7, a, ( ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level > ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.min_level) ? ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_mbuf_len : ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.pos), AST_COND );
            do {
              (p_ctx)->need_return = 1;
              goto _cr_iter_begin;
            } while ( 0 );
          } else if ( ( ( (v7)->cur_tok == ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->tok) ) ? next_tok( (v7) ), 1 : 0) ) {
            if ( levels[( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level].left_to_right ) {
              insert_node( v7, a, ( ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level > ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.min_level) ? ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_mbuf_len : ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.pos), (enum ast_tag) ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->ast );
              do {
                ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_binary.min_level = ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level);
                ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_binary.pos = ( ( ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level > ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.min_level) ? ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_mbuf_len : ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.pos) );
                do {
                  do {
                    *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_binary_4_after_parse_binary);
                    (p_ctx)->called_fid = (fid_parse_binary);
                    (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_binary_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_binary_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_binary_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
                    (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_binary_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_binary_arg_t) ) );
                  } while ( 0 );
                  goto _cr_iter_begin;
fid_p_binary_4_after_parse_binary :
                  ;
                } while ( 0 );
              } while ( 0 );
            } else {
              do {
                ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_binary.min_level = ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level);
                ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_binary.pos = (a->mbuf.len);
                do {
                  do {
                    *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_binary_5_after_parse_binary);
                    (p_ctx)->called_fid = (fid_parse_binary);
                    (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_binary_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_binary_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_binary_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
                    (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_binary_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_binary_arg_t) ) );
                  } while ( 0 );
                  goto _cr_iter_begin;
fid_p_binary_5_after_parse_binary :
                  ;
                } while ( 0 );
              } while ( 0 );
              insert_node( v7, a, ( ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level > ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.min_level) ? ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_mbuf_len : ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.pos), (enum ast_tag) ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->ast );
            }
          }
        } while ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->ast = (enum ast_tag) ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->ast + 1),
                  ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->tok < levels[( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->level].parts[( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->i].end_tok &&
                  ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->tok = (enum v7_tok) ( ( (fid_parse_binary_locals_t *) ( (p_ctx)->p_cur_func_locals) )->tok + 1) ) );
      }
    }

    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* enum v7_err parse_prefix(struct v7 *v7, struct ast *a) */
fid_parse_prefix:


  {
    for (;; ) {
      switch ( v7->cur_tok ) {
      case TOK_PLUS:
        next_tok( v7 );
        add_node( v7, a, AST_POSITIVE );
        break;
      case TOK_MINUS:
        next_tok( v7 );
        add_node( v7, a, AST_NEGATIVE );
        break;
      case TOK_PLUS_PLUS:
        next_tok( v7 );
        add_node( v7, a, AST_PREINC );
        break;
      case TOK_MINUS_MINUS:
        next_tok( v7 );
        add_node( v7, a, AST_PREDEC );
        break;
      case TOK_TILDA:
        next_tok( v7 );
        add_node( v7, a, AST_NOT );
        break;
      case TOK_NOT:
        next_tok( v7 );
        add_node( v7, a, AST_LOGICAL_NOT );
        break;
      case TOK_VOID:
        next_tok( v7 );
        add_node( v7, a, AST_VOID );
        break;
      case TOK_DELETE:
        next_tok( v7 );
        add_node( v7, a, AST_DELETE );
        break;
      case TOK_TYPEOF:
        next_tok( v7 );
        add_node( v7, a, AST_TYPEOF );
        break;
      default:
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_prefix_1_after_parse_postfix);
              (p_ctx)->called_fid = (fid_parse_postfix);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_postfix_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_postfix_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_postfix_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_postfix_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_postfix_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_prefix_1_after_parse_postfix :
            ;
          } while ( 0 );
        } while ( 0 );
        do {
          (p_ctx)->need_return = 1;
          goto _cr_iter_begin;
        } while ( 0 );
      }
    }
  }

  /* static enum v7_err parse_postfix(struct v7 *v7, struct ast *a) */
fid_parse_postfix:


  {
    ( (fid_parse_postfix_locals_t *) ( (p_ctx)->p_cur_func_locals) )->pos = a->mbuf.len;
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_postfix_1_after_parse_callexpr);
          (p_ctx)->called_fid = (fid_parse_callexpr);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_callexpr_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_callexpr_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_callexpr_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_callexpr_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_callexpr_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_postfix_1_after_parse_callexpr :
        ;
      } while ( 0 );
    } while ( 0 );

    if ( v7->after_newline ) {
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    }
    switch ( v7->cur_tok ) {
    case TOK_PLUS_PLUS:
      next_tok( v7 );
      insert_node( v7, a, ( (fid_parse_postfix_locals_t *) ( (p_ctx)->p_cur_func_locals) )->pos, AST_POSTINC );
      break;
    case TOK_MINUS_MINUS:
      next_tok( v7 );
      insert_node( v7, a, ( (fid_parse_postfix_locals_t *) ( (p_ctx)->p_cur_func_locals) )->pos, AST_POSTDEC );
      break;
    default:
      break; /* nothing */
    }
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_callexpr(struct v7 *v7, struct ast *a) */
fid_parse_callexpr:


  {
    ( (fid_parse_callexpr_locals_t *) ( (p_ctx)->p_cur_func_locals) )->pos = a->mbuf.len;
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_callexpr_1_after_parse_newexpr);
          (p_ctx)->called_fid = (fid_parse_newexpr);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_newexpr_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_newexpr_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_newexpr_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_newexpr_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_newexpr_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_callexpr_1_after_parse_newexpr :
        ;
      } while ( 0 );
    } while ( 0 );

    for (;; ) {
      switch ( v7->cur_tok ) {
      case TOK_DOT:
      case TOK_OPEN_BRACKET:
        do {
          ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_member.pos = ( ( (fid_parse_callexpr_locals_t *) ( (p_ctx)->p_cur_func_locals) )->pos);
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_callexpr_3_after_parse_member);
              (p_ctx)->called_fid = (fid_parse_member);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_member_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_member_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_member_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_member_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_member_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_callexpr_3_after_parse_member :
            ;
          } while ( 0 );
        } while ( 0 );
        break;
      case TOK_OPEN_PAREN:
        next_tok( v7 );
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_callexpr_2_after_parse_arglist);
              (p_ctx)->called_fid = (fid_parse_arglist);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_arglist_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_arglist_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_arglist_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_arglist_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_arglist_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_callexpr_2_after_parse_arglist :
            ;
          } while ( 0 );
        } while ( 0 );
        do {
          if ( (v7)->cur_tok != (TOK_CLOSE_PAREN) ) {
            do {
              assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
              (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
              goto _cr_iter_begin;
            } while ( 0 );
          }
          next_tok( v7 );
        } while ( 0 );
        insert_node( v7, a, ( (fid_parse_callexpr_locals_t *) ( (p_ctx)->p_cur_func_locals) )->pos, AST_CALL );
        break;
      default:
        do {
          (p_ctx)->need_return = 1;
          goto _cr_iter_begin;
        } while ( 0 );
      }
    }
  }

  /* static enum v7_err parse_newexpr(struct v7 *v7, struct ast *a) */
fid_parse_newexpr:


  {
    switch ( v7->cur_tok ) {
    case TOK_NEW:
      next_tok( v7 );
      ( (fid_parse_newexpr_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start = add_node( v7, a, AST_NEW );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_newexpr_3_after_parse_memberexpr);
            (p_ctx)->called_fid = (fid_parse_memberexpr);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_memberexpr_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_memberexpr_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_memberexpr_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_memberexpr_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_memberexpr_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_newexpr_3_after_parse_memberexpr :
          ;
        } while ( 0 );
      } while ( 0 );
      if ( ( ( (v7)->cur_tok == (TOK_OPEN_PAREN) ) ? next_tok( (v7) ), 1 : 0) ) {
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_newexpr_4_after_parse_arglist);
              (p_ctx)->called_fid = (fid_parse_arglist);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_arglist_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_arglist_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_arglist_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_arglist_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_arglist_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_newexpr_4_after_parse_arglist :
            ;
          } while ( 0 );
        } while ( 0 );
        do {
          if ( (v7)->cur_tok != (TOK_CLOSE_PAREN) ) {
            do {
              assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
              (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
              goto _cr_iter_begin;
            } while ( 0 );
          }
          next_tok( v7 );
        } while ( 0 );
      }
      ast_set_skip( a, ( (fid_parse_newexpr_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
      break;
    case TOK_FUNCTION:
      next_tok( v7 );
      do {
        ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_funcdecl.require_named = (0);
        ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_funcdecl.reserved_name = (0);
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_newexpr_2_after_parse_funcdecl);
            (p_ctx)->called_fid = (fid_parse_funcdecl);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_funcdecl_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_funcdecl_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_funcdecl_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_newexpr_2_after_parse_funcdecl :
          ;
        } while ( 0 );
      } while ( 0 );
      break;
    default:
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_newexpr_1_after_parse_terminal);
            (p_ctx)->called_fid = (fid_parse_terminal);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_terminal_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_terminal_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_terminal_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_terminal_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_terminal_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_newexpr_1_after_parse_terminal :
          ;
        } while ( 0 );
      } while ( 0 );
      break;
    }
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_terminal(struct v7 *v7, struct ast *a) */
fid_parse_terminal:


  {
    switch ( v7->cur_tok ) {
    case TOK_OPEN_PAREN:
      next_tok( v7 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_terminal_1_after_parse_expression);
            (p_ctx)->called_fid = (fid_parse_expression);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_terminal_1_after_parse_expression :
          ;
        } while ( 0 );
      } while ( 0 );
      do {
        if ( (v7)->cur_tok != (TOK_CLOSE_PAREN) ) {
          do {
            assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
            (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
            goto _cr_iter_begin;
          } while ( 0 );
        }
        next_tok( v7 );
      } while ( 0 );
      break;
    case TOK_OPEN_BRACKET:
      next_tok( v7 );
      ( (fid_parse_terminal_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start = add_node( v7, a, AST_ARRAY );
      while ( v7->cur_tok != TOK_CLOSE_BRACKET ) {
        if ( v7->cur_tok == TOK_COMMA ) {
          /* Array literals allow missing elements, e.g. [,,1,] */
          add_node( v7, a, AST_NOP );
        } else {
          do {
            do {
              do {
                *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_terminal_2_after_parse_assign);
                (p_ctx)->called_fid = (fid_parse_assign);
                (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_assign_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_assign_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_assign_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
                (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_assign_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_assign_arg_t) ) );
              } while ( 0 );
              goto _cr_iter_begin;
fid_p_terminal_2_after_parse_assign :
              ;
            } while ( 0 );
          } while ( 0 );
        }
        ( ( (v7)->cur_tok == (TOK_COMMA) ) ? next_tok( (v7) ), 1 : 0);
      }
      do {
        if ( (v7)->cur_tok != (TOK_CLOSE_BRACKET) ) {
          do {
            assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
            (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
            goto _cr_iter_begin;
          } while ( 0 );
        }
        next_tok( v7 );
      } while ( 0 );
      ast_set_skip( a, ( (fid_parse_terminal_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
      break;
    case TOK_OPEN_CURLY:
      next_tok( v7 );
      ( (fid_parse_terminal_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start = add_node( v7, a, AST_OBJECT );
      if ( v7->cur_tok != TOK_CLOSE_CURLY ) {
        do {
          if ( v7->cur_tok == TOK_CLOSE_CURLY ) {
            break;
          }
          do {
            do {
              do {
                *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_terminal_3_after_parse_prop);
                (p_ctx)->called_fid = (fid_parse_prop);
                (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_prop_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_prop_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_prop_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
                (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_prop_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_prop_arg_t) ) );
              } while ( 0 );
              goto _cr_iter_begin;
fid_p_terminal_3_after_parse_prop :
              ;
            } while ( 0 );
          } while ( 0 );
        } while ( ( ( (v7)->cur_tok == (TOK_COMMA) ) ? next_tok( (v7) ), 1 : 0) );
      }
      do {
        if ( (v7)->cur_tok != (TOK_CLOSE_CURLY) ) {
          do {
            assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
            (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
            goto _cr_iter_begin;
          } while ( 0 );
        }
        next_tok( v7 );
      } while ( 0 );
      ast_set_skip( a, ( (fid_parse_terminal_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
      break;
    case TOK_THIS:
      next_tok( v7 );
      add_node( v7, a, AST_THIS );
      break;
    case TOK_TRUE:
      next_tok( v7 );
      add_node( v7, a, AST_TRUE );
      break;
    case TOK_FALSE:
      next_tok( v7 );
      add_node( v7, a, AST_FALSE );
      break;
    case TOK_NULL:
      next_tok( v7 );
      add_node( v7, a, AST_NULL );
      break;
    case TOK_STRING_LITERAL:
      add_inlined_node( v7, a, AST_STRING, v7->tok + 1, v7->tok_len - 2 );
      next_tok( v7 );
      break;
    case TOK_NUMBER:
      add_inlined_node( v7, a, AST_NUM, v7->tok, v7->tok_len );
      next_tok( v7 );
      break;
    case TOK_REGEX_LITERAL:
      add_inlined_node( v7, a, AST_REGEX, v7->tok, v7->tok_len );
      next_tok( v7 );
      break;
    case TOK_IDENTIFIER:
      if ( v7->tok_len == 9 && strncmp( v7->tok, "undefined", v7->tok_len ) == 0 ) {
        add_node( v7, a, AST_UNDEFINED );
        next_tok( v7 );
        break;
      }
      /* fall through */
    default:
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_terminal_4_after_parse_ident);
            (p_ctx)->called_fid = (fid_parse_ident);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_ident_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_ident_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_ident_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_ident_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_ident_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_terminal_4_after_parse_ident :
          ;
        } while ( 0 );
      } while ( 0 );
    }
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_block(struct v7 *v7, struct ast *a) */
fid_parse_block:


  {
    do {
      if ( (v7)->cur_tok != (TOK_OPEN_CURLY) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_body.end = (TOK_CLOSE_CURLY);
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_block_1_after_parse_body);
          (p_ctx)->called_fid = (fid_parse_body);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_body_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_body_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_body_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_body_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_body_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_block_1_after_parse_body :
        ;
      } while ( 0 );
    } while ( 0 );
    do {
      if ( (v7)->cur_tok != (TOK_CLOSE_CURLY) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_if(struct v7 *v7, struct ast *a) */
fid_parse_if:


  {
    ( (fid_parse_if_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start = add_node( v7, a, AST_IF );
    do {
      if ( (v7)->cur_tok != (TOK_OPEN_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_if_1_after_parse_expression);
          (p_ctx)->called_fid = (fid_parse_expression);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_if_1_after_parse_expression :
        ;
      } while ( 0 );
    } while ( 0 );
    do {
      if ( (v7)->cur_tok != (TOK_CLOSE_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_if_2_after_parse_statement);
          (p_ctx)->called_fid = (fid_parse_statement);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_statement_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_statement_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_statement_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_if_2_after_parse_statement :
        ;
      } while ( 0 );
    } while ( 0 );
    ast_set_skip( a, ( (fid_parse_if_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_IF_TRUE_SKIP );
    if ( ( ( (v7)->cur_tok == (TOK_ELSE) ) ? next_tok( (v7) ), 1 : 0) ) {
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_if_3_after_parse_statement);
            (p_ctx)->called_fid = (fid_parse_statement);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_statement_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_statement_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_statement_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_if_3_after_parse_statement :
          ;
        } while ( 0 );
      } while ( 0 );
    }
    ast_set_skip( a, ( (fid_parse_if_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_while(struct v7 *v7, struct ast *a) */
fid_parse_while:


  {
    ( (fid_parse_while_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start		= add_node( v7, a, AST_WHILE );
    ( (fid_parse_while_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_loop	= v7->pstate.in_loop;
    do {
      if ( (v7)->cur_tok != (TOK_OPEN_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_while_1_after_parse_expression);
          (p_ctx)->called_fid = (fid_parse_expression);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_while_1_after_parse_expression :
        ;
      } while ( 0 );
    } while ( 0 );
    do {
      if ( (v7)->cur_tok != (TOK_CLOSE_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    v7->pstate.in_loop = 1;
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_while_2_after_parse_statement);
          (p_ctx)->called_fid = (fid_parse_statement);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_statement_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_statement_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_statement_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_while_2_after_parse_statement :
        ;
      } while ( 0 );
    } while ( 0 );
    ast_set_skip( a, ( (fid_parse_while_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
    v7->pstate.in_loop = ( (fid_parse_while_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_loop;
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_ident(struct v7 *v7, struct ast *a) */
fid_parse_ident:


  {
    if ( v7->cur_tok == TOK_IDENTIFIER ) {
      add_inlined_node( v7, a, AST_IDENT, v7->tok, v7->tok_len );
      next_tok( v7 );
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    }
    do {
      assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
      (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
      goto _cr_iter_begin;
    } while ( 0 );
  }


  /*
   * static enum v7_err parse_ident_allow_reserved_words(struct v7 *v7,
   *                                                     struct ast *a)
   *
   */
fid_parse_ident_allow_reserved_words:


  {
    /* Allow reserved words as property names. */
    if ( is_reserved_word_token( v7->cur_tok ) ) {
      add_inlined_node( v7, a, AST_IDENT, v7->tok, v7->tok_len );
      next_tok( v7 );
    } else {
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_ident_arw_1_after_parse_ident);
            (p_ctx)->called_fid = (fid_parse_ident);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_ident_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_ident_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_ident_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_ident_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_ident_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_ident_arw_1_after_parse_ident :
          ;
        } while ( 0 );
      } while ( 0 );
    }
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_funcdecl(struct v7 *, struct ast *, int, int) */
fid_parse_funcdecl:


  {
    ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start		= add_node( v7, a, AST_FUNC );
    ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->outer_last_var_node	= v7->last_var_node;
    ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_function	= v7->pstate.in_function;
    ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_strict	= v7->pstate.in_strict;

    v7->last_var_node = ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start;
    ast_modify_skip( a, ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_FUNC_FIRST_VAR_SKIP );

    do {
      do {
        mbuf_append( &( ( (p_ctx) )->stack_ret), ( (void *) 0), (2) );
      } while ( 0 );
      do {
        (p_ctx)->p_cur_func_locals = (uint8_t *) (p_ctx)->stack_data.buf + (p_ctx)->stack_data.len - ( (p_ctx)->p_func_descrs[*( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1)].locals_size);
      } while ( 0 );
      *( ( (uint8_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->stack_ret.len - 1) = CR_FID__TRY_MARKER;
      *( ( (uint8_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->stack_ret.len - 2) = (fid_p_funcdecl_2_catched_error_from_parse_ident_allow_reserved_words);
    } while ( 0 );
    {
      if ( ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.reserved_name ) {
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_funcdecl_1_after_parse_ident_allow_reserved_words);
              (p_ctx)->called_fid = (fid_parse_ident_allow_reserved_words);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_ident_allow_reserved_words_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_ident_allow_reserved_words_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_ident_allow_reserved_words_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_ident_allow_reserved_words_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_ident_allow_reserved_words_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_funcdecl_1_after_parse_ident_allow_reserved_words :
            ;
          } while ( 0 );
        } while ( 0 );
      } else {
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_funcdecl_9_after_parse_ident);
              (p_ctx)->called_fid = (fid_parse_ident);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_ident_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_ident_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_ident_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_ident_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_ident_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_funcdecl_9_after_parse_ident :
            ;
          } while ( 0 );
        } while ( 0 );
      }
    }
fid_p_funcdecl_2_catched_error_from_parse_ident_allow_reserved_words:
    do {
      if ( (p_ctx)->thrown_exc != (PARSER_EXC_ID__SYNTAX_ERROR) ) {
        goto fid_p_funcdecl_3_uncatched_error;
      }
      (p_ctx)->thrown_exc = CR_EXC_ID__NONE;
    } while ( 0 );
    {
      if ( ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.require_named ) {
        /* function name is required, so, rethrow SYNTAX ERROR */
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      } else {
        /* it's ok not to have a function name, just insert NOP */
        add_node( v7, a, AST_NOP );
      }
    }
fid_p_funcdecl_3_uncatched_error:
    do {
      (p_ctx)->stack_ret.len -= 2;
      if ( (p_ctx)->thrown_exc != CR_EXC_ID__NONE ) {
        goto _cr_iter_begin;
      }
    } while ( 0 );

    do {
      if ( (v7)->cur_tok != (TOK_OPEN_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_funcdecl_4_after_parse_arglist);
          (p_ctx)->called_fid = (fid_parse_arglist);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_arglist_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_arglist_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_arglist_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_arglist_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_arglist_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_funcdecl_4_after_parse_arglist :
        ;
      } while ( 0 );
    } while ( 0 );
    do {
      if ( (v7)->cur_tok != (TOK_CLOSE_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    ast_set_skip( a, ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_FUNC_BODY_SKIP );
    v7->pstate.in_function = 1;
    do {
      if ( (v7)->cur_tok != (TOK_OPEN_CURLY) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );

    do {
      do {
        mbuf_append( &( ( (p_ctx) )->stack_ret), ( (void *) 0), (2) );
      } while ( 0 );
      do {
        (p_ctx)->p_cur_func_locals = (uint8_t *) (p_ctx)->stack_data.buf + (p_ctx)->stack_data.len - ( (p_ctx)->p_func_descrs[*( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1)].locals_size);
      } while ( 0 );
      *( ( (uint8_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->stack_ret.len - 1) = CR_FID__TRY_MARKER;
      *( ( (uint8_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->stack_ret.len - 2) = (fid_p_funcdecl_5_catched_error_from_parse_use_strict);
    } while ( 0 );
    {
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_funcdecl_7_after_parse_use_strict);
            (p_ctx)->called_fid = (fid_parse_use_strict);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_use_strict_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_use_strict_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_use_strict_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_use_strict_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_use_strict_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_funcdecl_7_after_parse_use_strict :
          ;
        } while ( 0 );
      } while ( 0 );
      v7->pstate.in_strict = 1;
    }
fid_p_funcdecl_5_catched_error_from_parse_use_strict:
    do {
      if ( (p_ctx)->thrown_exc != (PARSER_EXC_ID__SYNTAX_ERROR) ) {
        goto fid_p_funcdecl_6_uncatched_error;
      }
      (p_ctx)->thrown_exc = CR_EXC_ID__NONE;
    } while ( 0 );
fid_p_funcdecl_6_uncatched_error:
    do {
      (p_ctx)->stack_ret.len -= 2;
      if ( (p_ctx)->thrown_exc != CR_EXC_ID__NONE ) {
        goto _cr_iter_begin;
      }
    } while ( 0 );

    do {
      ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_body.end = (TOK_CLOSE_CURLY);
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_funcdecl_8_after_parse_body);
          (p_ctx)->called_fid = (fid_parse_body);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_body_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_body_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_body_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_body_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_body_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_funcdecl_8_after_parse_body :
        ;
      } while ( 0 );
    } while ( 0 );
    do {
      if ( (v7)->cur_tok != (TOK_CLOSE_CURLY) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    v7->pstate.in_strict	= ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_strict;
    v7->pstate.in_function	= ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_function;
    ast_set_skip( a, ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
    v7->last_var_node = ( (fid_parse_funcdecl_locals_t *) ( (p_ctx)->p_cur_func_locals) )->outer_last_var_node;

    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_arglist(struct v7 *v7, struct ast *a) */
fid_parse_arglist:


  {
    if ( v7->cur_tok != TOK_CLOSE_PAREN ) {
      do {
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_arglist_1_after_parse_assign);
              (p_ctx)->called_fid = (fid_parse_assign);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_assign_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_assign_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_assign_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_assign_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_assign_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_arglist_1_after_parse_assign :
            ;
          } while ( 0 );
        } while ( 0 );
      } while ( ( ( (v7)->cur_tok == (TOK_COMMA) ) ? next_tok( (v7) ), 1 : 0) );
    }
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }


  /*
   * static enum v7_err parse_member(struct v7 *v7, struct ast *a, ast_off_t pos)
   */
fid_parse_member:


  {
    switch ( v7->cur_tok ) {
    case TOK_DOT:
      next_tok( v7 );
      /* Allow reserved words as member identifiers */
      if ( is_reserved_word_token( v7->cur_tok ) ||
           v7->cur_tok == TOK_IDENTIFIER ) {
        insert_inlined_node( v7, a, ( (fid_parse_member_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.pos, AST_MEMBER, v7->tok,
                             v7->tok_len );
        next_tok( v7 );
      } else {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      break;
    case TOK_OPEN_BRACKET:
      next_tok( v7 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_member_1_after_parse_expression);
            (p_ctx)->called_fid = (fid_parse_expression);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_member_1_after_parse_expression :
          ;
        } while ( 0 );
      } while ( 0 );
      do {
        if ( (v7)->cur_tok != (TOK_CLOSE_BRACKET) ) {
          do {
            assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
            (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
            goto _cr_iter_begin;
          } while ( 0 );
        }
        next_tok( v7 );
      } while ( 0 );
      insert_node( v7, a, ( (fid_parse_member_locals_t *) ( (p_ctx)->p_cur_func_locals) )->arg.pos, AST_INDEX );
      break;
    default:
      do {
        (p_ctx)->need_return = 1;
        goto _cr_iter_begin;
      } while ( 0 );
    }
    /* not necessary, but let it be anyway */
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_memberexpr(struct v7 *v7, struct ast *a) */
fid_parse_memberexpr:


  {
    ( (fid_parse_memberexpr_locals_t *) ( (p_ctx)->p_cur_func_locals) )->pos = a->mbuf.len;
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_memberexpr_1_after_parse_newexpr);
          (p_ctx)->called_fid = (fid_parse_newexpr);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_newexpr_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_newexpr_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_newexpr_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_newexpr_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_newexpr_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_memberexpr_1_after_parse_newexpr :
        ;
      } while ( 0 );
    } while ( 0 );

    for (;; ) {
      switch ( v7->cur_tok ) {
      case TOK_DOT:
      case TOK_OPEN_BRACKET:
        do {
          ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_member.pos = ( ( (fid_parse_memberexpr_locals_t *) ( (p_ctx)->p_cur_func_locals) )->pos);
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_memberexpr_2_after_parse_member);
              (p_ctx)->called_fid = (fid_parse_member);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_member_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_member_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_member_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_member_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_member_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_memberexpr_2_after_parse_member :
            ;
          } while ( 0 );
        } while ( 0 );
        break;
      default:
        do {
          (p_ctx)->need_return = 1;
          goto _cr_iter_begin;
        } while ( 0 );
      }
    }
  }

  /* static enum v7_err parse_var(struct v7 *v7, struct ast *a) */
fid_parse_var:


  {
    ( (fid_parse_var_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start = add_node( v7, a, AST_VAR );
    ast_modify_skip( a, v7->last_var_node, ( (fid_parse_var_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_FUNC_FIRST_VAR_SKIP );
    /* zero out var node pointer */
    ast_modify_skip( a, ( (fid_parse_var_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, ( (fid_parse_var_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_FUNC_FIRST_VAR_SKIP );
    v7->last_var_node = ( (fid_parse_var_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start;
    do {
      add_inlined_node( v7, a, AST_VAR_DECL, v7->tok, v7->tok_len );
      do {
        if ( (v7)->cur_tok != (TOK_IDENTIFIER) ) {
          do {
            assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
            (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
            goto _cr_iter_begin;
          } while ( 0 );
        }
        next_tok( v7 );
      } while ( 0 );
      if ( ( ( (v7)->cur_tok == (TOK_ASSIGN) ) ? next_tok( (v7) ), 1 : 0) ) {
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_var_1_after_parse_assign);
              (p_ctx)->called_fid = (fid_parse_assign);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_assign_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_assign_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_assign_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_assign_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_assign_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_var_1_after_parse_assign :
            ;
          } while ( 0 );
        } while ( 0 );
      } else {
        add_node( v7, a, AST_NOP );
      }
    } while ( ( ( (v7)->cur_tok == (TOK_COMMA) ) ? next_tok( (v7) ), 1 : 0) );
    ast_set_skip( a, ( (fid_parse_var_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_prop(struct v7 *v7, struct ast *a) */
fid_parse_prop:


  {
    if ( v7->cur_tok == TOK_IDENTIFIER && lookahead( v7 ) == TOK_OPEN_PAREN ) {
      /* ecmascript 6 feature */
      do {
        ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_funcdecl.require_named = (1);
        ( ( (p_ctx)->p_arg_retval)->arg).fid_parse_funcdecl.reserved_name = (1);
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_prop_2_after_parse_funcdecl);
            (p_ctx)->called_fid = (fid_parse_funcdecl);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_funcdecl_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_funcdecl_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_funcdecl_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_funcdecl_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_funcdecl_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_prop_2_after_parse_funcdecl :
          ;
        } while ( 0 );
      } while ( 0 );
    } else {
      /* Allow reserved words as property names. */
      if ( is_reserved_word_token( v7->cur_tok ) || v7->cur_tok == TOK_IDENTIFIER ||
           v7->cur_tok == TOK_NUMBER ) {
        add_inlined_node( v7, a, AST_PROP, v7->tok, v7->tok_len );
      } else if ( v7->cur_tok == TOK_STRING_LITERAL ) {
        add_inlined_node( v7, a, AST_PROP, v7->tok + 1, v7->tok_len - 2 );
      } else {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
      do {
        if ( (v7)->cur_tok != (TOK_COLON) ) {
          do {
            assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
            (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
            goto _cr_iter_begin;
          } while ( 0 );
        }
        next_tok( v7 );
      } while ( 0 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_prop_4_after_parse_assign);
            (p_ctx)->called_fid = (fid_parse_assign);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_assign_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_assign_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_assign_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_assign_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_assign_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_prop_4_after_parse_assign :
          ;
        } while ( 0 );
      } while ( 0 );
    }
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_dowhile(struct v7 *v7, struct ast *a) */
fid_parse_dowhile:


  {
    ( (fid_parse_dowhile_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start		= add_node( v7, a, AST_DOWHILE );
    ( (fid_parse_dowhile_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_loop = v7->pstate.in_loop;

    v7->pstate.in_loop = 1;
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_dowhile_1_after_parse_statement);
          (p_ctx)->called_fid = (fid_parse_statement);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_statement_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_statement_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_statement_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_dowhile_1_after_parse_statement :
        ;
      } while ( 0 );
    } while ( 0 );
    v7->pstate.in_loop = ( (fid_parse_dowhile_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_loop;
    ast_set_skip( a, ( (fid_parse_dowhile_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_DO_WHILE_COND_SKIP );
    do {
      if ( (v7)->cur_tok != (TOK_WHILE) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      if ( (v7)->cur_tok != (TOK_OPEN_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_dowhile_2_after_parse_expression);
          (p_ctx)->called_fid = (fid_parse_expression);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_dowhile_2_after_parse_expression :
        ;
      } while ( 0 );
    } while ( 0 );
    do {
      if ( (v7)->cur_tok != (TOK_CLOSE_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    ast_set_skip( a, ( (fid_parse_dowhile_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_for(struct v7 *v7, struct ast *a) */
fid_parse_for:


  {
    /* TODO(mkm): for of, for each in */


    /*
     * ast_off_t start;
     * int saved_in_loop;
     */

    ( (fid_parse_for_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start		= add_node( v7, a, AST_FOR );
    ( (fid_parse_for_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_loop	= v7->pstate.in_loop;

    do {
      if ( (v7)->cur_tok != (TOK_OPEN_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );

    if ( parse_optional( v7, a, TOK_SEMICOLON ) ) {
      /*
       * TODO(mkm): make this reentrant otherwise this pearl won't parse:
       * for((function(){return 1 in o.a ? o : x})().a in [1,2,3])
       */
      v7->pstate.inhibit_in = 1;
      if ( ( ( (v7)->cur_tok == (TOK_VAR) ) ? next_tok( (v7) ), 1 : 0) ) {
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_for_1_after_parse_var);
              (p_ctx)->called_fid = (fid_parse_var);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_var_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_var_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_var_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_var_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_var_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_for_1_after_parse_var :
            ;
          } while ( 0 );
        } while ( 0 );
      } else {
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_for_2_after_parse_expression);
              (p_ctx)->called_fid = (fid_parse_expression);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_for_2_after_parse_expression :
            ;
          } while ( 0 );
        } while ( 0 );
      }
      v7->pstate.inhibit_in = 0;

      if ( ( ( (v7)->cur_tok == (TOK_IN) ) ? next_tok( (v7) ), 1 : 0) ) {
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_for_3_after_parse_expression);
              (p_ctx)->called_fid = (fid_parse_expression);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_for_3_after_parse_expression :
            ;
          } while ( 0 );
        } while ( 0 );
        add_node( v7, a, AST_NOP );


        /*
         * Assumes that for and for in have the same AST format which is
         * suboptimal but avoids the need of fixing up the var offset chain.
         * TODO(mkm) improve this
         */
        ast_modify_tag( a, ( (fid_parse_for_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start - 1, AST_FOR_IN );
        goto for_loop_body;
      }
    }

    do {
      if ( (v7)->cur_tok != (TOK_SEMICOLON) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    if ( parse_optional( v7, a, TOK_SEMICOLON ) ) {
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_for_4_after_parse_expression);
            (p_ctx)->called_fid = (fid_parse_expression);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_for_4_after_parse_expression :
          ;
        } while ( 0 );
      } while ( 0 );
    }
    do {
      if ( (v7)->cur_tok != (TOK_SEMICOLON) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    if ( parse_optional( v7, a, TOK_CLOSE_PAREN ) ) {
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_for_5_after_parse_expression);
            (p_ctx)->called_fid = (fid_parse_expression);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_for_5_after_parse_expression :
          ;
        } while ( 0 );
      } while ( 0 );
    }

for_loop_body:
    do {
      if ( (v7)->cur_tok != (TOK_CLOSE_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    ast_set_skip( a, ( (fid_parse_for_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_FOR_BODY_SKIP );
    v7->pstate.in_loop = 1;
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_for_6_after_parse_statement);
          (p_ctx)->called_fid = (fid_parse_statement);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_statement_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_statement_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_statement_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_for_6_after_parse_statement :
        ;
      } while ( 0 );
    } while ( 0 );
    v7->pstate.in_loop = ( (fid_parse_for_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_loop;
    ast_set_skip( a, ( (fid_parse_for_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_try(struct v7 *v7, struct ast *a) */
fid_parse_try:


  {
    ( (fid_parse_try_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start		= add_node( v7, a, AST_TRY );
    ( (fid_parse_try_locals_t *) ( (p_ctx)->p_cur_func_locals) )->catch_or_finally	= 0;
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_try_1_after_parse_block);
          (p_ctx)->called_fid = (fid_parse_block);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_block_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_block_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_block_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_block_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_block_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_try_1_after_parse_block :
        ;
      } while ( 0 );
    } while ( 0 );
    ast_set_skip( a, ( (fid_parse_try_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_TRY_CATCH_SKIP );
    if ( ( ( (v7)->cur_tok == (TOK_CATCH) ) ? next_tok( (v7) ), 1 : 0) ) {
      ( (fid_parse_try_locals_t *) ( (p_ctx)->p_cur_func_locals) )->catch_or_finally = 1;
      do {
        if ( (v7)->cur_tok != (TOK_OPEN_PAREN) ) {
          do {
            assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
            (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
            goto _cr_iter_begin;
          } while ( 0 );
        }
        next_tok( v7 );
      } while ( 0 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_try_2_after_parse_ident);
            (p_ctx)->called_fid = (fid_parse_ident);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_ident_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_ident_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_ident_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_ident_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_ident_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_try_2_after_parse_ident :
          ;
        } while ( 0 );
      } while ( 0 );
      do {
        if ( (v7)->cur_tok != (TOK_CLOSE_PAREN) ) {
          do {
            assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
            (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
            goto _cr_iter_begin;
          } while ( 0 );
        }
        next_tok( v7 );
      } while ( 0 );
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_try_3_after_parse_block);
            (p_ctx)->called_fid = (fid_parse_block);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_block_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_block_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_block_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_block_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_block_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_try_3_after_parse_block :
          ;
        } while ( 0 );
      } while ( 0 );
    }
    ast_set_skip( a, ( (fid_parse_try_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_TRY_FINALLY_SKIP );
    if ( ( ( (v7)->cur_tok == (TOK_FINALLY) ) ? next_tok( (v7) ), 1 : 0) ) {
      ( (fid_parse_try_locals_t *) ( (p_ctx)->p_cur_func_locals) )->catch_or_finally = 1;
      do {
        do {
          do {
            *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_try_4_after_parse_block);
            (p_ctx)->called_fid = (fid_parse_block);
            (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_block_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_block_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_block_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
            (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_block_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_block_arg_t) ) );
          } while ( 0 );
          goto _cr_iter_begin;
fid_p_try_4_after_parse_block :
          ;
        } while ( 0 );
      } while ( 0 );
    }
    ast_set_skip( a, ( (fid_parse_try_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );

    /* make sure `catch` and `finally` aren't both missing */
    if ( !( (fid_parse_try_locals_t *) ( (p_ctx)->p_cur_func_locals) )->catch_or_finally ) {
      do {
        assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
        (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
        goto _cr_iter_begin;
      } while ( 0 );
    }

    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_switch(struct v7 *v7, struct ast *a) */
fid_parse_switch:


  {
    ( (fid_parse_switch_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start			= add_node( v7, a, AST_SWITCH );
    ( (fid_parse_switch_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_switch	= v7->pstate.in_switch;

    ast_set_skip( a, ( (fid_parse_switch_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_SWITCH_DEFAULT_SKIP ); /* clear out */
    do {
      if ( (v7)->cur_tok != (TOK_OPEN_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_switch_1_after_parse_expression);
          (p_ctx)->called_fid = (fid_parse_expression);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_switch_1_after_parse_expression :
        ;
      } while ( 0 );
    } while ( 0 );
    do {
      if ( (v7)->cur_tok != (TOK_CLOSE_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      if ( (v7)->cur_tok != (TOK_OPEN_CURLY) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    v7->pstate.in_switch = 1;
    while ( v7->cur_tok != TOK_CLOSE_CURLY ) {
      switch ( v7->cur_tok ) {
      case TOK_CASE:
        next_tok( v7 );
        ( (fid_parse_switch_locals_t *) ( (p_ctx)->p_cur_func_locals) )->case_start = add_node( v7, a, AST_CASE );
        do {
          do {
            do {
              *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_switch_2_after_parse_expression);
              (p_ctx)->called_fid = (fid_parse_expression);
              (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
              (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
            } while ( 0 );
            goto _cr_iter_begin;
fid_p_switch_2_after_parse_expression :
            ;
          } while ( 0 );
        } while ( 0 );
        do {
          if ( (v7)->cur_tok != (TOK_COLON) ) {
            do {
              assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
              (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
              goto _cr_iter_begin;
            } while ( 0 );
          }
          next_tok( v7 );
        } while ( 0 );
        while ( v7->cur_tok != TOK_CASE && v7->cur_tok != TOK_DEFAULT &&
                v7->cur_tok != TOK_CLOSE_CURLY ) {
          do {
            do {
              do {
                *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_switch_3_after_parse_statement);
                (p_ctx)->called_fid = (fid_parse_statement);
                (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_statement_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
                (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_statement_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_statement_arg_t) ) );
              } while ( 0 );
              goto _cr_iter_begin;
fid_p_switch_3_after_parse_statement :
              ;
            } while ( 0 );
          } while ( 0 );
        }
        ast_set_skip( a, ( (fid_parse_switch_locals_t *) ( (p_ctx)->p_cur_func_locals) )->case_start, AST_END_SKIP );
        break;
      case TOK_DEFAULT:
        next_tok( v7 );
        do {
          if ( (v7)->cur_tok != (TOK_COLON) ) {
            do {
              assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
              (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
              goto _cr_iter_begin;
            } while ( 0 );
          }
          next_tok( v7 );
        } while ( 0 );
        ast_set_skip( a, ( (fid_parse_switch_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_SWITCH_DEFAULT_SKIP );
        ( (fid_parse_switch_locals_t *) ( (p_ctx)->p_cur_func_locals) )->case_start = add_node( v7, a, AST_DEFAULT );
        while ( v7->cur_tok != TOK_CASE && v7->cur_tok != TOK_DEFAULT &&
                v7->cur_tok != TOK_CLOSE_CURLY ) {
          do {
            do {
              do {
                *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_switch_4_after_parse_statement);
                (p_ctx)->called_fid = (fid_parse_statement);
                (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_statement_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
                (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_statement_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_statement_arg_t) ) );
              } while ( 0 );
              goto _cr_iter_begin;
fid_p_switch_4_after_parse_statement :
              ;
            } while ( 0 );
          } while ( 0 );
        }
        ast_set_skip( a, ( (fid_parse_switch_locals_t *) ( (p_ctx)->p_cur_func_locals) )->case_start, AST_END_SKIP );
        break;
      default:
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
    }
    do {
      if ( (v7)->cur_tok != (TOK_CLOSE_CURLY) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    ast_set_skip( a, ( (fid_parse_switch_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
    v7->pstate.in_switch = ( (fid_parse_switch_locals_t *) ( (p_ctx)->p_cur_func_locals) )->saved_in_switch;
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

  /* static enum v7_err parse_with(struct v7 *v7, struct ast *a) */
fid_parse_with:


  {
    ( (fid_parse_with_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start = add_node( v7, a, AST_WITH );
    if ( v7->pstate.in_strict ) {
      do {
        assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
        (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
        goto _cr_iter_begin;
      } while ( 0 );
    }
    do {
      if ( (v7)->cur_tok != (TOK_OPEN_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_with_1_after_parse_expression);
          (p_ctx)->called_fid = (fid_parse_expression);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_expression_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_expression_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_expression_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_expression_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_expression_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_with_1_after_parse_expression :
        ;
      } while ( 0 );
    } while ( 0 );
    do {
      if ( (v7)->cur_tok != (TOK_CLOSE_PAREN) ) {
        do {
          assert( PARSER_EXC_ID__SYNTAX_ERROR != CR_EXC_ID__NONE );
          (p_ctx)->thrown_exc = (PARSER_EXC_ID__SYNTAX_ERROR);
          goto _cr_iter_begin;
        } while ( 0 );
      }
      next_tok( v7 );
    } while ( 0 );
    do {
      do {
        do {
          *( ( (cr_locals_size_t *) (p_ctx)->stack_ret.buf) + (p_ctx)->cur_fid_idx - 1) = (fid_p_with_2_after_parse_statement);
          (p_ctx)->called_fid = (fid_parse_statement);
          (p_ctx)->call_locals_size = ( ( (sizeof(fid_parse_statement_locals_t) == sizeof(cr_zero_size_type_t) ) ? 0 : (sizeof(fid_parse_statement_locals_t) <= ( (cr_locals_size_t) -1) ? ( (cr_locals_size_t) ( ( (sizeof(fid_parse_statement_locals_t) ) + (sizeof(void *) - 1) ) & (~(sizeof(void *) - 1) ) ) ) : ( (cr_locals_size_t) -1) ) ) );
          (p_ctx)->call_arg_size = ( ( (sizeof(fid_parse_statement_arg_t) == sizeof(cr_zero_size_type_t) ) ? 0 : sizeof(fid_parse_statement_arg_t) ) );
        } while ( 0 );
        goto _cr_iter_begin;
fid_p_with_2_after_parse_statement :
        ;
      } while ( 0 );
    } while ( 0 );
    ast_set_skip( a, ( (fid_parse_with_locals_t *) ( (p_ctx)->p_cur_func_locals) )->start, AST_END_SKIP );
    do {
      (p_ctx)->need_return = 1;
      goto _cr_iter_begin;
    } while ( 0 );
  }

fid_none:
  /* stack is empty, so we're done; return */
  return rc;
}

 enum v7_err parse(struct v7 *v7, struct ast *a, const char *src,
                             size_t src_len, int is_json) {
  enum v7_err rcode;
  const char *error_msg = ((void *)0);
  const char *p;
  struct cr_ctx cr_ctx;
  union user_arg_ret arg_retval;
  enum cr_status rc;


  int saved_line_no = v7->line_no;




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
    do { *(((cr_locals_size_t *) (&cr_ctx)->stack_ret.buf) + (&cr_ctx)->cur_fid_idx - 1) = (CR_FID__NONE); (&cr_ctx)->called_fid = (fid_parse_terminal); (&cr_ctx)->call_locals_size = (((sizeof(fid_parse_terminal_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_terminal_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_terminal_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))); (&cr_ctx)->call_arg_size = (((sizeof(fid_parse_terminal_arg_t) == sizeof(cr_zero_size_type_t)) ? 0 : sizeof(fid_parse_terminal_arg_t))); } while (0);
  } else {
    do { *(((cr_locals_size_t *) (&cr_ctx)->stack_ret.buf) + (&cr_ctx)->cur_fid_idx - 1) = (CR_FID__NONE); (&cr_ctx)->called_fid = (fid_parse_script); (&cr_ctx)->call_locals_size = (((sizeof(fid_parse_script_locals_t) == sizeof(cr_zero_size_type_t)) ? 0 : (sizeof(fid_parse_script_locals_t) <= ((cr_locals_size_t) -1) ? ((cr_locals_size_t)(((sizeof(fid_parse_script_locals_t)) + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))) : ((cr_locals_size_t) -1)))); (&cr_ctx)->call_arg_size = (((sizeof(fid_parse_script_arg_t) == sizeof(cr_zero_size_type_t)) ? 0 : sizeof(fid_parse_script_arg_t))); } while (0);
  }

  /* proceed to coroutine execution */
  rc = parser_cr_exec(&cr_ctx, v7, a);

  /* set `rcode` depending on coroutine state */
  switch (rc) {
    case CR_RES__OK:
      rcode = V7_OK;
      break;
    case CR_RES__ERR_UNCAUGHT_EXCEPTION:
      switch ((enum parser_exc_id) ((&cr_ctx)->thrown_exc)) {
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







































  /* free resources occupied by context (at least, "stack" arrays) */
  cr_context_free(&cr_ctx);









  /* Check if AST was overflown */
  if (a->has_overflow) {
    rcode = v7_throwf(v7, "SyntaxError",
                      "Script too large (try V7_LARGE_AST build option)");
    do { (void) v7; rcode = (V7_AST_TOO_LARGE); (void)( (!!(rcode != V7_OK)) || (_wassert(L"rcode != V7_OK", L"e:\\myproject\\scriptools\\unpacket_v7\\v7\\src\\parser.c", 2599), 0) ); (void)( (!!(!v7_is_undefined(v7->vals.thrown_error) && v7->is_thrown)) || (_wassert(L"!v7_is_undefined(v7->vals.thrown_error) && v7->is_thrown", L"e:\\myproject\\scriptools\\unpacket_v7\\v7\\src\\parser.c", 2599), 0) ); goto clean; } while (0);
  }

  if (rcode == V7_OK && v7->cur_tok != TOK_END_OF_INPUT) {
    rcode = V7_SYNTAX_ERROR;
    error_msg = "Syntax error";
  }

  if (rcode != V7_OK) {
    unsigned long col = get_column(v7->pstate.source_code, v7->tok);
    int line_len = 0;

    (void)( (!!(error_msg != ((void *)0))) || (_wassert(L"error_msg != NULL", L"e:\\myproject\\scriptools\\unpacket_v7\\v7\\src\\parser.c", 2611), 0) );

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
      _tmp = v7_throwf(v7, "SyntaxError", "%s at line %d col %lu:\n%.*s\n%*s^",
                       error_msg, v7->pstate.line_no, col, line_len,
                       v7->tok - col, (int) col - 1, "");
      (void) _tmp;
    }
    do { (void) v7; rcode = (rcode); (void)( (!!(rcode != V7_OK)) || (_wassert(L"rcode != V7_OK", L"e:\\myproject\\scriptools\\unpacket_v7\\v7\\src\\parser.c", 2641), 0) ); (void)( (!!(!v7_is_undefined(v7->vals.thrown_error) && v7->is_thrown)) || (_wassert(L"!v7_is_undefined(v7->vals.thrown_error) && v7->is_thrown", L"e:\\myproject\\scriptools\\unpacket_v7\\v7\\src\\parser.c", 2641), 0) ); goto clean; } while (0);
  }

clean:
  v7->line_no = saved_line_no;
  return rcode;
}

