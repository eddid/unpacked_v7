/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/utf.h"
#include "v7/src/internal.h"
#include "v7/src/std_string.h"
#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/string.h"
#include "v7/src/slre.h"
#include "v7/src/std_regex.h"
#include "v7/src/std_object.h"
#include "v7/src/eval.h"
#include "v7/src/conversion.h"
#include "v7/src/array.h"
#include "v7/src/object.h"
#include "v7/src/regexp.h"
#include "v7/src/exceptions.h"

/* Substring implementations: RegExp-based and String-based {{{ */

/*
 * Substring context: currently, used in Str_split() only, but will probably
 * be used in Str_replace() and other functions as well.
 *
 * Needed to provide different implementation for RegExp or String arguments,
 * keeping common parts reusable.
 */
struct _str_split_ctx {
  /* implementation-specific data */
  union {
#if V7_ENABLE__RegExp
    struct {
      struct slre_prog *prog;
      struct slre_loot loot;
    } regexp;
#endif

    struct {
      val_t sep;
    } string;
  } impl;

  struct v7 *v7;

  /* start and end of previous match (set by `p_exec()`) */
  const char *match_start;
  const char *match_end;

  /* pointers to implementation functions */

  /*
   * Initialize context
   */
  void (*p_init)(struct _str_split_ctx *ctx, struct v7 *v7, val_t sep);

  /*
   * Look for the next match, set `match_start` and `match_end` to appropriate
   * values.
   *
   * Returns 0 if match found, 1 otherwise (in accordance with `slre_exec()`)
   */
  int (*p_exec)(struct _str_split_ctx *ctx, const char *start, const char *end);

#if V7_ENABLE__RegExp
  /*
   * Add captured data to resulting array (for RegExp-based implementation only)
   *
   * Returns updated `elem` value
   */
  long (*p_add_caps)(struct _str_split_ctx *ctx, val_t res, long elem,
                     long limit);
#endif
};

#if V7_ENABLE__RegExp
/* RegExp-based implementation of `p_init` in `struct _str_split_ctx` */
static void subs_regexp_init(struct _str_split_ctx *ctx, struct v7 *v7,
                             val_t sep) {
  ctx->v7 = v7;
  ctx->impl.regexp.prog = v7_get_regexp_struct(v7, sep)->compiled_regexp;
}

/* RegExp-based implementation of `p_exec` in `struct _str_split_ctx` */
static int subs_regexp_exec(struct _str_split_ctx *ctx, const char *start,
                            const char *end) {
  int ret =
      slre_exec(ctx->impl.regexp.prog, 0, start, end, &ctx->impl.regexp.loot);

  ctx->match_start = ctx->impl.regexp.loot.caps[0].start;
  ctx->match_end = ctx->impl.regexp.loot.caps[0].end;

  return ret;
}

/* RegExp-based implementation of `p_add_caps` in `struct _str_split_ctx` */
static long subs_regexp_split_add_caps(struct _str_split_ctx *ctx, val_t res,
                                       long elem, long limit) {
  int i;
  for (i = 1; i < ctx->impl.regexp.loot.num_captures && elem < limit; i++) {
    size_t cap_len =
        ctx->impl.regexp.loot.caps[i].end - ctx->impl.regexp.loot.caps[i].start;
    v7_array_push(
        ctx->v7, res,
        (ctx->impl.regexp.loot.caps[i].start != NULL)
            ? v7_mk_string(ctx->v7, ctx->impl.regexp.loot.caps[i].start,
                           cap_len, 1)
            : V7_UNDEFINED);
    elem++;
  }
  return elem;
}
#endif

/* String-based implementation of `p_init` in `struct _str_split_ctx` */
static void subs_string_init(struct _str_split_ctx *ctx, struct v7 *v7,
                             val_t sep) {
  ctx->v7 = v7;
  ctx->impl.string.sep = sep;
}

/* String-based implementation of `p_exec` in `struct _str_split_ctx` */
static int subs_string_exec(struct _str_split_ctx *ctx, const char *start,
                            const char *end) {
  int ret = 1;
  size_t sep_len;
  const char *psep = v7_get_string(ctx->v7, &ctx->impl.string.sep, &sep_len);

  if (sep_len == 0) {
    /* separator is an empty string: match empty string */
    ctx->match_start = start;
    ctx->match_end = start;
    ret = 0;
  } else {
    size_t i;
    for (i = 0; start <= (end - sep_len); ++i, start = utfnshift(start, 1)) {
      if (memcmp(start, psep, sep_len) == 0) {
        ret = 0;
        ctx->match_start = start;
        ctx->match_end = start + sep_len;
        break;
      }
    }
  }

  return ret;
}

#if V7_ENABLE__RegExp
/* String-based implementation of `p_add_caps` in `struct _str_split_ctx` */
static long subs_string_split_add_caps(struct _str_split_ctx *ctx, val_t res,
                                       long elem, long limit) {
  /* this is a stub function */
  (void) ctx;
  (void) res;
  (void) limit;
  return elem;
}
#endif

/* }}} */

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err String_ctor(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t arg0 = v7_arg(v7, 0);

  *res = arg0;

  if (v7_argc(v7) == 0) {
    *res = v7_mk_string(v7, NULL, 0, 1);
  } else if (!v7_is_string(arg0)) {
    rcode = to_string(v7, arg0, res, NULL, 0, NULL);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

  if (v7_is_generic_object(this_obj) && this_obj != v7->vals.global_object) {
    obj_prototype_set(v7, get_object_struct(this_obj),
                      get_object_struct(v7->vals.string_prototype));
    v7_def(v7, this_obj, "", 0, _V7_DESC_HIDDEN(1), *res);
    /*
     * implicitly returning `this`: `call_cfunction()` in bcode.c will do
     * that for us
     */
    goto clean;
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_fromCharCode(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  int i, num_args = v7_argc(v7);

  *res = v7_mk_string(v7, "", 0, 1); /* Empty string */

  for (i = 0; i < num_args; i++) {
    char buf[10];
    val_t arg = v7_arg(v7, i);
    double d = v7_get_double(v7, arg);
    Rune r = (Rune)((int32_t)(isnan(d) || isinf(d) ? 0 : d) & 0xffff);
    int n = runetochar(buf, &r);
    val_t s = v7_mk_string(v7, buf, n, 1);
    *res = s_concat(v7, *res, s);
  }

  return rcode;
}

WARN_UNUSED_RESULT
static enum v7_err s_charCodeAt(struct v7 *v7, double *res) {
  return v7_char_code_at(v7, v7_get_this(v7), v7_arg(v7, 0), res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_charCodeAt(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  double dnum = 0;

  rcode = s_charCodeAt(v7, &dnum);
  if (rcode != V7_OK) {
    goto clean;
  }

  *res = v7_mk_number(v7, dnum);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_charAt(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  double code = 0;
  char buf[10] = {0};
  int len = 0;

  rcode = s_charCodeAt(v7, &code);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (!isnan(code)) {
    Rune r = (Rune) code;
    len = runetochar(buf, &r);
  }
  *res = v7_mk_string(v7, buf, len, 1);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_concat(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  int i, num_args = v7_argc(v7);

  rcode = to_string(v7, this_obj, res, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

  for (i = 0; i < num_args; i++) {
    val_t str = V7_UNDEFINED;

    rcode = to_string(v7, v7_arg(v7, i), &str, NULL, 0, NULL);
    if (rcode != V7_OK) {
      goto clean;
    }

    *res = s_concat(v7, *res, str);
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
static enum v7_err s_index_of(struct v7 *v7, int last, val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t arg0 = v7_arg(v7, 0);
  size_t fromIndex = 0;
  double dres = -1;

  if (!v7_is_undefined(arg0)) {
    const char *p1, *p2, *end;
    size_t i, len1, len2, bytecnt1, bytecnt2;
    val_t sub = V7_UNDEFINED;

    rcode = to_string(v7, arg0, &sub, NULL, 0, NULL);
    if (rcode != V7_OK) {
      goto clean;
    }

    rcode = to_string(v7, this_obj, &this_obj, NULL, 0, NULL);
    if (rcode != V7_OK) {
      goto clean;
    }

    p1 = v7_get_string(v7, &this_obj, &bytecnt1);
    p2 = v7_get_string(v7, &sub, &bytecnt2);

    if (bytecnt2 <= bytecnt1) {
      end = p1 + bytecnt1;
      len1 = utfnlen(p1, bytecnt1);
      len2 = utfnlen(p2, bytecnt2);

      if (v7_argc(v7) > 1) {
        /* `fromIndex` was provided. Normalize it */
        double d = 0;
        {
          val_t arg = v7_arg(v7, 1);
          rcode = to_number_v(v7, arg, &arg);
          if (rcode != V7_OK) {
            goto clean;
          }
          d = v7_get_double(v7, arg);
        }
        if (isnan(d) || d < 0) {
          d = 0.0;
        } else if (isinf(d) || d > len1) {
          d = len1;
        }
        fromIndex = d;

        /* adjust pointers accordingly to `fromIndex` */
        if (last) {
          const char *end_tmp = utfnshift(p1, fromIndex + len2);
          end = (end_tmp < end) ? end_tmp : end;
        } else {
          p1 = utfnshift(p1, fromIndex);
        }
      }

      /*
       * TODO(dfrank): when `last` is set, start from the end and look
       * backward. We'll need to improve `utfnshift()` for that, so that it can
       * handle negative offsets.
       */
      for (i = 0; p1 <= (end - bytecnt2); i++, p1 = utfnshift(p1, 1)) {
        if (memcmp(p1, p2, bytecnt2) == 0) {
          dres = i;
          if (!last) break;
        }
      }
    }
  }
  if (!last && dres >= 0) dres += fromIndex;
  *res = v7_mk_number(v7, dres);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_valueOf(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);

  if (!v7_is_string(this_obj) &&
      (v7_is_object(this_obj) &&
       obj_prototype_v(v7, this_obj) != v7->vals.string_prototype)) {
    rcode =
        v7_throwf(v7, TYPE_ERROR, "String.valueOf called on non-string object");
    goto clean;
  }

  rcode = Obj_valueOf(v7, res);
  goto clean;

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_indexOf(struct v7 *v7, v7_val_t *res) {
  return s_index_of(v7, 0, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_lastIndexOf(struct v7 *v7, v7_val_t *res) {
  return s_index_of(v7, 1, res);
}

#if V7_ENABLE__String__localeCompare
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_localeCompare(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t arg0 = V7_UNDEFINED;
  val_t s = V7_UNDEFINED;

  rcode = to_string(v7, v7_arg(v7, 0), &arg0, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = to_string(v7, this_obj, &s, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

  *res = v7_mk_number(v7, s_cmp(v7, s, arg0));

clean:
  return rcode;
}
#endif

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_toString(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);

  if (this_obj == v7->vals.string_prototype) {
    *res = v7_mk_string(v7, "false", 5, 1);
    goto clean;
  }

  if (!v7_is_string(this_obj) &&
      !(v7_is_generic_object(this_obj) &&
        is_prototype_of(v7, this_obj, v7->vals.string_prototype))) {
    rcode = v7_throwf(v7, TYPE_ERROR,
                      "String.toString called on non-string object");
    goto clean;
  }

  rcode = obj_value_of(v7, this_obj, &this_obj);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = to_string(v7, this_obj, res, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

clean:
  return rcode;
}

#if V7_ENABLE__RegExp
WARN_UNUSED_RESULT
enum v7_err call_regex_ctor(struct v7 *v7, val_t arg, val_t *res) {
  /* TODO(mkm): make general helper out of this */
  enum v7_err rcode = V7_OK;
  val_t saved_args = v7->vals.arguments, args = v7_mk_dense_array(v7);
  v7_array_push(v7, args, arg);
  v7->vals.arguments = args;

  rcode = Regex_ctor(v7, res);
  if (rcode != V7_OK) {
    goto clean;
  }
  v7->vals.arguments = saved_args;

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_match(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t so = V7_UNDEFINED, ro = V7_UNDEFINED;
  long previousLastIndex = 0;
  int lastMatch = 1, n = 0, flag_g;
  struct v7_regexp *rxp;

  *res = V7_NULL;

  rcode = to_string(v7, this_obj, &so, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (v7_argc(v7) == 0) {
    rcode = v7_mk_regexp(v7, "", 0, "", 0, &ro);
    if (rcode != V7_OK) {
      goto clean;
    }
  } else {
    rcode = obj_value_of(v7, v7_arg(v7, 0), &ro);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

  if (!v7_is_regexp(v7, ro)) {
    rcode = call_regex_ctor(v7, ro, &ro);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

  rxp = v7_get_regexp_struct(v7, ro);
  flag_g = slre_get_flags(rxp->compiled_regexp) & SLRE_FLAG_G;
  if (!flag_g) {
    rcode = rx_exec(v7, ro, so, 0, res);
    goto clean;
  }

  rxp->lastIndex = 0;
  *res = v7_mk_dense_array(v7);
  while (lastMatch) {
    val_t result;

    rcode = rx_exec(v7, ro, so, 1, &result);
    if (rcode != V7_OK) {
      goto clean;
    }

    if (v7_is_null(result)) {
      lastMatch = 0;
    } else {
      long thisIndex = rxp->lastIndex;
      if (thisIndex == previousLastIndex) {
        previousLastIndex = thisIndex + 1;
        rxp->lastIndex = previousLastIndex;
      } else {
        previousLastIndex = thisIndex;
      }
      rcode =
          v7_array_push_throwing(v7, *res, v7_array_get(v7, result, 0), NULL);
      if (rcode != V7_OK) {
        goto clean;
      }
      n++;
    }
  }

  if (n == 0) {
    *res = V7_NULL;
    goto clean;
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_replace(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  const char *s;
  size_t s_len;
  /*
   * Buffer of temporary strings returned by the replacement funciton.  Will be
   * allocated below if only the replacement is a function.  We need to store
   * each string in a separate `val_t`, because string data of length <= 5 is
   * stored right in `val_t`, so if there's more than one replacement,
   * each subsequent replacement will overwrite the previous one.
   */
  val_t *tmp_str_buf = NULL;
  val_t out_str_o;
  char *old_owned_mbuf_base = v7->owned_strings.buf;
  char *old_owned_mbuf_end = v7->owned_strings.buf + v7->owned_strings.len;

  rcode = to_string(v7, this_obj, &this_obj, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

  s = v7_get_string(v7, &this_obj, &s_len);

  if (s_len != 0 && v7_argc(v7) > 1) {
    const char *const str_end = s + s_len;
    char *p = (char *) s;
    uint32_t out_sub_num = 0;
    val_t ro = V7_UNDEFINED, str_func = V7_UNDEFINED;
    struct slre_prog *prog;
    struct slre_cap out_sub[V7_RE_MAX_REPL_SUB], *ptok = out_sub;
    struct slre_loot loot;
    int flag_g;

    rcode = obj_value_of(v7, v7_arg(v7, 0), &ro);
    if (rcode != V7_OK) {
      goto clean;
    }
    rcode = obj_value_of(v7, v7_arg(v7, 1), &str_func);
    if (rcode != V7_OK) {
      goto clean;
    }

    if (!v7_is_regexp(v7, ro)) {
      rcode = call_regex_ctor(v7, ro, &ro);
      if (rcode != V7_OK) {
        goto clean;
      }
    }
    prog = v7_get_regexp_struct(v7, ro)->compiled_regexp;
    flag_g = slre_get_flags(prog) & SLRE_FLAG_G;

    if (!v7_is_callable(v7, str_func)) {
      rcode = to_string(v7, str_func, &str_func, NULL, 0, NULL);
      if (rcode != V7_OK) {
        goto clean;
      }
    }

    do {
      int i;
      if (slre_exec(prog, 0, p, str_end, &loot)) break;
      if (p != loot.caps->start) {
        ptok->start = p;
        ptok->end = loot.caps->start;
        ptok++;
        out_sub_num++;
      }

      if (v7_is_callable(v7, str_func)) { /* replace function */
        const char *rez_str;
        size_t rez_len;
        val_t arr = v7_mk_dense_array(v7);

        for (i = 0; i < loot.num_captures; i++) {
          rcode = v7_array_push_throwing(
              v7, arr, v7_mk_string(v7, loot.caps[i].start,
                                    loot.caps[i].end - loot.caps[i].start, 1),
              NULL);
          if (rcode != V7_OK) {
            goto clean;
          }
        }
        rcode = v7_array_push_throwing(
            v7, arr, v7_mk_number(v7, utfnlen(s, loot.caps[0].start - s)),
            NULL);
        if (rcode != V7_OK) {
          goto clean;
        }

        rcode = v7_array_push_throwing(v7, arr, this_obj, NULL);
        if (rcode != V7_OK) {
          goto clean;
        }

        {
          val_t val = V7_UNDEFINED;

          rcode = b_apply(v7, str_func, this_obj, arr, 0, &val);
          if (rcode != V7_OK) {
            goto clean;
          }

          if (tmp_str_buf == NULL) {
            tmp_str_buf = (val_t *) calloc(sizeof(val_t), V7_RE_MAX_REPL_SUB);
          }

          rcode = to_string(v7, val, &tmp_str_buf[out_sub_num], NULL, 0, NULL);
          if (rcode != V7_OK) {
            goto clean;
          }
        }
        rez_str = v7_get_string(v7, &tmp_str_buf[out_sub_num], &rez_len);
        if (rez_len) {
          ptok->start = rez_str;
          ptok->end = rez_str + rez_len;
          ptok++;
          out_sub_num++;
        }
      } else { /* replace string */
        struct slre_loot newsub;
        size_t f_len;
        const char *f_str = v7_get_string(v7, &str_func, &f_len);
        slre_replace(&loot, s, s_len, f_str, f_len, &newsub);
        for (i = 0; i < newsub.num_captures; i++) {
          ptok->start = newsub.caps[i].start;
          ptok->end = newsub.caps[i].end;
          ptok++;
          out_sub_num++;
        }
      }
      p = (char *) loot.caps[0].end;
    } while (flag_g && p < str_end);
    if (p <= str_end) {
      ptok->start = p;
      ptok->end = str_end;
      ptok++;
      out_sub_num++;
    }
    out_str_o = v7_mk_string(v7, NULL, 0, 1);
    ptok = out_sub;
    do {
      size_t ln = ptok->end - ptok->start;
      const char *ps = ptok->start;
      if (ptok->start >= old_owned_mbuf_base &&
          ptok->start < old_owned_mbuf_end) {
        ps += v7->owned_strings.buf - old_owned_mbuf_base;
      }
      out_str_o = s_concat(v7, out_str_o, v7_mk_string(v7, ps, ln, 1));
      p += ln;
      ptok++;
    } while (--out_sub_num);

    *res = out_str_o;
    goto clean;
  }

  *res = this_obj;

clean:
  if (tmp_str_buf != NULL) {
    free(tmp_str_buf);
    tmp_str_buf = NULL;
  }
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_search(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  long utf_shift = -1;

  if (v7_argc(v7) > 0) {
    size_t s_len;
    struct slre_loot sub;
    val_t so = V7_UNDEFINED, ro = V7_UNDEFINED;
    const char *s;

    rcode = obj_value_of(v7, v7_arg(v7, 0), &ro);
    if (rcode != V7_OK) {
      goto clean;
    }

    if (!v7_is_regexp(v7, ro)) {
      rcode = call_regex_ctor(v7, ro, &ro);
      if (rcode != V7_OK) {
        goto clean;
      }
    }

    rcode = to_string(v7, this_obj, &so, NULL, 0, NULL);
    if (rcode != V7_OK) {
      goto clean;
    }

    s = v7_get_string(v7, &so, &s_len);

    if (!slre_exec(v7_get_regexp_struct(v7, ro)->compiled_regexp, 0, s,
                   s + s_len, &sub))
      utf_shift = utfnlen(s, sub.caps[0].start - s); /* calc shift for UTF-8 */
  } else {
    utf_shift = 0;
  }

  *res = v7_mk_number(v7, utf_shift);

clean:
  return rcode;
}

#endif /* V7_ENABLE__RegExp */

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_slice(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  long from = 0, to = 0;
  size_t len;
  val_t so = V7_UNDEFINED;
  const char *begin, *end;
  int num_args = v7_argc(v7);

  rcode = to_string(v7, this_obj, &so, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

  begin = v7_get_string(v7, &so, &len);

  to = len = utfnlen(begin, len);
  if (num_args > 0) {
    rcode = to_long(v7, v7_arg(v7, 0), 0, &from);
    if (rcode != V7_OK) {
      goto clean;
    }

    if (from < 0) {
      from += len;
      if (from < 0) from = 0;
    } else if ((size_t) from > len)
      from = len;
    if (num_args > 1) {
      rcode = to_long(v7, v7_arg(v7, 1), 0, &to);
      if (rcode != V7_OK) {
        goto clean;
      }

      if (to < 0) {
        to += len;
        if (to < 0) to = 0;
      } else if ((size_t) to > len)
        to = len;
    }
  }

  if (from > to) to = from;
  end = utfnshift(begin, to);
  begin = utfnshift(begin, from);

  *res = v7_mk_string(v7, begin, end - begin, 1);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
static enum v7_err s_transform(struct v7 *v7, val_t obj, Rune (*func)(Rune),
                               val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t s = V7_UNDEFINED;
  size_t i, n, len;
  const char *p2, *p;

  rcode = to_string(v7, obj, &s, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

  p = v7_get_string(v7, &s, &len);

  /* Pass NULL to make sure we're not creating dictionary value */
  *res = v7_mk_string(v7, NULL, len, 1);

  {
    Rune r;
    p2 = v7_get_string(v7, res, &len);
    for (i = 0; i < len; i += n) {
      n = chartorune(&r, p + i);
      r = func(r);
      runetochar((char *) p2 + i, &r);
    }
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_toLowerCase(struct v7 *v7, v7_val_t *res) {
  val_t this_obj = v7_get_this(v7);
  return s_transform(v7, this_obj, tolowerrune, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_toUpperCase(struct v7 *v7, v7_val_t *res) {
  val_t this_obj = v7_get_this(v7);
  return s_transform(v7, this_obj, toupperrune, res);
}

static int s_isspace(Rune c) {
  return isspacerune(c) || isnewline(c);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_trim(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t s = V7_UNDEFINED;
  size_t i, n, len, start = 0, end, state = 0;
  const char *p;
  Rune r;

  rcode = to_string(v7, this_obj, &s, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }
  p = v7_get_string(v7, &s, &len);

  end = len;
  for (i = 0; i < len; i += n) {
    n = chartorune(&r, p + i);
    if (!s_isspace(r)) {
      if (state++ == 0) start = i;
      end = i + n;
    }
  }

  *res = v7_mk_string(v7, p + start, end - start, 1);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_length(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  size_t len = 0;
  val_t s = V7_UNDEFINED;

  rcode = obj_value_of(v7, this_obj, &s);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (v7_is_string(s)) {
    const char *p = v7_get_string(v7, &s, &len);
    len = utfnlen(p, len);
  }

  *res = v7_mk_number(v7, len);
clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_at(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  long arg0;
  val_t s = V7_UNDEFINED;

  rcode = to_long(v7, v7_arg(v7, 0), -1, &arg0);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = obj_value_of(v7, this_obj, &s);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (v7_is_string(s)) {
    size_t n;
    const unsigned char *p = (unsigned char *) v7_get_string(v7, &s, &n);
    if (arg0 >= 0 && (size_t) arg0 < n) {
      *res = v7_mk_number(v7, p[arg0]);
      goto clean;
    }
  }

  *res = v7_mk_number(v7, NAN);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_blen(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  size_t len = 0;
  val_t s = V7_UNDEFINED;

  rcode = obj_value_of(v7, this_obj, &s);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (v7_is_string(s)) {
    v7_get_string(v7, &s, &len);
  }

  *res = v7_mk_number(v7, len);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
static enum v7_err s_substr(struct v7 *v7, val_t s, long start, long len,
                            val_t *res) {
  enum v7_err rcode = V7_OK;
  size_t n;
  const char *p;

  rcode = to_string(v7, s, &s, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

  p = v7_get_string(v7, &s, &n);
  n = utfnlen(p, n);

  if (start < (long) n && len > 0) {
    if (start < 0) start = (long) n + start;
    if (start < 0) start = 0;

    if (start > (long) n) start = n;
    if (len < 0) len = 0;
    if (len > (long) n - start) len = n - start;
    p = utfnshift(p, start);
  } else {
    len = 0;
  }

  *res = v7_mk_string(v7, p, len, 1);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_substr(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  long start, len;

  rcode = to_long(v7, v7_arg(v7, 0), 0, &start);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = to_long(v7, v7_arg(v7, 1), LONG_MAX, &len);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = s_substr(v7, this_obj, start, len, res);
  if (rcode != V7_OK) {
    goto clean;
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_substring(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  long start, end;

  rcode = to_long(v7, v7_arg(v7, 0), 0, &start);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = to_long(v7, v7_arg(v7, 1), LONG_MAX, &end);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (start < 0) start = 0;
  if (end < 0) end = 0;
  if (start > end) {
    long tmp = start;
    start = end;
    end = tmp;
  }

  rcode = s_substr(v7, this_obj, start, end - start, res);
  goto clean;

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Str_split(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  const char *s, *s_end;
  size_t s_len;
  long num_args = v7_argc(v7);
  rcode = to_string(v7, this_obj, &this_obj, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }
  s = v7_get_string(v7, &this_obj, &s_len);
  s_end = s + s_len;

  *res = v7_mk_dense_array(v7);

  if (num_args == 0) {
    /*
     * No arguments were given: resulting array will contain just a single
     * element: the source string
     */
    rcode = v7_array_push_throwing(v7, *res, this_obj, NULL);
    if (rcode != V7_OK) {
      goto clean;
    }
  } else {
    val_t ro = V7_UNDEFINED;
    long elem, limit;
    size_t lookup_idx = 0, substr_idx = 0;
    struct _str_split_ctx ctx;

    rcode = to_long(v7, v7_arg(v7, 1), LONG_MAX, &limit);
    if (rcode != V7_OK) {
      goto clean;
    }

    rcode = obj_value_of(v7, v7_arg(v7, 0), &ro);
    if (rcode != V7_OK) {
      goto clean;
    }

    /* Initialize substring context depending on the argument type */
    if (v7_is_regexp(v7, ro)) {
/* use RegExp implementation */
#if V7_ENABLE__RegExp
      ctx.p_init = subs_regexp_init;
      ctx.p_exec = subs_regexp_exec;
      ctx.p_add_caps = subs_regexp_split_add_caps;
#else
      assert(0);
#endif
    } else {
      /*
       * use String implementation: first of all, convert to String (if it's
       * not already a String)
       */
      rcode = to_string(v7, ro, &ro, NULL, 0, NULL);
      if (rcode != V7_OK) {
        goto clean;
      }

      ctx.p_init = subs_string_init;
      ctx.p_exec = subs_string_exec;
#if V7_ENABLE__RegExp
      ctx.p_add_caps = subs_string_split_add_caps;
#endif
    }
    /* initialize context */
    ctx.p_init(&ctx, v7, ro);

    if (s_len == 0) {
      /*
       * if `this` is (or converts to) an empty string, resulting array should
       * contain empty string if only separator does not match an empty string.
       * Otherwise, the array is left empty
       */
      int matches_empty = !ctx.p_exec(&ctx, s, s);
      if (!matches_empty) {
        rcode = v7_array_push_throwing(v7, *res, this_obj, NULL);
        if (rcode != V7_OK) {
          goto clean;
        }
      }
    } else {
      size_t last_match_len = 0;

      for (elem = 0; elem < limit && lookup_idx < s_len;) {
        size_t substr_len;
        /* find next match, and break if there's no match */
        if (ctx.p_exec(&ctx, s + lookup_idx, s_end)) break;

        last_match_len = ctx.match_end - ctx.match_start;
        substr_len = ctx.match_start - s - substr_idx;

        /* add next substring to the resulting array, if needed */
        if (substr_len > 0 || last_match_len > 0) {
          rcode = v7_array_push_throwing(
              v7, *res, v7_mk_string(v7, s + substr_idx, substr_len, 1), NULL);
          if (rcode != V7_OK) {
            goto clean;
          }
          elem++;

#if V7_ENABLE__RegExp
          /* Add captures (for RegExp only) */
          elem = ctx.p_add_caps(&ctx, *res, elem, limit);
#endif
        }

        /* advance lookup_idx appropriately */
        if (last_match_len == 0) {
          /* empty match: advance to the next char */
          const char *next = utfnshift((s + lookup_idx), 1);
          lookup_idx += (next - (s + lookup_idx));
        } else {
          /* non-empty match: advance to the end of match */
          lookup_idx = ctx.match_end - s;
        }

        /*
         * always remember the end of the match, so that next time we will take
         * substring from that position
         */
        substr_idx = ctx.match_end - s;
      }

      /* add the last substring to the resulting array, if needed */
      if (elem < limit) {
        size_t substr_len = s_len - substr_idx;
        if (substr_len > 0 || last_match_len > 0) {
          rcode = v7_array_push_throwing(
              v7, *res, v7_mk_string(v7, s + substr_idx, substr_len, 1), NULL);
          if (rcode != V7_OK) {
            goto clean;
          }
        }
      }
    }
  }

clean:
  return rcode;
}

V7_PRIVATE void init_string(struct v7 *v7) {
  val_t str = mk_cfunction_obj_with_proto(v7, String_ctor, 1,
                                          v7->vals.string_prototype);
  v7_def(v7, v7->vals.global_object, "String", 6, V7_DESC_ENUMERABLE(0), str);

  set_cfunc_prop(v7, str, "fromCharCode", Str_fromCharCode);
  set_cfunc_prop(v7, v7->vals.string_prototype, "charCodeAt", Str_charCodeAt);
  set_cfunc_prop(v7, v7->vals.string_prototype, "charAt", Str_charAt);
  set_cfunc_prop(v7, v7->vals.string_prototype, "concat", Str_concat);
  set_cfunc_prop(v7, v7->vals.string_prototype, "indexOf", Str_indexOf);
  set_cfunc_prop(v7, v7->vals.string_prototype, "substr", Str_substr);
  set_cfunc_prop(v7, v7->vals.string_prototype, "substring", Str_substring);
  set_cfunc_prop(v7, v7->vals.string_prototype, "valueOf", Str_valueOf);
  set_cfunc_prop(v7, v7->vals.string_prototype, "lastIndexOf", Str_lastIndexOf);
#if V7_ENABLE__String__localeCompare
  set_cfunc_prop(v7, v7->vals.string_prototype, "localeCompare",
                 Str_localeCompare);
#endif
#if V7_ENABLE__RegExp
  set_cfunc_prop(v7, v7->vals.string_prototype, "match", Str_match);
  set_cfunc_prop(v7, v7->vals.string_prototype, "replace", Str_replace);
  set_cfunc_prop(v7, v7->vals.string_prototype, "search", Str_search);
#endif
  set_cfunc_prop(v7, v7->vals.string_prototype, "split", Str_split);
  set_cfunc_prop(v7, v7->vals.string_prototype, "slice", Str_slice);
  set_cfunc_prop(v7, v7->vals.string_prototype, "trim", Str_trim);
  set_cfunc_prop(v7, v7->vals.string_prototype, "toLowerCase", Str_toLowerCase);
#if V7_ENABLE__String__localeLowerCase
  set_cfunc_prop(v7, v7->vals.string_prototype, "toLocaleLowerCase",
                 Str_toLowerCase);
#endif
  set_cfunc_prop(v7, v7->vals.string_prototype, "toUpperCase", Str_toUpperCase);
#if V7_ENABLE__String__localeUpperCase
  set_cfunc_prop(v7, v7->vals.string_prototype, "toLocaleUpperCase",
                 Str_toUpperCase);
#endif
  set_cfunc_prop(v7, v7->vals.string_prototype, "toString", Str_toString);

  v7_def(v7, v7->vals.string_prototype, "length", 6, V7_DESC_GETTER(1),
         v7_mk_cfunction(Str_length));

  /* Non-standard Cesanta extension */
  set_cfunc_prop(v7, v7->vals.string_prototype, "at", Str_at);
  v7_def(v7, v7->vals.string_prototype, "blen", 4, V7_DESC_GETTER(1),
         v7_mk_cfunction(Str_blen));
}
