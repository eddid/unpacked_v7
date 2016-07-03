/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/primitive.h"
#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/gc.h"
#include "v7/src/object.h"

static val_t js_function_to_value(struct v7_js_function *o) {
  return pointer_to_value(o) | V7_TAG_FUNCTION;
}

V7_PRIVATE struct v7_js_function *get_js_function_struct(val_t v) {
  struct v7_js_function *ret = NULL;
  assert(is_js_function(v));
  ret = (struct v7_js_function *) get_ptr(v);
#if defined(V7_ENABLE_ENTITY_IDS)
  if (ret->base.entity_id_spec != V7_ENTITY_ID_PART_JS_FUNC) {
    fprintf(stderr, "entity_id: not a function!\n");
    abort();
  } else if (ret->base.entity_id_base != V7_ENTITY_ID_PART_OBJ) {
    fprintf(stderr, "entity_id: not an object!\n");
    abort();
  }
#endif
  return ret;
}

V7_PRIVATE
val_t mk_js_function(struct v7 *v7, struct v7_generic_object *scope,
                     val_t proto) {
  struct v7_js_function *f;
  val_t fval = V7_NULL;
  struct gc_tmp_frame tf = new_tmp_frame(v7);
  tmp_stack_push(&tf, &proto);
  tmp_stack_push(&tf, &fval);

  f = new_function(v7);

  if (f == NULL) {
    /* fval is left `null` */
    goto cleanup;
  }

#if defined(V7_ENABLE_ENTITY_IDS)
  f->base.entity_id_base = V7_ENTITY_ID_PART_OBJ;
  f->base.entity_id_spec = V7_ENTITY_ID_PART_JS_FUNC;
#endif

  fval = js_function_to_value(f);

  f->base.properties = NULL;
  f->scope = scope;

  /*
   * Before setting a `V7_OBJ_FUNCTION` flag, make sure we don't have
   * `V7_OBJ_DENSE_ARRAY` flag set
   */
  assert(!(f->base.attributes & V7_OBJ_DENSE_ARRAY));
  f->base.attributes |= V7_OBJ_FUNCTION;

  /* TODO(mkm): lazily create these properties on first access */
  if (v7_is_object(proto)) {
    v7_def(v7, proto, "constructor", 11, V7_DESC_ENUMERABLE(0), fval);
    v7_def(v7, fval, "prototype", 9,
           V7_DESC_ENUMERABLE(0) | V7_DESC_CONFIGURABLE(0), proto);
  }

cleanup:
  tmp_frame_cleanup(&tf);
  return fval;
}

V7_PRIVATE int is_js_function(val_t v) {
  return (v & V7_TAG_MASK) == V7_TAG_FUNCTION;
}

V7_PRIVATE
v7_val_t mk_cfunction_obj(struct v7 *v7, v7_cfunction_t *f, int num_args) {
  val_t obj = mk_object(v7, v7->vals.function_prototype);
  struct gc_tmp_frame tf = new_tmp_frame(v7);
  tmp_stack_push(&tf, &obj);
  v7_def(v7, obj, "", 0, _V7_DESC_HIDDEN(1), v7_mk_cfunction(f));
  if (num_args >= 0) {
    v7_def(v7, obj, "length", 6, (V7_DESC_ENUMERABLE(0) | V7_DESC_WRITABLE(0) |
                                  V7_DESC_CONFIGURABLE(0)),
           v7_mk_number(v7, num_args));
  }
  tmp_frame_cleanup(&tf);
  return obj;
}

V7_PRIVATE v7_val_t mk_cfunction_obj_with_proto(struct v7 *v7,
                                                v7_cfunction_t *f, int num_args,
                                                v7_val_t proto) {
  struct gc_tmp_frame tf = new_tmp_frame(v7);
  v7_val_t res = mk_cfunction_obj(v7, f, num_args);

  tmp_stack_push(&tf, &res);

  v7_def(v7, res, "prototype", 9, (V7_DESC_ENUMERABLE(0) | V7_DESC_WRITABLE(0) |
                                   V7_DESC_CONFIGURABLE(0)),
         proto);
  v7_def(v7, proto, "constructor", 11, V7_DESC_ENUMERABLE(0), res);
  tmp_frame_cleanup(&tf);
  return res;
}

V7_PRIVATE v7_val_t mk_cfunction_lite(v7_cfunction_t *f) {
  union {
    void *p;
    v7_cfunction_t *f;
  } u;
  u.f = f;
  return pointer_to_value(u.p) | V7_TAG_CFUNCTION;
}

V7_PRIVATE v7_cfunction_t *get_cfunction_ptr(struct v7 *v7, val_t v) {
  v7_cfunction_t *ret = NULL;

  if (is_cfunction_lite(v)) {
    /* Implementation is identical to get_ptr but is separate since
     * object pointers are not directly convertible to function pointers
     * according to ISO C and generates a warning in -Wpedantic mode. */
    ret = (v7_cfunction_t *) (uintptr_t)(v & 0xFFFFFFFFFFFFUL);
  } else {
    /* maybe cfunction object */

    /* extract the hidden property from a cfunction_object */
    struct v7_property *p;
    p = v7_get_own_property2(v7, v, "", 0, _V7_PROPERTY_HIDDEN);
    if (p != NULL) {
      /* yes, it's cfunction object. Extract cfunction pointer from it */
      ret = get_cfunction_ptr(v7, p->value);
    }
  }

  return ret;
}

V7_PRIVATE int is_cfunction_lite(val_t v) {
  return (v & V7_TAG_MASK) == V7_TAG_CFUNCTION;
}

V7_PRIVATE int is_cfunction_obj(struct v7 *v7, val_t v) {
  int ret = 0;
  if (v7_is_object(v)) {
    /* extract the hidden property from a cfunction_object */
    struct v7_property *p;
    p = v7_get_own_property2(v7, v, "", 0, _V7_PROPERTY_HIDDEN);
    if (p != NULL) {
      v = p->value;
    }

    ret = is_cfunction_lite(v);
  }
  return ret;
}

v7_val_t v7_mk_function(struct v7 *v7, v7_cfunction_t *f) {
  return mk_cfunction_obj(v7, f, -1);
}

v7_val_t v7_mk_function_with_proto(struct v7 *v7, v7_cfunction_t *f,
                                   v7_val_t proto) {
  return mk_cfunction_obj_with_proto(v7, f, ~0, proto);
}

v7_val_t v7_mk_cfunction(v7_cfunction_t *f) {
  return mk_cfunction_lite(f);
}

int v7_is_callable(struct v7 *v7, val_t v) {
  return is_js_function(v) || is_cfunction_lite(v) || is_cfunction_obj(v7, v);
}
