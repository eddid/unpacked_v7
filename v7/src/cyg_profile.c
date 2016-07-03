/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * This file contains GCC/clang instrumentation callbacks. The actual
 * code in these callbacks depends on enabled features.
 *
 * Currently, the code from different subsystems is embedded right into
 * callbacks for performance reasons. It would be probably more elegant
 * to have subsystem-specific functions that will be called from these
 * callbacks, but since the callbacks are called really a lot (on each v7
 * function call), I decided it's better to inline the code right here.
 */

#include "v7/src/internal.h"
#include "v7/src/cyg_profile.h"
#include "v7/src/core.h"

#if defined(V7_CYG_PROFILE_ON)

#if defined(V7_ENABLE_CALL_TRACE)

#define CALL_TRACE_SIZE 32

typedef struct {
  uint16_t size;
  uint16_t missed_cnt;
  void *addresses[CALL_TRACE_SIZE];
} call_trace_t;

static call_trace_t call_trace = {0};

NOINSTR
void call_trace_print(const char *prefix, const char *suffix, size_t skip_cnt,
                      size_t max_cnt) {
  int i;
  if (call_trace.missed_cnt > 0) {
    fprintf(stderr, "missed calls! (%d) ", (int) call_trace.missed_cnt);
  }
  if (prefix != NULL) {
    fprintf(stderr, "%s", prefix);
  }
  for (i = (int) call_trace.size - 1 - skip_cnt; i >= 0; i--) {
    fprintf(stderr, " %lx", (unsigned long) call_trace.addresses[i]);
    if (max_cnt > 0) {
      if (--max_cnt == 0) {
        break;
      }
    }
  }
  if (suffix != NULL) {
    fprintf(stderr, "%s", suffix);
  }
  fprintf(stderr, "\n");
}

#endif

#ifndef IRAM
#define IRAM
#endif

#ifndef NOINSTR
#define NOINSTR __attribute__((no_instrument_function))
#endif

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */
IRAM NOINSTR void __cyg_profile_func_enter(void *this_fn, void *call_site);

IRAM NOINSTR void __cyg_profile_func_exit(void *this_fn, void *call_site);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

IRAM void __cyg_profile_func_enter(void *this_fn, void *call_site) {
#if defined(V7_STACK_GUARD_MIN_SIZE)
  {
    static int profile_enter = 0;
    void *fp = __builtin_frame_address(0);

    (void) call_site;

    if (profile_enter || v7_sp_limit == NULL) return;

    profile_enter++;
    if (v7_head != NULL && fp < v7_head->sp_lwm) v7_head->sp_lwm = fp;

    if (((int) fp - (int) v7_sp_limit) < V7_STACK_GUARD_MIN_SIZE) {
      printf("fun %p sp %p limit %p left %d\n", this_fn, fp, v7_sp_limit,
             (int) fp - (int) v7_sp_limit);
      abort();
    }
    profile_enter--;
  }
#endif

#if defined(V7_ENABLE_GC_CHECK)
  {
    (void) this_fn;
    (void) call_site;
  }
#endif

#if defined(V7_ENABLE_STACK_TRACKING)
  {
    struct v7 *v7;
    struct stack_track_ctx *ctx;
    void *fp = __builtin_frame_address(1);

    (void) this_fn;
    (void) call_site;

    /*
     * TODO(dfrank): it actually won't work for multiple instances of v7 running
     * in parallel threads. We need to know the exact v7 instance for which
     * current function is called, but so far I failed to find a way to do this.
     */
    for (v7 = v7_head; v7 != NULL; v7 = v7->next_v7) {
      for (ctx = v7->stack_track_ctx; ctx != NULL; ctx = ctx->next) {
        /* commented because it fails on legal code compiled with -O3 */
        /*assert(fp <= ctx->start);*/

        if (fp < ctx->max) {
          ctx->max = fp;
        }
      }
    }
  }
#endif

#if defined(V7_ENABLE_CALL_TRACE)
  if (call_trace.size < CALL_TRACE_SIZE) {
    call_trace.addresses[call_trace.size] = this_fn;
    call_trace.size++;
  } else {
    call_trace.missed_cnt++;
  }
#endif
}

IRAM void __cyg_profile_func_exit(void *this_fn, void *call_site) {
#if defined(V7_STACK_GUARD_MIN_SIZE)
  {
    (void) this_fn;
    (void) call_site;
  }
#endif

#if defined(V7_ENABLE_GC_CHECK)
  {
    struct v7 *v7;
    void *fp = __builtin_frame_address(1);

    (void) this_fn;
    (void) call_site;

    for (v7 = v7_head; v7 != NULL; v7 = v7->next_v7) {
      v7_val_t **vp;
      if (v7->owned_values.buf == NULL) continue;
      vp = (v7_val_t **) (v7->owned_values.buf + v7->owned_values.len -
                          sizeof(v7_val_t *));

      for (; (char *) vp >= v7->owned_values.buf; vp--) {
        /*
         * Check if a variable belongs to a dead stack frame.
         * Addresses lower than the parent frame belong to the
         * stack frame of the function about to return.
         * But the heap also usually below the stack and
         * we don't know the end of the stack. But this hook
         * is called at each function return, so we have
         * to check only up to the maximum stack frame size,
         * let's arbitrarily but reasonably set that at 8k.
         */
        if ((void *) *vp <= fp && (void *) *vp > (fp + 8196)) {
          fprintf(stderr, "Found owned variable after return\n");
          abort();
        }
      }
    }
  }
#endif

#if defined(V7_ENABLE_STACK_TRACKING)
  {
    (void) this_fn;
    (void) call_site;
  }
#endif

#if defined(V7_ENABLE_CALL_TRACE)
  if (call_trace.missed_cnt > 0) {
    call_trace.missed_cnt--;
  } else if (call_trace.size > 0) {
    if (call_trace.addresses[call_trace.size - 1] != this_fn) {
      abort();
    }
    call_trace.size--;
  } else {
    /*
     * We may get here if calls to `__cyg_profile_func_exit()` and
     * `__cyg_profile_func_enter()` are unbalanced.
     *
     * TODO(dfrank) understand, why in the beginning of the program execution
     * we get here. I was sure this should be impossible.
     */
    /* abort(); */
  }
#endif
}

#if defined(V7_ENABLE_STACK_TRACKING)

void v7_stack_track_start(struct v7 *v7, struct stack_track_ctx *ctx) {
  /* insert new context at the head of the list */
  ctx->next = v7->stack_track_ctx;
  v7->stack_track_ctx = ctx;

  /* init both `max` and `start` to the current frame pointer */
  ctx->max = ctx->start = __builtin_frame_address(0);
}

int v7_stack_track_end(struct v7 *v7, struct stack_track_ctx *ctx) {
  int diff;

  /* this function can be called only for the head context */
  assert(v7->stack_track_ctx == ctx);

  diff = (int) ((char *) ctx->start - (char *) ctx->max);

  /* remove context from the linked list */
  v7->stack_track_ctx = ctx->next;

  return (int) diff;
}

#endif /* V7_ENABLE_STACK_TRACKING */
#endif /* V7_CYG_PROFILE_ON */
