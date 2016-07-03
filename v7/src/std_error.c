/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/string.h"
#include "v7/src/std_error.h"
#include "v7/src/object.h"
#include "v7/src/bcode.h"
#include "v7/src/primitive.h"
#include "v7/src/util.h"

/*
 * TODO(dfrank): make the top of v7->call_frame to represent the current
 * frame, and thus get rid of the `CUR_LINENO()`
 */
#ifndef V7_DISABLE_LINE_NUMBERS
#define CALLFRAME_LINENO(call_frame) ((call_frame)->line_no)
#define CUR_LINENO() (v7->line_no)
#else
#define CALLFRAME_LINENO(call_frame) 0
#define CUR_LINENO() 0
#endif

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Error_ctor(struct v7 *v7, v7_val_t *res);

#if !defined(V7_DISABLE_FILENAMES) && !defined(V7_DISABLE_LINE_NUMBERS)
static int printf_stack_line(char *p, size_t len, struct bcode *bcode,
                             int line_no, const char *leading) {
  int ret;
  const char *fn = bcode_get_filename(bcode);
  if (fn == NULL) {
    fn = "<no filename>";
  }

  if (bcode->func_name_present) {
    /* this is a function's bcode: let's show the function's name as well */
    char *funcname;

    /*
     * read first name from the bcode ops, which is the function name,
     * since `func_name_present` is set
     */
    bcode_next_name(bcode->ops.p, &funcname, NULL);

    /* Check if it's an anonymous function */
    if (funcname[0] == '\0') {
      funcname = (char *) "<anonymous>";
    }
    ret =
        snprintf(p, len, "%s    at %s (%s:%d)", leading, funcname, fn, line_no);
  } else {
    /* it's a file's bcode: show only filename and line number */
    ret = snprintf(p, len, "%s    at %s:%d", leading, fn, line_no);
  }
  return ret;
}

static int printf_stack_line_cfunc(char *p, size_t len, v7_cfunction_t *cfunc,
                                   const char *leading) {
  int ret = 0;

#if !defined(V7_FILENAMES_SUPPRESS_CFUNC_ADDR)
  int name_len =
      snprintf(NULL, 0, "cfunc_%p", (void *) cfunc) + 1 /*null-term*/;
  char *buf = (char *) malloc(name_len);

  snprintf(buf, name_len, "cfunc_%p", (void *) cfunc);
#else
  /*
   * We need this mode only for ecma test reporting, so that the
   * report is not different from one run to another
   */
  char *buf = (char *) "cfunc";
  (void) cfunc;
#endif

  ret = snprintf(p, len, "%s    at %s", leading, buf);

#if !defined(V7_FILENAMES_SUPPRESS_CFUNC_ADDR)
  free(buf);
#endif

  return ret;
}

static int print_stack_trace(char *p, size_t len,
                             struct v7_call_frame_base *call_frame) {
  char *p_cur = p;
  int total_len = 0;

  assert(call_frame->type_mask == V7_CALL_FRAME_MASK_CFUNC &&
         ((struct v7_call_frame_cfunc *) call_frame)->cfunc == Error_ctor);
  call_frame = call_frame->prev;

  while (call_frame != NULL) {
    int cur_len = 0;
    const char *leading = (total_len ? "\n" : "");
    size_t line_len = len - (p_cur - p);

    if (call_frame->type_mask & V7_CALL_FRAME_MASK_BCODE) {
      struct bcode *bcode = ((struct v7_call_frame_bcode *) call_frame)->bcode;
      if (bcode != NULL) {
        cur_len = printf_stack_line(p_cur, line_len, bcode,
                                    CALLFRAME_LINENO(call_frame), leading);
      }
    } else if (call_frame->type_mask & V7_CALL_FRAME_MASK_CFUNC) {
      cur_len = printf_stack_line_cfunc(
          p_cur, line_len, ((struct v7_call_frame_cfunc *) call_frame)->cfunc,
          leading);
    }

    total_len += cur_len;
    if (p_cur != NULL) {
      p_cur += cur_len;
    }

    call_frame = call_frame->prev;

#if !(V7_ENABLE__StackTrace)
    break;
#endif
  }

  return total_len;
}
#endif

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Error_ctor(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t arg0 = v7_arg(v7, 0);

  if (v7_is_object(this_obj) && this_obj != v7->vals.global_object) {
    *res = this_obj;
  } else {
    *res = mk_object(v7, v7->vals.error_prototype);
  }
  /* TODO(mkm): set non enumerable but provide toString method */
  v7_set(v7, *res, "message", 7, arg0);

#if !defined(V7_DISABLE_FILENAMES) && !defined(V7_DISABLE_LINE_NUMBERS)
  /* Save the stack trace */
  {
    size_t len = 0;
    val_t st_v = V7_UNDEFINED;

    v7_own(v7, &st_v);

    len = print_stack_trace(NULL, 0, v7->call_stack);

    if (len > 0) {
      /* Now, create a placeholder for string */
      st_v = v7_mk_string(v7, NULL, len, 1);
      len += 1 /*null-term*/;

      /* And fill it with actual data */
      print_stack_trace((char *) v7_get_string(v7, &st_v, NULL), len,
                        v7->call_stack);

      v7_set(v7, *res, "stack", ~0, st_v);
    }

    v7_disown(v7, &st_v);
  }
#endif

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Error_toString(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t prefix, msg = v7_get(v7, this_obj, "message", ~0);

  if (!v7_is_string(msg)) {
    *res = v7_mk_string(v7, "Error", ~0, 1);
    goto clean;
  }

  prefix = v7_mk_string(v7, "Error: ", ~0, 1);
  *res = s_concat(v7, prefix, msg);
  goto clean;

clean:
  return rcode;
}

static const char *const error_names[] = {TYPE_ERROR,      SYNTAX_ERROR,
                                          REFERENCE_ERROR, INTERNAL_ERROR,
                                          RANGE_ERROR,     EVAL_ERROR};

V7_STATIC_ASSERT(ARRAY_SIZE(error_names) == ERROR_CTOR_MAX,
                 error_name_count_mismatch);

V7_PRIVATE void init_error(struct v7 *v7) {
  val_t error;
  size_t i;

  error =
      mk_cfunction_obj_with_proto(v7, Error_ctor, 1, v7->vals.error_prototype);
  v7_def(v7, v7->vals.global_object, "Error", 5, V7_DESC_ENUMERABLE(0), error);
  set_method(v7, v7->vals.error_prototype, "toString", Error_toString, 0);

  for (i = 0; i < ARRAY_SIZE(error_names); i++) {
    error = mk_cfunction_obj_with_proto(
        v7, Error_ctor, 1, mk_object(v7, v7->vals.error_prototype));
    v7_def(v7, v7->vals.global_object, error_names[i], strlen(error_names[i]),
           V7_DESC_ENUMERABLE(0), error);
    v7->vals.error_objects[i] = error;
  }
}
