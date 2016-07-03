/*
 * Copyright (c) 2014-2016 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/builtin/builtin.h"

#ifdef V7_ENABLE_UBJSON

#include <string.h>
#include <assert.h>

#include "common/ubjson.h"

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/object.h"
#include "v7/src/array.h"
#include "v7/src/primitive.h"
#include "v7/src/string.h"
#include "v7/src/util.h"
#include "v7/src/exceptions.h"
#include "v7/src/exec.h"
#include "v7/src/function.h"

struct ubjson_ctx {
  struct mbuf out;   /* output buffer */
  struct mbuf stack; /* visit stack */
  v7_val_t cb;       /* called to render data  */
  v7_val_t errb;     /* called to finish; successo or rerror */
  v7_val_t bin;      /* current Bin object */
  size_t bytes_left; /* bytes left in current Bin generator */
};

struct visit {
  v7_val_t obj;
  union {
    size_t next_idx;
    void *p;
  } v;
};

static void _ubjson_call_cb(struct v7 *v7, struct ubjson_ctx *ctx) {
  v7_val_t res, cb, args = v7_mk_array(v7);
  v7_own(v7, &args);

  if (ctx->out.buf == NULL) {
    /* signal end of stream */
    v7_array_push(v7, args, V7_UNDEFINED);
    cb = ctx->errb;
  } else if (ctx->out.len > 0) {
    v7_array_push(v7, args, v7_mk_string(v7, ctx->out.buf, ctx->out.len, 1));
    ctx->out.len = 0;
    cb = ctx->cb;
  } else {
    /* avoid calling cb with no output */
    goto cleanup;
  }

  if (v7_apply(v7, cb, V7_UNDEFINED, args, &res) != V7_OK) {
    fprintf(stderr, "Got error while calling ubjson cb: ");
    v7_fprintln(stderr, v7, res);
  }

cleanup:
  v7_disown(v7, &args);
}

struct visit *push_visit(struct mbuf *stack, v7_val_t obj) {
  struct visit *res;
  size_t pos = stack->len;
  mbuf_append(stack, NULL, sizeof(struct visit));
  res = (struct visit *) (stack->buf + pos);
  memset(res, 0, sizeof(struct visit));
  res->obj = obj;
  return res;
}

struct visit *cur_visit(struct mbuf *stack) {
  if (stack->len == 0) return NULL;
  return (struct visit *) (stack->buf + stack->len - sizeof(struct visit));
}

void pop_visit(struct mbuf *stack) {
  stack->len -= sizeof(struct visit);
}

static struct ubjson_ctx *ubjson_ctx_new(struct v7 *v7, val_t cb, val_t errb) {
  struct ubjson_ctx *ctx = (struct ubjson_ctx *) malloc(sizeof(*ctx));
  mbuf_init(&ctx->out, 0);
  mbuf_init(&ctx->stack, 0);
  ctx->cb = cb;
  ctx->errb = errb;
  ctx->bin = V7_UNDEFINED;
  v7_own(v7, &ctx->cb);
  v7_own(v7, &ctx->errb);
  v7_own(v7, &ctx->bin);
  return ctx;
}

static void ubjson_ctx_free(struct v7 *v7, struct ubjson_ctx *ctx) {
  /*
   * Clear out reference to this context in case there is some lingering
   * callback.
   */
  if (!v7_is_undefined(ctx->bin)) {
    v7_set(v7, ctx->bin, "ctx", ~0, V7_UNDEFINED);
  }
  v7_disown(v7, &ctx->bin);
  v7_disown(v7, &ctx->errb);
  v7_disown(v7, &ctx->cb);
  mbuf_free(&ctx->out);
  mbuf_free(&ctx->stack);
  free(ctx);
}

/* This will be called many time to advance rendering of an ubjson ctx */
static enum v7_err _ubjson_render_cont(struct v7 *v7, struct ubjson_ctx *ctx) {
  enum v7_err rcode = V7_OK;
  uint8_t fr = 1;
  struct mbuf *buf = &ctx->out, *stack = &ctx->stack;
  struct visit *cur;
  v7_val_t gen_proto = v7_get(
      v7, v7_get(v7, v7_get(v7, v7_get_global(v7), "UBJSON", ~0), "Bin", ~0),
      "prototype", ~0);

  if (ctx->out.len > 0) {
    _ubjson_call_cb(v7, ctx);
  }

  for (cur = cur_visit(stack); cur != NULL; cur = cur_visit(stack)) {
    v7_val_t obj = cur->obj;

    if (v7_is_undefined(obj)) {
      cs_ubjson_emit_null(buf);
    } else if (v7_is_null(obj)) {
      cs_ubjson_emit_null(buf);
    } else if (v7_is_boolean(obj)) {
      cs_ubjson_emit_boolean(buf, v7_get_bool(v7, obj));
    } else if (v7_is_number(obj)) {
      cs_ubjson_emit_autonumber(buf, v7_get_double(v7, obj));
    } else if (v7_is_string(obj)) {
      size_t n;
      const char *s = v7_get_string(v7, &obj, &n);
      cs_ubjson_emit_string(buf, s, n);
    } else if (v7_is_array(v7, obj)) {
      unsigned long cur_idx = cur->v.next_idx;

      if (cur->v.next_idx == 0) {
        cs_ubjson_open_array(buf);
      }

      cur->v.next_idx++;

      if (cur->v.next_idx > v7_array_length(v7, cur->obj)) {
        cs_ubjson_close_array(buf);
      } else {
        cur = push_visit(stack, v7_array_get(v7, obj, cur_idx));
        /* skip default popping of visitor frame */
        continue;
      }
    } else if (v7_is_object(obj)) {
      size_t n;
      v7_val_t name;
      const char *s;

      if (obj_prototype_v(v7, obj) == gen_proto) {
        ctx->bytes_left = v7_get_double(v7, v7_get(v7, obj, "size", ~0));
        cs_ubjson_emit_bin_header(buf, ctx->bytes_left);
        ctx->bin = obj;
        v7_set(v7, obj, "ctx", ~0, v7_mk_foreign(v7, ctx));
        pop_visit(stack);
        rcode =
            v7_apply(v7, v7_get(v7, obj, "user", ~0), obj, V7_UNDEFINED, NULL);
        if (rcode != V7_OK) {
          goto clean;
        }

        /*
         * The user generator will reenter calling this function again with the
         * same context.
         */
        fr = 0;
        goto clean;
      }

      if (cur->v.p == NULL) {
        cs_ubjson_open_object(buf);
      }

      cur->v.p = v7_next_prop(cur->v.p, obj, &name, NULL, NULL);

      if (cur->v.p == NULL) {
        cs_ubjson_close_object(buf);
      } else {
        v7_val_t tmp = V7_UNDEFINED;
        s = v7_get_string(v7, &name, &n);
        cs_ubjson_emit_object_key(buf, s, n);

        rcode = v7_get_throwing_v(v7, obj, name, &tmp);
        if (rcode != V7_OK) {
          goto clean;
        }

        cur = push_visit(stack, tmp);
        /* skip default popping of visitor frame */
        continue;
      }
    } else {
      fprintf(stderr, "ubsjon: unsupported object: ");
      v7_fprintln(stderr, v7, obj);
    }

    pop_visit(stack);
  }

  if (ctx->out.len > 0) {
    _ubjson_call_cb(v7, ctx);
  }
  _ubjson_call_cb(v7, ctx);

clean:
  if (fr) {
    ubjson_ctx_free(v7, ctx);
  }
  return rcode;
}

WARN_UNUSED_RESULT
static enum v7_err _ubjson_render(struct v7 *v7, struct ubjson_ctx *ctx,
                                  v7_val_t root) {
  push_visit(&ctx->stack, root);
  return _ubjson_render_cont(v7, ctx);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err UBJSON_render(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t obj = v7_arg(v7, 0), cb = v7_arg(v7, 1), errb = v7_arg(v7, 2);
  (void) res;

  struct ubjson_ctx *ctx = ubjson_ctx_new(v7, cb, errb);
  rcode = _ubjson_render(v7, ctx, obj);
  if (rcode != V7_OK) {
    goto clean;
  }

clean:
  return rcode;
}

V7_PRIVATE enum v7_err Bin_send(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  struct ubjson_ctx *ctx;
  size_t n;
  v7_val_t arg;
  val_t this_obj = v7_get_this(v7);
  const char *s;

  (void) res;

  arg = v7_arg(v7, 0);
  ctx = (struct ubjson_ctx *) v7_get_ptr(v7, v7_get(v7, this_obj, "ctx", ~0));
  if (ctx == NULL) {
    rcode = v7_throwf(v7, "Error", "UBJSON context closed\n");
    goto clean;
  }
  s = v7_get_string(v7, &arg, &n);
  if (n > ctx->bytes_left) {
    n = ctx->bytes_left;
  } else {
    ctx->bytes_left -= n;
  }
  /*
   * TODO(mkm):
   * this is useless buffering, we should call ubjson cb directly
   */
  mbuf_append(&ctx->out, s, n);
  _ubjson_call_cb(v7, ctx);

  if (ctx->bytes_left == 0) {
    _ubjson_render_cont(v7, ctx);
  }

clean:
  return rcode;
}

V7_PRIVATE enum v7_err UBJSON_Bin(struct v7 *v7, v7_val_t *res) {
  v7_val_t this_obj = v7_get_this(v7);

  (void) res;

  v7_set(v7, this_obj, "size", ~0, v7_arg(v7, 0));
  v7_set(v7, this_obj, "user", ~0, v7_arg(v7, 1));

  return V7_OK;
}

void init_ubjson(struct v7 *v7) {
  v7_val_t gen_proto, ubjson;
  ubjson = v7_mk_object(v7);
  v7_set(v7, v7_get_global(v7), "UBJSON", 6, ubjson);
  v7_set_method(v7, ubjson, "render", UBJSON_render);
  gen_proto = v7_mk_object(v7);
  v7_set(v7, ubjson, "Bin", ~0,
         v7_mk_function_with_proto(v7, UBJSON_Bin, gen_proto));
  v7_set_method(v7, gen_proto, "send", Bin_send);
}

#else
void init_ubjson(struct v7 *v7) {
  (void) v7;
}
#endif
