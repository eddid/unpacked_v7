/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/std_object.h"
#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/object.h"
#include "v7/src/conversion.h"
#include "v7/src/primitive.h"
#include "v7/src/exceptions.h"
#include "v7/src/string.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Boolean_ctor(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);

  *res = to_boolean_v(v7, v7_arg(v7, 0));

  if (v7_is_generic_object(this_obj) && this_obj != v7->vals.global_object) {
    /* called as "new Boolean(...)" */
    obj_prototype_set(v7, get_object_struct(this_obj),
                      get_object_struct(v7->vals.boolean_prototype));
    v7_def(v7, this_obj, "", 0, _V7_DESC_HIDDEN(1), *res);

    /*
     * implicitly returning `this`: `call_cfunction()` in bcode.c will do
     * that for us
     */
  }

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Boolean_valueOf(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  if (!v7_is_boolean(this_obj) &&
      (v7_is_object(this_obj) &&
       obj_prototype_v(v7, this_obj) != v7->vals.boolean_prototype)) {
    rcode = v7_throwf(v7, TYPE_ERROR,
                      "Boolean.valueOf called on non-boolean object");
    goto clean;
  }

  rcode = Obj_valueOf(v7, res);
  if (rcode != V7_OK) {
    goto clean;
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Boolean_toString(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);

  *res = V7_UNDEFINED;

  if (this_obj == v7->vals.boolean_prototype) {
    *res = v7_mk_string(v7, "false", 5, 1);
    goto clean;
  }

  if (!v7_is_boolean(this_obj) &&
      !(v7_is_generic_object(this_obj) &&
        is_prototype_of(v7, this_obj, v7->vals.boolean_prototype))) {
    rcode = v7_throwf(v7, TYPE_ERROR,
                      "Boolean.toString called on non-boolean object");
    goto clean;
  }

  rcode = obj_value_of(v7, this_obj, res);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = primitive_to_str(v7, *res, res, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

clean:
  return rcode;
}

V7_PRIVATE void init_boolean(struct v7 *v7) {
  val_t ctor = mk_cfunction_obj_with_proto(v7, Boolean_ctor, 1,
                                           v7->vals.boolean_prototype);
  v7_set(v7, v7->vals.global_object, "Boolean", 7, ctor);

  set_cfunc_prop(v7, v7->vals.boolean_prototype, "valueOf", Boolean_valueOf);
  set_cfunc_prop(v7, v7->vals.boolean_prototype, "toString", Boolean_toString);
}

#if defined(__cplusplus)
}
#endif /* __cplusplus */
