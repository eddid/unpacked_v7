/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_STD_OBJECT_H_
#define CS_V7_SRC_STD_OBJECT_H_

#include "v7/src/internal.h"
#include "v7/src/core.h"

struct v7;

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

V7_PRIVATE void init_object(struct v7 *v7);

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_valueOf(struct v7 *v7, v7_val_t *res);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_STD_OBJECT_H_ */
