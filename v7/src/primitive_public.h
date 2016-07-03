/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Primitives
 *
 * All primitive values but strings.
 *
 * "foreign" values are also here, see `v7_mk_foreign()`.
 */

#ifndef CS_V7_SRC_PRIMITIVE_PUBLIC_H_
#define CS_V7_SRC_PRIMITIVE_PUBLIC_H_

#include "v7/src/core_public.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Make numeric primitive value */
NOINSTR v7_val_t v7_mk_number(struct v7 *v7, double num);

/*
 * Returns number value stored in `v7_val_t` as `double`.
 *
 * Returns NaN for non-numbers.
 */
NOINSTR double v7_get_double(struct v7 *v7, v7_val_t v);

/*
 * Returns number value stored in `v7_val_t` as `int`. If the number value is
 * not an integer, the fraction part will be discarded.
 *
 * If the given value is a non-number, or NaN, the result is undefined.
 */
NOINSTR int v7_get_int(struct v7 *v7, v7_val_t v);

/* Returns true if given value is a primitive number value */
int v7_is_number(v7_val_t v);

/* Make boolean primitive value (either `true` or `false`) */
NOINSTR v7_val_t v7_mk_boolean(struct v7 *v7, int is_true);

/*
 * Returns boolean stored in `v7_val_t`:
 *  0 for `false` or non-boolean, non-0 for `true`
 */
NOINSTR int v7_get_bool(struct v7 *v7, v7_val_t v);

/* Returns true if given value is a primitive boolean value */
int v7_is_boolean(v7_val_t v);

/*
 * Make `null` primitive value.
 *
 * NOTE: this function is deprecated and will be removed in future releases.
 * Use `V7_NULL` instead.
 */
NOINSTR v7_val_t v7_mk_null(void);

/* Returns true if given value is a primitive `null` value */
int v7_is_null(v7_val_t v);

/*
 * Make `undefined` primitive value.
 *
 * NOTE: this function is deprecated and will be removed in future releases.
 * Use `V7_UNDEFINED` instead.
 */
NOINSTR v7_val_t v7_mk_undefined(void);

/* Returns true if given value is a primitive `undefined` value */
int v7_is_undefined(v7_val_t v);

/*
 * Make JavaScript value that holds C/C++ `void *` pointer.
 *
 * A foreign value is completely opaque and JS code cannot do anything useful
 * with it except holding it in properties and passing it around.
 * It behaves like a sealed object with no properties.
 *
 * NOTE:
 * Only valid pointers (as defined by each supported architecture) will fully
 * preserved. In particular, all supported 64-bit architectures (x86_64, ARM-64)
 * actually define a 48-bit virtual address space.
 * Foreign values will be sign-extended as required, i.e creating a foreign
 * value of something like `(void *) -1` will work as expected. This is
 * important because in some 64-bit OSs (e.g. Solaris) the user stack grows
 * downwards from the end of the address space.
 *
 * If you need to store exactly sizeof(void*) bytes of raw data where
 * `sizeof(void*)` >= 8, please use byte arrays instead.
 */
NOINSTR v7_val_t v7_mk_foreign(struct v7 *v7, void *ptr);

/*
 * Returns `void *` pointer stored in `v7_val_t`.
 *
 * Returns NULL if the value is not a foreign pointer.
 */
NOINSTR void *v7_get_ptr(struct v7 *v7, v7_val_t v);

/* Returns true if given value holds `void *` pointer */
int v7_is_foreign(v7_val_t v);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_PRIMITIVE_PUBLIC_H_ */
