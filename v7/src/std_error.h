/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_STD_ERROR_H_
#define CS_V7_SRC_STD_ERROR_H_

#include "v7/src/license.h"

struct v7;

/*
 * JavaScript error types
 */
#define TYPE_ERROR "TypeError"
#define SYNTAX_ERROR "SyntaxError"
#define REFERENCE_ERROR "ReferenceError"
#define INTERNAL_ERROR "InternalError"
#define RANGE_ERROR "RangeError"
#define EVAL_ERROR "EvalError"
#define ERROR_CTOR_MAX 6
/*
 * TODO(mkm): EvalError is not so important, we should guard it behind
 * something like `V7_ENABLE__EvalError`. However doing so makes it hard to
 * keep ERROR_CTOR_MAX up to date; perhaps let's find a better way of doing it.
 *
 * EvalError is useful mostly because we now have ecma tests failing:
 *
 * 8129 FAIL ch15/15.4/15.4.4/15.4.4.16/15.4.4.16-7-c-iii-24.js (tail -c
 * +7600043 tests/ecmac.db|head -c 496): [{"message":"[EvalError] is not
 * defined"}]
 *
 * Those tests are not EvalError specific, and they do test that the exception
 * handling machinery works as intended.
 */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

V7_PRIVATE void init_error(struct v7 *v7);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_STD_ERROR_H_ */
