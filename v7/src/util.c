/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/object.h"
#include "v7/src/util.h"
#include "v7/src/string.h"
#include "v7/src/conversion.h"
#include "v7/src/primitive.h"

void v7_print(struct v7 *v7, v7_val_t v) {
  v7_fprint(stdout, v7, v);
}

void v7_fprint(FILE *f, struct v7 *v7, val_t v) {
  char buf[16];
  char *s = v7_stringify(v7, v, buf, sizeof(buf), V7_STRINGIFY_DEBUG);
  fprintf(f, "%s", s);
  if (buf != s) free(s);
}

void v7_println(struct v7 *v7, v7_val_t v) {
  v7_fprintln(stdout, v7, v);
}

void v7_fprintln(FILE *f, struct v7 *v7, val_t v) {
  v7_fprint(f, v7, v);
  fprintf(f, ENDL);
}

void v7_fprint_stack_trace(FILE *f, struct v7 *v7, val_t e) {
  size_t s;
  val_t strace_v = v7_get(v7, e, "stack", ~0);
  const char *strace = NULL;
  if (v7_is_string(strace_v)) {
    strace = v7_get_string(v7, &strace_v, &s);
    fprintf(f, "%s\n", strace);
  }
}

void v7_print_error(FILE *f, struct v7 *v7, const char *ctx, val_t e) {
  /* TODO(mkm): figure out if this is an error object and which kind */
  v7_val_t msg;
  if (v7_is_undefined(e)) {
    fprintf(f, "undefined error [%s]\n ", ctx);
    return;
  }
  msg = v7_get(v7, e, "message", ~0);
  if (v7_is_undefined(msg)) {
    msg = e;
  }
  fprintf(f, "Exec error [%s]: ", ctx);
  v7_fprintln(f, v7, msg);
  v7_fprint_stack_trace(f, v7, e);
}

V7_PRIVATE enum v7_type val_type(struct v7 *v7, val_t v) {
  int tag;
  if (v7_is_number(v)) {
    return V7_TYPE_NUMBER;
  }
  tag = (v & V7_TAG_MASK) >> 48;
  switch (tag) {
    case V7_TAG_FOREIGN >> 48:
      if (v7_is_null(v)) {
        return V7_TYPE_NULL;
      }
      return V7_TYPE_FOREIGN;
    case V7_TAG_UNDEFINED >> 48:
      return V7_TYPE_UNDEFINED;
    case V7_TAG_OBJECT >> 48:
      if (obj_prototype_v(v7, v) == v7->vals.array_prototype) {
        return V7_TYPE_ARRAY_OBJECT;
      } else if (obj_prototype_v(v7, v) == v7->vals.boolean_prototype) {
        return V7_TYPE_BOOLEAN_OBJECT;
      } else if (obj_prototype_v(v7, v) == v7->vals.string_prototype) {
        return V7_TYPE_STRING_OBJECT;
      } else if (obj_prototype_v(v7, v) == v7->vals.number_prototype) {
        return V7_TYPE_NUMBER_OBJECT;
      } else if (obj_prototype_v(v7, v) == v7->vals.function_prototype) {
        return V7_TYPE_CFUNCTION_OBJECT;
      } else if (obj_prototype_v(v7, v) == v7->vals.date_prototype) {
        return V7_TYPE_DATE_OBJECT;
      } else {
        return V7_TYPE_GENERIC_OBJECT;
      }
    case V7_TAG_STRING_I >> 48:
    case V7_TAG_STRING_O >> 48:
    case V7_TAG_STRING_F >> 48:
    case V7_TAG_STRING_D >> 48:
    case V7_TAG_STRING_5 >> 48:
      return V7_TYPE_STRING;
    case V7_TAG_BOOLEAN >> 48:
      return V7_TYPE_BOOLEAN;
    case V7_TAG_FUNCTION >> 48:
      return V7_TYPE_FUNCTION_OBJECT;
    case V7_TAG_CFUNCTION >> 48:
      return V7_TYPE_CFUNCTION;
    case V7_TAG_REGEXP >> 48:
      return V7_TYPE_REGEXP_OBJECT;
    default:
      abort();
      return V7_TYPE_UNDEFINED;
  }
}

#ifndef V7_DISABLE_LINE_NUMBERS
V7_PRIVATE uint8_t msb_lsb_swap(uint8_t b) {
  if ((b & 0x01) != (b >> 7)) {
    b ^= 0x81;
  }
  return b;
}
#endif
