/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_CYG_PROFILE_H_
#define CS_V7_SRC_CYG_PROFILE_H_

/*
 * This file contains GCC/clang instrumentation callbacks, as well as
 * accompanying code. The actual code in these callbacks depends on enabled
 * features. See cyg_profile.c for some implementation details rationale.
 */

struct v7;

#if defined(V7_ENABLE_STACK_TRACKING)

/*
 * Stack-tracking functionality:
 *
 * The idea is that the caller should allocate `struct stack_track_ctx`
 * (typically on stack) in the function to track the stack usage of, and call
 * `v7_stack_track_start()` in the beginning.
 *
 * Before quitting current stack frame (for example, before returning from
 * function), call `v7_stack_track_end()`, which returns the maximum stack
 * consumed size.
 *
 * These calls can be nested: for example, we may track the stack usage of the
 * whole application by using these functions in `main()`, as well as track
 * stack usage of any nested functions.
 *
 * Just to stress: both `v7_stack_track_start()` / `v7_stack_track_end()`
 * should be called for the same instance of `struct stack_track_ctx` in the
 * same stack frame.
 */

/* stack tracking context */
struct stack_track_ctx {
  struct stack_track_ctx *next;
  void *start;
  void *max;
};

/* see explanation above */
void v7_stack_track_start(struct v7 *v7, struct stack_track_ctx *ctx);
/* see explanation above */
int v7_stack_track_end(struct v7 *v7, struct stack_track_ctx *ctx);

void v7_stack_stat_clean(struct v7 *v7);

#endif /* V7_ENABLE_STACK_TRACKING */

#endif /* CS_V7_SRC_CYG_PROFILE_H_ */
