/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/object.h"
#include "v7/src/primitive.h"

#if V7_ENABLE__Math

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#ifdef __WATCOM__
int matherr(struct _exception *exc) {
  if (exc->type == DOMAIN) {
    exc->retval = NAN;
    return 0;
  }
}
#endif

#if V7_ENABLE__Math__abs || V7_ENABLE__Math__acos || V7_ENABLE__Math__asin ||  \
    V7_ENABLE__Math__atan || V7_ENABLE__Math__ceil || V7_ENABLE__Math__cos ||  \
    V7_ENABLE__Math__exp || V7_ENABLE__Math__floor || V7_ENABLE__Math__log ||  \
    V7_ENABLE__Math__round || V7_ENABLE__Math__sin || V7_ENABLE__Math__sqrt || \
    V7_ENABLE__Math__tan
WARN_UNUSED_RESULT
static enum v7_err m_one_arg(struct v7 *v7, double (*f)(double), val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t arg0 = v7_arg(v7, 0);
  double d0 = v7_get_double(v7, arg0);
#ifdef V7_BROKEN_NAN
  if (isnan(d0)) {
    *res = V7_TAG_NAN;
    goto clean;
  }
#endif
  *res = v7_mk_number(v7, f(d0));
  goto clean;

clean:
  return rcode;
}
#endif /* V7_ENABLE__Math__* */

#if V7_ENABLE__Math__pow || V7_ENABLE__Math__atan2
WARN_UNUSED_RESULT
static enum v7_err m_two_arg(struct v7 *v7, double (*f)(double, double),
                             val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t arg0 = v7_arg(v7, 0);
  val_t arg1 = v7_arg(v7, 1);
  double d0 = v7_get_double(v7, arg0);
  double d1 = v7_get_double(v7, arg1);
#ifdef V7_BROKEN_NAN
  /* pow(NaN,0) == 1, doesn't fix atan2, but who cares */
  if (isnan(d1)) {
    *res = V7_TAG_NAN;
    goto clean;
  }
#endif
  *res = v7_mk_number(v7, f(d0, d1));
  goto clean;

clean:
  return rcode;
}
#endif /* V7_ENABLE__Math__pow || V7_ENABLE__Math__atan2 */

#define DEFINE_WRAPPER(name, func)                                   \
  WARN_UNUSED_RESULT                                                 \
  V7_PRIVATE enum v7_err Math_##name(struct v7 *v7, v7_val_t *res) { \
    return func(v7, name, res);                                      \
  }

/* Visual studio 2012+ has round() */
#if V7_ENABLE__Math__round && \
    ((defined(V7_WINDOWS) && _MSC_VER < 1700) || defined(__WATCOM__))
static double round(double n) {
  return n;
}
#endif

#if V7_ENABLE__Math__abs
DEFINE_WRAPPER(fabs, m_one_arg)
#endif
#if V7_ENABLE__Math__acos
DEFINE_WRAPPER(acos, m_one_arg)
#endif
#if V7_ENABLE__Math__asin
DEFINE_WRAPPER(asin, m_one_arg)
#endif
#if V7_ENABLE__Math__atan
DEFINE_WRAPPER(atan, m_one_arg)
#endif
#if V7_ENABLE__Math__atan2
DEFINE_WRAPPER(atan2, m_two_arg)
#endif
#if V7_ENABLE__Math__ceil
DEFINE_WRAPPER(ceil, m_one_arg)
#endif
#if V7_ENABLE__Math__cos
DEFINE_WRAPPER(cos, m_one_arg)
#endif
#if V7_ENABLE__Math__exp
DEFINE_WRAPPER(exp, m_one_arg)
#endif
#if V7_ENABLE__Math__floor
DEFINE_WRAPPER(floor, m_one_arg)
#endif
#if V7_ENABLE__Math__log
DEFINE_WRAPPER(log, m_one_arg)
#endif
#if V7_ENABLE__Math__pow
DEFINE_WRAPPER(pow, m_two_arg)
#endif
#if V7_ENABLE__Math__round
DEFINE_WRAPPER(round, m_one_arg)
#endif
#if V7_ENABLE__Math__sin
DEFINE_WRAPPER(sin, m_one_arg)
#endif
#if V7_ENABLE__Math__sqrt
DEFINE_WRAPPER(sqrt, m_one_arg)
#endif
#if V7_ENABLE__Math__tan
DEFINE_WRAPPER(tan, m_one_arg)
#endif

#if V7_ENABLE__Math__random
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Math_random(struct v7 *v7, v7_val_t *res) {
  (void) v7;
  *res = v7_mk_number(v7, (double) rand() / RAND_MAX);
  return V7_OK;
}
#endif /* V7_ENABLE__Math__random */

#if V7_ENABLE__Math__min || V7_ENABLE__Math__max
WARN_UNUSED_RESULT
static enum v7_err min_max(struct v7 *v7, int is_min, val_t *res) {
  enum v7_err rcode = V7_OK;
  double dres = NAN;
  int i, len = v7_argc(v7);

  for (i = 0; i < len; i++) {
    double v = v7_get_double(v7, v7_arg(v7, i));
    if (isnan(dres) || (is_min && v < dres) || (!is_min && v > dres)) {
      dres = v;
    }
  }

  *res = v7_mk_number(v7, dres);

  return rcode;
}
#endif /* V7_ENABLE__Math__min || V7_ENABLE__Math__max */

#if V7_ENABLE__Math__min
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Math_min(struct v7 *v7, v7_val_t *res) {
  return min_max(v7, 1, res);
}
#endif

#if V7_ENABLE__Math__max
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Math_max(struct v7 *v7, v7_val_t *res) {
  return min_max(v7, 0, res);
}
#endif

V7_PRIVATE void init_math(struct v7 *v7) {
  val_t math = v7_mk_object(v7);

#if V7_ENABLE__Math__abs
  set_cfunc_prop(v7, math, "abs", Math_fabs);
#endif
#if V7_ENABLE__Math__acos
  set_cfunc_prop(v7, math, "acos", Math_acos);
#endif
#if V7_ENABLE__Math__asin
  set_cfunc_prop(v7, math, "asin", Math_asin);
#endif
#if V7_ENABLE__Math__atan
  set_cfunc_prop(v7, math, "atan", Math_atan);
#endif
#if V7_ENABLE__Math__atan2
  set_cfunc_prop(v7, math, "atan2", Math_atan2);
#endif
#if V7_ENABLE__Math__ceil
  set_cfunc_prop(v7, math, "ceil", Math_ceil);
#endif
#if V7_ENABLE__Math__cos
  set_cfunc_prop(v7, math, "cos", Math_cos);
#endif
#if V7_ENABLE__Math__exp
  set_cfunc_prop(v7, math, "exp", Math_exp);
#endif
#if V7_ENABLE__Math__floor
  set_cfunc_prop(v7, math, "floor", Math_floor);
#endif
#if V7_ENABLE__Math__log
  set_cfunc_prop(v7, math, "log", Math_log);
#endif
#if V7_ENABLE__Math__max
  set_cfunc_prop(v7, math, "max", Math_max);
#endif
#if V7_ENABLE__Math__min
  set_cfunc_prop(v7, math, "min", Math_min);
#endif
#if V7_ENABLE__Math__pow
  set_cfunc_prop(v7, math, "pow", Math_pow);
#endif
#if V7_ENABLE__Math__random
  /* Incorporate our pointer into the RNG.
   * If srand() has not been called before, this will provide some randomness.
   * If it has, it will hopefully not make things worse.
   */
  srand(rand() ^ ((uintptr_t) v7));
  set_cfunc_prop(v7, math, "random", Math_random);
#endif
#if V7_ENABLE__Math__round
  set_cfunc_prop(v7, math, "round", Math_round);
#endif
#if V7_ENABLE__Math__sin
  set_cfunc_prop(v7, math, "sin", Math_sin);
#endif
#if V7_ENABLE__Math__sqrt
  set_cfunc_prop(v7, math, "sqrt", Math_sqrt);
#endif
#if V7_ENABLE__Math__tan
  set_cfunc_prop(v7, math, "tan", Math_tan);
#endif

#if V7_ENABLE__Math__constants
  v7_set(v7, math, "E", 1, v7_mk_number(v7, M_E));
  v7_set(v7, math, "PI", 2, v7_mk_number(v7, M_PI));
  v7_set(v7, math, "LN2", 3, v7_mk_number(v7, M_LN2));
  v7_set(v7, math, "LN10", 4, v7_mk_number(v7, M_LN10));
  v7_set(v7, math, "LOG2E", 5, v7_mk_number(v7, M_LOG2E));
  v7_set(v7, math, "LOG10E", 6, v7_mk_number(v7, M_LOG10E));
  v7_set(v7, math, "SQRT1_2", 7, v7_mk_number(v7, M_SQRT1_2));
  v7_set(v7, math, "SQRT2", 5, v7_mk_number(v7, M_SQRT2));
#endif

  v7_set(v7, v7->vals.global_object, "Math", 4, math);
}

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* V7_ENABLE__Math */
