/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === Execution of JavaScript code
 */

#ifndef CS_V7_SRC_EXEC_PUBLIC_H_
#define CS_V7_SRC_EXEC_PUBLIC_H_

#include "v7/src/core_public.h"

/*
 * Execute JavaScript `js_code`. The result of evaluation is stored in
 * the `result` variable.
 *
 * Return:
 *
 *  - V7_OK on success. `result` contains the result of execution.
 *  - V7_SYNTAX_ERROR if `js_code` in not a valid code. `result` is undefined.
 *  - V7_EXEC_EXCEPTION if `js_code` threw an exception. `result` stores
 *    an exception object.
 *  - V7_AST_TOO_LARGE if `js_code` contains an AST segment longer than 16 bit.
 *    `result` is undefined. To avoid this error, build V7 with V7_LARGE_AST.
 */
WARN_UNUSED_RESULT
enum v7_err v7_exec(struct v7 *v7, const char *js_code, v7_val_t *result);

/*
 * Options for `v7_exec_opt()`. To get default options, like `v7_exec()` uses,
 * just zero out this struct.
 */
struct v7_exec_opts {
  /* Filename, used for stack traces only */
  const char *filename;

  /*
   * Object to be used as `this`. Note: when it is zeroed out, i.e. it's a
   * number `0`, the `undefined` value is assumed. It means that it's
   * impossible to actually use the number `0` as `this` object, but it makes
   * little sense anyway.
   */
  v7_val_t this_obj;

  /* Whether the given `js_code` should be interpreted as JSON, not JS code */
  unsigned is_json : 1;
};

/*
 * Customizable version of `v7_exec()`: allows to specify various options, see
 * `struct v7_exec_opts`.
 */
enum v7_err v7_exec_opt(struct v7 *v7, const char *js_code,
                        const struct v7_exec_opts *opts, v7_val_t *res);

/*
 * Same as `v7_exec()`, but loads source code from `path` file.
 */
WARN_UNUSED_RESULT
enum v7_err v7_exec_file(struct v7 *v7, const char *path, v7_val_t *result);

/*
 * Parse `str` and store corresponding JavaScript object in `res` variable.
 * String `str` should be '\0'-terminated.
 * Return value and semantic is the same as for `v7_exec()`.
 */
WARN_UNUSED_RESULT
enum v7_err v7_parse_json(struct v7 *v7, const char *str, v7_val_t *res);

/*
 * Same as `v7_parse_json()`, but loads JSON string from `path`.
 */
WARN_UNUSED_RESULT
enum v7_err v7_parse_json_file(struct v7 *v7, const char *path, v7_val_t *res);

/*
 * Compile JavaScript code `js_code` into the byte code and write generated
 * byte code into opened file stream `fp`. If `generate_binary_output` is 0,
 * then generated byte code is in human-readable text format. Otherwise, it is
 * in the binary format, suitable for execution by V7 instance.
 * NOTE: `fp` must be a valid, opened, writable file stream.
 */
WARN_UNUSED_RESULT
enum v7_err v7_compile(const char *js_code, int generate_binary_output,
                       int use_bcode, FILE *fp);

/*
 * Call function `func` with arguments `args`, using `this_obj` as `this`.
 * `args` should be an array containing arguments or `undefined`.
 *
 * `res` can be `NULL` if return value is not required.
 */
WARN_UNUSED_RESULT
enum v7_err v7_apply(struct v7 *v7, v7_val_t func, v7_val_t this_obj,
                     v7_val_t args, v7_val_t *res);

#endif /* CS_V7_SRC_EXEC_PUBLIC_H_ */
