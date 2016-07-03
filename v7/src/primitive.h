/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_PRIMITIVE_H_
#define CS_V7_SRC_PRIMITIVE_H_

#include "v7/src/primitive_public.h"

#include "v7/src/core.h"

/* Returns true if given value is a number, not NaN and not Infinity. */
V7_PRIVATE int is_finite(struct v7 *v7, v7_val_t v);

V7_PRIVATE val_t pointer_to_value(void *p);
V7_PRIVATE void *get_ptr(val_t v);

#endif /* CS_V7_SRC_PRIMITIVE_H_ */
