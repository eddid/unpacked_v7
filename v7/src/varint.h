/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_VARINT_H_
#define CS_V7_SRC_VARINT_H_

#include "v7/src/internal.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

V7_PRIVATE int encode_varint(size_t len, unsigned char *p);
V7_PRIVATE size_t decode_varint(const unsigned char *p, int *llen);
V7_PRIVATE int calc_llen(size_t len);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_VARINT_H_ */
