/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_INTERNAL_H_
#define CS_V7_SRC_INTERNAL_H_

#include "v7/src/license.h"

/* Check whether we're compiling in an environment with no filesystem */
#if defined(ARDUINO) && (ARDUINO == 106)
#define V7_NO_FS
#endif

#ifndef FAST
#define FAST
#endif

#ifndef STATIC
#define STATIC
#endif

#ifndef ENDL
#define ENDL "\n"
#endif

/*
 * In some compilers (watcom) NAN == NAN (and other comparisons) don't follow
 * the rules of IEEE 754. Since we don't know a priori which compilers
 * will generate correct code, we disable the fallback on selected platforms.
 * TODO(mkm): selectively disable on clang/gcc once we test this out.
 */
#define V7_BROKEN_NAN

#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#ifndef NO_LIBC
#include <ctype.h>
#endif
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

/* Public API. Implemented in api.c */
#include "common/platform.h"

#ifdef V7_WINDOWS
#define vsnprintf _vsnprintf
#define snprintf _snprintf

/* VS2015 Update 1 has ISO C99 `isnan` and `isinf` defined in math.h */
#if _MSC_FULL_VER < 190023506
#define isnan(x) _isnan(x)
#define isinf(x) (!_finite(x))
#endif

#define __unused __pragma(warning(suppress : 4100))
typedef __int64 int64_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

/* For 64bit VisualStudio 2010 */
#ifndef _UINTPTR_T_DEFINED
typedef unsigned long uintptr_t;
#endif

#ifndef __func__
#define __func__ ""
#endif

#else
#include <stdint.h>
#endif

#include "v7/src/v7_features.h"

/* MSVC6 doesn't have standard C math constants defined */
#ifndef M_E
#define M_E 2.71828182845904523536028747135266250
#endif

#ifndef M_LOG2E
#define M_LOG2E 1.44269504088896340735992468100189214
#endif

#ifndef M_LOG10E
#define M_LOG10E 0.434294481903251827651128918916605082
#endif

#ifndef M_LN2
#define M_LN2 0.693147180559945309417232121458176568
#endif

#ifndef M_LN10
#define M_LN10 2.30258509299404568401799145468436421
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880168872420969808
#endif

#ifndef M_SQRT1_2
#define M_SQRT1_2 0.707106781186547524400844362104849039
#endif

#ifndef NAN
extern double _v7_nan;
#define HAS_V7_NAN
#define NAN (_v7_nan)
#endif

#ifndef INFINITY
extern double _v7_infinity;
#define HAS_V7_INFINITY
#define INFINITY (_v7_infinity)
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#if defined(V7_ENABLE_GC_CHECK) || defined(V7_STACK_GUARD_MIN_SIZE) || \
    defined(V7_ENABLE_STACK_TRACKING) || defined(V7_ENABLE_CALL_TRACE)
/* Need to enable GCC/clang instrumentation */
#define V7_CYG_PROFILE_ON
#endif

#if defined(V7_CYG_PROFILE_ON)
extern struct v7 *v7_head;

#if defined(V7_STACK_GUARD_MIN_SIZE)
extern void *v7_sp_limit;
#endif
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

#define V7_STATIC_ASSERT(COND, MSG) \
  typedef char static_assertion_##MSG[2 * (!!(COND)) - 1]

#define BUF_LEFT(size, used) (((size_t)(used) < (size)) ? ((size) - (used)) : 0)

#endif /* CS_V7_SRC_INTERNAL_H_ */
