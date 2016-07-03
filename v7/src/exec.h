/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_EXEC_H_
#define CS_V7_SRC_EXEC_H_

#include "v7/src/exec_public.h"

#include "v7/src/core.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*
 * At the moment, all exec-related functions are public, and are declared in
 * `exec_public.h`
 */

WARN_UNUSED_RESULT
enum v7_err _v7_compile(const char *js_code, size_t js_code_size,
                        int generate_binary_output, int use_bcode, FILE *fp);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_EXEC_H_ */
