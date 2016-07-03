/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_STD_STRING_H_
#define CS_V7_SRC_STD_STRING_H_

#include "v7/src/internal.h"
#include "v7/src/core.h"

/* Max captures for String.replace() */
#define V7_RE_MAX_REPL_SUB 20

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

V7_PRIVATE void init_string(struct v7 *v7);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_STD_STRING_H_ */
