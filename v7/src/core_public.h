/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Core
 */

#ifndef CS_V7_SRC_CORE_PUBLIC_H_
#define CS_V7_SRC_CORE_PUBLIC_H_

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "v7/src/license.h"
#include "v7/src/v7_features.h"

#include <stddef.h> /* For size_t */
#include <stdio.h>  /* For FILE */

/*
 * TODO(dfrank) : improve amalgamation, so that we'll be able to include
 * files here, and include common/platform.h
 *
 * For now, copy-pasting `WARN_UNUSED_RESULT` here
 */
#ifdef __GNUC__
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#define NOINSTR __attribute__((no_instrument_function))
#else
#define WARN_UNUSED_RESULT
#define NOINSTR
#endif

#define V7_VERSION "1.0"

#if (defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)) || \
    (defined(_MSC_VER) && _MSC_VER <= 1200)
#define V7_WINDOWS
#endif

#ifdef V7_WINDOWS
typedef unsigned __int64 uint64_t;
#else
#include <inttypes.h>
#endif
/* 64-bit value, used to store JS values */
typedef uint64_t v7_val_t;

/* JavaScript `null` value */
#define V7_NULL ((uint64_t) 0xfffe << 48)

/* JavaScript `undefined` value */
#define V7_UNDEFINED ((uint64_t) 0xfffd << 48)

/* This if-0 is a dirty workaround to force etags to pick `struct v7` */
#if 0
/* Opaque structure. V7 engine context. */
struct v7 {
  /* ... */
};
#endif

struct v7;

/*
 * Code which is returned by some of the v7 functions. If something other than
 * `V7_OK` is returned from some function, the caller function typically should
 * either immediately cleanup and return the code further, or handle the error.
 */
enum v7_err {
  V7_OK,
  V7_SYNTAX_ERROR,
  V7_EXEC_EXCEPTION,
  V7_AST_TOO_LARGE,
  V7_INTERNAL_ERROR,
};

/* JavaScript -> C call interface */
WARN_UNUSED_RESULT
typedef enum v7_err(v7_cfunction_t)(struct v7 *v7, v7_val_t *res);

/* Create V7 instance */
struct v7 *v7_create(void);

/*
 * Customizations of initial V7 state; used by `v7_create_opt()`.
 */
struct v7_create_opts {
  size_t object_arena_size;
  size_t function_arena_size;
  size_t property_arena_size;
#ifdef V7_STACK_SIZE
  void *c_stack_base;
#endif
#ifdef V7_FREEZE
  /* if not NULL, dump JS heap after init */
  char *freeze_file;
#endif
};

/*
 * Like `v7_create()`, but allows to customize initial v7 state, see `struct
 * v7_create_opts`.
 */
struct v7 *v7_create_opt(struct v7_create_opts opts);

/* Destroy V7 instance */
void v7_destroy(struct v7 *v7);

/* Return root level (`global`) object of the given V7 instance. */
v7_val_t v7_get_global(struct v7 *v);

/* Return current `this` object. */
v7_val_t v7_get_this(struct v7 *v);

/* Return current `arguments` array */
v7_val_t v7_get_arguments(struct v7 *v);

/* Return i-th argument */
v7_val_t v7_arg(struct v7 *v, unsigned long i);

/* Return the length of `arguments` */
unsigned long v7_argc(struct v7 *v7);

/*
 * Tells the GC about a JS value variable/field owned
 * by C code.
 *
 * User C code should own v7_val_t variables
 * if the value's lifetime crosses any invocation
 * to the v7 runtime that creates new objects or new
 * properties and thus can potentially trigger GC.
 *
 * The registration of the variable prevents the GC from mistakenly treat
 * the object as garbage. The GC might be triggered potentially
 * allows the GC to update pointers
 *
 * User code should also explicitly disown the variables with v7_disown once
 * it goes out of scope or the structure containing the v7_val_t field is freed.
 *
 * Example:
 *
 *  ```
 *    struct v7_val cb;
 *    v7_own(v7, &cb);
 *    cb = v7_array_get(v7, args, 0);
 *    // do something with cb
 *    v7_disown(v7, &cb);
 *  ```
 */
void v7_own(struct v7 *v7, v7_val_t *v);

/*
 * Returns 1 if value is found, 0 otherwise
 */
int v7_disown(struct v7 *v7, v7_val_t *v);

/*
 * Enable or disable GC.
 *
 * Must be called before invoking v7_exec or v7_apply
 * from within a cfunction unless you know what you're doing.
 *
 * GC is disabled during execution of cfunctions in order to simplify
 * memory management of simple cfunctions.
 * However executing even small snippets of JS code causes a lot of memory
 * pressure. Enabling GC solves that but forces you to take care of the
 * reachability of your temporary V7 v7_val_t variables, as the GC needs
 * to know where they are since objects and strings can be either reclaimed
 * or relocated during a GC pass.
 */
void v7_set_gc_enabled(struct v7 *v7, int enabled);

/*
 * Set an optional C stack limit.
 *
 * It sets a flag that will cause the interpreter
 * to throw an InterruptedError.
 * It's safe to call it from signal handlers and ISRs
 * on single threaded environments.
 */
void v7_interrupt(struct v7 *v7);

/* Returns last parser error message. TODO: rename it to `v7_get_error()` */
const char *v7_get_parser_error(struct v7 *v7);

#if defined(V7_ENABLE_STACK_TRACKING)
/*
 * Available if only `V7_ENABLE_STACK_TRACKING` is defined.
 *
 * Stack metric id. See `v7_stack_stat()`
 */
enum v7_stack_stat_what {
  /* max stack size consumed by `i_exec()` */
  V7_STACK_STAT_EXEC,
  /* max stack size consumed by `parse()` (which is called from `i_exec()`) */
  V7_STACK_STAT_PARSER,

  V7_STACK_STATS_CNT
};

/*
 * Available if only `V7_ENABLE_STACK_TRACKING` is defined.
 *
 * Returns stack metric specified by the metric id `what`. See
 * `v7_stack_stat_clean()`
 */
int v7_stack_stat(struct v7 *v7, enum v7_stack_stat_what what);

/*
 * Available if only `V7_ENABLE_STACK_TRACKING` is defined.
 *
 * Clean all stack statistics gathered so far. See `v7_stack_stat()`
 */
void v7_stack_stat_clean(struct v7 *v7);
#endif

#endif /* CS_V7_SRC_CORE_PUBLIC_H_ */
