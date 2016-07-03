/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/stdlib.h"
#include "v7/src/core.h"
#include "v7/src/object.h"
#include "v7/src/conversion.h"
#include "v7/src/string.h"
#include "v7/src/primitive.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Json_stringify(struct v7 *v7, v7_val_t *res) {
  val_t arg0 = v7_arg(v7, 0);
  char buf[100], *p = v7_to_json(v7, arg0, buf, sizeof(buf));
  *res = v7_mk_string(v7, p, strlen(p), 1);

  if (p != buf) free(p);
  return V7_OK;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Json_parse(struct v7 *v7, v7_val_t *res) {
  v7_val_t arg = v7_arg(v7, 0);
  return std_eval(v7, arg, V7_UNDEFINED, 1, res);
}

V7_PRIVATE void init_json(struct v7 *v7) {
  val_t o = v7_mk_object(v7);
  set_method(v7, o, "stringify", Json_stringify, 1);
  set_method(v7, o, "parse", Json_parse, 1);
  v7_def(v7, v7->vals.global_object, "JSON", 4, V7_DESC_ENUMERABLE(0), o);
}

#if defined(__cplusplus)
}
#endif /* __cplusplus */
