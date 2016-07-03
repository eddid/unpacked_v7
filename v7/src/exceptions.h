/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_EXCEPTIONS_H_
#define CS_V7_SRC_EXCEPTIONS_H_

#include "v7/src/exceptions_public.h"

#include "v7/src/core.h"

/*
 * Try to perform some arbitrary call, and if the result is other than `V7_OK`,
 * "throws" an error with `V7_THROW()`
 */
#define V7_TRY2(call, clean_label)           \
  do {                                       \
    enum v7_err _e = call;                   \
    V7_CHECK2(_e == V7_OK, _e, clean_label); \
  } while (0)

/*
 * Sets return value to the provided one, and `goto`s `clean`.
 *
 * For this to work, you should have local `enum v7_err rcode` variable,
 * and a `clean` label.
 */
#define V7_THROW2(err_code, clean_label)                              \
  do {                                                                \
    (void) v7;                                                        \
    rcode = (err_code);                                               \
    assert(rcode != V7_OK);                                           \
    assert(!v7_is_undefined(v7->vals.thrown_error) && v7->is_thrown); \
    goto clean_label;                                                 \
  } while (0)

/*
 * Checks provided condition `cond`, and if it's false, then "throws"
 * provided `err_code` (see `V7_THROW()`)
 */
#define V7_CHECK2(cond, err_code, clean_label) \
  do {                                         \
    if (!(cond)) {                             \
      V7_THROW2(err_code, clean_label);        \
    }                                          \
  } while (0)

/*
 * Checks provided condition `cond`, and if it's false, then "throws"
 * internal error
 *
 * TODO(dfrank): it would be good to have formatted string: then, we can
 * specify file and line.
 */
#define V7_CHECK_INTERNAL2(cond, clean_label)                         \
  do {                                                                \
    if (!(cond)) {                                                    \
      enum v7_err __rcode = v7_throwf(v7, "Error", "Internal error"); \
      (void) __rcode;                                                 \
      V7_THROW2(V7_INTERNAL_ERROR, clean_label);                      \
    }                                                                 \
  } while (0)

/*
 * Shortcuts for the macros above, but they assume the clean label `clean`.
 */

#define V7_TRY(call) V7_TRY2(call, clean)
#define V7_THROW(err_code) V7_THROW2(err_code, clean)
#define V7_CHECK(cond, err_code) V7_CHECK2(cond, err_code, clean)
#define V7_CHECK_INTERNAL(cond) V7_CHECK_INTERNAL2(cond, clean)

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*
 * At the moment, most of the exception-related functions are public, and are
 * declared in `exceptions_public.h`
 */

/*
 * Create an instance of the exception with type `typ` (see `TYPE_ERROR`,
 * `SYNTAX_ERROR`, etc), and message `msg`.
 */
V7_PRIVATE enum v7_err create_exception(struct v7 *v7, const char *typ,
                                        const char *msg, val_t *res);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_EXCEPTIONS_H_ */
