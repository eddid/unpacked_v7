/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_STDLIB_H_
#define CS_V7_SRC_STDLIB_H_

#include "v7/src/internal.h"
#include "v7/src/core.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

V7_PRIVATE void init_stdlib(struct v7 *v7);

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err std_eval(struct v7 *v7, v7_val_t arg, v7_val_t this_obj,
                                int is_json, v7_val_t *res);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_STDLIB_H_ */
