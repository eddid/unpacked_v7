/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_FUNCTION_H_
#define CS_V7_SRC_FUNCTION_H_

#include "v7/src/function_public.h"

#include "v7/src/internal.h"
#include "v7/src/core.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

V7_PRIVATE struct v7_js_function *get_js_function_struct(val_t v);
V7_PRIVATE val_t
mk_js_function(struct v7 *v7, struct v7_generic_object *scope, val_t proto);
V7_PRIVATE int is_js_function(val_t v);
V7_PRIVATE v7_val_t mk_cfunction_lite(v7_cfunction_t *f);

/* Returns true if given value holds a pointer to C callback */
V7_PRIVATE int is_cfunction_lite(v7_val_t v);

/* Returns true if given value holds an object which represents C callback */
V7_PRIVATE int is_cfunction_obj(struct v7 *v7, v7_val_t v);

/*
 * Returns `v7_cfunction_t *` callback pointer stored in `v7_val_t`, or NULL
 * if given value is neither cfunction pointer nor cfunction object.
 */
V7_PRIVATE v7_cfunction_t *get_cfunction_ptr(struct v7 *v7, v7_val_t v);

/*
 * Like v7_mk_function but also sets the function's `length` property.
 *
 * The `length` property is useful for introspection and the stdlib defines it
 * for many core functions mostly because the ECMA test suite requires it and we
 * don't want to skip otherwise useful tests just because the `length` property
 * check fails early in the test. User defined functions don't need to specify
 * the length and passing -1 is a safe choice, as it will also reduce the
 * footprint.
 *
 * The subtle difference between set `length` explicitly to 0 rather than
 * just defaulting the `0` value from the prototype is that in the former case
 * the property cannot be change since it's read only. This again, is important
 * only for ecma compliance and your user code might or might not find this
 * relevant.
 *
 * NODO(lsm): please don't combine v7_mk_function_arg and v7_mk_function
 * into one function. Currently `num_args` is useful only internally. External
 * users can just use `v7_def` to set the length.
 */
V7_PRIVATE
v7_val_t mk_cfunction_obj(struct v7 *v7, v7_cfunction_t *func, int num_args);

/*
 * Like v7_mk_function_with_proto but also sets the function's `length`
 *property.
 *
 * NODO(lsm): please don't combine mk_cfunction_obj_with_proto and
 * v7_mk_function_with_proto.
 * into one function. Currently `num_args` is useful only internally. External
 * users can just use `v7_def` to set the length.
 */
V7_PRIVATE
v7_val_t mk_cfunction_obj_with_proto(struct v7 *v7, v7_cfunction_t *f,
                                     int num_args, v7_val_t proto);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_FUNCTION_H_ */
