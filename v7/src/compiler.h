/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_COMPILER_H_
#define CS_V7_SRC_COMPILER_H_

#include "v7/src/internal.h"
#include "v7/src/bcode.h"
#include "v7/src/ast.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

V7_PRIVATE enum v7_err compile_script(struct v7 *v7, struct ast *a,
                                      struct bcode *bcode);

V7_PRIVATE enum v7_err compile_expr(struct v7 *v7, struct ast *a,
                                    ast_off_t *ppos, struct bcode *bcode);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_COMPILER_H_ */
