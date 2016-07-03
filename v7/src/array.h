/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_ARRAY_H_
#define CS_V7_SRC_ARRAY_H_

#include "v7/src/array_public.h"

#include "v7/src/core.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

V7_PRIVATE v7_val_t v7_mk_dense_array(struct v7 *v7);
V7_PRIVATE val_t
v7_array_get2(struct v7 *v7, v7_val_t arr, unsigned long index, int *has);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_ARRAY_H_ */
