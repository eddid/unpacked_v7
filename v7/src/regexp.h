/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_REGEXP_H_
#define CS_V7_SRC_REGEXP_H_

#include "v7/src/regexp_public.h"

#include "v7/src/core.h"

#if V7_ENABLE__RegExp

/*
 * Maximum number of flags returned by get_regexp_flags_str().
 * NOTE: does not include null-terminate byte.
 */
#define _V7_REGEXP_MAX_FLAGS_LEN 3

struct v7_regexp;

V7_PRIVATE struct v7_regexp *v7_get_regexp_struct(struct v7 *, v7_val_t);

/*
 * Generates a string containing regexp flags, e.g. "gi".
 *
 * `buf` should point to a buffer of minimum `_V7_REGEXP_MAX_FLAGS_LEN` bytes.
 * Returns length of the resulted string (saved into `buf`)
 */
V7_PRIVATE size_t
get_regexp_flags_str(struct v7 *v7, struct v7_regexp *rp, char *buf);
#endif /* V7_ENABLE__RegExp */

#endif /* CS_V7_SRC_REGEXP_H_ */
