/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_MM_H_
#define CS_V7_SRC_MM_H_

#include "v7/src/internal.h"

typedef void (*gc_cell_destructor_t)(struct v7 *v7, void *);

struct gc_block {
  struct gc_block *next;
  struct gc_cell *base;
  size_t size;
};

struct gc_arena {
  struct gc_block *blocks;
  size_t size_increment;
  struct gc_cell *free; /* head of free list */
  size_t cell_size;

#if V7_ENABLE__Memory__stats
  unsigned long allocations; /* cumulative counter of allocations */
  unsigned long garbage;     /* cumulative counter of garbage */
  unsigned long alive;       /* number of living cells */
#endif

  gc_cell_destructor_t destructor;

  int verbose;
  const char *name; /* for debugging purposes */
};

#endif /* CS_V7_SRC_MM_H_ */
