/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_FREEZE_H_
#define CS_V7_SRC_FREEZE_H_

#ifdef V7_FREEZE

#include "v7/src/internal.h"

struct v7_property;

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

V7_PRIVATE void freeze(struct v7 *v7, char *filename);
V7_PRIVATE void freeze_obj(struct v7 *v7, FILE *f, v7_val_t v);
V7_PRIVATE void freeze_prop(struct v7 *v7, FILE *f, struct v7_property *prop);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* V7_FREEZE */

#endif /* CS_V7_SRC_FREEZE_H_ */
