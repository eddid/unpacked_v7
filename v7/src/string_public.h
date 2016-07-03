/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Strings
 */

#ifndef CS_V7_SRC_STRING_PUBLIC_H_
#define CS_V7_SRC_STRING_PUBLIC_H_

#include "v7/src/core_public.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*
 * Creates a string primitive value.
 * `str` must point to the utf8 string of length `len`.
 * If `len` is ~0, `str` is assumed to be NUL-terminated and `strlen(str)` is
 * used.
 *
 * If `copy` is non-zero, the string data is copied and owned by the GC. The
 * caller can free the string data afterwards. Otherwise (`copy` is zero), the
 * caller owns the string data, and is responsible for not freeing it while it
 * is used.
 */
v7_val_t v7_mk_string(struct v7 *v7, const char *str, size_t len, int copy);

/* Returns true if given value is a primitive string value */
int v7_is_string(v7_val_t v);

/*
 * Returns a pointer to the string stored in `v7_val_t`.
 *
 * String length returned in `len`, which is allowed to be NULL. Returns NULL
 * if the value is not a string.
 *
 * JS strings can contain embedded NUL chars and may or may not be NUL
 * terminated.
 *
 * CAUTION: creating new JavaScript object, array, or string may kick in a
 * garbage collector, which in turn may relocate string data and invalidate
 * pointer returned by `v7_get_string()`.
 *
 * Short JS strings are embedded inside the `v7_val_t` value itself. This is why
 * a pointer to a `v7_val_t` is required. It also means that the string data
 * will become invalid once that `v7_val_t` value goes out of scope.
 */
const char *v7_get_string(struct v7 *v7, v7_val_t *v, size_t *len);

/*
 * Returns a pointer to the string stored in `v7_val_t`.
 *
 * Returns NULL if the value is not a string or if the string is not compatible
 * with a C string.
 *
 * C compatible strings contain exactly one NUL char, in terminal position.
 *
 * All strings owned by the V7 engine (see `v7_mk_string()`) are guaranteed to
 * be NUL terminated. Out of these, those that don't include embedded NUL chars
 * are guaranteed to be C compatible.
 */
const char *v7_get_cstring(struct v7 *v7, v7_val_t *v);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_STRING_PUBLIC_H_ */
