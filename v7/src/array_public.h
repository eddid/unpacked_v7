/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Arrays
 */

#ifndef CS_V7_SRC_ARRAY_PUBLIC_H_
#define CS_V7_SRC_ARRAY_PUBLIC_H_

#include "v7/src/core_public.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Make an empty array object */
v7_val_t v7_mk_array(struct v7 *v7);

/* Returns true if given value is an array object */
int v7_is_array(struct v7 *v7, v7_val_t v);

/* Returns length on an array. If `arr` is not an array, 0 is returned. */
unsigned long v7_array_length(struct v7 *v7, v7_val_t arr);

/* Insert value `v` in array `arr` at the end of the array. */
int v7_array_push(struct v7 *, v7_val_t arr, v7_val_t v);

/*
 * Like `v7_array_push()`, but "returns" value through the `res` pointer
 * argument. `res` is allowed to be `NULL`.
 *
 * Caller should check the error code returned, and if it's something other
 * than `V7_OK`, perform cleanup and return this code further.
 */
WARN_UNUSED_RESULT
enum v7_err v7_array_push_throwing(struct v7 *v7, v7_val_t arr, v7_val_t v,
                                   int *res);

/*
 * Return array member at index `index`. If `index` is out of bounds, undefined
 * is returned.
 */
v7_val_t v7_array_get(struct v7 *, v7_val_t arr, unsigned long index);

/* Insert value `v` into `arr` at index `index`. */
int v7_array_set(struct v7 *v7, v7_val_t arr, unsigned long index, v7_val_t v);

/*
 * Like `v7_array_set()`, but "returns" value through the `res` pointer
 * argument. `res` is allowed to be `NULL`.
 *
 * Caller should check the error code returned, and if it's something other
 * than `V7_OK`, perform cleanup and return this code further.
 */
WARN_UNUSED_RESULT
enum v7_err v7_array_set_throwing(struct v7 *v7, v7_val_t arr,
                                  unsigned long index, v7_val_t v, int *res);

/* Delete value in array `arr` at index `index`, if it exists. */
void v7_array_del(struct v7 *v7, v7_val_t arr, unsigned long index);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_ARRAY_PUBLIC_H_ */
