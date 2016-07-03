/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/primitive.h"
#include "v7/src/object.h"
#include "v7/src/regexp.h"
#include "v7/src/exceptions.h"
#include "v7/src/string.h"
#include "v7/src/slre.h"

#if V7_ENABLE__RegExp
enum v7_err v7_mk_regexp(struct v7 *v7, const char *re, size_t re_len,
                         const char *flags, size_t flags_len, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  struct slre_prog *p = NULL;
  struct v7_regexp *rp;

  if (re_len == ~((size_t) 0)) re_len = strlen(re);

  if (slre_compile(re, re_len, flags, flags_len, &p, 1) != SLRE_OK ||
      p == NULL) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Invalid regex");
    goto clean;
  } else {
    *res = mk_object(v7, v7->vals.regexp_prototype);
    rp = (struct v7_regexp *) malloc(sizeof(*rp));
    rp->regexp_string = v7_mk_string(v7, re, re_len, 1);
    v7_own(v7, &rp->regexp_string);
    rp->compiled_regexp = p;
    rp->lastIndex = 0;

    v7_def(v7, *res, "", 0, _V7_DESC_HIDDEN(1),
           pointer_to_value(rp) | V7_TAG_REGEXP);
  }

clean:
  return rcode;
}

V7_PRIVATE struct v7_regexp *v7_get_regexp_struct(struct v7 *v7, val_t v) {
  struct v7_property *p;
  int is = v7_is_regexp(v7, v);
  (void) is;
  assert(is == 1);
  /* TODO(mkm): make regexp use user data API */
  p = v7_get_own_property2(v7, v, "", 0, _V7_PROPERTY_HIDDEN);
  assert(p != NULL);
  return (struct v7_regexp *) get_ptr(p->value);
}

int v7_is_regexp(struct v7 *v7, val_t v) {
  struct v7_property *p;
  if (!v7_is_generic_object(v)) return 0;
  /* TODO(mkm): make regexp use user data API */
  p = v7_get_own_property2(v7, v, "", 0, _V7_PROPERTY_HIDDEN);
  if (p == NULL) return 0;
  return (p->value & V7_TAG_MASK) == V7_TAG_REGEXP;
}

V7_PRIVATE size_t
get_regexp_flags_str(struct v7 *v7, struct v7_regexp *rp, char *buf) {
  int re_flags = slre_get_flags(rp->compiled_regexp);
  size_t n = 0;

  (void) v7;
  if (re_flags & SLRE_FLAG_G) buf[n++] = 'g';
  if (re_flags & SLRE_FLAG_I) buf[n++] = 'i';
  if (re_flags & SLRE_FLAG_M) buf[n++] = 'm';

  assert(n <= _V7_REGEXP_MAX_FLAGS_LEN);

  return n;
}

#else /* V7_ENABLE__RegExp */

/*
 * Dummy implementation when RegExp support is disabled: just return 0
 */
int v7_is_regexp(struct v7 *v7, val_t v) {
  (void) v7;
  (void) v;
  return 0;
}

#endif /* V7_ENABLE__RegExp */
