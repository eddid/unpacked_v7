/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_PARSER_H_
#define CS_V7_SRC_PARSER_H_

#include "v7/src/internal.h"
#include "v7/src/core_public.h"

struct v7;
struct ast;

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

struct v7_pstate {
  const char *file_name;
  const char *source_code;
  const char *pc;      /* Current parsing position */
  const char *src_end; /* End of source code */
  int line_no;         /* Line number */
  int prev_line_no;    /* Line number of previous token */
  int inhibit_in;      /* True while `in` expressions are inhibited */
  int in_function;     /* True if in a function */
  int in_loop;         /* True if in a loop */
  int in_switch;       /* True if in a switch block */
  int in_strict;       /* True if in strict mode */
};

V7_PRIVATE enum v7_err parse(struct v7 *v7, struct ast *a, const char *src,
                             size_t src_len, int is_json);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_PARSER_H_ */
