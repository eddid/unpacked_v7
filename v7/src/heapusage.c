/*
 * Copyright (c) 2014-2016 Cesanta Software Limited
 * All rights reserved
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#if defined(V7_HEAPUSAGE_ENABLE)

/*
 * A flag that is set by GC before allocating its buffers, so we can
 * distinguish these buffers from other allocations
 */
volatile int heap_dont_count = 0;

extern void *__real_malloc(size_t size);
extern void *__real_calloc(size_t num, size_t size);
extern void *__real_realloc(void *p, size_t size);
extern void __real_free(void *p);

/* TODO(dfrank): make it dynamically allocated from heap */
#define CELLS_CNT (1024 * 32)

typedef struct cell {
  void *p;
  unsigned dont_count : 1;
  unsigned size : 31;
} cell_t;

typedef struct alloc_registry {
  size_t used_cells_cnt;
  size_t allocated_size;
  size_t real_used_cells_cnt;
  size_t real_allocated_size;
  cell_t cells[CELLS_CNT];
} alloc_registry_t;

static alloc_registry_t registry = {0};

/*
 * Make a record about an allocated buffer `p` of size `size`
 */
static void cell_allocated(void *p, size_t size) {
  int i;
  int cell_num = -1;

  if (p != NULL && size != 0) {
    /* TODO(dfrank): make it dynamically allocated from heap */
    assert(registry.real_used_cells_cnt < CELLS_CNT);

    for (i = 0; i < CELLS_CNT; ++i) {
      if (registry.cells[i].p == NULL) {
        cell_num = i;
        break;
      }
    }

    assert(cell_num != -1);

    registry.cells[cell_num].p = p;
    registry.cells[cell_num].size = size;
    registry.cells[cell_num].dont_count = !!heap_dont_count;

    registry.real_allocated_size += size;
    registry.real_used_cells_cnt += 1;

    if (!heap_dont_count) {
      registry.allocated_size += size;
      registry.used_cells_cnt += 1;
    }

#if 0
    printf("alloc=0x%lx, size=%lu, total=%lu\n", (unsigned long)p, size,
        registry.allocated_size);
#endif
  }
}

/*
 * Delete a record about an allocated buffer `p`. If our registry does not
 * contain anything about the given pointer, the call is ignored. We can't
 * generate an error because shared libraries still use unwrapped heap
 * functions, so we can face "unknown" pointers.
 */
static void cell_freed(void *p) {
  int i;
  int cell_num = -1;

  if (p != NULL) {
    assert(registry.real_used_cells_cnt > 0);

    for (i = 0; i < CELLS_CNT; ++i) {
      if (registry.cells[i].p == p) {
        cell_num = i;
        break;
      }
    }

    /*
     * NOTE: it would be nice to have `assert(cell_num != -1);`, but
     * unfortunately not all allocations are wrapped: shared libraries will
     * still use unwrapped mallocs, so we might get unknown pointers here.
     */

    if (cell_num != -1) {
      registry.real_allocated_size -= registry.cells[cell_num].size;
      registry.real_used_cells_cnt -= 1;

      if (!registry.cells[cell_num].dont_count) {
        registry.allocated_size -= registry.cells[cell_num].size;
        registry.used_cells_cnt -= 1;
      }

      registry.cells[cell_num].p = NULL;
      registry.cells[cell_num].size = 0;
      registry.cells[cell_num].dont_count = 0;

#if 0
      printf("free=0x%lx, total=%lu\n", (unsigned long)p, registry.allocated_size);
#endif
    }
  }
}

/*
 * Wrappers of the standard heap functions
 */

void *__wrap_malloc(size_t size) {
  void *ret = __real_malloc(size);
  cell_allocated(ret, size);
  return ret;
}

void *__wrap_calloc(size_t num, size_t size) {
  void *ret = __real_calloc(num, size);
  cell_allocated(ret, num * size);
  return ret;
}

void *__wrap_realloc(void *p, size_t size) {
  void *ret;
  cell_freed(p);
  ret = __real_realloc(p, size);
  cell_allocated(ret, size);
  return ret;
}

void __wrap_free(void *p) {
  __real_free(p);
  cell_freed(p);
}

/*
 * Small API to get some stats, see header file for details
 */

size_t heapusage_alloc_size(void) {
  return registry.allocated_size;
}

size_t heapusage_allocs_cnt(void) {
  return registry.used_cells_cnt;
}

#endif /* V7_HEAPUSAGE_ENABLE */
