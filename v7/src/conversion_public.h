/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Conversion
 */

#ifndef CS_V7_SRC_CONVERSION_PUBLIC_H_
#define CS_V7_SRC_CONVERSION_PUBLIC_H_

#include "v7/src/core_public.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Stringify mode, see `v7_stringify()` and `v7_stringify_throwing()` */
enum v7_stringify_mode {
  V7_STRINGIFY_DEFAULT,
  V7_STRINGIFY_JSON,
  V7_STRINGIFY_DEBUG,
};

/*
 * Generate string representation of the JavaScript value `val` into a buffer
 * `buf`, `len`. If `len` is too small to hold a generated string,
 * `v7_stringify()` allocates required memory. In that case, it is caller's
 * responsibility to free the allocated buffer. Generated string is guaranteed
 * to be 0-terminated.
 *
 * Available stringification modes are:
 *
 * - `V7_STRINGIFY_DEFAULT`:
 *   Convert JS value to string, using common JavaScript semantics:
 *   - If value is an object:
 *     - call `toString()`;
 *     - If `toString()` returned non-primitive value, call `valueOf()`;
 *     - If `valueOf()` returned non-primitive value, throw `TypeError`.
 *   - Now we have a primitive, and if it's not a string, then stringify it.
 *
 * - `V7_STRINGIFY_JSON`:
 *   Generate JSON output
 *
 * - `V7_STRINGIFY_DEBUG`:
 *   Mostly like JSON, but will not omit non-JSON objects like functions.
 *
 * Example code:
 *
 *     char buf[100], *p;
 *     p = v7_stringify(v7, obj, buf, sizeof(buf), V7_STRINGIFY_DEFAULT);
 *     printf("JSON string: [%s]\n", p);
 *     if (p != buf) {
 *       free(p);
 *     }
 */
char *v7_stringify(struct v7 *v7, v7_val_t v, char *buf, size_t len,
                   enum v7_stringify_mode mode);

/*
 * Like `v7_stringify()`, but "returns" value through the `res` pointer
 * argument. `res` must not be `NULL`.
 *
 * Caller should check the error code returned, and if it's something other
 * than `V7_OK`, perform cleanup and return this code further.
 */
WARN_UNUSED_RESULT
enum v7_err v7_stringify_throwing(struct v7 *v7, v7_val_t v, char *buf,
                                  size_t size, enum v7_stringify_mode mode,
                                  char **res);

/*
 * A shortcut for `v7_stringify()` with `V7_STRINGIFY_JSON`
 */
#define v7_to_json(a, b, c, d) v7_stringify(a, b, c, d, V7_STRINGIFY_JSON)

/* Returns true if given value evaluates to true, as in `if (v)` statement. */
int v7_is_truthy(struct v7 *v7, v7_val_t v);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_CONVERSION_PUBLIC_H_ */
