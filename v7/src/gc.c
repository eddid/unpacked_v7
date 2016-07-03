/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/bcode.h"
#include "v7/src/varint.h"
#include "v7/src/gc.h"
#include "v7/src/freeze.h"
#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/primitive.h"
#include "v7/src/object.h"
#include "v7/src/string.h"
#include "v7/src/util.h"
#include "v7/src/primitive.h"
#include "v7/src/heapusage.h"

#include <stdio.h>

#ifdef V7_STACK_GUARD_MIN_SIZE
void *v7_sp_limit = NULL;
#endif

void gc_mark_string(struct v7 *, val_t *);

static struct gc_block *gc_new_block(struct gc_arena *a, size_t size);
static void gc_free_block(struct gc_block *b);
static void gc_mark_mbuf_pt(struct v7 *v7, const struct mbuf *mbuf);
static void gc_mark_mbuf_val(struct v7 *v7, const struct mbuf *mbuf);
static void gc_mark_vec_val(struct v7 *v7, const struct v7_vec *vec);

V7_PRIVATE struct v7_generic_object *new_generic_object(struct v7 *v7) {
  return (struct v7_generic_object *) gc_alloc_cell(v7,
                                                    &v7->generic_object_arena);
}

V7_PRIVATE struct v7_property *new_property(struct v7 *v7) {
  return (struct v7_property *) gc_alloc_cell(v7, &v7->property_arena);
}

V7_PRIVATE struct v7_js_function *new_function(struct v7 *v7) {
  return (struct v7_js_function *) gc_alloc_cell(v7, &v7->function_arena);
}

V7_PRIVATE struct gc_tmp_frame new_tmp_frame(struct v7 *v7) {
  struct gc_tmp_frame frame;
  frame.v7 = v7;
  frame.pos = v7->tmp_stack.len;
  return frame;
}

V7_PRIVATE void tmp_frame_cleanup(struct gc_tmp_frame *tf) {
  tf->v7->tmp_stack.len = tf->pos;
}

/*
 * TODO(mkm): perhaps it's safer to keep val_t in the temporary
 * roots stack, instead of keeping val_t*, in order to be better
 * able to debug the relocating GC.
 */
V7_PRIVATE void tmp_stack_push(struct gc_tmp_frame *tf, val_t *vp) {
  mbuf_append(&tf->v7->tmp_stack, (char *) &vp, sizeof(val_t *));
}

/* Initializes a new arena. */
V7_PRIVATE void gc_arena_init(struct gc_arena *a, size_t cell_size,
                              size_t initial_size, size_t size_increment,
                              const char *name) {
  assert(cell_size >= sizeof(uintptr_t));

  memset(a, 0, sizeof(*a));
  a->cell_size = cell_size;
  a->name = name;
  a->size_increment = size_increment;
  a->blocks = gc_new_block(a, initial_size);
}

V7_PRIVATE void gc_arena_destroy(struct v7 *v7, struct gc_arena *a) {
  struct gc_block *b;

  if (a->blocks != NULL) {
    gc_sweep(v7, a, 0);
    for (b = a->blocks; b != NULL;) {
      struct gc_block *tmp;
      tmp = b;
      b = b->next;
      gc_free_block(tmp);
    }
  }
}

static void gc_free_block(struct gc_block *b) {
  free(b->base);
  free(b);
}

static struct gc_block *gc_new_block(struct gc_arena *a, size_t size) {
  struct gc_cell *cur;
  struct gc_block *b;

  heapusage_dont_count(1);
  b = (struct gc_block *) calloc(1, sizeof(*b));
  heapusage_dont_count(0);
  if (b == NULL) abort();

  b->size = size;
  heapusage_dont_count(1);
  b->base = (struct gc_cell *) calloc(a->cell_size, b->size);
  heapusage_dont_count(0);
  if (b->base == NULL) abort();

  for (cur = GC_CELL_OP(a, b->base, +, 0);
       cur < GC_CELL_OP(a, b->base, +, b->size);
       cur = GC_CELL_OP(a, cur, +, 1)) {
    cur->head.link = a->free;
    a->free = cur;
  }

  return b;
}

V7_PRIVATE void *gc_alloc_cell(struct v7 *v7, struct gc_arena *a) {
#if V7_MALLOC_GC
  struct gc_cell *r;
  maybe_gc(v7);
  heapusage_dont_count(1);
  r = (struct gc_cell *) calloc(1, a->cell_size);
  heapusage_dont_count(0);
  mbuf_append(&v7->malloc_trace, &r, sizeof(r));
  return r;
#else
  struct gc_cell *r;
  if (a->free == NULL) {
    maybe_gc(v7);

    if (a->free == NULL) {
      struct gc_block *b = gc_new_block(a, a->size_increment);
      b->next = a->blocks;
      a->blocks = b;
    }
  }
  r = a->free;

  UNMARK(r);

  a->free = r->head.link;

#if V7_ENABLE__Memory__stats
  a->allocations++;
  a->alive++;
#endif

  /*
   * TODO(mkm): minor opt possible since most of the fields
   * are overwritten downstream, but not worth the yak shave time
   * when fields are added to GC-able structures */
  memset(r, 0, a->cell_size);
  return (void *) r;
#endif
}

#ifdef V7_MALLOC_GC
/*
 * Scans trough the memory blocks registered in the malloc trace.
 * Free the unmarked ones and reset the mark on the rest.
 */
void gc_sweep_malloc(struct v7 *v7) {
  struct gc_cell **cur;
  for (cur = (struct gc_cell **) v7->malloc_trace.buf;
       cur < (struct gc_cell **) (v7->malloc_trace.buf + v7->malloc_trace.len);
       cur++) {
    if (*cur == NULL) continue;

    if (MARKED(*cur)) {
      UNMARK(*cur);
    } else {
      free(*cur);
      /* TODO(mkm): compact malloc trace buffer */
      *cur = NULL;
    }
  }
}
#endif

/*
 * Scans the arena and add all unmarked cells to the free list.
 *
 * Empty blocks get deallocated. The head of the free list will contais cells
 * from the last (oldest) block. Cells will thus be allocated in block order.
 */
void gc_sweep(struct v7 *v7, struct gc_arena *a, size_t start) {
  struct gc_block *b;
  struct gc_cell *cur;
  struct gc_block **prevp = &a->blocks;
#if V7_ENABLE__Memory__stats
  a->alive = 0;
#endif

  /*
   * Before we sweep, we should mark all free cells in a way that is
   * distinguishable from marked used cells.
   */
  {
    struct gc_cell *next;
    for (cur = a->free; cur != NULL; cur = next) {
      next = cur->head.link;
      MARK_FREE(cur);
    }
  }

  /*
   * We'll rebuild the whole `free` list, so initially we just reset it
   */
  a->free = NULL;

  for (b = a->blocks; b != NULL;) {
    size_t freed_in_block = 0;
    /*
     * if it turns out that this block is 100% garbage
     * we can release the whole block, but the addition
     * of it's cells to the free list has to be undone.
     */
    struct gc_cell *prev_free = a->free;

    for (cur = GC_CELL_OP(a, b->base, +, start);
         cur < GC_CELL_OP(a, b->base, +, b->size);
         cur = GC_CELL_OP(a, cur, +, 1)) {
      if (MARKED(cur)) {
        /* The cell is used and marked  */
        UNMARK(cur);
#if V7_ENABLE__Memory__stats
        a->alive++;
#endif
      } else {
        /*
         * The cell is either:
         * - free
         * - garbage that's about to be freed
         */

        if (MARKED_FREE(cur)) {
          /* The cell is free, so, just unmark it */
          UNMARK_FREE(cur);
        } else {
          /*
           * The cell is used and should be freed: call the destructor and
           * reset the memory
           */
          if (a->destructor != NULL) {
            a->destructor(v7, cur);
          }
          memset(cur, 0, a->cell_size);
        }

        /* Add this cell to the `free` list */
        cur->head.link = a->free;
        a->free = cur;
        freed_in_block++;
#if V7_ENABLE__Memory__stats
        a->garbage++;
#endif
      }
    }

    /*
     * don't free the initial block, which is at the tail
     * because it has a special size aimed at reducing waste
     * and simplifying initial startup. TODO(mkm): improve
     * */
    if (b->next != NULL && freed_in_block == b->size) {
      *prevp = b->next;
      gc_free_block(b);
      b = *prevp;
      a->free = prev_free;
    } else {
      prevp = &b->next;
      b = b->next;
    }
  }
}

/*
 * dense arrays contain only one property pointing to an mbuf with array values.
 */
V7_PRIVATE void gc_mark_dense_array(struct v7 *v7,
                                    struct v7_generic_object *obj) {
  val_t v;
  struct mbuf *mbuf;
  val_t *vp;

#if 0
  /* TODO(mkm): use this when dense array promotion is implemented */
  v = obj->properties->value;
#else
  v = v7_get(v7, v7_object_to_value(&obj->base), "", 0);
#endif

  mbuf = (struct mbuf *) v7_get_ptr(v7, v);

  /* function scope pointer is aliased to the object's prototype pointer */
  gc_mark(v7, v7_object_to_value(obj_prototype(v7, &obj->base)));
  MARK(obj);

  if (mbuf == NULL) return;
  for (vp = (val_t *) mbuf->buf; (char *) vp < mbuf->buf + mbuf->len; vp++) {
    gc_mark(v7, *vp);
    gc_mark_string(v7, vp);
  }
  UNMARK(obj);
}

V7_PRIVATE void gc_mark(struct v7 *v7, val_t v) {
  struct v7_object *obj_base;
  struct v7_property *prop;
  struct v7_property *next;

  if (!v7_is_object(v)) {
    return;
  }
  obj_base = get_object_struct(v);

  /*
   * we ignore objects that are not managed by V7 heap, such as frozen
   * objects, especially when on flash.
   */
  if (obj_base->attributes & V7_OBJ_OFF_HEAP) {
    return;
  }

  /*
   * we treat all object like things like objects but they might be functions,
   * gc_gheck_val checks the appropriate arena per actual value type.
   */
  if (!gc_check_val(v7, v)) {
    abort();
  }

  if (MARKED(obj_base)) return;

#ifdef V7_FREEZE
  if (v7->freeze_file != NULL) {
    freeze_obj(v7, v7->freeze_file, v);
  }
#endif

  if (obj_base->attributes & V7_OBJ_DENSE_ARRAY) {
    struct v7_generic_object *obj = get_generic_object_struct(v);
    gc_mark_dense_array(v7, obj);
  }

  /* mark object itself, and its properties */
  for ((prop = obj_base->properties), MARK(obj_base); prop != NULL;
       prop = next) {
    if (prop->attributes & _V7_PROPERTY_OFF_HEAP) {
      break;
    }

    if (!gc_check_ptr(&v7->property_arena, prop)) {
      abort();
    }

#ifdef V7_FREEZE
    if (v7->freeze_file != NULL) {
      freeze_prop(v7, v7->freeze_file, prop);
    }
#endif

    gc_mark_string(v7, &prop->value);
    gc_mark_string(v7, &prop->name);
    gc_mark(v7, prop->value);

    next = prop->next;
    MARK(prop);
  }

  /* mark object's prototype */
  gc_mark(v7, obj_prototype_v(v7, v));

  if (is_js_function(v)) {
    struct v7_js_function *func = get_js_function_struct(v);

    /* mark function's scope */
    gc_mark(v7, v7_object_to_value(&func->scope->base));

    if (func->bcode != NULL) {
      gc_mark_vec_val(v7, &func->bcode->lit);
    }
  }
}

#if V7_ENABLE__Memory__stats

V7_PRIVATE size_t gc_arena_size(struct gc_arena *a) {
  size_t size = 0;
  struct gc_block *b;
  for (b = a->blocks; b != NULL; b = b->next) {
    size += b->size;
  }
  return size;
}

/*
 * TODO(dfrank): move to core
 */
int v7_heap_stat(struct v7 *v7, enum v7_heap_stat_what what) {
  switch (what) {
    case V7_HEAP_STAT_HEAP_SIZE:
      return gc_arena_size(&v7->generic_object_arena) *
                 v7->generic_object_arena.cell_size +
             gc_arena_size(&v7->function_arena) * v7->function_arena.cell_size +
             gc_arena_size(&v7->property_arena) * v7->property_arena.cell_size;
    case V7_HEAP_STAT_HEAP_USED:
      return v7->generic_object_arena.alive *
                 v7->generic_object_arena.cell_size +
             v7->function_arena.alive * v7->function_arena.cell_size +
             v7->property_arena.alive * v7->property_arena.cell_size;
    case V7_HEAP_STAT_STRING_HEAP_RESERVED:
      return v7->owned_strings.size;
    case V7_HEAP_STAT_STRING_HEAP_USED:
      return v7->owned_strings.len;
    case V7_HEAP_STAT_OBJ_HEAP_MAX:
      return gc_arena_size(&v7->generic_object_arena);
    case V7_HEAP_STAT_OBJ_HEAP_FREE:
      return gc_arena_size(&v7->generic_object_arena) -
             v7->generic_object_arena.alive;
    case V7_HEAP_STAT_OBJ_HEAP_CELL_SIZE:
      return v7->generic_object_arena.cell_size;
    case V7_HEAP_STAT_FUNC_HEAP_MAX:
      return gc_arena_size(&v7->function_arena);
    case V7_HEAP_STAT_FUNC_HEAP_FREE:
      return gc_arena_size(&v7->function_arena) - v7->function_arena.alive;
    case V7_HEAP_STAT_FUNC_HEAP_CELL_SIZE:
      return v7->function_arena.cell_size;
    case V7_HEAP_STAT_PROP_HEAP_MAX:
      return gc_arena_size(&v7->property_arena);
    case V7_HEAP_STAT_PROP_HEAP_FREE:
      return gc_arena_size(&v7->property_arena) - v7->property_arena.alive;
    case V7_HEAP_STAT_PROP_HEAP_CELL_SIZE:
      return v7->property_arena.cell_size;
    case V7_HEAP_STAT_FUNC_AST_SIZE:
      return v7->function_arena_ast_size;
    case V7_HEAP_STAT_BCODE_OPS_SIZE:
      return v7->bcode_ops_size;
    case V7_HEAP_STAT_BCODE_LIT_TOTAL_SIZE:
      return v7->bcode_lit_total_size;
    case V7_HEAP_STAT_BCODE_LIT_DESER_SIZE:
      return v7->bcode_lit_deser_size;
    case V7_HEAP_STAT_FUNC_OWNED:
      return v7->owned_values.len / sizeof(val_t *);
    case V7_HEAP_STAT_FUNC_OWNED_MAX:
      return v7->owned_values.size / sizeof(val_t *);
  }

  return -1;
}
#endif

V7_PRIVATE void gc_dump_arena_stats(const char *msg, struct gc_arena *a) {
  (void) msg;
  (void) a;
#ifndef NO_LIBC
#if V7_ENABLE__Memory__stats
  if (a->verbose) {
    fprintf(stderr, "%s: total allocations %lu, max %lu, alive %lu\n", msg,
            (long unsigned int) a->allocations,
            (long unsigned int) gc_arena_size(a), (long unsigned int) a->alive);
  }
#endif
#endif
}

V7_PRIVATE uint64_t gc_string_val_to_offset(val_t v) {
  return (((uint64_t)(uintptr_t) get_ptr(v)) & ~V7_TAG_MASK)
#ifndef V7_DISABLE_STR_ALLOC_SEQ
         & 0xFFFFFFFF
#endif
      ;
}

V7_PRIVATE val_t gc_string_val_from_offset(uint64_t s) {
  return s | V7_TAG_STRING_O;
}

#ifndef V7_DISABLE_STR_ALLOC_SEQ

static uint16_t next_asn(struct v7 *v7) {
  if (v7->gc_next_asn == 0xFFFF) {
    /* Wrap around explicitly. */
    v7->gc_next_asn = 0;
    return 0xFFFF;
  }
  return v7->gc_next_asn++;
}

uint16_t gc_next_allocation_seqn(struct v7 *v7, const char *str, size_t len) {
  uint16_t asn = next_asn(v7);
  (void) str;
  (void) len;
#ifdef V7_GC_VERBOSE
  /*
   * ESP SDK printf cannot cope with null strings
   * as created by s_concat.
   */
  if (str == NULL) {
    fprintf(stderr, "GC ASN %d: <nil>\n", asn);
  } else {
    fprintf(stderr, "GC ASN %d: \"%.*s\"\n", asn, (int) len, str);
  }
#endif
#ifdef V7_GC_PANIC_ON_ASN
  if (asn == (V7_GC_PANIC_ON_ASN)) {
    abort();
  }
#endif
  return asn;
}

int gc_is_valid_allocation_seqn(struct v7 *v7, uint16_t n) {
  /*
   * This functions attempts to handle integer wraparound in a naive way and
   * will give false positives when more than 65536 strings are allocated
   * between GC runs.
   */
  int r = (n >= v7->gc_min_asn && n < v7->gc_next_asn) ||
          (v7->gc_min_asn > v7->gc_next_asn &&
           (n >= v7->gc_min_asn || n < v7->gc_next_asn));
  if (!r) {
    fprintf(stderr, "GC ASN %d is not in [%d,%d)\n", n, v7->gc_min_asn,
            v7->gc_next_asn);
  }
  return r;
}

void gc_check_valid_allocation_seqn(struct v7 *v7, uint16_t n) {
  if (!gc_is_valid_allocation_seqn(v7, n)) {
/*
 * TODO(dfrank) throw exception if V7_GC_ASN_PANIC is not defined.
 */
#if 0 && !defined(V7_GC_ASN_PANIC)
    throw_exception(v7, INTERNAL_ERROR, "Invalid ASN: %d", (int) n);
#else
    fprintf(stderr, "Invalid ASN: %d\n", (int) n);
    abort();
#endif
  }
}

#endif /* V7_DISABLE_STR_ALLOC_SEQ */

/* Mark a string value */
void gc_mark_string(struct v7 *v7, val_t *v) {
  val_t h, tmp = 0;
  char *s;

  /* clang-format off */

  /*
   * If a value points to an unmarked string we shall:
   *  1. save the first 6 bytes of the string
   *     since we need to be able to distinguish real values from
   *     the saved first 6 bytes of the string, we need to tag the chunk
   *     as V7_TAG_STRING_C
   *  2. encode value's address (v) into the first 6 bytes of the string.
   *  3. put the saved 8 bytes (tag + chunk) back into the value.
   *  4. mark the string by putting '\1' in the NUL terminator of the previous
   *     string chunk.
   *
   * If a value points to an already marked string we shall:
   *     (0, <6 bytes of a pointer to a val_t>), hence we have to skip
   *     the first byte. We tag the value pointer as a V7_TAG_FOREIGN
   *     so that it won't be followed during recursive mark.
   *
   *  ... the rest is the same
   *
   *  Note: 64-bit pointers can be represented with 48-bits
   */

  /* clang-format on */

  if ((*v & V7_TAG_MASK) != V7_TAG_STRING_O) {
    return;
  }

#ifdef V7_FREEZE
  if (v7->freeze_file != NULL) {
    return;
  }
#endif

#ifdef V7_GC_VERBOSE
  {
    uint16_t asn = (*v >> 32) & 0xFFFF;
    size_t size;
    fprintf(stderr, "GC marking ASN %d: '%s'\n", asn,
            v7_get_string(v7, v, &size));
  }
#endif

#ifndef V7_DISABLE_STR_ALLOC_SEQ
  gc_check_valid_allocation_seqn(v7, (*v >> 32) & 0xFFFF);
#endif

  s = v7->owned_strings.buf + gc_string_val_to_offset(*v);
  assert(s < v7->owned_strings.buf + v7->owned_strings.len);
  if (s[-1] == '\0') {
    memcpy(&tmp, s, sizeof(tmp) - 2);
    tmp |= V7_TAG_STRING_C;
  } else {
    memcpy(&tmp, s, sizeof(tmp) - 2);
    tmp |= V7_TAG_FOREIGN;
  }

  h = (val_t)(uintptr_t) v;
  s[-1] = 1;
  memcpy(s, &h, sizeof(h) - 2);
  memcpy(v, &tmp, sizeof(tmp));
}

void gc_compact_strings(struct v7 *v7) {
  char *p = v7->owned_strings.buf + 1;
  uint64_t h, next, head = 1;
  int len, llen;

#ifndef V7_DISABLE_STR_ALLOC_SEQ
  v7->gc_min_asn = v7->gc_next_asn;
#endif
  while (p < v7->owned_strings.buf + v7->owned_strings.len) {
    if (p[-1] == '\1') {
#ifndef V7_DISABLE_STR_ALLOC_SEQ
      /* Not using gc_next_allocation_seqn() as we don't have full string. */
      uint16_t asn = next_asn(v7);
#endif
      /* relocate and update ptrs */
      h = 0;
      memcpy(&h, p, sizeof(h) - 2);

      /*
       * relocate pointers until we find the tail.
       * The tail is marked with V7_TAG_STRING_C,
       * while val_t link pointers are tagged with V7_TAG_FOREIGN
       */
      for (; (h & V7_TAG_MASK) != V7_TAG_STRING_C; h = next) {
        h &= ~V7_TAG_MASK;
        memcpy(&next, (char *) (uintptr_t) h, sizeof(h));

        *(val_t *) (uintptr_t) h = gc_string_val_from_offset(head)
#ifndef V7_DISABLE_STR_ALLOC_SEQ
                                   | ((val_t) asn << 32)
#endif
            ;
      }
      h &= ~V7_TAG_MASK;

      /*
       * the tail contains the first 6 bytes we stole from
       * the actual string.
       */
      len = decode_varint((unsigned char *) &h, &llen);
      len += llen + 1;

      /*
       * restore the saved 6 bytes
       * TODO(mkm): think about endianness
       */
      memcpy(p, &h, sizeof(h) - 2);

      /*
       * and relocate the string data by packing it to the left.
       */
      memmove(v7->owned_strings.buf + head, p, len);
      v7->owned_strings.buf[head - 1] = 0x0;
#if defined(V7_GC_VERBOSE) && !defined(V7_DISABLE_STR_ALLOC_SEQ)
      fprintf(stderr, "GC updated ASN %d: \"%.*s\"\n", asn, len - llen - 1,
              v7->owned_strings.buf + head + llen);
#endif
      p += len;
      head += len;
    } else {
      len = decode_varint((unsigned char *) p, &llen);
      len += llen + 1;

      p += len;
    }
  }

#if defined(V7_GC_VERBOSE) && !defined(V7_DISABLE_STR_ALLOC_SEQ)
  fprintf(stderr, "GC valid ASN range: [%d,%d)\n", v7->gc_min_asn,
          v7->gc_next_asn);
#endif

  v7->owned_strings.len = head;
}

void gc_dump_owned_strings(struct v7 *v7) {
  size_t i;
  for (i = 0; i < v7->owned_strings.len; i++) {
    if (isprint((unsigned char) v7->owned_strings.buf[i])) {
      fputc(v7->owned_strings.buf[i], stderr);
    } else {
      fputc('.', stderr);
    }
  }
  fputc('\n', stderr);
}

/*
 * builting on gcc, tried out by redefining it.
 * Using null pointer as base can trigger undefined behavior, hence
 * a portable workaround that involves a valid yet dummy pointer.
 * It's meant to be used as a contant expression.
 */
#ifndef offsetof
#define offsetof(st, m) (((ptrdiff_t)(&((st *) 32)->m)) - 32)
#endif

V7_PRIVATE void compute_need_gc(struct v7 *v7) {
  struct mbuf *m = &v7->owned_strings;
  if ((double) m->len / (double) m->size > 0.9) {
    v7->need_gc = 1;
  }
  /* TODO(mkm): check free heap */
}

V7_PRIVATE void maybe_gc(struct v7 *v7) {
  if (!v7->inhibit_gc) {
    v7_gc(v7, 0);
  }
}
#if defined(V7_GC_VERBOSE)
static int gc_pass = 0;
#endif

/*
 * mark an array of `val_t` values (*not pointers* to them)
 */
static void gc_mark_val_array(struct v7 *v7, val_t *vals, size_t len) {
  val_t *vp;
  for (vp = vals; vp < vals + len; vp++) {
    gc_mark(v7, *vp);
    gc_mark_string(v7, vp);
  }
}

/*
 * mark an mbuf containing *pointers* to `val_t` values
 */
static void gc_mark_mbuf_pt(struct v7 *v7, const struct mbuf *mbuf) {
  val_t **vp;
  for (vp = (val_t **) mbuf->buf; (char *) vp < mbuf->buf + mbuf->len; vp++) {
    gc_mark(v7, **vp);
    gc_mark_string(v7, *vp);
  }
}

/*
 * mark an mbuf containing `val_t` values (*not pointers* to them)
 */
static void gc_mark_mbuf_val(struct v7 *v7, const struct mbuf *mbuf) {
  gc_mark_val_array(v7, (val_t *) mbuf->buf, mbuf->len / sizeof(val_t));
}

/*
 * mark a vector containing `val_t` values (*not pointers* to them)
 */
static void gc_mark_vec_val(struct v7 *v7, const struct v7_vec *vec) {
  gc_mark_val_array(v7, (val_t *) vec->p, vec->len / sizeof(val_t));
}

/*
 * mark an mbuf containing foreign pointers to `struct bcode`
 */
static void gc_mark_mbuf_bcode_pt(struct v7 *v7, const struct mbuf *mbuf) {
  struct bcode **vp;
  for (vp = (struct bcode **) mbuf->buf; (char *) vp < mbuf->buf + mbuf->len;
       vp++) {
    gc_mark_vec_val(v7, &(*vp)->lit);
  }
}

static void gc_mark_call_stack_private(
    struct v7 *v7, struct v7_call_frame_private *call_stack) {
  gc_mark_val_array(v7, (val_t *) &call_stack->vals,
                    sizeof(call_stack->vals) / sizeof(val_t));
}

static void gc_mark_call_stack_cfunc(struct v7 *v7,
                                     struct v7_call_frame_cfunc *call_stack) {
  gc_mark_val_array(v7, (val_t *) &call_stack->vals,
                    sizeof(call_stack->vals) / sizeof(val_t));
}

static void gc_mark_call_stack_bcode(struct v7 *v7,
                                     struct v7_call_frame_bcode *call_stack) {
  gc_mark_val_array(v7, (val_t *) &call_stack->vals,
                    sizeof(call_stack->vals) / sizeof(val_t));
}

/*
 * mark `struct v7_call_frame` and all its back-linked frames
 */
static void gc_mark_call_stack(struct v7 *v7,
                               struct v7_call_frame_base *call_stack) {
  while (call_stack != NULL) {
    if (call_stack->type_mask & V7_CALL_FRAME_MASK_BCODE) {
      gc_mark_call_stack_bcode(v7, (struct v7_call_frame_bcode *) call_stack);
    }

    if (call_stack->type_mask & V7_CALL_FRAME_MASK_PRIVATE) {
      gc_mark_call_stack_private(v7,
                                 (struct v7_call_frame_private *) call_stack);
    }

    if (call_stack->type_mask & V7_CALL_FRAME_MASK_CFUNC) {
      gc_mark_call_stack_cfunc(v7, (struct v7_call_frame_cfunc *) call_stack);
    }

    call_stack = call_stack->prev;
  }
}

/* Perform garbage collection */
void v7_gc(struct v7 *v7, int full) {
#ifdef V7_DISABLE_GC
  (void) v7;
  (void) full;
  return;
#else

#if defined(V7_GC_VERBOSE)
  fprintf(stderr, "V7 GC pass %d\n", ++gc_pass);
#endif

  gc_dump_arena_stats("Before GC objects", &v7->generic_object_arena);
  gc_dump_arena_stats("Before GC functions", &v7->function_arena);
  gc_dump_arena_stats("Before GC properties", &v7->property_arena);

  gc_mark_call_stack(v7, v7->call_stack);

  gc_mark_val_array(v7, (val_t *) &v7->vals, sizeof(v7->vals) / sizeof(val_t));
  /* mark all items on bcode stack */
  gc_mark_mbuf_val(v7, &v7->stack);

  /* mark literals and names of all the active bcodes */
  gc_mark_mbuf_bcode_pt(v7, &v7->act_bcodes);

  gc_mark_mbuf_pt(v7, &v7->tmp_stack);
  gc_mark_mbuf_pt(v7, &v7->owned_values);

  gc_compact_strings(v7);

#ifdef V7_MALLOC_GC
  gc_sweep_malloc(v7);
#else
  gc_sweep(v7, &v7->generic_object_arena, 0);
  gc_sweep(v7, &v7->function_arena, 0);
  gc_sweep(v7, &v7->property_arena, 0);
#endif

  gc_dump_arena_stats("After GC objects", &v7->generic_object_arena);
  gc_dump_arena_stats("After GC functions", &v7->function_arena);
  gc_dump_arena_stats("After GC properties", &v7->property_arena);

  if (full) {
    /*
     * In case of full GC, we also resize strings buffer, but we still leave
     * some extra space (at most, `_V7_STRING_BUF_RESERVE`) in order to avoid
     * frequent reallocations
     */
    size_t trimmed_size = v7->owned_strings.len + _V7_STRING_BUF_RESERVE;
    if (trimmed_size < v7->owned_strings.size) {
      heapusage_dont_count(1);
      mbuf_resize(&v7->owned_strings, trimmed_size);
      heapusage_dont_count(0);
    }
  }
#endif /* V7_DISABLE_GC */
}

V7_PRIVATE int gc_check_val(struct v7 *v7, val_t v) {
  if (is_js_function(v)) {
    return gc_check_ptr(&v7->function_arena, get_js_function_struct(v));
  } else if (v7_is_object(v)) {
    return gc_check_ptr(&v7->generic_object_arena, get_object_struct(v));
  }
  return 1;
}

V7_PRIVATE int gc_check_ptr(const struct gc_arena *a, const void *ptr) {
#ifdef V7_MALLOC_GC
  (void) a;
  (void) ptr;
  return 1;
#else
  const struct gc_cell *p = (const struct gc_cell *) ptr;
  struct gc_block *b;
  for (b = a->blocks; b != NULL; b = b->next) {
    if (p >= b->base && p < GC_CELL_OP(a, b->base, +, b->size)) {
      return 1;
    }
  }
  return 0;
#endif
}
