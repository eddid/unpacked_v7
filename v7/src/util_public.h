/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Utility functions
 */

#ifndef CS_V7_SRC_UTIL_PUBLIC_H_
#define CS_V7_SRC_UTIL_PUBLIC_H_

#include "v7/src/core_public.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Output a string representation of the value to stdout.
 * V7_STRINGIFY_DEBUG mode is used. */
void v7_print(struct v7 *v7, v7_val_t v);

/* Output a string representation of the value to stdout followed by a newline.
 * V7_STRINGIFY_DEBUG mode is used. */
void v7_println(struct v7 *v7, v7_val_t v);

/* Output a string representation of the value to a file.
 * V7_STRINGIFY_DEBUG mode is used. */
void v7_fprint(FILE *f, struct v7 *v7, v7_val_t v);

/* Output a string representation of the value to a file followed by a newline.
 * V7_STRINGIFY_DEBUG mode is used. */
void v7_fprintln(FILE *f, struct v7 *v7, v7_val_t v);

/* Output stack trace recorded in the exception `e` to file `f` */
void v7_fprint_stack_trace(FILE *f, struct v7 *v7, v7_val_t e);

/* Output error object message and possibly stack trace to f */
void v7_print_error(FILE *f, struct v7 *v7, const char *ctx, v7_val_t e);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_UTIL_PUBLIC_H_ */
