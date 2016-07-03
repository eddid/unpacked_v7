/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/util.h"
#include "v7/src/freeze.h"
#include "v7/src/bcode.h"
#include "v7/src/gc.h"
#include "common/base64.h"
#include "v7/src/object.h"

#include <stdio.h>

#ifdef V7_FREEZE

V7_PRIVATE void freeze(struct v7 *v7, char *filename) {
  size_t i;

  v7->freeze_file = fopen(filename, "w");
  assert(v7->freeze_file != NULL);

#ifndef V7_FREEZE_NOT_READONLY
  /*
   * We have to remove `global` from the global object since
   * when thawing global will actually be a new mutable object
   * living on the heap.
   */
  v7_del(v7, v7->vals.global_object, "global", 6);
#endif

  for (i = 0; i < sizeof(v7->vals) / sizeof(val_t); i++) {
    val_t v = ((val_t *) &v7->vals)[i];
    fprintf(v7->freeze_file,
            "{\"type\":\"global\", \"idx\":%zu, \"value\":\"%p\"}\n", i,
            (void *) (v7_is_object(v) ? get_object_struct(v) : 0x0));
  }

  /*
   * since v7->freeze_file is not NULL this will cause freeze_obj and
   * freeze_prop to be called for each reachable object and property.
   */
  v7_gc(v7, 1);
  assert(v7->stack.len == 0);

  fclose(v7->freeze_file);
  v7->freeze_file = NULL;
}

static char *freeze_vec(struct v7_vec *vec) {
  char *res = (char *) malloc(512 + vec->len);
  res[0] = '"';
  cs_base64_encode((const unsigned char *) vec->p, vec->len, &res[1]);
  strcat(res, "\"");
  return res;
}

V7_PRIVATE void freeze_obj(struct v7 *v7, FILE *f, v7_val_t v) {
  struct v7_object *obj_base = get_object_struct(v);
  unsigned int attrs = V7_OBJ_OFF_HEAP;

#ifndef V7_FREEZE_NOT_READONLY
  attrs |= V7_OBJ_NOT_EXTENSIBLE;
#endif

  if (is_js_function(v)) {
    struct v7_js_function *func = get_js_function_struct(v);
    struct bcode *bcode = func->bcode;
    char *jops = freeze_vec(&bcode->ops);
    int i;

    fprintf(f,
            "{\"type\":\"func\", \"addr\":\"%p\", \"props\":\"%p\", "
            "\"attrs\":%d, \"scope\":\"%p\", \"bcode\":\"%p\""
#if defined(V7_ENABLE_ENTITY_IDS)
            ", \"entity_id_base\":%d, \"entity_id_spec\":\"%d\" "
#endif
            "}\n",
            (void *) obj_base,
            (void *) ((uintptr_t) obj_base->properties & ~0x1),
            obj_base->attributes | attrs, (void *) func->scope, (void *) bcode
#if defined(V7_ENABLE_ENTITY_IDS)
            ,
            obj_base->entity_id_base, obj_base->entity_id_spec
#endif
            );
    fprintf(f,
            "{\"type\":\"bcode\", \"addr\":\"%p\", \"args_cnt\":%d, "
            "\"names_cnt\":%d, "
            "\"strict_mode\": %d, \"func_name_present\": %d, \"ops\":%s, "
            "\"lit\": [",
            (void *) bcode, bcode->args_cnt, bcode->names_cnt,
            bcode->strict_mode, bcode->func_name_present, jops);

    for (i = 0; (size_t) i < bcode->lit.len / sizeof(val_t); i++) {
      val_t v = ((val_t *) bcode->lit.p)[i];
      const char *str;

      if (((v & V7_TAG_MASK) == V7_TAG_STRING_O ||
           (v & V7_TAG_MASK) == V7_TAG_STRING_F) &&
          (str = v7_get_cstring(v7, &v)) != NULL) {
        fprintf(f, "{\"str\": \"%s\"}", str);
      } else {
        fprintf(f, "{\"val\": \"0x%" INT64_X_FMT "\"}", v);
      }
      if ((size_t) i != bcode->lit.len / sizeof(val_t) - 1) {
        fprintf(f, ",");
      }
    }

    fprintf(f, "]}\n");
    free(jops);
  } else {
    struct v7_generic_object *gob = get_generic_object_struct(v);
    fprintf(f,
            "{\"type\":\"obj\", \"addr\":\"%p\", \"props\":\"%p\", "
            "\"attrs\":%d, \"proto\":\"%p\""
#if defined(V7_ENABLE_ENTITY_IDS)
            ", \"entity_id_base\":%d, \"entity_id_spec\":\"%d\" "
#endif
            "}\n",
            (void *) obj_base,
            (void *) ((uintptr_t) obj_base->properties & ~0x1),
            obj_base->attributes | attrs, (void *) gob->prototype
#if defined(V7_ENABLE_ENTITY_IDS)
            ,
            obj_base->entity_id_base, obj_base->entity_id_spec
#endif
            );
  }
}

V7_PRIVATE void freeze_prop(struct v7 *v7, FILE *f, struct v7_property *prop) {
  unsigned int attrs = _V7_PROPERTY_OFF_HEAP;
#ifndef V7_FREEZE_NOT_READONLY
  attrs |= V7_PROPERTY_NON_WRITABLE | V7_PROPERTY_NON_CONFIGURABLE;
#endif

  fprintf(f,
          "{\"type\":\"prop\","
          " \"addr\":\"%p\","
          " \"next\":\"%p\","
          " \"attrs\":%d,"
          " \"name\":\"0x%" INT64_X_FMT
          "\","
          " \"value_type\":%d,"
          " \"value\":\"0x%" INT64_X_FMT
          "\","
          " \"name_str\":\"%s\""
#if defined(V7_ENABLE_ENTITY_IDS)
          ", \"entity_id\":\"%d\""
#endif
          "}\n",
          (void *) prop, (void *) prop->next, prop->attributes | attrs,
          prop->name, val_type(v7, prop->value), prop->value,
          v7_get_cstring(v7, &prop->name)
#if defined(V7_ENABLE_ENTITY_IDS)
              ,
          prop->entity_id
#endif
          );
}

#endif
