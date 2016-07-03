/*
 * Copyright (c) 2014-2016 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_HEAPUSAGE_H_
#define CS_V7_SRC_HEAPUSAGE_H_

#if defined(V7_HEAPUSAGE_ENABLE)

extern volatile int heap_dont_count;

/*
 * Returns total heap-allocated size in bytes (without any overhead of the
 * heap implementation)
 */
size_t heapusage_alloc_size(void);

/*
 * Returns number of active allocations
 */
size_t heapusage_allocs_cnt(void);

/*
 * Must be called before allocating some memory that should not be indicated as
 * memory consumed for some particular operation: for example, when we
 * preallocate some GC buffer.
 */
#define heapusage_dont_count(a) \
  do {                          \
    heap_dont_count = a;        \
  } while (0)

#else /* V7_HEAPUSAGE_ENABLE */

#define heapusage_alloc_size() (0)
#define heapusage_allocs_cnt() (0)
#define heapusage_dont_count(a)

#endif /* V7_HEAPUSAGE_ENABLE */

#endif /* CS_V7_SRC_HEAPUSAGE_H_ */
