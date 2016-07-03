/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Functions
 */

#ifndef CS_V7_SRC_FUNCTION_PUBLIC_H_
#define CS_V7_SRC_FUNCTION_PUBLIC_H_

#include "v7/src/core_public.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*
 * Make a JS function object backed by a cfunction.
 *
 * `func` is a C callback.
 *
 * A function object is JS object having the Function prototype that holds a
 * cfunction value in a hidden property.
 *
 * The function object will have a `prototype` property holding an object that
 * will be used as the prototype of objects created when calling the function
 * with the `new` operator.
 */
v7_val_t v7_mk_function(struct v7 *, v7_cfunction_t *func);

/*
 * Make f a JS function with specified prototype `proto`, so that the resulting
 * function is better suited for the usage as a constructor.
 */
v7_val_t v7_mk_function_with_proto(struct v7 *v7, v7_cfunction_t *f,
                                   v7_val_t proto);

/*
 * Make a JS value that holds C/C++ callback pointer.
 *
 * CAUTION: This is a low-level function value. It's not a real object and
 * cannot hold user defined properties. You should use `v7_mk_function` unless
 * you know what you're doing.
 */
v7_val_t v7_mk_cfunction(v7_cfunction_t *func);

/*
 * Returns true if given value is callable (i.e. it's either a JS function or
 * cfunction)
 */
int v7_is_callable(struct v7 *v7, v7_val_t v);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_FUNCTION_PUBLIC_H_ */
