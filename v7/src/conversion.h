/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_CONVERSION_H_
#define CS_V7_SRC_CONVERSION_H_

#include "v7/src/conversion_public.h"

#include "v7/src/core.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*
 * Conversion API
 * ==============
 *
 * - If you need to convert any JS value to string using common JavaScript
 *   semantics, use `to_string()`, which can convert to both `v7_val_t` or your
 *   C buffer.
 *
 * - If you need to convert any JS value to number using common JavaScript
 *   semantics, use `to_number_v()`;
 *
 * - If you need to convert any JS value to primitive, without forcing it to
 *   string or number, use `to_primitive()` (see comments for this function for
 *   details);
 *
 * - If you have a primitive value, and you want to convert it to either string
 *   or number, you can still use functions above: `to_string()` and
 *   `to_number_v()`. But if you want to save a bit of work, use:
 *   - `primitive_to_str()`
 *   - `primitive_to_number()`
 *
 *   In fact, these are a bit lower level functions, which are used by
 *   `to_string()` and `to_number_v()` after converting value to
 *   primitive.
 *
 * - If you want to call `valueOf()` on the object, use `obj_value_of()`;
 * - If you want to call `toString()` on the object, use `obj_to_string()`;
 *
 * - If you need to convert any JS value to boolean using common JavaScript
 *   semantics (as in the expression `if (v)` or `Boolean(v)`), use
 *   `to_boolean_v()`.
 *
 * - If you want to get the JSON representation of a value, use
 *   `to_json_or_debug()`, passing `0` as `is_debug` : writes data to your C
 *   buffer;
 *
 * - There is one more kind of representation: `DEBUG`. It's very similar to
 *   JSON, but it will not omit non-JSON values, such as functions. Again, use
 *   `to_json_or_debug()`, but pass `1` as `is_debug` this time: writes data to
 *   your C buffer;
 *
 * Additionally, for any kind of to-string conversion into C buffer, you can
 * use a convenience wrapper function (mostly for public API), which can
 * allocate the buffer for you:
 *
 *   - `v7_stringify_throwing()`;
 *   - `v7_stringify()` : the same as above, but doesn't throw.
 *
 * There are a couple of more specific conversions, which I'd like to probably
 * refactor or remove in the future:
 *
 * - `to_long()` : if given value is `undefined`, returns provided default
 *   value; otherwise, converts value to number, and then truncates to `long`.
 * - `str_to_ulong()` : converts the value to string, and tries to parse it as
 *   an integer. Use it if only you need strong conformity ov the value to an
 *   integer (currently, it's used only when examining keys of array object)
 *
 * ----------------------------------------------------------------------------
 *
 * TODO(dfrank):
 *   - Rename functions like `v7_get_double(v7, )`, `get_object_struct()` to
 *something
 *     that will clearly identify that they convert to some C entity, not
 *     `v7_val_t`
 *   - Maybe make `to_string()` private? But then, there will be no way
 *     in public API to convert value to `v7_val_t` string, so, for now
 *     it's here.
 *   - When we agree on what goes to public API, and what does not, write
 *     similar conversion guide for public API (in `conversion_public.h`)
 */

/*
 * Convert any JS value to number, using common JavaScript semantics:
 *
 * - If value is an object:
 *   - call `valueOf()`;
 *   - If `valueOf()` returned non-primitive value, call `toString()`;
 *   - If `toString()` returned non-primitive value, throw `TypeError`.
 * - Now we have a primitive, and if it's not a number, then:
 *   - If `undefined`, return `NaN`
 *   - If `null`, return 0.0
 *   - If boolean, return either 1 or 0
 *   - If string, try to parse it.
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err to_number_v(struct v7 *v7, v7_val_t v, v7_val_t *res);

/*
 * Convert any JS value to string, using common JavaScript semantics,
 * see `v7_stringify()` and `V7_STRINGIFY_DEFAULT`.
 *
 * This function can return multiple things:
 *
 * - String as a `v7_val_t` (if `res` is not `NULL`)
 * - String copied to buffer `buf` with max size `buf_size` (if `buf` is not
 *   `NULL`)
 * - Length of actual string, independently of `buf_size` (if `res_len` is not
 *   `NULL`)
 *
 * The rationale of having multiple formats of returned value is the following:
 *
 * Initially, to-string conversion always returned `v7_val_t`. But it turned
 * out that there are situations where such an approach adds useless pressure
 * on GC: e.g. when converting `undefined` to string, and the caller actually
 * needs a C buffer, not a `v7_val_t`.
 *
 * Always returning string through `buf`+`buf_size` is bad as well: if we
 * convert from object to string, and either `toString()` or `valueOf()`
 * returned string, then we'd have to get string data from it, write to buffer,
 * and if caller actually need `v7_val_t`, then it will have to create new
 * instance of the same string: again, useless GC pressure.
 *
 * So, we have to use the combined approach. This function will make minimal
 * work depending on give `res` and `buf`.
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err to_string(struct v7 *v7, v7_val_t v, v7_val_t *res,
                                 char *buf, size_t buf_size, size_t *res_len);

/*
 * Convert value to primitive, if it's not already.
 *
 * For object-to-primitive conversion, each object in JavaScript has two
 * methods: `toString()` and `valueOf()`.
 *
 * When converting object to string, JavaScript does the following:
 *   - call `toString()`;
 *   - If `toString()` returned non-primitive value, call `valueOf()`;
 *   - If `valueOf()` returned non-primitive value, throw `TypeError`.
 *
 * When converting object to number, JavaScript calls the same functions,
 * but in reverse:
 *   - call `valueOf()`;
 *   - If `valueOf()` returned non-primitive value, call `toString()`;
 *   - If `toString()` returned non-primitive value, throw `TypeError`.
 *
 * This function `to_primitive()` performs either type of conversion,
 * depending on the `hint` argument (see `enum to_primitive_hint`).
 */
enum to_primitive_hint {
  /* Call `valueOf()` first, then `toString()` if needed */
  V7_TO_PRIMITIVE_HINT_NUMBER,

  /* Call `toString()` first, then `valueOf()` if needed */
  V7_TO_PRIMITIVE_HINT_STRING,

  /* STRING for Date, NUMBER for everything else */
  V7_TO_PRIMITIVE_HINT_AUTO,
};
WARN_UNUSED_RESULT
enum v7_err to_primitive(struct v7 *v7, v7_val_t v, enum to_primitive_hint hint,
                         v7_val_t *res);

/*
 * Convert primitive value to string, using common JavaScript semantics. If
 * you need to convert any value to string (either object or primitive),
 * see `to_string()` or `v7_stringify_throwing()`.
 *
 * This function can return multiple things:
 *
 * - String as a `v7_val_t` (if `res` is not `NULL`)
 * - String copied to buffer `buf` with max size `buf_size` (if `buf` is not
 *   `NULL`)
 * - Length of actual string, independently of `buf_size` (if `res_len` is not
 *   `NULL`)
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err primitive_to_str(struct v7 *v7, val_t v, val_t *res,
                                        char *buf, size_t buf_size,
                                        size_t *res_len);

/*
 * Convert primitive value to number, using common JavaScript semantics. If you
 * need to convert any value to number (either object or primitive), see
 * `to_number_v()`
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err primitive_to_number(struct v7 *v7, val_t v, val_t *res);

/*
 * Convert value to JSON or "debug" representation, depending on whether
 * `is_debug` is non-zero. The "debug" is the same as JSON, but non-JSON values
 * (functions, `undefined`, etc) will not be omitted.
 *
 * See also `v7_stringify()`, `v7_stringify_throwing()`.
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err to_json_or_debug(struct v7 *v7, val_t v, char *buf,
                                        size_t size, size_t *res_len,
                                        uint8_t is_debug);

/*
 * Calls `valueOf()` on given object `v`
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err obj_value_of(struct v7 *v7, val_t v, val_t *res);

/*
 * Calls `toString()` on given object `v`
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err obj_to_string(struct v7 *v7, val_t v, val_t *res);

/*
 * If given value is `undefined`, returns `default_value`; otherwise,
 * converts value to number, and then truncates to `long`.
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err to_long(struct v7 *v7, val_t v, long default_value,
                               long *res);

/*
 * Converts value to boolean as in the expression `if (v)` or `Boolean(v)`.
 *
 * NOTE: it can't throw (even if the given value is an object with `valueOf()`
 * that throws), so it returns `val_t` directly.
 */
WARN_UNUSED_RESULT
V7_PRIVATE val_t to_boolean_v(struct v7 *v7, val_t v);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_CONVERSION_H_ */
