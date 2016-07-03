/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/utf.h"
#include "common/str_util.h"
#include "v7/src/internal.h"
#include "v7/src/std_regex.h"
#include "v7/src/std_string.h"
#include "v7/src/slre.h"
#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/conversion.h"
#include "v7/src/array.h"
#include "v7/src/object.h"
#include "v7/src/regexp.h"
#include "v7/src/exceptions.h"
#include "v7/src/string.h"
#include "v7/src/primitive.h"

#if V7_ENABLE__RegExp

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Regex_ctor(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  long argnum = v7_argc(v7);

  if (argnum > 0) {
    val_t arg = v7_arg(v7, 0);
    val_t ro, fl;
    size_t re_len, flags_len = 0;
    const char *re, *flags = NULL;

    if (v7_is_regexp(v7, arg)) {
      if (argnum > 1) {
        /* ch15/15.10/15.10.3/S15.10.3.1_A2_T1.js */
        rcode = v7_throwf(v7, TYPE_ERROR, "invalid flags");
        goto clean;
      }
      *res = arg;
      goto clean;
    }
    rcode = to_string(v7, arg, &ro, NULL, 0, NULL);
    if (rcode != V7_OK) {
      goto clean;
    }

    if (argnum > 1) {
      rcode = to_string(v7, v7_arg(v7, 1), &fl, NULL, 0, NULL);
      if (rcode != V7_OK) {
        goto clean;
      }

      flags = v7_get_string(v7, &fl, &flags_len);
    }
    re = v7_get_string(v7, &ro, &re_len);
    rcode = v7_mk_regexp(v7, re, re_len, flags, flags_len, res);
    if (rcode != V7_OK) {
      goto clean;
    }

  } else {
    rcode = v7_mk_regexp(v7, "(?:)", 4, NULL, 0, res);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Regex_global(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  int flags = 0;
  val_t this_obj = v7_get_this(v7);
  val_t r = V7_UNDEFINED;
  rcode = obj_value_of(v7, this_obj, &r);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (v7_is_regexp(v7, r)) {
    flags = slre_get_flags(v7_get_regexp_struct(v7, r)->compiled_regexp);
  }

  *res = v7_mk_boolean(v7, flags & SLRE_FLAG_G);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Regex_ignoreCase(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  int flags = 0;
  val_t this_obj = v7_get_this(v7);
  val_t r = V7_UNDEFINED;
  rcode = obj_value_of(v7, this_obj, &r);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (v7_is_regexp(v7, r)) {
    flags = slre_get_flags(v7_get_regexp_struct(v7, r)->compiled_regexp);
  }

  *res = v7_mk_boolean(v7, flags & SLRE_FLAG_I);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Regex_multiline(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  int flags = 0;
  val_t this_obj = v7_get_this(v7);
  val_t r = V7_UNDEFINED;
  rcode = obj_value_of(v7, this_obj, &r);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (v7_is_regexp(v7, r)) {
    flags = slre_get_flags(v7_get_regexp_struct(v7, r)->compiled_regexp);
  }

  *res = v7_mk_boolean(v7, flags & SLRE_FLAG_M);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Regex_source(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t r = V7_UNDEFINED;
  const char *buf = 0;
  size_t len = 0;

  rcode = obj_value_of(v7, this_obj, &r);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (v7_is_regexp(v7, r)) {
    buf = v7_get_string(v7, &v7_get_regexp_struct(v7, r)->regexp_string, &len);
  }

  *res = v7_mk_string(v7, buf, len, 1);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Regex_get_lastIndex(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  long lastIndex = 0;
  val_t this_obj = v7_get_this(v7);

  if (v7_is_regexp(v7, this_obj)) {
    lastIndex = v7_get_regexp_struct(v7, this_obj)->lastIndex;
  }

  *res = v7_mk_number(v7, lastIndex);

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Regex_set_lastIndex(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  long lastIndex = 0;
  val_t this_obj = v7_get_this(v7);

  if (v7_is_regexp(v7, this_obj)) {
    rcode = to_long(v7, v7_arg(v7, 0), 0, &lastIndex);
    if (rcode != V7_OK) {
      goto clean;
    }
    v7_get_regexp_struct(v7, this_obj)->lastIndex = lastIndex;
  }

  *res = v7_mk_number(v7, lastIndex);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err rx_exec(struct v7 *v7, val_t rx, val_t vstr, int lind,
                               val_t *res) {
  enum v7_err rcode = V7_OK;
  if (v7_is_regexp(v7, rx)) {
    val_t s = V7_UNDEFINED;
    size_t len;
    struct slre_loot sub;
    struct slre_cap *ptok = sub.caps;
    const char *str = NULL;
    const char *end = NULL;
    const char *begin = NULL;
    struct v7_regexp *rp = v7_get_regexp_struct(v7, rx);
    int flag_g = slre_get_flags(rp->compiled_regexp) & SLRE_FLAG_G;

    rcode = to_string(v7, vstr, &s, NULL, 0, NULL);
    if (rcode != V7_OK) {
      goto clean;
    }
    str = v7_get_string(v7, &s, &len);
    end = str + len;
    begin = str;

    if (rp->lastIndex < 0) rp->lastIndex = 0;
    if (flag_g || lind) begin = utfnshift(str, rp->lastIndex);

    if (!slre_exec(rp->compiled_regexp, 0, begin, end, &sub)) {
      int i;
      val_t arr = v7_mk_array(v7);
      char *old_mbuf_base = v7->owned_strings.buf;
      ptrdiff_t rel = 0; /* creating strings might relocate the mbuf */

      for (i = 0; i < sub.num_captures; i++, ptok++) {
        rel = v7->owned_strings.buf - old_mbuf_base;
        v7_array_push(v7, arr, v7_mk_string(v7, ptok->start + rel,
                                            ptok->end - ptok->start, 1));
      }
      if (flag_g) rp->lastIndex = utfnlen(str, sub.caps->end + rel - str);
      v7_def(v7, arr, "index", 5, V7_DESC_WRITABLE(0),
             v7_mk_number(v7, utfnlen(str + rel, sub.caps->start - str)));
      *res = arr;
      goto clean;
    } else {
      rp->lastIndex = 0;
    }
  }

  *res = V7_NULL;

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Regex_exec(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);

  if (v7_argc(v7) > 0) {
    rcode = rx_exec(v7, this_obj, v7_arg(v7, 0), 0, res);
    if (rcode != V7_OK) {
      goto clean;
    }
  } else {
    *res = V7_NULL;
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Regex_test(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t tmp = V7_UNDEFINED;

  rcode = Regex_exec(v7, &tmp);
  if (rcode != V7_OK) {
    goto clean;
  }

  *res = v7_mk_boolean(v7, !v7_is_null(tmp));

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Regex_flags(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  char buf[3] = {0};
  val_t this_obj = v7_get_this(v7);
  struct v7_regexp *rp = v7_get_regexp_struct(v7, this_obj);
  size_t n = get_regexp_flags_str(v7, rp, buf);
  *res = v7_mk_string(v7, buf, n, 1);

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Regex_toString(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  size_t n1, n2 = 0;
  char s2[3] = {0};
  char buf[50];
  val_t this_obj = v7_get_this(v7);
  struct v7_regexp *rp;
  const char *s1;

  rcode = obj_value_of(v7, this_obj, &this_obj);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (!v7_is_regexp(v7, this_obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Not a regexp");
    goto clean;
  }

  rp = v7_get_regexp_struct(v7, this_obj);
  s1 = v7_get_string(v7, &rp->regexp_string, &n1);
  n2 = get_regexp_flags_str(v7, rp, s2);

  c_snprintf(buf, sizeof(buf), "/%.*s/%.*s", (int) n1, s1, (int) n2, s2);

  *res = v7_mk_string(v7, buf, strlen(buf), 1);

clean:
  return rcode;
}

V7_PRIVATE void init_regex(struct v7 *v7) {
  val_t ctor =
      mk_cfunction_obj_with_proto(v7, Regex_ctor, 1, v7->vals.regexp_prototype);
  val_t lastIndex = v7_mk_dense_array(v7);

  v7_def(v7, v7->vals.global_object, "RegExp", 6, V7_DESC_ENUMERABLE(0), ctor);

  set_cfunc_prop(v7, v7->vals.regexp_prototype, "exec", Regex_exec);
  set_cfunc_prop(v7, v7->vals.regexp_prototype, "test", Regex_test);
  set_method(v7, v7->vals.regexp_prototype, "toString", Regex_toString, 0);

  v7_def(v7, v7->vals.regexp_prototype, "global", 6, V7_DESC_GETTER(1),
         v7_mk_cfunction(Regex_global));
  v7_def(v7, v7->vals.regexp_prototype, "ignoreCase", 10, V7_DESC_GETTER(1),
         v7_mk_cfunction(Regex_ignoreCase));
  v7_def(v7, v7->vals.regexp_prototype, "multiline", 9, V7_DESC_GETTER(1),
         v7_mk_cfunction(Regex_multiline));
  v7_def(v7, v7->vals.regexp_prototype, "source", 6, V7_DESC_GETTER(1),
         v7_mk_cfunction(Regex_source));
  v7_def(v7, v7->vals.regexp_prototype, "flags", 5, V7_DESC_GETTER(1),
         v7_mk_cfunction(Regex_flags));

  v7_array_set(v7, lastIndex, 0, v7_mk_cfunction(Regex_get_lastIndex));
  v7_array_set(v7, lastIndex, 1, v7_mk_cfunction(Regex_set_lastIndex));
  v7_def(v7, v7->vals.regexp_prototype, "lastIndex", 9,
         (V7_DESC_GETTER(1) | V7_DESC_SETTER(1)), lastIndex);
}

#endif /* V7_ENABLE__RegExp */
