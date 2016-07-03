/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Garbage Collector
 */

#ifndef CS_V7_SRC_GC_PUBLIC_H_
#define CS_V7_SRC_GC_PUBLIC_H_

#include "v7/src/core_public.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#if V7_ENABLE__Memory__stats

/* Heap metric id, see `v7_heap_stat()` */
enum v7_heap_stat_what {
  V7_HEAP_STAT_HEAP_SIZE,
  V7_HEAP_STAT_HEAP_USED,
  V7_HEAP_STAT_STRING_HEAP_RESERVED,
  V7_HEAP_STAT_STRING_HEAP_USED,
  V7_HEAP_STAT_OBJ_HEAP_MAX,
  V7_HEAP_STAT_OBJ_HEAP_FREE,
  V7_HEAP_STAT_OBJ_HEAP_CELL_SIZE,
  V7_HEAP_STAT_FUNC_HEAP_MAX,
  V7_HEAP_STAT_FUNC_HEAP_FREE,
  V7_HEAP_STAT_FUNC_HEAP_CELL_SIZE,
  V7_HEAP_STAT_PROP_HEAP_MAX,
  V7_HEAP_STAT_PROP_HEAP_FREE,
  V7_HEAP_STAT_PROP_HEAP_CELL_SIZE,
  V7_HEAP_STAT_FUNC_AST_SIZE,
  V7_HEAP_STAT_BCODE_OPS_SIZE,
  V7_HEAP_STAT_BCODE_LIT_TOTAL_SIZE,
  V7_HEAP_STAT_BCODE_LIT_DESER_SIZE,
  V7_HEAP_STAT_FUNC_OWNED,
  V7_HEAP_STAT_FUNC_OWNED_MAX
};

/* Returns a given heap statistics */
int v7_heap_stat(struct v7 *v7, enum v7_heap_stat_what what);
#endif

/*
 * Perform garbage collection.
 * Pass true to full in order to reclaim unused heap back to the OS.
 */
void v7_gc(struct v7 *v7, int full);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_GC_PUBLIC_H_ */
