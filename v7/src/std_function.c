/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/str_util.h"
#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/bcode.h"
#include "v7/src/eval.h"
#include "v7/src/conversion.h"
#include "v7/src/object.h"
#include "v7/src/exec.h"
#include "v7/src/exceptions.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Function_ctor(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  long i, num_args = v7_argc(v7);
  size_t size;
  const char *s;
  struct mbuf m;

  mbuf_init(&m, 0);

  if (num_args <= 0) {
    goto clean;
  }

  mbuf_append(&m, "(function(", 10);

  for (i = 0; i < num_args - 1; i++) {
    rcode = obj_value_of(v7, v7_arg(v7, i), res);
    if (rcode != V7_OK) {
      goto clean;
    }
    if (v7_is_string(*res)) {
      if (i > 0) mbuf_append(&m, ",", 1);
      s = v7_get_string(v7, res, &size);
      mbuf_append(&m, s, size);
    }
  }
  mbuf_append(&m, "){", 2);
  rcode = obj_value_of(v7, v7_arg(v7, num_args - 1), res);
  if (rcode != V7_OK) {
    goto clean;
  }
  if (v7_is_string(*res)) {
    s = v7_get_string(v7, res, &size);
    mbuf_append(&m, s, size);
  }
  mbuf_append(&m, "})\0", 3);

  rcode = v7_exec(v7, m.buf, res);
  if (rcode != V7_OK) {
    rcode = v7_throwf(v7, SYNTAX_ERROR, "Invalid function body");
    goto clean;
  }

clean:
  mbuf_free(&m);
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Function_length(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t this_obj = v7_get_this(v7);
  struct v7_js_function *func;

  rcode = obj_value_of(v7, this_obj, &this_obj);
  if (rcode != V7_OK) {
    goto clean;
  }
  if (!is_js_function(this_obj)) {
    *res = v7_mk_number(v7, 0);
    goto clean;
  }

  func = get_js_function_struct(this_obj);

  *res = v7_mk_number(v7, func->bcode->args_cnt);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Function_name(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t this_obj = v7_get_this(v7);
  struct v7_js_function *func;

  rcode = obj_value_of(v7, this_obj, &this_obj);
  if (rcode != V7_OK) {
    goto clean;
  }
  if (!is_js_function(this_obj)) {
    goto clean;
  }

  func = get_js_function_struct(this_obj);

  assert(func->bcode != NULL);

  assert(func->bcode->names_cnt >= 1);
  bcode_next_name_v(v7, func->bcode, func->bcode->ops.p, res);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Function_apply(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t this_arg = v7_arg(v7, 0);
  val_t func_args = v7_arg(v7, 1);

  rcode = obj_value_of(v7, this_obj, &this_obj);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = b_apply(v7, this_obj, this_arg, func_args, 0, res);
  if (rcode != V7_OK) {
    goto clean;
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Function_toString(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  char *ops;
  char *name;
  size_t name_len;
  char buf[50];
  char *b = buf;
  struct v7_js_function *func = get_js_function_struct(v7_get_this(v7));
  int i;

  b += c_snprintf(b, BUF_LEFT(sizeof(buf), b - buf), "[function");

  assert(func->bcode != NULL);
  ops = func->bcode->ops.p;

  /* first entry in name list */
  ops = bcode_next_name(ops, &name, &name_len);

  if (name_len > 0) {
    b += c_snprintf(b, BUF_LEFT(sizeof(buf), b - buf), " %.*s", (int) name_len,
                    name);
  }
  b += c_snprintf(b, BUF_LEFT(sizeof(buf), b - buf), "(");
  for (i = 0; i < func->bcode->args_cnt; i++) {
    ops = bcode_next_name(ops, &name, &name_len);

    b += c_snprintf(b, BUF_LEFT(sizeof(buf), b - buf), "%.*s", (int) name_len,
                    name);
    if (i < func->bcode->args_cnt - 1) {
      b += c_snprintf(b, BUF_LEFT(sizeof(buf), b - buf), ",");
    }
  }
  b += c_snprintf(b, BUF_LEFT(sizeof(buf), b - buf), ")");

  {
    uint8_t loc_cnt =
        func->bcode->names_cnt - func->bcode->args_cnt - 1 /*func name*/;

    if (loc_cnt > 0) {
      b += c_snprintf(b, BUF_LEFT(sizeof(buf), b - buf), "{var ");
      for (i = 0; i < loc_cnt; ++i) {
        ops = bcode_next_name(ops, &name, &name_len);

        b += c_snprintf(b, BUF_LEFT(sizeof(buf), b - buf), "%.*s",
                        (int) name_len, name);
        if (i < (loc_cnt - 1)) {
          b += c_snprintf(b, BUF_LEFT(sizeof(buf), b - buf), ",");
        }
      }

      b += c_snprintf(b, BUF_LEFT(sizeof(buf), b - buf), "}");
    }
  }

  b += c_snprintf(b, BUF_LEFT(sizeof(buf), b - buf), "]");

  *res = v7_mk_string(v7, buf, strlen(buf), 1);

  return rcode;
}

V7_PRIVATE void init_function(struct v7 *v7) {
  val_t ctor = mk_cfunction_obj(v7, Function_ctor, 1);

  v7_set(v7, ctor, "prototype", 9, v7->vals.function_prototype);
  v7_set(v7, v7->vals.global_object, "Function", 8, ctor);
  set_method(v7, v7->vals.function_prototype, "apply", Function_apply, 1);
  set_method(v7, v7->vals.function_prototype, "toString", Function_toString, 0);
  v7_def(v7, v7->vals.function_prototype, "length", 6,
         (V7_DESC_ENUMERABLE(0) | V7_DESC_GETTER(1)),
         v7_mk_cfunction(Function_length));
  v7_def(v7, v7->vals.function_prototype, "name", 4,
         (V7_DESC_ENUMERABLE(0) | V7_DESC_GETTER(1)),
         v7_mk_cfunction(Function_name));
}

#if defined(__cplusplus)
}
#endif /* __cplusplus */
