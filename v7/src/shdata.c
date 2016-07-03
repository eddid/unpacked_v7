/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/shdata.h"

#if !defined(V7_DISABLE_FILENAMES) && !defined(V7_DISABLE_LINE_NUMBERS)
V7_PRIVATE struct shdata *shdata_create(const void *payload, size_t size) {
  struct shdata *ret =
      (struct shdata *) calloc(1, sizeof(struct shdata) + size);
  shdata_retain(ret);
  if (payload != NULL) {
    memcpy((char *) shdata_get_payload(ret), (char *) payload, size);
  }
  return ret;
}

V7_PRIVATE struct shdata *shdata_create_from_string(const char *src) {
  return shdata_create(src, strlen(src) + 1 /*null-term*/);
}

V7_PRIVATE void shdata_retain(struct shdata *p) {
  p->refcnt++;
  assert(p->refcnt > 0);
}

V7_PRIVATE void shdata_release(struct shdata *p) {
  assert(p->refcnt > 0);
  p->refcnt--;
  if (p->refcnt == 0) {
    free(p);
  }
}

V7_PRIVATE void *shdata_get_payload(struct shdata *p) {
  return (char *) p + sizeof(*p);
}
#endif
