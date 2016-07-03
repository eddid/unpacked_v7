/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * shdata (stands for "shared data") is a simple module that allows to have
 * reference count for an arbitrary payload data, which will be freed as
 * necessary. A poor man's shared_ptr.
 */

#ifndef CS_V7_SRC_SHDATA_H_
#define CS_V7_SRC_SHDATA_H_

#include "v7/src/internal.h"

#if !defined(V7_DISABLE_FILENAMES) && !defined(V7_DISABLE_LINE_NUMBERS)
struct shdata {
  /* Reference count */
  uint8_t refcnt;

  /*
   * Note: we'd use `unsigned char payload[];` here, but we can't, since this
   * feature was introduced in C99 only
   */
};

/*
 * Allocate memory chunk of appropriate size, copy given `payload` data there,
 * retain (`shdata_retain()`), and return it.
 */
V7_PRIVATE struct shdata *shdata_create(const void *payload, size_t size);

V7_PRIVATE struct shdata *shdata_create_from_string(const char *src);

/*
 * Increment reference count for the given shared data
 */
V7_PRIVATE void shdata_retain(struct shdata *p);

/*
 * Decrement reference count for the given shared data
 */
V7_PRIVATE void shdata_release(struct shdata *p);

/*
 * Get payload data
 */
V7_PRIVATE void *shdata_get_payload(struct shdata *p);

#endif
#endif /* CS_V7_SRC_SHDATA_H_ */
