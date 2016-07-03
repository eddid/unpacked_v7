/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/cs_strtod.h"

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/primitive.h"
#include "v7/src/conversion.h"
#include "v7/src/stdlib.h"
#include "v7/src/std_array.h"
#include "v7/src/std_boolean.h"
#include "v7/src/std_date.h"
#include "v7/src/std_error.h"
#include "v7/src/std_function.h"
#include "v7/src/std_json.h"
#include "v7/src/std_math.h"
#include "v7/src/std_number.h"
#include "v7/src/std_object.h"
#include "v7/src/std_regex.h"
#include "v7/src/std_string.h"
#include "v7/src/js_stdlib.h"
#include "v7/src/object.h"
#include "v7/src/string.h"
#include "v7/src/util.h"
#include "v7/src/exec.h"

#ifdef NO_LIBC
void print_str(const char *str);
#endif

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Std_print(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  int i, num_args = v7_argc(v7);
  val_t v;

  (void) res;

  for (i = 0; i < num_args; i++) {
    v = v7_arg(v7, i);
    if (v7_is_string(v)) {
      size_t n;
      const char *s = v7_get_string(v7, &v, &n);
      printf("%.*s", (int) n, s);
    } else {
      v7_print(v7, v);
    }
    printf(" ");
  }
  printf(ENDL);

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err std_eval(struct v7 *v7, v7_val_t arg, val_t this_obj,
                                int is_json, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  char buf[100], *p = buf;
  struct v7_exec_opts opts;
  memset(&opts, 0x00, sizeof(opts));
  opts.filename = "Eval'd code";

  if (arg != V7_UNDEFINED) {
    size_t len;
    rcode = to_string(v7, arg, NULL, buf, sizeof(buf), &len);
    if (rcode != V7_OK) {
      goto clean;
    }

    /* Fit null terminating byte and quotes */
    if (len >= sizeof(buf) - 2) {
      /* Buffer is not large enough. Allocate a bigger one */
      p = (char *) malloc(len + 3);
      rcode = to_string(v7, arg, NULL, p, len + 2, NULL);
      if (rcode != V7_OK) {
        goto clean;
      }
    }

    v7_set_gc_enabled(v7, 1);
    if (is_json) {
      opts.is_json = 1;
    } else {
      opts.this_obj = this_obj;
    }
    rcode = v7_exec_opt(v7, p, &opts, res);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

clean:
  if (p != buf) {
    free(p);
  }

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Std_eval(struct v7 *v7, v7_val_t *res) {
  val_t this_obj = v7_get_this(v7);
  v7_val_t arg = v7_arg(v7, 0);
  return std_eval(v7, arg, this_obj, 0, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Std_parseInt(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = V7_UNDEFINED;
  v7_val_t arg1 = V7_UNDEFINED;
  long sign = 1, base, n;
  char buf[20], *p = buf, *end;

  *res = V7_TAG_NAN;

  arg0 = v7_arg(v7, 0);
  arg1 = v7_arg(v7, 1);

  rcode = to_string(v7, arg0, &arg0, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = to_number_v(v7, arg1, &arg1);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (is_finite(v7, arg1)) {
    base = v7_get_double(v7, arg1);
  } else {
    base = 0;
  }

  if (base == 0) {
    base = 10;
  }

  if (base < 2 || base > 36) {
    *res = V7_TAG_NAN;
    goto clean;
  }

  {
    size_t str_len;
    p = (char *) v7_get_string(v7, &arg0, &str_len);
  }

  /* Strip leading whitespaces */
  while (*p != '\0' && isspace(*(unsigned char *) p)) {
    p++;
  }

  if (*p == '+') {
    sign = 1;
    p++;
  } else if (*p == '-') {
    sign = -1;
    p++;
  }

  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
    base = 16;
    p += 2;
  }

  n = strtol(p, &end, base);

  *res = (p == end) ? V7_TAG_NAN : v7_mk_number(v7, n * sign);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Std_parseFloat(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = V7_UNDEFINED;
  char buf[20], *p = buf, *end;
  double result;

  rcode = to_primitive(v7, v7_arg(v7, 0), V7_TO_PRIMITIVE_HINT_NUMBER, &arg0);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (v7_is_string(arg0)) {
    size_t str_len;
    p = (char *) v7_get_string(v7, &arg0, &str_len);
  } else {
    rcode = to_string(v7, arg0, NULL, buf, sizeof(buf), NULL);
    if (rcode != V7_OK) {
      goto clean;
    }
    buf[sizeof(buf) - 1] = '\0';
  }

  while (*p != '\0' && isspace(*(unsigned char *) p)) {
    p++;
  }

  result = cs_strtod(p, &end);

  *res = (p == end) ? V7_TAG_NAN : v7_mk_number(v7, result);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Std_isNaN(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = V7_TAG_NAN;
  rcode = to_number_v(v7, v7_arg(v7, 0), &arg0);
  if (rcode != V7_OK) {
    goto clean;
  }

  *res = v7_mk_boolean(v7, isnan(v7_get_double(v7, arg0)));

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Std_isFinite(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = V7_TAG_NAN;

  rcode = to_number_v(v7, v7_arg(v7, 0), &arg0);
  if (rcode != V7_OK) {
    goto clean;
  }

  *res = v7_mk_boolean(v7, is_finite(v7, arg0));

clean:
  return rcode;
}

#ifndef NO_LIBC
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Std_exit(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  long exit_code;

  (void) res;

  rcode = to_long(v7, v7_arg(v7, 0), 0, &exit_code);
  if (rcode != V7_OK) {
    /* `to_long` has thrown, so, will return 1 */
    exit_code = 1;
  }
  exit(exit_code);

  return rcode;
}
#endif

V7_PRIVATE void init_stdlib(struct v7 *v7) {
  v7_prop_attr_desc_t attr_internal =
      (V7_DESC_ENUMERABLE(0) | V7_DESC_WRITABLE(0) | V7_DESC_CONFIGURABLE(0));

  /*
   * Ensure the first call to v7_mk_value will use a null proto:
   * {}.__proto__.__proto__ == null
   */
  v7->vals.object_prototype = mk_object(v7, V7_NULL);
  v7->vals.array_prototype = v7_mk_object(v7);
  v7->vals.boolean_prototype = v7_mk_object(v7);
  v7->vals.string_prototype = v7_mk_object(v7);
  v7->vals.regexp_prototype = v7_mk_object(v7);
  v7->vals.number_prototype = v7_mk_object(v7);
  v7->vals.error_prototype = v7_mk_object(v7);
  v7->vals.global_object = v7_mk_object(v7);
  v7->vals.date_prototype = v7_mk_object(v7);
  v7->vals.function_prototype = v7_mk_object(v7);

  set_method(v7, v7->vals.global_object, "eval", Std_eval, 1);
  set_method(v7, v7->vals.global_object, "print", Std_print, 1);
#ifndef NO_LIBC
  set_method(v7, v7->vals.global_object, "exit", Std_exit, 1);
#endif
  set_method(v7, v7->vals.global_object, "parseInt", Std_parseInt, 2);
  set_method(v7, v7->vals.global_object, "parseFloat", Std_parseFloat, 1);
  set_method(v7, v7->vals.global_object, "isNaN", Std_isNaN, 1);
  set_method(v7, v7->vals.global_object, "isFinite", Std_isFinite, 1);

  v7_def(v7, v7->vals.global_object, "Infinity", 8, attr_internal,
         v7_mk_number(v7, INFINITY));
  v7_set(v7, v7->vals.global_object, "global", 6, v7->vals.global_object);

  init_object(v7);
  init_array(v7);
  init_error(v7);
  init_boolean(v7);
#if V7_ENABLE__Math
  init_math(v7);
#endif
  init_string(v7);
#if V7_ENABLE__RegExp
  init_regex(v7);
#endif
  init_number(v7);
  init_json(v7);
#if V7_ENABLE__Date
  init_date(v7);
#endif
  init_function(v7);
  init_js_stdlib(v7);
}
