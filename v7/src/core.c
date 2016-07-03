/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/builtin/builtin.h"
#include "v7/src/internal.h"
#include "v7/src/object.h"
#include "v7/src/core.h"
#include "v7/src/primitive.h"
#include "v7/src/array.h"
#include "v7/src/slre.h"
#include "v7/src/bcode.h"
#include "v7/src/stdlib.h"
#include "v7/src/gc.h"
#include "v7/src/heapusage.h"
#include "v7/src/eval.h"

#ifdef V7_THAW
extern struct v7_vals *fr_vals;
#endif

#ifdef HAS_V7_INFINITY
double _v7_infinity;
#endif

#ifdef HAS_V7_NAN
double _v7_nan;
#endif

#if defined(V7_CYG_PROFILE_ON)
struct v7 *v7_head = NULL;
#endif

static void generic_object_destructor(struct v7 *v7, void *ptr) {
  struct v7_generic_object *o = (struct v7_generic_object *) ptr;
  struct v7_property *p;
  struct mbuf *abuf;

  /* TODO(mkm): make regexp use user data API */
  p = v7_get_own_property2(v7, v7_object_to_value(&o->base), "", 0,
                           _V7_PROPERTY_HIDDEN);

#if V7_ENABLE__RegExp
  if (p != NULL && (p->value & V7_TAG_MASK) == V7_TAG_REGEXP) {
    struct v7_regexp *rp = (struct v7_regexp *) get_ptr(p->value);
    v7_disown(v7, &rp->regexp_string);
    slre_free(rp->compiled_regexp);
    free(rp);
  }
#endif

  if (o->base.attributes & V7_OBJ_DENSE_ARRAY) {
    if (p != NULL &&
        ((abuf = (struct mbuf *) v7_get_ptr(v7, p->value)) != NULL)) {
      mbuf_free(abuf);
      free(abuf);
    }
  }

  if (o->base.attributes & V7_OBJ_HAS_DESTRUCTOR) {
    struct v7_property *p;
    for (p = o->base.properties; p != NULL; p = p->next) {
      if (p->attributes & _V7_PROPERTY_USER_DATA_AND_DESTRUCTOR) {
        if (v7_is_foreign(p->name)) {
          v7_destructor_cb_t *cb =
              (v7_destructor_cb_t *) v7_get_ptr(v7, p->name);
          cb(v7_get_ptr(v7, p->value));
        }
        break;
      }
    }
  }

#if defined(V7_ENABLE_ENTITY_IDS)
  o->base.entity_id_base = V7_ENTITY_ID_PART_NONE;
  o->base.entity_id_spec = V7_ENTITY_ID_PART_NONE;
#endif
}

static void function_destructor(struct v7 *v7, void *ptr) {
  struct v7_js_function *f = (struct v7_js_function *) ptr;
  (void) v7;
  if (f == NULL) return;

  if (f->bcode != NULL) {
    release_bcode(v7, f->bcode);
  }

#if defined(V7_ENABLE_ENTITY_IDS)
  f->base.entity_id_base = V7_ENTITY_ID_PART_NONE;
  f->base.entity_id_spec = V7_ENTITY_ID_PART_NONE;
#endif
}

#if defined(V7_ENABLE_ENTITY_IDS)
static void property_destructor(struct v7 *v7, void *ptr) {
  struct v7_property *p = (struct v7_property *) ptr;
  (void) v7;
  if (p == NULL) return;

  p->entity_id = V7_ENTITY_ID_NONE;
}
#endif

struct v7 *v7_create(void) {
  struct v7_create_opts opts;
  memset(&opts, 0, sizeof(opts));
  return v7_create_opt(opts);
}

struct v7 *v7_create_opt(struct v7_create_opts opts) {
  struct v7 *v7 = NULL;
  char z = 0;

#if defined(HAS_V7_INFINITY) || defined(HAS_V7_NAN)
  double zero = 0.0;
#endif

#ifdef HAS_V7_INFINITY
  _v7_infinity = 1.0 / zero;
#endif
#ifdef HAS_V7_NAN
  _v7_nan = zero / zero;
#endif

  if (opts.object_arena_size == 0) opts.object_arena_size = 200;
  if (opts.function_arena_size == 0) opts.function_arena_size = 100;
  if (opts.property_arena_size == 0) opts.property_arena_size = 400;

  if ((v7 = (struct v7 *) calloc(1, sizeof(*v7))) != NULL) {
#ifdef V7_STACK_SIZE
    v7->sp_limit = (void *) ((uintptr_t) opts.c_stack_base - (V7_STACK_SIZE));
    v7->sp_lwm = opts.c_stack_base;
#ifdef V7_STACK_GUARD_MIN_SIZE
    v7_sp_limit = v7->sp_limit;
#endif
#endif

#if defined(V7_CYG_PROFILE_ON)
    v7->next_v7 = v7_head;
    v7_head = v7;
#endif

#ifndef V7_DISABLE_STR_ALLOC_SEQ
    v7->gc_next_asn = 0;
    v7->gc_min_asn = 0;
#endif

    v7->cur_dense_prop =
        (struct v7_property *) calloc(1, sizeof(struct v7_property));
    gc_arena_init(&v7->generic_object_arena, sizeof(struct v7_generic_object),
                  opts.object_arena_size, 10, "object");
    v7->generic_object_arena.destructor = generic_object_destructor;
    gc_arena_init(&v7->function_arena, sizeof(struct v7_js_function),
                  opts.function_arena_size, 10, "function");
    v7->function_arena.destructor = function_destructor;
    gc_arena_init(&v7->property_arena, sizeof(struct v7_property),
                  opts.property_arena_size, 10, "property");
#if defined(V7_ENABLE_ENTITY_IDS)
    v7->property_arena.destructor = property_destructor;
#endif

    /*
     * The compacting GC exploits the null terminator of the previous
     * string as marker.
     */
    mbuf_append(&v7->owned_strings, &z, 1);

    v7->inhibit_gc = 1;
    v7->vals.thrown_error = V7_UNDEFINED;

    v7->call_stack = NULL;
    v7->bottom_call_frame = NULL;

#if defined(V7_THAW) && !defined(V7_FREEZE_NOT_READONLY)
    {
      struct v7_generic_object *obj;
      v7->vals = *fr_vals;
      v7->vals.global_object = v7_mk_object(v7);

      /*
       * The global object has to be mutable.
       */
      obj = get_generic_object_struct(v7->vals.global_object);
      *obj = *get_generic_object_struct(fr_vals->global_object);
      obj->base.attributes &= ~(V7_OBJ_NOT_EXTENSIBLE | V7_OBJ_OFF_HEAP);
      v7_set(v7, v7->vals.global_object, "global", 6, v7->vals.global_object);
    }
#else
    init_stdlib(v7);
    init_file(v7);
    init_crypto(v7);
    init_socket(v7);
    init_ubjson(v7);
#endif

    v7->inhibit_gc = 0;
  }

  return v7;
}

val_t v7_get_global(struct v7 *v7) {
  return v7->vals.global_object;
}

void v7_destroy(struct v7 *v7) {
  if (v7 == NULL) return;
  gc_arena_destroy(v7, &v7->generic_object_arena);
  gc_arena_destroy(v7, &v7->function_arena);
  gc_arena_destroy(v7, &v7->property_arena);

  mbuf_free(&v7->owned_strings);
  mbuf_free(&v7->owned_values);
  mbuf_free(&v7->foreign_strings);
  mbuf_free(&v7->json_visited_stack);
  mbuf_free(&v7->tmp_stack);
  mbuf_free(&v7->act_bcodes);
  mbuf_free(&v7->stack);

#if defined(V7_CYG_PROFILE_ON)
  /* delete this v7 */
  {
    struct v7 *v, **prevp = &v7_head;
    for (v = v7_head; v != NULL; prevp = &v->next_v7, v = v->next_v7) {
      if (v == v7) {
        *prevp = v->next_v7;
        break;
      }
    }
  }
#endif

  free(v7->cur_dense_prop);
  free(v7);
}

v7_val_t v7_get_this(struct v7 *v7) {
  /*
   * By default, when there's no active call frame, will return Global Object
   */
  v7_val_t ret = v7->vals.global_object;

  struct v7_call_frame_base *call_frame =
      find_call_frame(v7, V7_CALL_FRAME_MASK_BCODE | V7_CALL_FRAME_MASK_CFUNC);

  if (call_frame != NULL) {
    if (call_frame->type_mask & V7_CALL_FRAME_MASK_BCODE) {
      ret = ((struct v7_call_frame_bcode *) call_frame)->vals.this_obj;
    } else if (call_frame->type_mask & V7_CALL_FRAME_MASK_CFUNC) {
      ret = ((struct v7_call_frame_cfunc *) call_frame)->vals.this_obj;
    } else {
      assert(0);
    }
  }

  return ret;
}

V7_PRIVATE v7_val_t get_scope(struct v7 *v7) {
  struct v7_call_frame_private *call_frame =
      (struct v7_call_frame_private *) find_call_frame(
          v7, V7_CALL_FRAME_MASK_PRIVATE);

  if (call_frame != NULL) {
    return call_frame->vals.scope;
  } else {
    /* No active call frame, return global object */
    return v7->vals.global_object;
  }
}

V7_PRIVATE uint8_t is_strict_mode(struct v7 *v7) {
  struct v7_call_frame_bcode *call_frame =
      (struct v7_call_frame_bcode *) find_call_frame(v7,
                                                     V7_CALL_FRAME_MASK_BCODE);

  if (call_frame != NULL) {
    return call_frame->bcode->strict_mode;
  } else {
    /* No active call frame, assume no strict mode */
    return 0;
  }
}

v7_val_t v7_get_arguments(struct v7 *v7) {
  return v7->vals.arguments;
}

v7_val_t v7_arg(struct v7 *v7, unsigned long n) {
  return v7_array_get(v7, v7->vals.arguments, n);
}

unsigned long v7_argc(struct v7 *v7) {
  return v7_array_length(v7, v7->vals.arguments);
}

void v7_own(struct v7 *v7, v7_val_t *v) {
  heapusage_dont_count(1);
  mbuf_append(&v7->owned_values, &v, sizeof(v));
  heapusage_dont_count(0);
}

int v7_disown(struct v7 *v7, v7_val_t *v) {
  v7_val_t **vp =
      (v7_val_t **) (v7->owned_values.buf + v7->owned_values.len - sizeof(v));

  for (; (char *) vp >= v7->owned_values.buf; vp--) {
    if (*vp == v) {
      *vp = *(v7_val_t **) (v7->owned_values.buf + v7->owned_values.len -
                            sizeof(v));
      v7->owned_values.len -= sizeof(v);
      return 1;
    }
  }

  return 0;
}

void v7_set_gc_enabled(struct v7 *v7, int enabled) {
  v7->inhibit_gc = !enabled;
}

void v7_interrupt(struct v7 *v7) {
  v7->interrupted = 1;
}

const char *v7_get_parser_error(struct v7 *v7) {
  return v7->error_msg;
}

#if defined(V7_ENABLE_STACK_TRACKING)

int v7_stack_stat(struct v7 *v7, enum v7_stack_stat_what what) {
  assert(what < V7_STACK_STATS_CNT);
  return v7->stack_stat[what];
}

void v7_stack_stat_clean(struct v7 *v7) {
  memset(v7->stack_stat, 0x00, sizeof(v7->stack_stat));
}

#endif /* V7_ENABLE_STACK_TRACKING */
