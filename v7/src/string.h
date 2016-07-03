/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_STRING_H_
#define CS_V7_SRC_STRING_H_

#include "v7/src/string_public.h"

#include "v7/src/core.h"

/*
 * Size of the extra space for strings mbuf that is needed to avoid frequent
 * reallocations
 */
#define _V7_STRING_BUF_RESERVE 500

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err v7_char_code_at(struct v7 *v7, v7_val_t s, v7_val_t at,
                                       double *res);
V7_PRIVATE int s_cmp(struct v7 *, val_t a, val_t b);
V7_PRIVATE val_t s_concat(struct v7 *, val_t, val_t);

/*
 * Convert a C string to to an unsigned integer.
 * `ok` will be set to true if the string conforms to
 * an unsigned long.
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err str_to_ulong(struct v7 *v7, val_t v, int *ok,
                                    unsigned long *res);

/*
 * Convert a V7 string to to an unsigned integer.
 * `ok` will be set to true if the string conforms to
 * an unsigned long.
 *
 * Use it if only you need strong conformity of the value to an integer;
 * otherwise, use `to_long()` or `to_number_v()` instead.
 */
V7_PRIVATE unsigned long cstr_to_ulong(const char *s, size_t len, int *ok);

enum embstr_flags {
  EMBSTR_ZERO_TERM = (1 << 0),
  EMBSTR_UNESCAPE = (1 << 1),
};

V7_PRIVATE void embed_string(struct mbuf *m, size_t offset, const char *p,
                             size_t len, uint8_t /*enum embstr_flags*/ flags);

V7_PRIVATE size_t unescape(const char *s, size_t len, char *to);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_STRING_H_ */
