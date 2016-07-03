/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_EVAL_H_
#define CS_V7_SRC_EVAL_H_

#include "v7/src/internal.h"
#include "v7/src/bcode.h"

struct v7_call_frame_base;

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err eval_bcode(struct v7 *v7, struct bcode *bcode,
                                  val_t this_object, uint8_t reset_line_no,
                                  val_t *_res);

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err b_apply(struct v7 *v7, v7_val_t func, v7_val_t this_obj,
                               v7_val_t args, uint8_t is_constructor,
                               v7_val_t *res);

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err b_exec(struct v7 *v7, const char *src, size_t src_len,
                              const char *filename, val_t func, val_t args,
                              val_t this_object, int is_json, int fr,
                              uint8_t is_constructor, val_t *res);

/*
 * Try to find the call frame whose `type_mask` intersects with the given
 * `type_mask`.
 *
 * Start from the top call frame, and go deeper until the matching frame is
 * found, or there's no more call frames. If the needed frame was not found,
 * returns `NULL`.
 */
V7_PRIVATE struct v7_call_frame_base *find_call_frame(struct v7 *v7,
                                                      uint8_t type_mask);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_EVAL_H_ */
