/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Exceptions
 */

#ifndef CS_V7_SRC_EXCEPTIONS_PUBLIC_H_
#define CS_V7_SRC_EXCEPTIONS_PUBLIC_H_

#include "v7/src/core_public.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Throw an exception with an already existing value. */
WARN_UNUSED_RESULT
enum v7_err v7_throw(struct v7 *v7, v7_val_t v);

/*
 * Throw an exception with given formatted message.
 *
 * Pass "Error" as typ for a generic error.
 */
WARN_UNUSED_RESULT
enum v7_err v7_throwf(struct v7 *v7, const char *typ, const char *err_fmt, ...);

/*
 * Rethrow the currently thrown object. In fact, it just returns
 * V7_EXEC_EXCEPTION.
 */
WARN_UNUSED_RESULT
enum v7_err v7_rethrow(struct v7 *v7);

/*
 * Returns the value that is being thrown at the moment, or `undefined` if
 * nothing is being thrown. If `is_thrown` is not `NULL`, it will be set
 * to either 0 or 1, depending on whether something is thrown at the moment.
 */
v7_val_t v7_get_thrown_value(struct v7 *v7, unsigned char *is_thrown);

/* Clears currently thrown value, if any. */
void v7_clear_thrown_value(struct v7 *v7);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_EXCEPTIONS_PUBLIC_H_ */
