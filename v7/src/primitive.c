/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/primitive.h"

/* Number {{{ */

NOINSTR static v7_val_t mk_number(double v) {
  val_t res;
  /* not every NaN is a JS NaN */
  if (isnan(v)) {
    res = V7_TAG_NAN;
  } else {
    union {
      double d;
      val_t r;
    } u;
    u.d = v;
    res = u.r;
  }
  return res;
}

NOINSTR static double get_double(val_t v) {
  union {
    double d;
    val_t v;
  } u;
  u.v = v;
  /* Due to NaN packing, any non-numeric value is already a valid NaN value */
  return u.d;
}

NOINSTR static v7_val_t mk_boolean(int v) {
  return (!!v) | V7_TAG_BOOLEAN;
}

NOINSTR static int get_bool(val_t v) {
  if (v7_is_boolean(v)) {
    return v & 1;
  } else {
    return 0;
  }
}

NOINSTR v7_val_t v7_mk_number(struct v7 *v7, double v) {
  (void) v7;
  return mk_number(v);
}

NOINSTR double v7_get_double(struct v7 *v7, v7_val_t v) {
  (void) v7;
  return get_double(v);
}

NOINSTR int v7_get_int(struct v7 *v7, v7_val_t v) {
  (void) v7;
  return (int) get_double(v);
}

int v7_is_number(val_t v) {
  return v == V7_TAG_NAN || !isnan(get_double(v));
}

V7_PRIVATE int is_finite(struct v7 *v7, val_t v) {
  return v7_is_number(v) && v != V7_TAG_NAN && !isinf(v7_get_double(v7, v));
}

/* }}} Number */

/* Boolean {{{ */

NOINSTR v7_val_t v7_mk_boolean(struct v7 *v7, int v) {
  (void) v7;
  return mk_boolean(v);
}

NOINSTR int v7_get_bool(struct v7 *v7, val_t v) {
  (void) v7;
  return get_bool(v);
}

int v7_is_boolean(val_t v) {
  return (v & V7_TAG_MASK) == V7_TAG_BOOLEAN;
}

/* }}} Boolean */

/* null {{{ */

NOINSTR v7_val_t v7_mk_null(void) {
  return V7_NULL;
}

int v7_is_null(val_t v) {
  return v == V7_NULL;
}

/* }}} null */

/* undefined {{{ */

NOINSTR v7_val_t v7_mk_undefined(void) {
  return V7_UNDEFINED;
}

int v7_is_undefined(val_t v) {
  return v == V7_UNDEFINED;
}

/* }}} undefined */

/* Foreign {{{ */

V7_PRIVATE val_t pointer_to_value(void *p) {
  uint64_t n = ((uint64_t)(uintptr_t) p);

  assert((n & V7_TAG_MASK) == 0 || (n & V7_TAG_MASK) == (~0 & V7_TAG_MASK));
  return n & ~V7_TAG_MASK;
}

V7_PRIVATE void *get_ptr(val_t v) {
  return (void *) (uintptr_t)(v & 0xFFFFFFFFFFFFUL);
}

NOINSTR void *v7_get_ptr(struct v7 *v7, val_t v) {
  (void) v7;
  if (!v7_is_foreign(v)) {
    return NULL;
  }
  return get_ptr(v);
}

NOINSTR v7_val_t v7_mk_foreign(struct v7 *v7, void *p) {
  (void) v7;
  return pointer_to_value(p) | V7_TAG_FOREIGN;
}

int v7_is_foreign(val_t v) {
  return (v & V7_TAG_MASK) == V7_TAG_FOREIGN;
}

/* }}} Foreign */
