/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/primitive.h"
#include "v7/src/function.h"
#include "v7/src/gc.h"
#include "v7/src/object.h"
#include "v7/src/string.h"
#include "v7/src/array.h"
#include "v7/src/eval.h"
#include "v7/src/exceptions.h"
#include "v7/src/conversion.h"

/*
 * Default property attributes (see `v7_prop_attr_t`)
 */
#define V7_DEFAULT_PROPERTY_ATTRS 0

V7_PRIVATE val_t mk_object(struct v7 *v7, val_t prototype) {
  struct v7_generic_object *o = new_generic_object(v7);
  if (o == NULL) {
    return V7_NULL;
  }
  (void) v7;
#if defined(V7_ENABLE_ENTITY_IDS)
  o->base.entity_id_base = V7_ENTITY_ID_PART_OBJ;
  o->base.entity_id_spec = V7_ENTITY_ID_PART_GEN_OBJ;
#endif
  o->base.properties = NULL;
  obj_prototype_set(v7, &o->base, get_object_struct(prototype));
  return v7_object_to_value(&o->base);
}

v7_val_t v7_mk_object(struct v7 *v7) {
  return mk_object(v7, v7->vals.object_prototype);
}

V7_PRIVATE val_t v7_object_to_value(struct v7_object *o) {
  if (o == NULL) {
    return V7_NULL;
  } else if (o->attributes & V7_OBJ_FUNCTION) {
    return pointer_to_value(o) | V7_TAG_FUNCTION;
  } else {
    return pointer_to_value(o) | V7_TAG_OBJECT;
  }
}

V7_PRIVATE struct v7_generic_object *get_generic_object_struct(val_t v) {
  struct v7_generic_object *ret = NULL;
  if (v7_is_null(v)) {
    ret = NULL;
  } else {
    assert(v7_is_generic_object(v));
    ret = (struct v7_generic_object *) get_ptr(v);
#if defined(V7_ENABLE_ENTITY_IDS)
    if (ret->base.entity_id_base != V7_ENTITY_ID_PART_OBJ) {
      fprintf(stderr, "not a generic object!\n");
      abort();
    } else if (ret->base.entity_id_spec != V7_ENTITY_ID_PART_GEN_OBJ) {
      fprintf(stderr, "not an object (but is a generic object)!\n");
      abort();
    }
#endif
  }
  return ret;
}

V7_PRIVATE struct v7_object *get_object_struct(val_t v) {
  struct v7_object *ret = NULL;
  if (v7_is_null(v)) {
    ret = NULL;
  } else {
    assert(v7_is_object(v));
    ret = (struct v7_object *) get_ptr(v);
#if defined(V7_ENABLE_ENTITY_IDS)
    if (ret->entity_id_base != V7_ENTITY_ID_PART_OBJ) {
      fprintf(stderr, "not an object!\n");
      abort();
    }
#endif
  }
  return ret;
}

int v7_is_object(val_t v) {
  return (v & V7_TAG_MASK) == V7_TAG_OBJECT ||
         (v & V7_TAG_MASK) == V7_TAG_FUNCTION;
}

V7_PRIVATE int v7_is_generic_object(val_t v) {
  return (v & V7_TAG_MASK) == V7_TAG_OBJECT;
}

/* Object properties {{{ */

V7_PRIVATE struct v7_property *v7_mk_property(struct v7 *v7) {
  struct v7_property *p = new_property(v7);
#if defined(V7_ENABLE_ENTITY_IDS)
  p->entity_id = V7_ENTITY_ID_PROP;
#endif
  p->next = NULL;
  p->name = V7_UNDEFINED;
  p->value = V7_UNDEFINED;
  p->attributes = 0;
  return p;
}

V7_PRIVATE struct v7_property *v7_get_own_property2(struct v7 *v7, val_t obj,
                                                    const char *name,
                                                    size_t len,
                                                    v7_prop_attr_t attrs) {
  struct v7_property *p;
  struct v7_object *o;
  val_t ss;
  if (!v7_is_object(obj)) {
    return NULL;
  }
  if (len == (size_t) ~0) {
    len = strlen(name);
  }

  o = get_object_struct(obj);
  /*
   * len check is needed to allow getting the mbuf from the hidden property.
   * TODO(mkm): however hidden properties cannot be safely represented with
   * a zero length string anyway, so this will change.
   */
  if (o->attributes & V7_OBJ_DENSE_ARRAY && len > 0) {
    int ok, has;
    unsigned long i = cstr_to_ulong(name, len, &ok);
    if (ok) {
      v7->cur_dense_prop->value = v7_array_get2(v7, obj, i, &has);
      return has ? v7->cur_dense_prop : NULL;
    }
  }

  if (len <= 5) {
    ss = v7_mk_string(v7, name, len, 1);
    for (p = o->properties; p != NULL; p = p->next) {
#if defined(V7_ENABLE_ENTITY_IDS)
      if (p->entity_id != V7_ENTITY_ID_PROP) {
        fprintf(stderr, "not a prop!=0x%x\n", p->entity_id);
        abort();
      }
#endif
      if (p->name == ss && (attrs == 0 || (p->attributes & attrs))) {
        return p;
      }
    }
  } else {
    for (p = o->properties; p != NULL; p = p->next) {
      size_t n;
      const char *s = v7_get_string(v7, &p->name, &n);
#if defined(V7_ENABLE_ENTITY_IDS)
      if (p->entity_id != V7_ENTITY_ID_PROP) {
        fprintf(stderr, "not a prop!=0x%x\n", p->entity_id);
        abort();
      }
#endif
      if (n == len && strncmp(s, name, len) == 0 &&
          (attrs == 0 || (p->attributes & attrs))) {
        return p;
      }
    }
  }
  return NULL;
}

V7_PRIVATE struct v7_property *v7_get_own_property(struct v7 *v7, val_t obj,
                                                   const char *name,
                                                   size_t len) {
  return v7_get_own_property2(v7, obj, name, len, 0);
}

V7_PRIVATE struct v7_property *v7_get_property(struct v7 *v7, val_t obj,
                                               const char *name, size_t len) {
  if (!v7_is_object(obj)) {
    return NULL;
  }
  for (; obj != V7_NULL; obj = obj_prototype_v(v7, obj)) {
    struct v7_property *prop;
    if ((prop = v7_get_own_property(v7, obj, name, len)) != NULL) {
      return prop;
    }
  }
  return NULL;
}

V7_PRIVATE enum v7_err v7_get_property_v(struct v7 *v7, val_t obj,
                                         v7_val_t name,
                                         struct v7_property **res) {
  enum v7_err rcode = V7_OK;
  size_t name_len;
  STATIC char buf[8];
  const char *s = buf;
  uint8_t fr = 0;

  if (v7_is_string(name)) {
    s = v7_get_string(v7, &name, &name_len);
  } else {
    char *stmp;
    V7_TRY(v7_stringify_throwing(v7, name, buf, sizeof(buf),
                                 V7_STRINGIFY_DEFAULT, &stmp));
    s = stmp;
    if (s != buf) {
      fr = 1;
    }
    name_len = strlen(s);
  }

  *res = v7_get_property(v7, obj, s, name_len);

clean:
  if (fr) {
    free((void *) s);
  }
  return rcode;
}

WARN_UNUSED_RESULT
enum v7_err v7_get_throwing(struct v7 *v7, val_t obj, const char *name,
                            size_t name_len, val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t v = obj;
  if (v7_is_string(obj)) {
    v = v7->vals.string_prototype;
  } else if (v7_is_number(obj)) {
    v = v7->vals.number_prototype;
  } else if (v7_is_boolean(obj)) {
    v = v7->vals.boolean_prototype;
  } else if (v7_is_undefined(obj)) {
    rcode =
        v7_throwf(v7, TYPE_ERROR, "cannot read property '%.*s' of undefined",
                  (int) name_len, name);
    goto clean;
  } else if (v7_is_null(obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "cannot read property '%.*s' of null",
                      (int) name_len, name);
    goto clean;
  } else if (is_cfunction_lite(obj)) {
    v = v7->vals.function_prototype;
  }

  V7_TRY(
      v7_property_value(v7, obj, v7_get_property(v7, v, name, name_len), res));

clean:
  return rcode;
}

v7_val_t v7_get(struct v7 *v7, val_t obj, const char *name, size_t name_len) {
  enum v7_err rcode = V7_OK;
  uint8_t saved_is_thrown = 0;
  val_t saved_thrown = v7_get_thrown_value(v7, &saved_is_thrown);
  v7_val_t ret = V7_UNDEFINED;

  rcode = v7_get_throwing(v7, obj, name, name_len, &ret);
  if (rcode != V7_OK) {
    rcode = V7_OK;
    if (saved_is_thrown) {
      rcode = v7_throw(v7, saved_thrown);
    } else {
      v7_clear_thrown_value(v7);
    }
    ret = V7_UNDEFINED;
  }

  return ret;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err v7_get_throwing_v(struct v7 *v7, v7_val_t obj,
                                         v7_val_t name, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  size_t name_len;
  STATIC char buf[8];
  const char *s = buf;
  uint8_t fr = 0;

  /* subscripting strings */
  if (v7_is_string(obj)) {
    char ch;
    double dch = 0;

    rcode = v7_char_code_at(v7, obj, name, &dch);
    if (rcode != V7_OK) {
      goto clean;
    }

    if (!isnan(dch)) {
      ch = dch;
      *res = v7_mk_string(v7, &ch, 1, 1);
      goto clean;
    }
  }

  if (v7_is_string(name)) {
    s = v7_get_string(v7, &name, &name_len);
  } else {
    char *stmp;
    V7_TRY(v7_stringify_throwing(v7, name, buf, sizeof(buf),
                                 V7_STRINGIFY_DEFAULT, &stmp));
    s = stmp;
    if (s != buf) {
      fr = 1;
    }
    name_len = strlen(s);
  }
  V7_TRY(v7_get_throwing(v7, obj, s, name_len, res));

clean:
  if (fr) {
    free((void *) s);
  }
  return rcode;
}

V7_PRIVATE void v7_destroy_property(struct v7_property **p) {
  *p = NULL;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err v7_invoke_setter(struct v7 *v7, struct v7_property *prop,
                                        val_t obj, val_t val) {
  enum v7_err rcode = V7_OK;
  val_t setter = prop->value, args;
  v7_own(v7, &val);
  args = v7_mk_dense_array(v7);
  v7_own(v7, &args);
  if (prop->attributes & V7_PROPERTY_GETTER) {
    setter = v7_array_get(v7, prop->value, 1);
  }
  v7_array_set(v7, args, 0, val);
  v7_disown(v7, &args);
  v7_disown(v7, &val);
  {
    val_t val = V7_UNDEFINED;
    V7_TRY(b_apply(v7, setter, obj, args, 0, &val));
  }

clean:
  return rcode;
}

static v7_prop_attr_t apply_attrs_desc(v7_prop_attr_desc_t attrs_desc,
                                       v7_prop_attr_t old_attrs) {
  v7_prop_attr_t ret = old_attrs;
  if (old_attrs & V7_PROPERTY_NON_CONFIGURABLE) {
    /*
     * The property is non-configurable: we can only change it from being
     * writable to non-writable
     */

    if ((attrs_desc >> _V7_DESC_SHIFT) & V7_PROPERTY_NON_WRITABLE &&
        (attrs_desc & V7_PROPERTY_NON_WRITABLE)) {
      ret |= V7_PROPERTY_NON_WRITABLE;
    }

  } else {
    /* The property is configurable: we can change any attributes */
    ret = (old_attrs & ~(attrs_desc >> _V7_DESC_SHIFT)) |
          (attrs_desc & _V7_DESC_MASK);
  }

  return ret;
}

int v7_def(struct v7 *v7, val_t obj, const char *name, size_t len,
           v7_prop_attr_desc_t attrs_desc, v7_val_t val) {
  enum v7_err rcode = V7_OK;
  uint8_t saved_is_thrown = 0;
  val_t saved_thrown = v7_get_thrown_value(v7, &saved_is_thrown);
  int ret = -1;

  {
    struct v7_property *tmp = NULL;
    rcode = def_property(v7, obj, name, len, attrs_desc, val, 0 /*not assign*/,
                         &tmp);
    ret = (tmp == NULL) ? -1 : 0;
  }

  if (rcode != V7_OK) {
    rcode = V7_OK;
    if (saved_is_thrown) {
      rcode = v7_throw(v7, saved_thrown);
    } else {
      v7_clear_thrown_value(v7);
    }
    ret = -1;
  }

  return ret;
}

int v7_set(struct v7 *v7, val_t obj, const char *name, size_t len,
           v7_val_t val) {
  enum v7_err rcode = V7_OK;
  uint8_t saved_is_thrown = 0;
  val_t saved_thrown = v7_get_thrown_value(v7, &saved_is_thrown);
  int ret = -1;

  {
    struct v7_property *tmp = NULL;
    rcode = set_property(v7, obj, name, len, val, &tmp);
    ret = (tmp == NULL) ? -1 : 0;
  }

  if (rcode != V7_OK) {
    rcode = V7_OK;
    if (saved_is_thrown) {
      rcode = v7_throw(v7, saved_thrown);
    } else {
      v7_clear_thrown_value(v7);
    }
    ret = -1;
  }

  return ret;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err set_property_v(struct v7 *v7, val_t obj, val_t name,
                                      val_t val, struct v7_property **res) {
  return def_property_v(v7, obj, name, 0, val, 1 /*as_assign*/, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err set_property(struct v7 *v7, val_t obj, const char *name,
                                    size_t len, v7_val_t val,
                                    struct v7_property **res) {
  return def_property(v7, obj, name, len, 0, val, 1 /*as_assign*/, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err def_property_v(struct v7 *v7, val_t obj, val_t name,
                                      v7_prop_attr_desc_t attrs_desc, val_t val,
                                      uint8_t as_assign,
                                      struct v7_property **res) {
  enum v7_err rcode = V7_OK;
  struct v7_property *prop = NULL;
  size_t len;
  const char *n = v7_get_string(v7, &name, &len);

  v7_own(v7, &name);
  v7_own(v7, &val);

  if (!v7_is_object(obj)) {
    prop = NULL;
    goto clean;
  }

  prop = v7_get_own_property(v7, obj, n, len);
  if (prop == NULL) {
    /*
     * The own property with given `name` doesn't exist yet: try to create it,
     * set requested `name` and `attributes`, and append to the object's
     * properties
     */

    /* make sure the object is extensible */
    if (get_object_struct(obj)->attributes & V7_OBJ_NOT_EXTENSIBLE) {
      /*
       * We should throw if we use `Object.defineProperty`, or if we're in
       * strict mode.
       */
      if (is_strict_mode(v7) || !as_assign) {
        V7_THROW(v7_throwf(v7, TYPE_ERROR, "Object is not extensible"));
      }
      prop = NULL;
      goto clean;
    }

    if ((prop = v7_mk_property(v7)) == NULL) {
      prop = NULL; /* LCOV_EXCL_LINE */
      goto clean;
    }
    prop->name = name;
    prop->value = val;
    prop->attributes = apply_attrs_desc(attrs_desc, V7_DEFAULT_PROPERTY_ATTRS);

    prop->next = get_object_struct(obj)->properties;
    get_object_struct(obj)->properties = prop;
    goto clean;
  } else {
    /* Property already exists */

    if (prop->attributes & V7_PROPERTY_NON_WRITABLE) {
      /* The property is read-only */

      if (as_assign) {
        /* Plain assignment: in strict mode throw, otherwise ignore */
        if (is_strict_mode(v7)) {
          V7_THROW(
              v7_throwf(v7, TYPE_ERROR, "Cannot assign to read-only property"));
        } else {
          prop = NULL;
          goto clean;
        }
      } else if (prop->attributes & V7_PROPERTY_NON_CONFIGURABLE) {
        /*
         * Use `Object.defineProperty` semantic, and the property is
         * non-configurable: if no value is provided, or if new value is equal
         * to the existing one, then just fall through to change attributes;
         * otherwise, throw.
         */

        if (!(attrs_desc & V7_DESC_PRESERVE_VALUE)) {
          uint8_t equal = 0;
          if (v7_is_string(val) && v7_is_string(prop->value)) {
            equal = (s_cmp(v7, val, prop->value) == 0);
          } else {
            equal = (val == prop->value);
          }

          if (!equal) {
            /* Values are not equal: should throw */
            V7_THROW(v7_throwf(v7, TYPE_ERROR,
                               "Cannot redefine read-only property"));
          } else {
            /*
             * Values are equal. Will fall through so that attributes might
             * change.
             */
          }
        } else {
          /*
           * No value is provided. Will fall through so that attributes might
           * change.
           */
        }
      } else {
        /*
         * Use `Object.defineProperty` semantic, and the property is
         * configurable: will fall through and assign new value, effectively
         * ignoring non-writable flag. This is the same as making a property
         * writable, then assigning a new value, and making a property
         * non-writable again.
         */
      }
    } else if (prop->attributes & V7_PROPERTY_SETTER) {
      /* Invoke setter */
      V7_TRY(v7_invoke_setter(v7, prop, obj, val));
      prop = NULL;
      goto clean;
    }

    /* Set value and apply attrs delta */
    if (!(attrs_desc & V7_DESC_PRESERVE_VALUE)) {
      prop->value = val;
    }
    prop->attributes = apply_attrs_desc(attrs_desc, prop->attributes);
  }

clean:

  if (res != NULL) {
    *res = prop;
  }

  v7_disown(v7, &val);
  v7_disown(v7, &name);

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err def_property(struct v7 *v7, val_t obj, const char *name,
                                    size_t len, v7_prop_attr_desc_t attrs_desc,
                                    v7_val_t val, uint8_t as_assign,
                                    struct v7_property **res) {
  enum v7_err rcode = V7_OK;
  val_t name_val = V7_UNDEFINED;

  v7_own(v7, &obj);
  v7_own(v7, &val);
  v7_own(v7, &name_val);

  if (len == (size_t) ~0) {
    len = strlen(name);
  }

  name_val = v7_mk_string(v7, name, len, 1);
  V7_TRY(def_property_v(v7, obj, name_val, attrs_desc, val, as_assign, res));

clean:
  v7_disown(v7, &name_val);
  v7_disown(v7, &val);
  v7_disown(v7, &obj);

  return rcode;
}

V7_PRIVATE int set_method(struct v7 *v7, v7_val_t obj, const char *name,
                          v7_cfunction_t *func, int num_args) {
  return v7_def(v7, obj, name, strlen(name), V7_DESC_ENUMERABLE(0),
                mk_cfunction_obj(v7, func, num_args));
}

int v7_set_method(struct v7 *v7, v7_val_t obj, const char *name,
                  v7_cfunction_t *func) {
  return set_method(v7, obj, name, func, ~0);
}

V7_PRIVATE int set_cfunc_prop(struct v7 *v7, val_t o, const char *name,
                              v7_cfunction_t *f) {
  return v7_def(v7, o, name, strlen(name), V7_DESC_ENUMERABLE(0),
                v7_mk_cfunction(f));
}

/*
 * See comments in `object_public.h`
 */
int v7_del(struct v7 *v7, val_t obj, const char *name, size_t len) {
  struct v7_property *prop, *prev;

  if (!v7_is_object(obj)) {
    return -1;
  }
  if (len == (size_t) ~0) {
    len = strlen(name);
  }
  for (prev = NULL, prop = get_object_struct(obj)->properties; prop != NULL;
       prev = prop, prop = prop->next) {
    size_t n;
    const char *s = v7_get_string(v7, &prop->name, &n);
    if (n == len && strncmp(s, name, len) == 0) {
      if (prev) {
        prev->next = prop->next;
      } else {
        get_object_struct(obj)->properties = prop->next;
      }
      v7_destroy_property(&prop);
      return 0;
    }
  }
  return -1;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err v7_property_value(struct v7 *v7, val_t obj,
                                         struct v7_property *p, val_t *res) {
  enum v7_err rcode = V7_OK;
  if (p == NULL) {
    *res = V7_UNDEFINED;
    goto clean;
  }
  if (p->attributes & V7_PROPERTY_GETTER) {
    val_t getter = p->value;
    if (p->attributes & V7_PROPERTY_SETTER) {
      getter = v7_array_get(v7, p->value, 0);
    }
    {
      V7_TRY(b_apply(v7, getter, obj, V7_UNDEFINED, 0, res));
      goto clean;
    }
  }

  *res = p->value;
  goto clean;

clean:
  return rcode;
}

void *v7_next_prop(void *handle, v7_val_t obj, v7_val_t *name, v7_val_t *value,
                   v7_prop_attr_t *attrs) {
  struct v7_property *p;
  if (handle == NULL) {
    p = get_object_struct(obj)->properties;
  } else {
    p = ((struct v7_property *) handle)->next;
  }
  if (p != NULL) {
    if (name != NULL) *name = p->name;
    if (value != NULL) *value = p->value;
    if (attrs != NULL) *attrs = p->attributes;
  }
  return p;
}

/* }}} Object properties */

/* Object prototypes {{{ */

V7_PRIVATE int obj_prototype_set(struct v7 *v7, struct v7_object *obj,
                                 struct v7_object *proto) {
  int ret = -1;
  (void) v7;

  if (obj->attributes & V7_OBJ_FUNCTION) {
    ret = -1;
  } else {
    ((struct v7_generic_object *) obj)->prototype = proto;
    ret = 0;
  }

  return ret;
}

V7_PRIVATE struct v7_object *obj_prototype(struct v7 *v7,
                                           struct v7_object *obj) {
  if (obj->attributes & V7_OBJ_FUNCTION) {
    return get_object_struct(v7->vals.function_prototype);
  } else {
    return ((struct v7_generic_object *) obj)->prototype;
  }
}

V7_PRIVATE val_t obj_prototype_v(struct v7 *v7, val_t obj) {
  /*
   * NOTE: we don't use v7_is_callable() here, because it involves walking
   * through the object's properties, which may be expensive. And it's done
   * anyway for cfunction objects as it would for any other generic objects by
   * the call to `obj_prototype()`.
   *
   * Since this function is called quite often (at least, GC walks the
   * prototype chain), it's better to just handle cfunction objects as generic
   * objects.
   */
  if (is_js_function(obj) || is_cfunction_lite(obj)) {
    return v7->vals.function_prototype;
  }
  return v7_object_to_value(obj_prototype(v7, get_object_struct(obj)));
}

V7_PRIVATE int is_prototype_of(struct v7 *v7, val_t o, val_t p) {
  if (!v7_is_object(o) || !v7_is_object(p)) {
    return 0;
  }

  /* walk the prototype chain */
  for (; !v7_is_null(o); o = obj_prototype_v(v7, o)) {
    if (obj_prototype_v(v7, o) == p) {
      return 1;
    }
  }
  return 0;
}

int v7_is_instanceof(struct v7 *v7, val_t o, const char *c) {
  return v7_is_instanceof_v(v7, o, v7_get(v7, v7->vals.global_object, c, ~0));
}

int v7_is_instanceof_v(struct v7 *v7, val_t o, val_t c) {
  return is_prototype_of(v7, o, v7_get(v7, c, "prototype", 9));
}

v7_val_t v7_set_proto(struct v7 *v7, v7_val_t obj, v7_val_t proto) {
  if (v7_is_generic_object(obj)) {
    v7_val_t old_proto =
        v7_object_to_value(obj_prototype(v7, get_object_struct(obj)));
    obj_prototype_set(v7, get_object_struct(obj), get_object_struct(proto));
    return old_proto;
  } else {
    return V7_UNDEFINED;
  }
}

V7_PRIVATE struct v7_property *get_user_data_property(v7_val_t obj) {
  struct v7_property *p;
  struct v7_object *o;
  if (!v7_is_object(obj)) return NULL;
  o = get_object_struct(obj);

  for (p = o->properties; p != NULL; p = p->next) {
    if (p->attributes & _V7_PROPERTY_USER_DATA_AND_DESTRUCTOR) {
      return p;
    }
  }

  return NULL;
}

/*
 * Returns the user data property structure associated with obj, or NULL if
 * `obj` is not an object.
 */
static struct v7_property *get_or_create_user_data_property(struct v7 *v7,
                                                            v7_val_t obj) {
  struct v7_property *p = get_user_data_property(obj);
  struct v7_object *o;

  if (p != NULL) return p;

  if (!v7_is_object(obj)) return NULL;
  o = get_object_struct(obj);
  v7_own(v7, &obj);
  p = v7_mk_property(v7);
  v7_disown(v7, &obj);

  p->attributes |= _V7_PROPERTY_USER_DATA_AND_DESTRUCTOR | _V7_PROPERTY_HIDDEN;

  p->next = o->properties;
  o->properties = p;

  return p;
}

void v7_set_user_data(struct v7 *v7, v7_val_t obj, void *ud) {
  struct v7_property *p = get_or_create_user_data_property(v7, obj);
  if (p == NULL) return;
  p->value = v7_mk_foreign(v7, ud);
}

void *v7_get_user_data(struct v7 *v7, v7_val_t obj) {
  struct v7_property *p = get_user_data_property(obj);
  (void) v7;
  if (p == NULL) return NULL;
  return v7_get_ptr(v7, p->value);
}

void v7_set_destructor_cb(struct v7 *v7, v7_val_t obj, v7_destructor_cb_t *d) {
  struct v7_property *p = get_or_create_user_data_property(v7, obj);
  struct v7_object *o;
  union {
    void *v;
    v7_destructor_cb_t *f;
  } fu;

  if (p == NULL) return;

  o = get_object_struct(obj);
  if (d != NULL) {
    o->attributes |= V7_OBJ_HAS_DESTRUCTOR;
    fu.f = d;
    p->name = v7_mk_foreign(v7, fu.v);
  } else {
    o->attributes &= ~V7_OBJ_HAS_DESTRUCTOR;
    p->name = V7_UNDEFINED;
  }
}

/* }}} Object prototypes */
