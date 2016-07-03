/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_OBJECT_H_
#define CS_V7_SRC_OBJECT_H_

#include "v7/src/object_public.h"

#include "v7/src/internal.h"
#include "v7/src/core.h"

V7_PRIVATE val_t mk_object(struct v7 *v7, val_t prototype);
V7_PRIVATE val_t v7_object_to_value(struct v7_object *o);
V7_PRIVATE struct v7_generic_object *get_generic_object_struct(val_t v);

/*
 * Returns pointer to the struct representing an object.
 * Given value must be an object (the caller can verify it
 * by calling `v7_is_object()`)
 */
V7_PRIVATE struct v7_object *get_object_struct(v7_val_t v);

/*
 * Return true if given value is a JavaScript object (will return
 * false for function)
 */
V7_PRIVATE int v7_is_generic_object(v7_val_t v);

V7_PRIVATE struct v7_property *v7_mk_property(struct v7 *v7);

V7_PRIVATE struct v7_property *v7_get_own_property2(struct v7 *v7, val_t obj,
                                                    const char *name,
                                                    size_t len,
                                                    v7_prop_attr_t attrs);

V7_PRIVATE struct v7_property *v7_get_own_property(struct v7 *v7, val_t obj,
                                                   const char *name,
                                                   size_t len);

/*
 * If `len` is -1/MAXUINT/~0, then `name` must be 0-terminated
 *
 * Returns a pointer to the property structure, given an object and a name of
 * the property as a pointer to string buffer and length.
 *
 * See also `v7_get_property_v`
 */
V7_PRIVATE struct v7_property *v7_get_property(struct v7 *v7, val_t obj,
                                               const char *name, size_t len);

/*
 * Just like `v7_get_property`, but takes name as a `v7_val_t`
 */
V7_PRIVATE enum v7_err v7_get_property_v(struct v7 *v7, val_t obj,
                                         v7_val_t name,
                                         struct v7_property **res);

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err v7_get_throwing_v(struct v7 *v7, v7_val_t obj,
                                         v7_val_t name, v7_val_t *res);

V7_PRIVATE void v7_destroy_property(struct v7_property **p);

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err v7_invoke_setter(struct v7 *v7, struct v7_property *prop,
                                        val_t obj, val_t val);

/*
 * Like `set_property`, but takes property name as a `val_t`
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err set_property_v(struct v7 *v7, val_t obj, val_t name,
                                      val_t val, struct v7_property **res);

/*
 * Like JavaScript assignment: set a property with given `name` + `len` at
 * the object `obj` to value `val`. Returns a property through the `res`
 * (which may be `NULL` if return value is not required)
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err set_property(struct v7 *v7, val_t obj, const char *name,
                                    size_t len, v7_val_t val,
                                    struct v7_property **res);

/*
 * Like `def_property()`, but takes property name as a `val_t`
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err def_property_v(struct v7 *v7, val_t obj, val_t name,
                                      v7_prop_attr_desc_t attrs_desc, val_t val,
                                      uint8_t as_assign,
                                      struct v7_property **res);

/*
 * Define object property, similar to JavaScript `Object.defineProperty()`.
 *
 * Just like public `v7_def()`, but returns `enum v7_err`, and therefore can
 * throw.
 *
 * Additionally, takes param `as_assign`: if it is non-zero, it behaves
 * similarly to plain JavaScript assignment in terms of some exception-related
 * corner cases.
 *
 * `res` may be `NULL`.
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err def_property(struct v7 *v7, val_t obj, const char *name,
                                    size_t len, v7_prop_attr_desc_t attrs_desc,
                                    v7_val_t val, uint8_t as_assign,
                                    struct v7_property **res);

V7_PRIVATE int set_method(struct v7 *v7, val_t obj, const char *name,
                          v7_cfunction_t *func, int num_args);

V7_PRIVATE int set_cfunc_prop(struct v7 *v7, val_t o, const char *name,
                              v7_cfunction_t *func);

/* Return address of property value or NULL if the passed property is NULL */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err v7_property_value(struct v7 *v7, val_t obj,
                                         struct v7_property *p, val_t *res);

/*
 * Set new prototype `proto` for the given object `obj`. Returns `0` at
 * success, `-1` at failure (it may fail if given `obj` is a function object:
 * it's impossible to change function object's prototype)
 */
V7_PRIVATE int obj_prototype_set(struct v7 *v7, struct v7_object *obj,
                                 struct v7_object *proto);

/*
 * Given a pointer to the object structure, returns a
 * pointer to the prototype object, or `NULL` if there is
 * no prototype.
 */
V7_PRIVATE struct v7_object *obj_prototype(struct v7 *v7,
                                           struct v7_object *obj);

V7_PRIVATE val_t obj_prototype_v(struct v7 *v7, val_t obj);

V7_PRIVATE int is_prototype_of(struct v7 *v7, val_t o, val_t p);

/* Get the property holding user data and destructor, or NULL */
V7_PRIVATE struct v7_property *get_user_data_property(v7_val_t obj);

#endif /* CS_V7_SRC_OBJECT_H_ */
