/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === RegExp
 */

#ifndef CS_V7_SRC_REGEXP_PUBLIC_H_
#define CS_V7_SRC_REGEXP_PUBLIC_H_

#include "v7/src/core_public.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*
 * Make RegExp object.
 * `regex`, `regex_len` specify a pattern, `flags` and `flags_len` specify
 * flags. Both utf8 encoded. For example, `regex` is `(.+)`, `flags` is `gi`.
 * If `regex_len` is ~0, `regex` is assumed to be NUL-terminated and
 * `strlen(regex)` is used.
 */
WARN_UNUSED_RESULT
enum v7_err v7_mk_regexp(struct v7 *v7, const char *regex, size_t regex_len,
                         const char *flags, size_t flags_len, v7_val_t *res);

/* Returns true if given value is a JavaScript RegExp object*/
int v7_is_regexp(struct v7 *v7, v7_val_t v);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_REGEXP_PUBLIC_H_ */
