/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/std_object.h"
#include "v7/src/conversion.h"
#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/object.h"
#include "v7/src/primitive.h"
#include "v7/src/string.h"
#include "v7/src/exceptions.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Number_ctor(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t arg0 = v7_argc(v7) == 0 ? v7_mk_number(v7, 0.0) : v7_arg(v7, 0);

  if (v7_is_number(arg0)) {
    *res = arg0;
  } else {
    rcode = to_number_v(v7, arg0, res);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

  if (v7_is_generic_object(this_obj) && this_obj != v7->vals.global_object) {
    obj_prototype_set(v7, get_object_struct(this_obj),
                      get_object_struct(v7->vals.number_prototype));
    v7_def(v7, this_obj, "", 0, _V7_DESC_HIDDEN(1), *res);

    /*
     * implicitly returning `this`: `call_cfunction()` in bcode.c will do
     * that for us
     */
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err n_to_str(struct v7 *v7, const char *format, val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t arg0 = v7_arg(v7, 0);
  int len, digits = 0;
  char fmt[10], buf[100];

  rcode = to_number_v(v7, arg0, &arg0);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (v7_get_double(v7, arg0) > 0) {
    digits = (int) v7_get_double(v7, arg0);
  }

  /*
   * NOTE: we don't own `arg0` and `this_obj`, since this function is called
   * from cfunctions only, and GC is inhibited during these calls
   */

  rcode = obj_value_of(v7, this_obj, &this_obj);
  if (rcode != V7_OK) {
    goto clean;
  }

  snprintf(fmt, sizeof(fmt), format, digits);
  len = snprintf(buf, sizeof(buf), fmt, v7_get_double(v7, this_obj));

  *res = v7_mk_string(v7, buf, len, 1);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Number_toFixed(struct v7 *v7, v7_val_t *res) {
  return n_to_str(v7, "%%.%dlf", res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Number_toExp(struct v7 *v7, v7_val_t *res) {
  return n_to_str(v7, "%%.%de", res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Number_toPrecision(struct v7 *v7, v7_val_t *res) {
  return Number_toExp(v7, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Number_valueOf(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);

  if (!v7_is_number(this_obj) &&
      (v7_is_object(this_obj) &&
       obj_prototype_v(v7, this_obj) != v7->vals.number_prototype)) {
    rcode =
        v7_throwf(v7, TYPE_ERROR, "Number.valueOf called on non-number object");
    goto clean;
  }

  rcode = Obj_valueOf(v7, res);
  if (rcode != V7_OK) {
    goto clean;
  }

clean:
  return rcode;
}

/*
 * Converts a 64 bit signed integer into a string of a given base.
 * Requires space for 65 bytes (64 bit + null terminator) in the result buffer
 */
static char *cs_itoa(int64_t value, char *result, int base) {
  char *ptr = result, *ptr1 = result, tmp_char;
  int64_t tmp_value;
  int64_t sign = value < 0 ? -1 : 1;
  const char *base36 = "0123456789abcdefghijklmnopqrstuvwxyz";

  if (base < 2 || base > 36) {
    *result = '\0';
    return result;
  }

  /* let's think positive */
  value = value * sign;
  do {
    tmp_value = value;
    value /= base;
    *ptr++ = base36[tmp_value - value * base];
  } while (value);

  /* sign */
  if (sign < 0) *ptr++ = '-';
  *ptr-- = '\0';
  while (ptr1 < ptr) {
    tmp_char = *ptr;
    *ptr-- = *ptr1;
    *ptr1++ = tmp_char;
  }
  return result;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Number_toString(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t radixv = v7_arg(v7, 0);
  char buf[65];
  double d, radix;

  if (this_obj == v7->vals.number_prototype) {
    *res = v7_mk_string(v7, "0", 1, 1);
    goto clean;
  }

  /* Make sure this function was called on Number instance */
  if (!v7_is_number(this_obj) &&
      !(v7_is_generic_object(this_obj) &&
        is_prototype_of(v7, this_obj, v7->vals.number_prototype))) {
    rcode = v7_throwf(v7, TYPE_ERROR,
                      "Number.toString called on non-number object");
    goto clean;
  }

  /* Get number primitive */
  rcode = to_number_v(v7, this_obj, &this_obj);
  if (rcode != V7_OK) {
    goto clean;
  }

  /* Get radix if provided, or 10 otherwise */
  if (!v7_is_undefined(radixv)) {
    rcode = to_number_v(v7, radixv, &radixv);
    if (rcode != V7_OK) {
      goto clean;
    }
    radix = v7_get_double(v7, radixv);
  } else {
    radix = 10.0;
  }

  d = v7_get_double(v7, this_obj);
  if (!isnan(d) && (int64_t) d == d && radix >= 2) {
    cs_itoa(d, buf, radix);
    *res = v7_mk_string(v7, buf, strlen(buf), 1);
  } else {
    rcode = to_string(v7, this_obj, res, NULL, 0, NULL);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err n_isNaN(struct v7 *v7, v7_val_t *res) {
  val_t arg0 = v7_arg(v7, 0);
  *res = v7_mk_boolean(v7, !v7_is_number(arg0) || arg0 == V7_TAG_NAN);
  return V7_OK;
}

V7_PRIVATE void init_number(struct v7 *v7) {
  v7_prop_attr_desc_t attrs_desc =
      (V7_DESC_WRITABLE(0) | V7_DESC_ENUMERABLE(0) | V7_DESC_CONFIGURABLE(0));
  val_t num = mk_cfunction_obj_with_proto(v7, Number_ctor, 1,
                                          v7->vals.number_prototype);

  v7_def(v7, v7->vals.global_object, "Number", 6, V7_DESC_ENUMERABLE(0), num);

  set_cfunc_prop(v7, v7->vals.number_prototype, "toFixed", Number_toFixed);
  set_cfunc_prop(v7, v7->vals.number_prototype, "toPrecision",
                 Number_toPrecision);
  set_cfunc_prop(v7, v7->vals.number_prototype, "toExponential", Number_toExp);
  set_cfunc_prop(v7, v7->vals.number_prototype, "valueOf", Number_valueOf);
  set_cfunc_prop(v7, v7->vals.number_prototype, "toString", Number_toString);

  v7_def(v7, num, "MAX_VALUE", 9, attrs_desc,
         v7_mk_number(v7, 1.7976931348623157e+308));
  v7_def(v7, num, "MIN_VALUE", 9, attrs_desc, v7_mk_number(v7, 5e-324));
#if V7_ENABLE__NUMBER__NEGATIVE_INFINITY
  v7_def(v7, num, "NEGATIVE_INFINITY", 17, attrs_desc,
         v7_mk_number(v7, -INFINITY));
#endif
#if V7_ENABLE__NUMBER__POSITIVE_INFINITY
  v7_def(v7, num, "POSITIVE_INFINITY", 17, attrs_desc,
         v7_mk_number(v7, INFINITY));
#endif
  v7_def(v7, num, "NaN", 3, attrs_desc, V7_TAG_NAN);

  v7_def(v7, v7->vals.global_object, "NaN", 3, attrs_desc, V7_TAG_NAN);
  v7_def(v7, v7->vals.global_object, "isNaN", 5, V7_DESC_ENUMERABLE(0),
         v7_mk_cfunction(n_isNaN));
}

#if defined(__cplusplus)
}
#endif /* __cplusplus */
