/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/str_util.h"
#include "v7/src/internal.h"
#include "v7/src/exceptions.h"
#include "v7/src/array.h"
#include "v7/src/core.h"
#include "v7/src/eval.h"
#include "v7/src/object.h"

enum v7_err v7_throw(struct v7 *v7, v7_val_t val) {
  v7->vals.thrown_error = val;
  v7->is_thrown = 1;
  return V7_EXEC_EXCEPTION;
}

void v7_clear_thrown_value(struct v7 *v7) {
  v7->vals.thrown_error = V7_UNDEFINED;
  v7->is_thrown = 0;
}

enum v7_err v7_throwf(struct v7 *v7, const char *typ, const char *err_fmt,
                      ...) {
  /* TODO(dfrank) : get rid of v7->error_msg, allocate mem right here */
  enum v7_err rcode = V7_OK;
  va_list ap;
  val_t e = V7_UNDEFINED;
  va_start(ap, err_fmt);
  c_vsnprintf(v7->error_msg, sizeof(v7->error_msg), err_fmt, ap);
  va_end(ap);

  v7_own(v7, &e);
  rcode = create_exception(v7, typ, v7->error_msg, &e);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = v7_throw(v7, e);

clean:
  v7_disown(v7, &e);
  return rcode;
}

enum v7_err v7_rethrow(struct v7 *v7) {
  assert(v7->is_thrown);
#ifdef NDEBUG
  (void) v7;
#endif
  return V7_EXEC_EXCEPTION;
}

v7_val_t v7_get_thrown_value(struct v7 *v7, uint8_t *is_thrown) {
  if (is_thrown != NULL) {
    *is_thrown = v7->is_thrown;
  }
  return v7->vals.thrown_error;
}

/*
 * Create an instance of the exception with type `typ` (see `TYPE_ERROR`,
 * `SYNTAX_ERROR`, etc) and message `msg`.
 */
V7_PRIVATE enum v7_err create_exception(struct v7 *v7, const char *typ,
                                        const char *msg, val_t *res) {
  enum v7_err rcode = V7_OK;
  uint8_t saved_creating_exception = v7->creating_exception;
  val_t ctor_args = V7_UNDEFINED, ctor_func = V7_UNDEFINED;
#if 0
  assert(v7_is_undefined(v7->vals.thrown_error));
#endif

  *res = V7_UNDEFINED;

  v7_own(v7, &ctor_args);
  v7_own(v7, &ctor_func);

  if (v7->creating_exception) {
#ifndef NO_LIBC
    fprintf(stderr, "Exception creation throws an exception %s: %s\n", typ,
            msg);
#endif
  } else {
    v7->creating_exception = 1;

    /* Prepare arguments for the `Error` constructor */
    ctor_args = v7_mk_dense_array(v7);
    v7_array_set(v7, ctor_args, 0, v7_mk_string(v7, msg, strlen(msg), 1));

    /* Get constructor for the given error `typ` */
    ctor_func = v7_get(v7, v7->vals.global_object, typ, ~0);
    if (v7_is_undefined(ctor_func)) {
      fprintf(stderr, "cannot find exception %s\n", typ);
    }

    /* Create an error object, with prototype from constructor function */
    *res = mk_object(v7, v7_get(v7, ctor_func, "prototype", 9));

    /*
     * Finally, call the error constructor, passing an error object as `this`
     */
    V7_TRY(b_apply(v7, ctor_func, *res, ctor_args, 0, NULL));
  }

clean:
  v7->creating_exception = saved_creating_exception;

  v7_disown(v7, &ctor_func);
  v7_disown(v7, &ctor_args);

  return rcode;
}
