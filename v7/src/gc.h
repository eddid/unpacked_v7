/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_GC_H_
#define CS_V7_SRC_GC_H_

#include "v7/src/gc_public.h"

#include "v7/src/internal.h"
#include "v7/src/core.h"

/*
 * Macros for marking reachable things: use bit 0.
 */
#define MARK(p) (((struct gc_cell *) (p))->head.word |= 1)
#define UNMARK(p) (((struct gc_cell *) (p))->head.word &= ~1)
#define MARKED(p) (((struct gc_cell *) (p))->head.word & 1)

/*
 * Similar to `MARK()` / `UNMARK()` / `MARKED()`, but `.._FREE` counterparts
 * are intended to mark free cells (as opposed to used ones), so they use
 * bit 1.
 */
#define MARK_FREE(p) (((struct gc_cell *) (p))->head.word |= 2)
#define UNMARK_FREE(p) (((struct gc_cell *) (p))->head.word &= ~2)
#define MARKED_FREE(p) (((struct gc_cell *) (p))->head.word & 2)

/*
 * performs arithmetics on gc_cell pointers as if they were arena->cell_size
 * bytes wide
 */
#define GC_CELL_OP(arena, cell, op, arg) \
  ((struct gc_cell *) (((char *) (cell)) op((arg) * (arena)->cell_size)))

struct gc_tmp_frame {
  struct v7 *v7;
  size_t pos;
};

struct gc_cell {
  union {
    struct gc_cell *link;
    uintptr_t word;
  } head;
};

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

V7_PRIVATE struct v7_generic_object *new_generic_object(struct v7 *);
V7_PRIVATE struct v7_property *new_property(struct v7 *);
V7_PRIVATE struct v7_js_function *new_function(struct v7 *);

V7_PRIVATE void gc_mark(struct v7 *, val_t);

V7_PRIVATE void gc_arena_init(struct gc_arena *, size_t, size_t, size_t,
                              const char *);
V7_PRIVATE void gc_arena_destroy(struct v7 *, struct gc_arena *a);
V7_PRIVATE void gc_sweep(struct v7 *, struct gc_arena *, size_t);
V7_PRIVATE void *gc_alloc_cell(struct v7 *, struct gc_arena *);

V7_PRIVATE struct gc_tmp_frame new_tmp_frame(struct v7 *);
V7_PRIVATE void tmp_frame_cleanup(struct gc_tmp_frame *);
V7_PRIVATE void tmp_stack_push(struct gc_tmp_frame *, val_t *);

V7_PRIVATE void compute_need_gc(struct v7 *);
/* perform gc if not inhibited */
V7_PRIVATE void maybe_gc(struct v7 *);

#ifndef V7_DISABLE_STR_ALLOC_SEQ
V7_PRIVATE uint16_t
gc_next_allocation_seqn(struct v7 *v7, const char *str, size_t len);
V7_PRIVATE int gc_is_valid_allocation_seqn(struct v7 *v7, uint16_t n);
V7_PRIVATE void gc_check_valid_allocation_seqn(struct v7 *v7, uint16_t n);
#endif

V7_PRIVATE uint64_t gc_string_val_to_offset(val_t v);

/* return 0 if v is an object/function with a bad pointer */
V7_PRIVATE int gc_check_val(struct v7 *v7, val_t v);

/* checks whether a pointer is within the ranges of an arena */
V7_PRIVATE int gc_check_ptr(const struct gc_arena *a, const void *p);

#if V7_ENABLE__Memory__stats
V7_PRIVATE size_t gc_arena_size(struct gc_arena *);
#endif

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_GC_H_ */
