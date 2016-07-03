/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/str_util.h"
#include "v7/src/internal.h"
#include "v7/src/std_object.h"
#include "v7/src/function.h"
#include "v7/src/core.h"
#include "v7/src/conversion.h"
#include "v7/src/array.h"
#include "v7/src/object.h"
#include "v7/src/exceptions.h"
#include "v7/src/primitive.h"
#include "v7/src/string.h"
#include "v7/src/regexp.h"
#include "v7/src/exec.h"

#if V7_ENABLE__Object__getPrototypeOf
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_getPrototypeOf(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t arg = v7_arg(v7, 0);

  if (!v7_is_object(arg)) {
    rcode =
        v7_throwf(v7, TYPE_ERROR, "Object.getPrototypeOf called on non-object");
    goto clean;
  }
  *res = obj_prototype_v(v7, arg);

clean:
  return rcode;
}
#endif

#if V7_ENABLE__Object__isPrototypeOf
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_isPrototypeOf(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t obj = v7_arg(v7, 0);
  val_t proto = v7_get_this(v7);

  *res = v7_mk_boolean(v7, is_prototype_of(v7, obj, proto));

  return rcode;
}
#endif

#if V7_ENABLE__Object__getOwnPropertyNames || V7_ENABLE__Object__keys
/*
 * Hack to ensure that the iteration order of the keys array is consistent
 * with the iteration order if properties in `for in`
 * This will be obsoleted when arrays will have a special object type.
 */
static void _Obj_append_reverse(struct v7 *v7, struct v7_property *p, val_t res,
                                int i, v7_prop_attr_t ignore_flags) {
  while (p && p->attributes & ignore_flags) p = p->next;
  if (p == NULL) return;
  if (p->next) _Obj_append_reverse(v7, p->next, res, i + 1, ignore_flags);

  v7_array_set(v7, res, i, p->name);
}

WARN_UNUSED_RESULT
static enum v7_err _Obj_ownKeys(struct v7 *v7, unsigned int ignore_flags,
                                val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t obj = v7_arg(v7, 0);

  *res = v7_mk_dense_array(v7);

  if (!v7_is_object(obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Object.keys called on non-object");
    goto clean;
  }

  _Obj_append_reverse(v7, get_object_struct(obj)->properties, *res, 0,
                      ignore_flags);

clean:
  return rcode;
}
#endif

#if V7_ENABLE__Object__hasOwnProperty ||       \
    V7_ENABLE__Object__propertyIsEnumerable || \
    V7_ENABLE__Object__getOwnPropertyDescriptor
static enum v7_err _Obj_getOwnProperty(struct v7 *v7, val_t obj, val_t name,
                                       struct v7_property **res) {
  enum v7_err rcode = V7_OK;
  char name_buf[512];
  size_t name_len;

  rcode = to_string(v7, name, NULL, name_buf, sizeof(name_buf), &name_len);
  if (rcode != V7_OK) {
    goto clean;
  }

  *res = v7_get_own_property(v7, obj, name_buf, name_len);

clean:
  return rcode;
}
#endif

#if V7_ENABLE__Object__keys
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_keys(struct v7 *v7, v7_val_t *res) {
  return _Obj_ownKeys(v7, _V7_PROPERTY_HIDDEN | V7_PROPERTY_NON_ENUMERABLE,
                      res);
}
#endif

#if V7_ENABLE__Object__getOwnPropertyNames
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_getOwnPropertyNames(struct v7 *v7, v7_val_t *res) {
  return _Obj_ownKeys(v7, _V7_PROPERTY_HIDDEN, res);
}
#endif

#if V7_ENABLE__Object__getOwnPropertyDescriptor
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_getOwnPropertyDescriptor(struct v7 *v7,
                                                    v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  struct v7_property *prop;
  val_t obj = v7_arg(v7, 0);
  val_t name = v7_arg(v7, 1);
  val_t desc;

  rcode = _Obj_getOwnProperty(v7, obj, name, &prop);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (prop == NULL) {
    goto clean;
  }

  desc = v7_mk_object(v7);
  v7_set(v7, desc, "value", 5, prop->value);
  v7_set(v7, desc, "writable", 8,
         v7_mk_boolean(v7, !(prop->attributes & V7_PROPERTY_NON_WRITABLE)));
  v7_set(v7, desc, "enumerable", 10,
         v7_mk_boolean(v7, !(prop->attributes & (_V7_PROPERTY_HIDDEN |
                                                 V7_PROPERTY_NON_ENUMERABLE))));
  v7_set(v7, desc, "configurable", 12,
         v7_mk_boolean(v7, !(prop->attributes & V7_PROPERTY_NON_CONFIGURABLE)));

  *res = desc;

clean:
  return rcode;
}
#endif

WARN_UNUSED_RESULT
static enum v7_err o_set_attr(struct v7 *v7, val_t desc, const char *name,
                              size_t n, v7_prop_attr_desc_t *pattrs_delta,
                              v7_prop_attr_desc_t flag_true,
                              v7_prop_attr_desc_t flag_false) {
  enum v7_err rcode = V7_OK;

  val_t v = V7_UNDEFINED;
  rcode = v7_get_throwing(v7, desc, name, n, &v);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (v7_is_truthy(v7, v)) {
    *pattrs_delta |= flag_true;
  } else {
    *pattrs_delta |= flag_false;
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
static enum v7_err _Obj_defineProperty(struct v7 *v7, val_t obj,
                                       const char *name, int name_len,
                                       val_t desc, val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t val = V7_UNDEFINED;
  v7_prop_attr_desc_t attrs_desc = 0;

  /*
   * get provided value, or set `V7_DESC_PRESERVE_VALUE` flag if no value is
   * provided at all
   */
  {
    struct v7_property *prop = v7_get_property(v7, desc, "value", 5);
    if (prop == NULL) {
      /* no value is provided */
      attrs_desc |= V7_DESC_PRESERVE_VALUE;
    } else {
      /* value is provided: use it */
      rcode = v7_property_value(v7, desc, prop, &val);
      if (rcode != V7_OK) {
        goto clean;
      }
    }
  }

  /* Examine given properties, and set appropriate flags for `def_property` */

  rcode = o_set_attr(v7, desc, "enumerable", 10, &attrs_desc,
                     V7_DESC_ENUMERABLE(1), V7_DESC_ENUMERABLE(0));
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = o_set_attr(v7, desc, "writable", 8, &attrs_desc, V7_DESC_WRITABLE(1),
                     V7_DESC_WRITABLE(0));
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = o_set_attr(v7, desc, "configurable", 12, &attrs_desc,
                     V7_DESC_CONFIGURABLE(1), V7_DESC_CONFIGURABLE(0));
  if (rcode != V7_OK) {
    goto clean;
  }

  /* TODO(dfrank) : add getter/setter support */

  /* Finally, do the job on defining the property */
  rcode = def_property(v7, obj, name, name_len, attrs_desc, val,
                       0 /*not assign*/, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

  *res = obj;
  goto clean;

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_defineProperty(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t obj = v7_arg(v7, 0);
  val_t name = v7_arg(v7, 1);
  val_t desc = v7_arg(v7, 2);
  char name_buf[512];
  size_t name_len;

  if (!v7_is_object(obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "object expected");
    goto clean;
  }

  rcode = to_string(v7, name, NULL, name_buf, sizeof(name_buf), &name_len);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = _Obj_defineProperty(v7, obj, name_buf, name_len, desc, res);
  goto clean;

clean:
  return rcode;
}

#if V7_ENABLE__Object__create || V7_ENABLE__Object__defineProperties
WARN_UNUSED_RESULT
static enum v7_err o_define_props(struct v7 *v7, val_t obj, val_t descs,
                                  val_t *res) {
  enum v7_err rcode = V7_OK;
  struct v7_property *p;

  if (!v7_is_object(descs)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "object expected");
    goto clean;
  }

  for (p = get_object_struct(descs)->properties; p; p = p->next) {
    size_t n;
    const char *s = v7_get_string(v7, &p->name, &n);
    if (p->attributes & (_V7_PROPERTY_HIDDEN | V7_PROPERTY_NON_ENUMERABLE)) {
      continue;
    }
    rcode = _Obj_defineProperty(v7, obj, s, n, p->value, res);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

clean:
  return rcode;
}
#endif

#if V7_ENABLE__Object__defineProperties
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_defineProperties(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t descs = V7_UNDEFINED;

  *res = v7_arg(v7, 0);
  descs = v7_arg(v7, 1);
  rcode = o_define_props(v7, *res, descs, res);
  if (rcode != V7_OK) {
    goto clean;
  }

clean:
  return rcode;
}
#endif

#if V7_ENABLE__Object__create
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_create(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t proto = v7_arg(v7, 0);
  val_t descs = v7_arg(v7, 1);
  if (!v7_is_null(proto) && !v7_is_object(proto)) {
    rcode = v7_throwf(v7, TYPE_ERROR,
                      "Object prototype may only be an Object or null");
    goto clean;
  }
  *res = mk_object(v7, proto);
  if (v7_is_object(descs)) {
    rcode = o_define_props(v7, *res, descs, res);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

clean:
  return rcode;
}
#endif

#if V7_ENABLE__Object__propertyIsEnumerable
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_propertyIsEnumerable(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  struct v7_property *prop;
  val_t name = v7_arg(v7, 0);

  rcode = _Obj_getOwnProperty(v7, this_obj, name, &prop);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (prop == NULL) {
    *res = v7_mk_boolean(v7, 0);
  } else {
    *res =
        v7_mk_boolean(v7, !(prop->attributes & (_V7_PROPERTY_HIDDEN |
                                                V7_PROPERTY_NON_ENUMERABLE)));
  }

  goto clean;

clean:
  return rcode;
}
#endif

#if V7_ENABLE__Object__hasOwnProperty
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_hasOwnProperty(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t name = v7_arg(v7, 0);
  struct v7_property *ptmp = NULL;

  rcode = _Obj_getOwnProperty(v7, this_obj, name, &ptmp);
  if (rcode != V7_OK) {
    goto clean;
  }

  *res = v7_mk_boolean(v7, ptmp != NULL);
  goto clean;

clean:
  return rcode;
}
#endif

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_valueOf(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  struct v7_property *p;

  *res = this_obj;

  if (v7_is_regexp(v7, this_obj)) {
    /* res is `this_obj` */
    goto clean;
  }

  p = v7_get_own_property2(v7, this_obj, "", 0, _V7_PROPERTY_HIDDEN);
  if (p != NULL) {
    *res = p->value;
    goto clean;
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_toString(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t ctor, name, this_obj = v7_get_this(v7);
  char buf[20];
  const char *str = "Object";
  size_t name_len = ~0;

  if (v7_is_undefined(this_obj)) {
    str = "Undefined";
  } else if (v7_is_null(this_obj)) {
    str = "Null";
  } else if (v7_is_number(this_obj)) {
    str = "Number";
  } else if (v7_is_boolean(this_obj)) {
    str = "Boolean";
  } else if (v7_is_string(this_obj)) {
    str = "String";
  } else if (v7_is_callable(v7, this_obj)) {
    str = "Function";
  } else {
    rcode = v7_get_throwing(v7, this_obj, "constructor", ~0, &ctor);
    if (rcode != V7_OK) {
      goto clean;
    }

    if (!v7_is_undefined(ctor)) {
      rcode = v7_get_throwing(v7, ctor, "name", ~0, &name);
      if (rcode != V7_OK) {
        goto clean;
      }

      if (!v7_is_undefined(name)) {
        size_t tmp_len;
        const char *tmp_str;
        tmp_str = v7_get_string(v7, &name, &tmp_len);
        /*
         * objects constructed with an anonymous constructor are represented as
         * Object, ch11/11.1/11.1.1/S11.1.1_A4.2.js
         */
        if (tmp_len > 0) {
          str = tmp_str;
          name_len = tmp_len;
        }
      }
    }
  }

  if (name_len == (size_t) ~0) {
    name_len = strlen(str);
  }

  c_snprintf(buf, sizeof(buf), "[object %.*s]", (int) name_len, str);
  *res = v7_mk_string(v7, buf, strlen(buf), 1);

clean:
  return rcode;
}

#if V7_ENABLE__Object__preventExtensions
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_preventExtensions(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t arg = v7_arg(v7, 0);
  if (!v7_is_object(arg)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Object expected");
    goto clean;
  }
  get_object_struct(arg)->attributes |= V7_OBJ_NOT_EXTENSIBLE;
  *res = arg;

clean:
  return rcode;
}
#endif

#if V7_ENABLE__Object__isExtensible
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_isExtensible(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t arg = v7_arg(v7, 0);

  if (!v7_is_object(arg)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Object expected");
    goto clean;
  }

  *res = v7_mk_boolean(
      v7, !(get_object_struct(arg)->attributes & V7_OBJ_NOT_EXTENSIBLE));

clean:
  return rcode;
}
#endif

#if V7_ENABLE__Object__isFrozen || V7_ENABLE__Object__isSealed
static enum v7_err is_rigid(struct v7 *v7, v7_val_t *res, int is_frozen) {
  enum v7_err rcode = V7_OK;
  val_t arg = v7_arg(v7, 0);

  if (!v7_is_object(arg)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Object expected");
    goto clean;
  }

  *res = v7_mk_boolean(v7, 0);

  if (get_object_struct(arg)->attributes & V7_OBJ_NOT_EXTENSIBLE) {
    void *h = NULL;
    v7_prop_attr_t attrs;
    while ((h = v7_next_prop(h, arg, NULL, NULL, &attrs)) != NULL) {
      if (!(attrs & V7_PROPERTY_NON_CONFIGURABLE)) {
        goto clean;
      }
      if (is_frozen) {
        if (!(attrs & V7_PROPERTY_SETTER) &&
            !(attrs & V7_PROPERTY_NON_WRITABLE)) {
          goto clean;
        }
      }
    }

    *res = v7_mk_boolean(v7, 1);
    goto clean;
  }

clean:
  return rcode;
}
#endif

#if V7_ENABLE__Object__isSealed
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_isSealed(struct v7 *v7, v7_val_t *res) {
  return is_rigid(v7, res, 0 /* is_frozen */);
}
#endif

#if V7_ENABLE__Object__isFrozen
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Obj_isFrozen(struct v7 *v7, v7_val_t *res) {
  return is_rigid(v7, res, 1 /* is_frozen */);
}
#endif

static const char js_function_Object[] =
    "function Object(v) {"
    "if (typeof v === 'boolean') return new Boolean(v);"
    "if (typeof v === 'number') return new Number(v);"
    "if (typeof v === 'string') return new String(v);"
    "if (typeof v === 'date') return new Date(v);"
    "}";

V7_PRIVATE void init_object(struct v7 *v7) {
  enum v7_err rcode = V7_OK;
  val_t object, v;
  /* TODO(mkm): initialize global object without requiring a parser */
  rcode = v7_exec(v7, js_function_Object, &v);
  assert(rcode == V7_OK);
#if defined(NDEBUG)
  (void) rcode;
#endif

  object = v7_get(v7, v7->vals.global_object, "Object", 6);
  v7_set(v7, object, "prototype", 9, v7->vals.object_prototype);
  v7_def(v7, v7->vals.object_prototype, "constructor", 11,
         V7_DESC_ENUMERABLE(0), object);

  set_method(v7, v7->vals.object_prototype, "toString", Obj_toString, 0);
#if V7_ENABLE__Object__getPrototypeOf
  set_cfunc_prop(v7, object, "getPrototypeOf", Obj_getPrototypeOf);
#endif
#if V7_ENABLE__Object__getOwnPropertyDescriptor
  set_cfunc_prop(v7, object, "getOwnPropertyDescriptor",
                 Obj_getOwnPropertyDescriptor);
#endif

  /* defineProperty is currently required to perform stdlib initialization */
  set_method(v7, object, "defineProperty", Obj_defineProperty, 3);

#if V7_ENABLE__Object__defineProperties
  set_cfunc_prop(v7, object, "defineProperties", Obj_defineProperties);
#endif
#if V7_ENABLE__Object__create
  set_cfunc_prop(v7, object, "create", Obj_create);
#endif
#if V7_ENABLE__Object__keys
  set_cfunc_prop(v7, object, "keys", Obj_keys);
#endif
#if V7_ENABLE__Object__getOwnPropertyNames
  set_cfunc_prop(v7, object, "getOwnPropertyNames", Obj_getOwnPropertyNames);
#endif
#if V7_ENABLE__Object__preventExtensions
  set_method(v7, object, "preventExtensions", Obj_preventExtensions, 1);
#endif
#if V7_ENABLE__Object__isExtensible
  set_method(v7, object, "isExtensible", Obj_isExtensible, 1);
#endif
#if V7_ENABLE__Object__isSealed
  set_method(v7, object, "isSealed", Obj_isSealed, 1);
#endif
#if V7_ENABLE__Object__isFrozen
  set_method(v7, object, "isFrozen", Obj_isFrozen, 1);
#endif

#if V7_ENABLE__Object__propertyIsEnumerable
  set_cfunc_prop(v7, v7->vals.object_prototype, "propertyIsEnumerable",
                 Obj_propertyIsEnumerable);
#endif
#if V7_ENABLE__Object__hasOwnProperty
  set_cfunc_prop(v7, v7->vals.object_prototype, "hasOwnProperty",
                 Obj_hasOwnProperty);
#endif
#if V7_ENABLE__Object__isPrototypeOf
  set_cfunc_prop(v7, v7->vals.object_prototype, "isPrototypeOf",
                 Obj_isPrototypeOf);
#endif
  set_cfunc_prop(v7, v7->vals.object_prototype, "valueOf", Obj_valueOf);
}
