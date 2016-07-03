/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Objects
 */

#ifndef CS_V7_SRC_OBJECT_PUBLIC_H_
#define CS_V7_SRC_OBJECT_PUBLIC_H_

#include "v7/src/core_public.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*
 * Property attributes bitmask
 */
typedef unsigned short v7_prop_attr_t;
#define V7_PROPERTY_NON_WRITABLE (1 << 0)
#define V7_PROPERTY_NON_ENUMERABLE (1 << 1)
#define V7_PROPERTY_NON_CONFIGURABLE (1 << 2)
#define V7_PROPERTY_GETTER (1 << 3)
#define V7_PROPERTY_SETTER (1 << 4)
#define _V7_PROPERTY_HIDDEN (1 << 5)
/* property not managed by V7 HEAP */
#define _V7_PROPERTY_OFF_HEAP (1 << 6)
/* special property holding user data and destructor cb */
#define _V7_PROPERTY_USER_DATA_AND_DESTRUCTOR (1 << 7)
/*
 * not a property attribute, but a flag for `v7_def()`. It's here in order to
 * keep all offsets in one place
 */
#define _V7_DESC_PRESERVE_VALUE (1 << 8)

/*
 * Internal helpers for `V7_DESC_...` macros
 */
#define _V7_DESC_SHIFT 16
#define _V7_DESC_MASK ((1 << _V7_DESC_SHIFT) - 1)
#define _V7_MK_DESC(v, n) \
  (((v7_prop_attr_desc_t)(n)) << _V7_DESC_SHIFT | ((v) ? (n) : 0))
#define _V7_MK_DESC_INV(v, n) _V7_MK_DESC(!(v), (n))

/*
 * Property attribute descriptors that may be given to `v7_def()`: for each
 * attribute (`v7_prop_attr_t`), there is a corresponding macro, which takes
 * param: either 1 (set attribute) or 0 (clear attribute). If some particular
 * attribute isn't mentioned at all, it's left unchanged (or default, if the
 * property is being created)
 *
 * There is additional flag: `V7_DESC_PRESERVE_VALUE`. If it is set, the
 * property value isn't changed (or set to `undefined` if the property is being
 * created)
 */
typedef unsigned long v7_prop_attr_desc_t;
#define V7_DESC_WRITABLE(v) _V7_MK_DESC_INV(v, V7_PROPERTY_NON_WRITABLE)
#define V7_DESC_ENUMERABLE(v) _V7_MK_DESC_INV(v, V7_PROPERTY_NON_ENUMERABLE)
#define V7_DESC_CONFIGURABLE(v) _V7_MK_DESC_INV(v, V7_PROPERTY_NON_CONFIGURABLE)
#define V7_DESC_GETTER(v) _V7_MK_DESC(v, V7_PROPERTY_GETTER)
#define V7_DESC_SETTER(v) _V7_MK_DESC(v, V7_PROPERTY_SETTER)
#define V7_DESC_PRESERVE_VALUE _V7_DESC_PRESERVE_VALUE

#define _V7_DESC_HIDDEN(v) _V7_MK_DESC(v, _V7_PROPERTY_HIDDEN)
#define _V7_DESC_OFF_HEAP(v) _V7_MK_DESC(v, _V7_PROPERTY_OFF_HEAP)

/* See `v7_set_destructor_cb` */
typedef void(v7_destructor_cb_t)(void *ud);

/* Make an empty object */
v7_val_t v7_mk_object(struct v7 *v7);

/*
 * Returns true if the given value is an object or function.
 * i.e. it returns true if the value holds properties and can be
 * used as argument to `v7_get`, `v7_set` and `v7_def`.
 */
int v7_is_object(v7_val_t v);

/* Set object's prototype. Return old prototype or undefined on error. */
v7_val_t v7_set_proto(struct v7 *v7, v7_val_t obj, v7_val_t proto);

/*
 * Lookup property `name` in object `obj`. If `obj` holds no such property,
 * an `undefined` value is returned.
 *
 * If `name_len` is ~0, `name` is assumed to be NUL-terminated and
 * `strlen(name)` is used.
 */
v7_val_t v7_get(struct v7 *v7, v7_val_t obj, const char *name, size_t name_len);

/*
 * Like `v7_get()`, but "returns" value through `res` pointer argument.
 * `res` must not be `NULL`.
 *
 * Caller should check the error code returned, and if it's something other
 * than `V7_OK`, perform cleanup and return this code further.
 */
WARN_UNUSED_RESULT
enum v7_err v7_get_throwing(struct v7 *v7, v7_val_t obj, const char *name,
                            size_t name_len, v7_val_t *res);

/*
 * Define object property, similar to JavaScript `Object.defineProperty()`.
 *
 * `name`, `name_len` specify property name, `val` is a property value.
 * `attrs_desc` is a set of flags which can affect property's attributes,
 * see comment of `v7_prop_attr_desc_t` for details.
 *
 * If `name_len` is ~0, `name` is assumed to be NUL-terminated and
 * `strlen(name)` is used.
 *
 * Returns non-zero on success, 0 on error (e.g. out of memory).
 *
 * See also `v7_set()`.
 */
int v7_def(struct v7 *v7, v7_val_t obj, const char *name, size_t name_len,
           v7_prop_attr_desc_t attrs_desc, v7_val_t v);

/*
 * Set object property. Behaves just like JavaScript assignment.
 *
 * See also `v7_def()`.
 */
int v7_set(struct v7 *v7, v7_val_t obj, const char *name, size_t len,
           v7_val_t val);

/*
 * A helper function to define object's method backed by a C function `func`.
 * `name` must be NUL-terminated.
 *
 * Return value is the same as for `v7_set()`.
 */
int v7_set_method(struct v7 *, v7_val_t obj, const char *name,
                  v7_cfunction_t *func);

/*
 * Delete own property `name` of the object `obj`. Does not follow the
 * prototype chain.
 *
 * If `name_len` is ~0, `name` is assumed to be NUL-terminated and
 * `strlen(name)` is used.
 *
 * Returns 0 on success, -1 on error.
 */
int v7_del(struct v7 *v7, v7_val_t obj, const char *name, size_t name_len);

/*
 * Iterate over the `obj`'s properties.
 *
 * Usage example:
 *
 *     void *h = NULL;
 *     v7_val_t name, val;
 *     v7_prop_attr_t attrs;
 *     while ((h = v7_next_prop(h, obj, &name, &val, &attrs)) != NULL) {
 *       ...
 *     }
 */
void *v7_next_prop(void *handle, v7_val_t obj, v7_val_t *name, v7_val_t *value,
                   v7_prop_attr_t *attrs);

/* Returns true if the object is an instance of a given constructor. */
int v7_is_instanceof(struct v7 *v7, v7_val_t o, const char *c);

/* Returns true if the object is an instance of a given constructor. */
int v7_is_instanceof_v(struct v7 *v7, v7_val_t o, v7_val_t c);

/*
 * Associates an opaque C value (anything that can be casted to a `void * )
 * with an object.
 *
 * You can achieve a similar effect by just setting a special property with
 * a foreign value (see `v7_mk_foreign`), except user data offers the following
 * advantages:
 *
 * 1. You don't have to come up with some arbitrary "special" property name.
 * 2. JS scripts cannot access user data by mistake via property lookup.
 * 3. The user data is available to the destructor. When the desctructor is
 *    invoked you cannot access any of its properties.
 * 4. Allows the implementation to use a more compact encoding
 *
 * Does nothing if `obj` is not a mutable object.
 */
void v7_set_user_data(struct v7 *v7, v7_val_t obj, void *ud);

/*
 * Get the opaque user data set with `v7_set_user_data`.
 *
 * Returns NULL if there is no user data set or if `obj` is not an object.
 */
void *v7_get_user_data(struct v7 *v7, v7_val_t obj);

/*
 * Register a callback which will be invoked when a given object gets
 * reclaimed by the garbage collector.
 *
 * The callback will be invoked while garbage collection is still in progress
 * and hence the internal state of the JS heap is in an undefined state.
 * The callback thus cannot perform any calls to the V7 API and will receive
 * only the user data associated with the destructed object.
 *
 * The intended use case is to reclaim resources allocated by C code.
 */
void v7_set_destructor_cb(struct v7 *v7, v7_val_t obj, v7_destructor_cb_t *d);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_OBJECT_PUBLIC_H_ */
