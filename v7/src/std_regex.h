/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_STD_REGEX_H_
#define CS_V7_SRC_STD_REGEX_H_

#include "v7/src/internal.h"
#include "v7/src/core.h"

#if V7_ENABLE__RegExp

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

V7_PRIVATE enum v7_err Regex_ctor(struct v7 *v7, v7_val_t *res);
V7_PRIVATE enum v7_err rx_exec(struct v7 *v7, v7_val_t rx, v7_val_t vstr,
                               int lind, v7_val_t *res);

V7_PRIVATE void init_regex(struct v7 *v7);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* V7_ENABLE__RegExp */

#endif /* CS_V7_SRC_STD_REGEX_H_ */
