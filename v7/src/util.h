/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_UTIL_H_
#define CS_V7_SRC_UTIL_H_

#include "v7/src/core.h"
#include "v7/src/util_public.h"

struct bcode;

V7_PRIVATE enum v7_type val_type(struct v7 *v7, val_t v);

#ifndef V7_DISABLE_LINE_NUMBERS
V7_PRIVATE uint8_t msb_lsb_swap(uint8_t b);
#endif

/*
 * At the moment, all other utility functions are public, and are declared in
 * `util_public.h`
 */

#endif /* CS_V7_SRC_UTIL_H_ */
