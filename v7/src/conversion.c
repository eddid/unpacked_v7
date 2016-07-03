/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/cs_strtod.h"
#include "common/str_util.h"

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/util.h"
#include "v7/src/primitive.h"
#include "v7/src/function.h"
#include "v7/src/conversion.h"
#include "v7/src/exceptions.h"
#include "v7/src/eval.h"
#include "v7/src/gc.h"
#include "v7/src/array.h"
#include "v7/src/object.h"

#ifdef V7_TEMP_OFF
int double_to_str(char *buf, size_t buf_size, double val, int prec);
#endif

static void save_val(struct v7 *v7, const char *str, size_t str_len,
                     val_t *dst_v, char *dst, size_t dst_size, int wanted_len,
                     size_t *res_wanted_len) {
  if (dst_v != NULL) {
    *dst_v = v7_mk_string(v7, str, str_len, 1);
  }

  if (dst != NULL && dst_size > 0) {
    size_t size = str_len + 1 /*null-term*/;
    if (size > dst_size) {
      size = dst_size;
    }
    memcpy(dst, str, size);

    /* make sure we have null-term */
    dst[dst_size - 1] = '\0';
  }

  if (res_wanted_len != NULL) {
    *res_wanted_len = (wanted_len >= 0) ? (size_t) wanted_len : str_len;
  }
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err primitive_to_str(struct v7 *v7, val_t v, val_t *res,
                                        char *buf, size_t buf_size,
                                        size_t *res_len) {
  enum v7_err rcode = V7_OK;
  char tmp_buf[25];
  double num;
  size_t wanted_len;

  assert(!v7_is_object(v));

  memset(tmp_buf, 0x00, sizeof(tmp_buf));

  v7_own(v7, &v);

  switch (val_type(v7, v)) {
    case V7_TYPE_STRING: {
      /* if `res` provided, set it to source value */
      if (res != NULL) {
        *res = v;
      }

      /* if buf provided, copy string data there */
      if (buf != NULL && buf_size > 0) {
        size_t size;
        const char *str = v7_get_string(v7, &v, &size);
        size += 1 /*null-term*/;

        if (size > buf_size) {
          size = buf_size;
        }

        memcpy(buf, str, size);

        /* make sure we have a null-term */
        buf[buf_size - 1] = '\0';
      }

      if (res_len != NULL) {
        v7_get_string(v7, &v, res_len);
      }

      goto clean;
    }
    case V7_TYPE_NULL:
      strncpy(tmp_buf, "null", sizeof(tmp_buf) - 1);
      save_val(v7, tmp_buf, strlen(tmp_buf), res, buf, buf_size, -1, res_len);
      goto clean;
    case V7_TYPE_UNDEFINED:
      strncpy(tmp_buf, "undefined", sizeof(tmp_buf) - 1);
      save_val(v7, tmp_buf, strlen(tmp_buf), res, buf, buf_size, -1, res_len);
      goto clean;
    case V7_TYPE_BOOLEAN:
      if (v7_get_bool(v7, v)) {
        strncpy(tmp_buf, "true", sizeof(tmp_buf) - 1);
        save_val(v7, tmp_buf, strlen(tmp_buf), res, buf, buf_size, -1, res_len);
        goto clean;
      } else {
        strncpy(tmp_buf, "false", sizeof(tmp_buf) - 1);
        save_val(v7, tmp_buf, strlen(tmp_buf), res, buf, buf_size, -1, res_len);
        goto clean;
      }
    case V7_TYPE_NUMBER:
      if (v == V7_TAG_NAN) {
        strncpy(tmp_buf, "NaN", sizeof(tmp_buf) - 1);
        save_val(v7, tmp_buf, strlen(tmp_buf), res, buf, buf_size, -1, res_len);
        goto clean;
      }
      num = v7_get_double(v7, v);
      if (isinf(num)) {
        if (num < 0.0) {
          strncpy(tmp_buf, "-Infinity", sizeof(tmp_buf) - 1);
        } else {
          strncpy(tmp_buf, "Infinity", sizeof(tmp_buf) - 1);
        }
        save_val(v7, tmp_buf, strlen(tmp_buf), res, buf, buf_size, -1, res_len);
        goto clean;
      }
      {
/*
 * ESP8266's sprintf doesn't support double & float.
 * TODO(alashkin): fix this
 */
#ifndef V7_TEMP_OFF
        const char *fmt = num > 1e10 ? "%.21g" : "%.10g";
        wanted_len = snprintf(tmp_buf, sizeof(tmp_buf), fmt, num);
#else
        const int prec = num > 1e10 ? 21 : 10;
        wanted_len = double_to_str(tmp_buf, sizeof(tmp_buf), num, prec);
#endif
        save_val(v7, tmp_buf, strlen(tmp_buf), res, buf, buf_size, wanted_len,
                 res_len);
        goto clean;
      }
    case V7_TYPE_CFUNCTION:
#ifdef V7_UNIT_TEST
      wanted_len = c_snprintf(tmp_buf, sizeof(tmp_buf), "cfunc_xxxxxx");
      save_val(v7, tmp_buf, strlen(tmp_buf), res, buf, buf_size, wanted_len,
               res_len);
      goto clean;
#else
      wanted_len = c_snprintf(tmp_buf, sizeof(tmp_buf), "cfunc_%p", get_ptr(v));
      save_val(v7, tmp_buf, strlen(tmp_buf), res, buf, buf_size, wanted_len,
               res_len);
      goto clean;
#endif
    case V7_TYPE_FOREIGN:
      wanted_len = c_snprintf(tmp_buf, sizeof(tmp_buf), "[foreign_%p]",
                              v7_get_ptr(v7, v));
      save_val(v7, tmp_buf, strlen(tmp_buf), res, buf, buf_size, wanted_len,
               res_len);
      goto clean;
    default:
      abort();
  }

clean:

  v7_disown(v7, &v);
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err primitive_to_number(struct v7 *v7, val_t v, val_t *res) {
  enum v7_err rcode = V7_OK;

  assert(!v7_is_object(v));

  *res = v;

  if (v7_is_number(*res)) {
    goto clean;
  }

  if (v7_is_undefined(*res)) {
    *res = V7_TAG_NAN;
    goto clean;
  }

  if (v7_is_null(*res)) {
    *res = v7_mk_number(v7, 0.0);
    goto clean;
  }

  if (v7_is_boolean(*res)) {
    *res = v7_mk_number(v7, !!v7_get_bool(v7, v));
    goto clean;
  }

  if (is_cfunction_lite(*res)) {
    *res = v7_mk_number(v7, 0.0);
    goto clean;
  }

  if (v7_is_string(*res)) {
    double d;
    size_t n;
    char *e, *s = (char *) v7_get_string(v7, res, &n);
    if (n != 0) {
      d = cs_strtod(s, &e);
      if (e - n != s) {
        d = NAN;
      }
    } else {
      /* empty string: convert to 0 */
      d = 0.0;
    }
    *res = v7_mk_number(v7, d);
    goto clean;
  }

  assert(0);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
enum v7_err to_primitive(struct v7 *v7, val_t v, enum to_primitive_hint hint,
                         val_t *res) {
  enum v7_err rcode = V7_OK;
  enum v7_err (*p_func)(struct v7 *v7, val_t v, val_t *res);

  v7_own(v7, &v);

  *res = v;

  /*
   * If given value is an object, try to convert it to string by calling first
   * preferred function (`toString()` or `valueOf()`, depending on the `hint`
   * argument)
   */
  if (v7_is_object(*res)) {
    /* Handle special case for Date object */
    if (hint == V7_TO_PRIMITIVE_HINT_AUTO) {
      hint = (obj_prototype_v(v7, *res) == v7->vals.date_prototype)
                 ? V7_TO_PRIMITIVE_HINT_STRING
                 : V7_TO_PRIMITIVE_HINT_NUMBER;
    }

    p_func =
        (hint == V7_TO_PRIMITIVE_HINT_NUMBER) ? obj_value_of : obj_to_string;
    rcode = p_func(v7, *res, res);
    if (rcode != V7_OK) {
      goto clean;
    }

    /*
     * If returned value is still an object, get original argument value
     */
    if (v7_is_object(*res)) {
      *res = v;
    }
  }

  /*
   * If the value is still an object, try to call second function (`valueOf()`
   * or `toString()`)
   */
  if (v7_is_object(*res)) {
    p_func =
        (hint == V7_TO_PRIMITIVE_HINT_NUMBER) ? obj_to_string : obj_value_of;
    rcode = p_func(v7, *res, res);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

  /*
   * If the value is still an object, then throw.
   */
  if (v7_is_object(*res)) {
    rcode =
        v7_throwf(v7, TYPE_ERROR, "Cannot convert object to primitive value");
    goto clean;
  }

clean:
  v7_disown(v7, &v);
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err to_string(struct v7 *v7, val_t v, val_t *res, char *buf,
                                 size_t buf_size, size_t *res_len) {
  enum v7_err rcode = V7_OK;

  v7_own(v7, &v);

  /*
   * Convert value to primitive if needed, calling `toString()` first
   */
  V7_TRY(to_primitive(v7, v, V7_TO_PRIMITIVE_HINT_STRING, &v));

  /*
   * Now, we're guaranteed to have a primitive here. Convert it to string.
   */
  V7_TRY(primitive_to_str(v7, v, res, buf, buf_size, res_len));

clean:
  v7_disown(v7, &v);
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err to_number_v(struct v7 *v7, val_t v, val_t *res) {
  enum v7_err rcode = V7_OK;

  *res = v;

  /*
   * Convert value to primitive if needed, calling `valueOf()` first
   */
  rcode = to_primitive(v7, *res, V7_TO_PRIMITIVE_HINT_NUMBER, res);
  if (rcode != V7_OK) {
    goto clean;
  }

  /*
   * Now, we're guaranteed to have a primitive here. Convert it to number.
   */
  rcode = primitive_to_number(v7, *res, res);
  if (rcode != V7_OK) {
    goto clean;
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err to_long(struct v7 *v7, val_t v, long default_value,
                               long *res) {
  enum v7_err rcode = V7_OK;
  double d;

  /* if value is `undefined`, just return `default_value` */
  if (v7_is_undefined(v)) {
    *res = default_value;
    goto clean;
  }

  /* Try to convert value to number */
  rcode = to_number_v(v7, v, &v);
  if (rcode != V7_OK) {
    goto clean;
  }

  /*
   * Conversion to number succeeded, so, convert it to long
   */

  d = v7_get_double(v7, v);
  /* We want to return LONG_MAX if d is positive Inf, thus d < 0 check */
  if (isnan(d) || (isinf(d) && d < 0)) {
    *res = 0;
    goto clean;
  } else if (d > LONG_MAX) {
    *res = LONG_MAX;
    goto clean;
  }
  *res = (long) d;
  goto clean;

clean:
  return rcode;
}

V7_PRIVATE enum v7_err obj_value_of(struct v7 *v7, val_t v, val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t func_valueOf = V7_UNDEFINED;

  v7_own(v7, &func_valueOf);
  v7_own(v7, &v);

  /*
   * TODO(dfrank): use `assert(v7_is_object(v))` instead, like `obj_to_string()`
   * does, and fix all callers to ensure it's an object before calling.
   *
   * Or, conversely, make `obj_to_string()` to accept objects.
   */
  if (!v7_is_object(v)) {
    *res = v;
    goto clean;
  }

  V7_TRY(v7_get_throwing(v7, v, "valueOf", 7, &func_valueOf));

  if (v7_is_callable(v7, func_valueOf)) {
    V7_TRY(b_apply(v7, func_valueOf, v, V7_UNDEFINED, 0, res));
  }

clean:
  if (rcode != V7_OK) {
    *res = v;
  }

  v7_disown(v7, &v);
  v7_disown(v7, &func_valueOf);

  return rcode;
}

/*
 * Caller should ensure that `v` is an object
 */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err obj_to_string(struct v7 *v7, val_t v, val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t to_string_func = V7_UNDEFINED;

  /* Caller should ensure that `v` is an object */
  assert(v7_is_object(v));

  v7_own(v7, &to_string_func);
  v7_own(v7, &v);

  /*
   * If `toString` is callable, then call it; otherwise, just return source
   * value
   */
  V7_TRY(v7_get_throwing(v7, v, "toString", 8, &to_string_func));
  if (v7_is_callable(v7, to_string_func)) {
    V7_TRY(b_apply(v7, to_string_func, v, V7_UNDEFINED, 0, res));
  } else {
    *res = v;
  }

clean:
  v7_disown(v7, &v);
  v7_disown(v7, &to_string_func);

  return rcode;
}

static const char *hex_digits = "0123456789abcdef";
static char *append_hex(char *buf, char *limit, uint8_t c) {
  if (buf < limit) *buf++ = 'u';
  if (buf < limit) *buf++ = '0';
  if (buf < limit) *buf++ = '0';
  if (buf < limit) *buf++ = hex_digits[(int) ((c >> 4) % 0xf)];
  if (buf < limit) *buf++ = hex_digits[(int) (c & 0xf)];
  return buf;
}

/*
 * Appends quoted s to buf. Any double quote contained in s will be escaped.
 * Returns the number of characters that would have been added,
 * like snprintf.
 * If size is zero it doesn't output anything but keeps counting.
 */
static int snquote(char *buf, size_t size, const char *s, size_t len) {
  char *limit = buf + size - 1;
  const char *end;
  /*
   * String single character escape sequence:
   * http://www.ecma-international.org/ecma-262/6.0/index.html#table-34
   *
   * 0x8 -> \b
   * 0x9 -> \t
   * 0xa -> \n
   * 0xb -> \v
   * 0xc -> \f
   * 0xd -> \r
   */
  const char *specials = "btnvfr";
  size_t i = 0;

  i++;
  if (buf < limit) *buf++ = '"';

  for (end = s + len; s < end; s++) {
    if (*s == '"' || *s == '\\') {
      i++;
      if (buf < limit) *buf++ = '\\';
    } else if (*s >= '\b' && *s <= '\r') {
      i += 2;
      if (buf < limit) *buf++ = '\\';
      if (buf < limit) *buf++ = specials[*s - '\b'];
      continue;
    } else if ((unsigned char) *s < '\b' || (*s > '\r' && *s < ' ')) {
      i += 6 /* \uXXXX */;
      if (buf < limit) *buf++ = '\\';
      buf = append_hex(buf, limit, (uint8_t) *s);
      continue;
    }
    i++;
    if (buf < limit) *buf++ = *s;
  }

  i++;
  if (buf < limit) *buf++ = '"';

  if (size != 0) {
    *buf = '\0';
  }
  return i;
}

/*
 * Returns whether the value of given type should be skipped when generating
 * JSON output
 */
static int should_skip_for_json(enum v7_type type) {
  int ret;
  switch (type) {
    /* All permitted values */
    case V7_TYPE_NULL:
    case V7_TYPE_BOOLEAN:
    case V7_TYPE_BOOLEAN_OBJECT:
    case V7_TYPE_NUMBER:
    case V7_TYPE_NUMBER_OBJECT:
    case V7_TYPE_STRING:
    case V7_TYPE_STRING_OBJECT:
    case V7_TYPE_GENERIC_OBJECT:
    case V7_TYPE_ARRAY_OBJECT:
    case V7_TYPE_DATE_OBJECT:
    case V7_TYPE_REGEXP_OBJECT:
    case V7_TYPE_ERROR_OBJECT:
      ret = 0;
      break;
    default:
      ret = 1;
      break;
  }
  return ret;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err to_json_or_debug(struct v7 *v7, val_t v, char *buf,
                                        size_t size, size_t *res_len,
                                        uint8_t is_debug) {
  val_t el;
  char *vp;
  enum v7_err rcode = V7_OK;
  size_t len = 0;
  struct gc_tmp_frame tf = new_tmp_frame(v7);

  tmp_stack_push(&tf, &v);
  tmp_stack_push(&tf, &el);
  /*
   * TODO(dfrank) : also push all `v7_val_t`s that are declared below
   */

  if (size > 0) *buf = '\0';

  if (!is_debug && should_skip_for_json(val_type(v7, v))) {
    goto clean;
  }

  for (vp = v7->json_visited_stack.buf;
       vp < v7->json_visited_stack.buf + v7->json_visited_stack.len;
       vp += sizeof(val_t)) {
    if (*(val_t *) vp == v) {
      strncpy(buf, "[Circular]", size);
      len = 10;
      goto clean;
    }
  }

  switch (val_type(v7, v)) {
    case V7_TYPE_NULL:
    case V7_TYPE_BOOLEAN:
    case V7_TYPE_NUMBER:
    case V7_TYPE_UNDEFINED:
    case V7_TYPE_CFUNCTION:
    case V7_TYPE_FOREIGN:
      /* For those types, regular `primitive_to_str()` works */
      V7_TRY(primitive_to_str(v7, v, NULL, buf, size, &len));
      goto clean;

    case V7_TYPE_STRING: {
      /*
       * For strings we can't just use `primitive_to_str()`, because we need
       * quoted value
       */
      size_t n;
      const char *str = v7_get_string(v7, &v, &n);
      len = snquote(buf, size, str, n);
      goto clean;
    }

    case V7_TYPE_DATE_OBJECT: {
      v7_val_t func = V7_UNDEFINED, val = V7_UNDEFINED;
      V7_TRY(v7_get_throwing(v7, v, "toString", 8, &func));
#if V7_ENABLE__Date__toJSON
      if (!is_debug) {
        V7_TRY(v7_get_throwing(v7, v, "toJSON", 6, &func));
      }
#endif
      V7_TRY(b_apply(v7, func, v, V7_UNDEFINED, 0, &val));
      V7_TRY(to_json_or_debug(v7, val, buf, size, &len, is_debug));
      goto clean;
    }
    case V7_TYPE_GENERIC_OBJECT:
    case V7_TYPE_BOOLEAN_OBJECT:
    case V7_TYPE_STRING_OBJECT:
    case V7_TYPE_NUMBER_OBJECT:
    case V7_TYPE_REGEXP_OBJECT:
    case V7_TYPE_ERROR_OBJECT: {
      /* TODO(imax): make it return the desired size of the buffer */
      char *b = buf;
      void *h = NULL;
      v7_val_t name = V7_UNDEFINED, val = V7_UNDEFINED;
      v7_prop_attr_t attrs;

      mbuf_append(&v7->json_visited_stack, (char *) &v, sizeof(v));
      b += c_snprintf(b, BUF_LEFT(size, b - buf), "{");
      while ((h = v7_next_prop(h, v, &name, &val, &attrs)) != NULL) {
        size_t n;
        const char *s;
        if (attrs & (_V7_PROPERTY_HIDDEN | V7_PROPERTY_NON_ENUMERABLE)) {
          continue;
        }
        if (!is_debug && should_skip_for_json(val_type(v7, val))) {
          continue;
        }
        if (b - buf != 1) { /* Not the first property to be printed */
          b += c_snprintf(b, BUF_LEFT(size, b - buf), ",");
        }
        s = v7_get_string(v7, &name, &n);
        b += c_snprintf(b, BUF_LEFT(size, b - buf), "\"%.*s\":", (int) n, s);
        {
          size_t tmp = 0;
          V7_TRY(to_json_or_debug(v7, val, b, BUF_LEFT(size, b - buf), &tmp,
                                  is_debug));
          b += tmp;
        }
      }
      b += c_snprintf(b, BUF_LEFT(size, b - buf), "}");
      v7->json_visited_stack.len -= sizeof(v);
      len = b - buf;
      goto clean;
    }
    case V7_TYPE_ARRAY_OBJECT: {
      int has;
      char *b = buf;
      size_t i, alen = v7_array_length(v7, v);
      mbuf_append(&v7->json_visited_stack, (char *) &v, sizeof(v));
      b += c_snprintf(b, BUF_LEFT(size, b - buf), "[");
      for (i = 0; i < alen; i++) {
        el = v7_array_get2(v7, v, i, &has);
        if (has) {
          size_t tmp = 0;
          if (!is_debug && should_skip_for_json(val_type(v7, el))) {
            b += c_snprintf(b, BUF_LEFT(size, b - buf), "null");
          } else {
            V7_TRY(to_json_or_debug(v7, el, b, BUF_LEFT(size, b - buf), &tmp,
                                    is_debug));
          }
          b += tmp;
        }
        if (i != alen - 1) {
          b += c_snprintf(b, BUF_LEFT(size, b - buf), ",");
        }
      }
      b += c_snprintf(b, BUF_LEFT(size, b - buf), "]");
      v7->json_visited_stack.len -= sizeof(v);
      len = b - buf;
      goto clean;
    }
    case V7_TYPE_CFUNCTION_OBJECT:
      V7_TRY(obj_value_of(v7, v, &v));
      len = c_snprintf(buf, size, "Function cfunc_%p", get_ptr(v));
      goto clean;
    case V7_TYPE_FUNCTION_OBJECT:
      V7_TRY(to_string(v7, v, NULL, buf, size, &len));
      goto clean;

    case V7_TYPE_MAX_OBJECT_TYPE:
    case V7_NUM_TYPES:
      abort();
  }

  abort();

  len = 0; /* for compilers that don't know about abort() */
  goto clean;

clean:
  if (rcode != V7_OK) {
    len = 0;
  }
  if (res_len != NULL) {
    *res_len = len;
  }
  tmp_frame_cleanup(&tf);
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE val_t to_boolean_v(struct v7 *v7, val_t v) {
  size_t len;
  int is_truthy;

  is_truthy = ((v7_is_boolean(v) && v7_get_bool(v7, v)) ||
               (v7_is_number(v) && v7_get_double(v7, v) != 0.0) ||
               (v7_is_string(v) && v7_get_string(v7, &v, &len) && len > 0) ||
               (v7_is_object(v))) &&
              v != V7_TAG_NAN;

  return v7_mk_boolean(v7, is_truthy);
}

/*
 * v7_stringify allocates a new buffer if value representation doesn't fit into
 * buf. Caller is responsible for freeing that buffer.
 */
char *v7_stringify(struct v7 *v7, val_t v, char *buf, size_t size,
                   enum v7_stringify_mode mode) {
  enum v7_err rcode = V7_OK;
  uint8_t saved_is_thrown = 0;
  val_t saved_thrown = v7_get_thrown_value(v7, &saved_is_thrown);
  char *ret = NULL;

  rcode = v7_stringify_throwing(v7, v, buf, size, mode, &ret);
  if (rcode != V7_OK) {
    rcode = V7_OK;
    if (saved_is_thrown) {
      rcode = v7_throw(v7, saved_thrown);
    } else {
      v7_clear_thrown_value(v7);
    }

    buf[0] = '\0';
    ret = buf;
  }

  return ret;
}

enum v7_err v7_stringify_throwing(struct v7 *v7, val_t v, char *buf,
                                  size_t size, enum v7_stringify_mode mode,
                                  char **res) {
  enum v7_err rcode = V7_OK;
  char *p = buf;
  size_t len;

  switch (mode) {
    case V7_STRINGIFY_DEFAULT:
      V7_TRY(to_string(v7, v, NULL, buf, size, &len));
      break;

    case V7_STRINGIFY_JSON:
      V7_TRY(to_json_or_debug(v7, v, buf, size, &len, 0));
      break;

    case V7_STRINGIFY_DEBUG:
      V7_TRY(to_json_or_debug(v7, v, buf, size, &len, 1));
      break;
  }

  /* fit null terminating byte */
  if (len >= size) {
    /* Buffer is not large enough. Allocate a bigger one */
    p = (char *) malloc(len + 1);
    V7_TRY(v7_stringify_throwing(v7, v, p, len + 1, mode, res));
    assert(*res == p);
    goto clean;
  } else {
    *res = p;
    goto clean;
  }

clean:
  /*
   * If we're going to throw, and we allocated a buffer, then free it.
   * But if we don't throw, then the caller will free it.
   */
  if (rcode != V7_OK && p != buf) {
    free(p);
  }
  return rcode;
}

int v7_is_truthy(struct v7 *v7, val_t v) {
  return v7_get_bool(v7, to_boolean_v(v7, v));
}
