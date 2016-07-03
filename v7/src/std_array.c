/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/str_util.h"
#include "v7/src/internal.h"
#include "v7/src/gc.h"
#include "v7/src/eval.h"
#include "v7/src/std_string.h"
#include "v7/src/conversion.h"
#include "v7/src/core.h"
#include "v7/src/function.h"
#include "v7/src/array.h"
#include "v7/src/object.h"
#include "v7/src/exceptions.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

struct a_sort_data {
  val_t sort_func;
};

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_ctor(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  unsigned long i, len;

  (void) v7;
  *res = v7_mk_array(v7);
  /*
   * The interpreter passes dense array to C functions.
   * However dense array implementation is not yet complete
   * so we don't want to propagate them at each call to Array()
   */
  len = v7_argc(v7);
  for (i = 0; i < len; i++) {
    rcode = v7_array_set_throwing(v7, *res, i, v7_arg(v7, i), NULL);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_push(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  int i, len = v7_argc(v7);

  *res = V7_UNDEFINED;

  for (i = 0; i < len; i++) {
    *res = v7_arg(v7, i);
    rcode = v7_array_push_throwing(v7, v7_get_this(v7), *res, NULL);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

/*
 * TODO(dfrank) : we need to implement `length` as a real property, and here
 * we need to set new length and return it (even if the object is not an
 * array)
 */

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_get_length(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  long len = 0;

  if (is_prototype_of(v7, this_obj, v7->vals.array_prototype)) {
    len = v7_array_length(v7, this_obj);
  }
  *res = v7_mk_number(v7, len);

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_set_length(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t arg0 = v7_arg(v7, 0);
  val_t this_obj = v7_get_this(v7);
  long new_len = 0;

  rcode = to_long(v7, v7_arg(v7, 0), -1, &new_len);
  if (rcode != V7_OK) {
    goto clean;
  }

  if (!v7_is_object(this_obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Array expected");
    goto clean;
  } else if (new_len < 0 ||
             (v7_is_number(arg0) && (isnan(v7_get_double(v7, arg0)) ||
                                     isinf(v7_get_double(v7, arg0))))) {
    rcode = v7_throwf(v7, RANGE_ERROR, "Invalid array length");
    goto clean;
  } else {
    struct v7_property **p, **next;
    long index, max_index = -1;

    /* Remove all items with an index higher than new_len */
    for (p = &get_object_struct(this_obj)->properties; *p != NULL; p = next) {
      size_t n;
      const char *s = v7_get_string(v7, &p[0]->name, &n);
      next = &p[0]->next;
      index = strtol(s, NULL, 10);
      if (index >= new_len) {
        v7_destroy_property(p);
        *p = *next;
        next = p;
      } else if (index > max_index) {
        max_index = index;
      }
    }

    /* If we have to expand, insert an item with appropriate index */
    if (new_len > 0 && max_index < new_len - 1) {
      char buf[40];
      c_snprintf(buf, sizeof(buf), "%ld", new_len - 1);
      v7_set(v7, this_obj, buf, strlen(buf), V7_UNDEFINED);
    }
  }

  *res = v7_mk_number(v7, new_len);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
static enum v7_err a_cmp(struct v7 *v7, void *user_data, const void *pa,
                         const void *pb, int *res) {
  enum v7_err rcode = V7_OK;
  struct a_sort_data *sort_data = (struct a_sort_data *) user_data;
  val_t a = *(val_t *) pa, b = *(val_t *) pb, func = sort_data->sort_func;

  if (v7_is_callable(v7, func)) {
    int saved_inhibit_gc = v7->inhibit_gc;
    val_t vres = V7_UNDEFINED, args = v7_mk_dense_array(v7);
    v7_array_push(v7, args, a);
    v7_array_push(v7, args, b);
    v7->inhibit_gc = 0;
    rcode = b_apply(v7, func, V7_UNDEFINED, args, 0, &vres);
    if (rcode != V7_OK) {
      goto clean;
    }
    v7->inhibit_gc = saved_inhibit_gc;
    *res = (int) -v7_get_double(v7, vres);
    goto clean;
  } else {
    char sa[100], sb[100];

    rcode = to_string(v7, a, NULL, sa, sizeof(sa), NULL);
    if (rcode != V7_OK) {
      goto clean;
    }

    rcode = to_string(v7, b, NULL, sb, sizeof(sb), NULL);
    if (rcode != V7_OK) {
      goto clean;
    }

    sa[sizeof(sa) - 1] = sb[sizeof(sb) - 1] = '\0';
    *res = strcmp(sb, sa);
    goto clean;
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
static enum v7_err a_partition(struct v7 *v7, val_t *a, int l, int r,
                               void *user_data, int *res) {
  enum v7_err rcode = V7_OK;
  val_t t, pivot = a[l];
  int i = l, j = r + 1;

  for (;;) {
    while (1) {
      ++i;

      if (i <= r) {
        int tmp = 0;
        rcode = a_cmp(v7, user_data, &a[i], &pivot, &tmp);
        if (rcode != V7_OK) {
          goto clean;
        }

        if (tmp > 0) {
          break;
        }
      } else {
        break;
      }
    }
    while (1) {
      int tmp = 0;
      --j;

      rcode = a_cmp(v7, user_data, &a[j], &pivot, &tmp);
      if (rcode != V7_OK) {
        goto clean;
      }

      if (tmp <= 0) {
        break;
      }
    }
    if (i >= j) break;
    t = a[i];
    a[i] = a[j];
    a[j] = t;
  }
  t = a[l];
  a[l] = a[j];
  a[j] = t;

  *res = j;
clean:
  return rcode;
}

WARN_UNUSED_RESULT
static enum v7_err a_qsort(struct v7 *v7, val_t *a, int l, int r,
                           void *user_data) {
  enum v7_err rcode = V7_OK;
  if (l < r) {
    int j = 0;
    rcode = a_partition(v7, a, l, r, user_data, &j);
    if (rcode != V7_OK) {
      goto clean;
    }

    rcode = a_qsort(v7, a, l, j - 1, user_data);
    if (rcode != V7_OK) {
      goto clean;
    }

    rcode = a_qsort(v7, a, j + 1, r, user_data);
    if (rcode != V7_OK) {
      goto clean;
    }
  }
clean:
  return rcode;
}

WARN_UNUSED_RESULT
static enum v7_err a_sort(struct v7 *v7,
                          enum v7_err (*sorting_func)(struct v7 *v7, void *,
                                                      const void *,
                                                      const void *, int *res),
                          v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  int i = 0, len = 0;
  val_t *arr = NULL;
  val_t arg0 = v7_arg(v7, 0);

  *res = v7_get_this(v7);
  len = v7_array_length(v7, *res);

  if (!v7_is_object(*res)) {
    goto clean;
  }

  arr = (val_t *) malloc(len * sizeof(arr[0]));

  assert(*res != v7->vals.global_object);

  for (i = 0; i < len; i++) {
    arr[i] = v7_array_get(v7, *res, i);
  }

  /* TODO(dfrank): sorting_func isn't actually used! something is wrong here */
  if (sorting_func != NULL) {
    struct a_sort_data sort_data;
    sort_data.sort_func = arg0;
    rcode = a_qsort(v7, arr, 0, len - 1, &sort_data);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

  for (i = 0; i < len; i++) {
    v7_array_set(v7, *res, i, arr[len - (i + 1)]);
  }

clean:
  if (arr != NULL) {
    free(arr);
  }
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_sort(struct v7 *v7, v7_val_t *res) {
  return a_sort(v7, a_cmp, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_reverse(struct v7 *v7, v7_val_t *res) {
  return a_sort(v7, NULL, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_join(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t arg0 = v7_arg(v7, 0);
  size_t sep_size = 0;
  const char *sep = NULL;

  *res = V7_UNDEFINED;

  /* Get pointer to the separator string */
  if (!v7_is_string(arg0)) {
    /* If no separator is provided, use comma */
    arg0 = v7_mk_string(v7, ",", 1, 1);
  }
  sep = v7_get_string(v7, &arg0, &sep_size);

  /* Do the actual join */
  if (is_prototype_of(v7, this_obj, v7->vals.array_prototype)) {
    struct mbuf m;
    char buf[100], *p;
    long i, n, num_elems = v7_array_length(v7, this_obj);

    mbuf_init(&m, 0);

    for (i = 0; i < num_elems; i++) {
      /* Append separator */
      if (i > 0) {
        mbuf_append(&m, sep, sep_size);
      }

      /* Append next item from an array */
      p = buf;
      {
        size_t tmp;
        rcode = to_string(v7, v7_array_get(v7, this_obj, i), NULL, buf,
                          sizeof(buf), &tmp);
        if (rcode != V7_OK) {
          goto clean;
        }
        n = tmp;
      }
      if (n > (long) sizeof(buf)) {
        p = (char *) malloc(n + 1);
        rcode = to_string(v7, v7_array_get(v7, this_obj, i), NULL, p, n, NULL);
        if (rcode != V7_OK) {
          goto clean;
        }
      }
      mbuf_append(&m, p, n);
      if (p != buf) {
        free(p);
      }
    }

    /* mbuf contains concatenated string now. Copy it to the result. */
    *res = v7_mk_string(v7, m.buf, m.len, 1);
    mbuf_free(&m);
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_toString(struct v7 *v7, v7_val_t *res) {
  return Array_join(v7, res);
}

WARN_UNUSED_RESULT
static enum v7_err a_splice(struct v7 *v7, int mutate, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  long i, len = v7_array_length(v7, this_obj);
  long num_args = v7_argc(v7);
  long elems_to_insert = num_args > 2 ? num_args - 2 : 0;
  long arg0, arg1;

  if (!v7_is_object(this_obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR,
                      "Array.splice or Array.slice called on non-object value");
    goto clean;
  }

  *res = v7_mk_dense_array(v7);

  rcode = to_long(v7, v7_arg(v7, 0), 0, &arg0);
  if (rcode != V7_OK) {
    goto clean;
  }

  rcode = to_long(v7, v7_arg(v7, 1), len, &arg1);
  if (rcode != V7_OK) {
    goto clean;
  }

  /* Bounds check */
  if (!mutate && len <= 0) {
    goto clean;
  }
  if (arg0 < 0) arg0 = len + arg0;
  if (arg0 < 0) arg0 = 0;
  if (arg0 > len) arg0 = len;
  if (mutate) {
    if (arg1 < 0) arg1 = 0;
    arg1 += arg0;
  } else if (arg1 < 0) {
    arg1 = len + arg1;
  }

  /* Create return value - slice */
  for (i = arg0; i < arg1 && i < len; i++) {
    rcode =
        v7_array_push_throwing(v7, *res, v7_array_get(v7, this_obj, i), NULL);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

  if (mutate && get_object_struct(this_obj)->attributes & V7_OBJ_DENSE_ARRAY) {
    /*
     * dense arrays are spliced by memmoving leaving the trailing
     * space allocated for future appends.
     * TODO(mkm): figure out if trimming is better
     */
    struct v7_property *p =
        v7_get_own_property2(v7, this_obj, "", 0, _V7_PROPERTY_HIDDEN);
    struct mbuf *abuf;
    if (p == NULL) {
      goto clean;
    }
    abuf = (struct mbuf *) v7_get_ptr(v7, p->value);
    if (abuf == NULL) {
      goto clean;
    }

    memmove(abuf->buf + arg0 * sizeof(val_t), abuf->buf + arg1 * sizeof(val_t),
            (len - arg1) * sizeof(val_t));
    abuf->len -= (arg1 - arg0) * sizeof(val_t);
  } else if (mutate) {
    /* If splicing, modify this_obj array: remove spliced sub-array */
    struct v7_property **p, **next;
    long i;

    for (p = &get_object_struct(this_obj)->properties; *p != NULL; p = next) {
      size_t n;
      const char *s = v7_get_string(v7, &p[0]->name, &n);
      next = &p[0]->next;
      i = strtol(s, NULL, 10);
      if (i >= arg0 && i < arg1) {
        /* Remove items from spliced sub-array */
        v7_destroy_property(p);
        *p = *next;
        next = p;
      } else if (i >= arg1) {
        /* Modify indices of the elements past sub-array */
        char key[20];
        size_t n = c_snprintf(key, sizeof(key), "%ld",
                              i - (arg1 - arg0) + elems_to_insert);
        p[0]->name = v7_mk_string(v7, key, n, 1);
      }
    }

    /* Insert optional extra elements */
    for (i = 2; i < num_args; i++) {
      char key[20];
      size_t n = c_snprintf(key, sizeof(key), "%ld", arg0 + i - 2);
      rcode = set_property(v7, this_obj, key, n, v7_arg(v7, i), NULL);
      if (rcode != V7_OK) {
        goto clean;
      }
    }
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_slice(struct v7 *v7, v7_val_t *res) {
  return a_splice(v7, 0, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_splice(struct v7 *v7, v7_val_t *res) {
  return a_splice(v7, 1, res);
}

static void a_prep1(struct v7 *v7, val_t t, val_t *a0, val_t *a1) {
  *a0 = v7_arg(v7, 0);
  *a1 = v7_arg(v7, 1);
  if (v7_is_undefined(*a1)) {
    *a1 = t;
  }
}

/*
 * Call callback function `cb`, passing `this_obj` as `this`, with the
 * following arguments:
 *
 *   cb(v, n, this_obj);
 *
 */
WARN_UNUSED_RESULT
static enum v7_err a_prep2(struct v7 *v7, val_t cb, val_t v, val_t n,
                           val_t this_obj, val_t *res) {
  enum v7_err rcode = V7_OK;
  int saved_inhibit_gc = v7->inhibit_gc;
  val_t args = v7_mk_dense_array(v7);

  *res = v7_mk_dense_array(v7);

  v7_own(v7, &args);

  v7_array_push(v7, args, v);
  v7_array_push(v7, args, n);
  v7_array_push(v7, args, this_obj);

  v7->inhibit_gc = 0;
  rcode = b_apply(v7, cb, this_obj, args, 0, res);
  if (rcode != V7_OK) {
    goto clean;
  }

clean:
  v7->inhibit_gc = saved_inhibit_gc;
  v7_disown(v7, &args);

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_forEach(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t v = V7_UNDEFINED, cb = v7_arg(v7, 0);
  unsigned long len, i;
  int has;
  /* a_prep2 uninhibits GC when calling cb */
  struct gc_tmp_frame vf = new_tmp_frame(v7);

  if (!v7_is_object(this_obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Array expected");
    goto clean;
  }

  if (!v7_is_callable(v7, cb)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Function expected");
    goto clean;
  }

  tmp_stack_push(&vf, &v);

  len = v7_array_length(v7, this_obj);
  for (i = 0; i < len; i++) {
    v = v7_array_get2(v7, this_obj, i, &has);
    if (!has) continue;

    rcode = a_prep2(v7, cb, v, v7_mk_number(v7, i), this_obj, res);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

clean:
  tmp_frame_cleanup(&vf);
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_map(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t arg0, arg1, el, v;
  unsigned long len, i;
  int has;
  /* a_prep2 uninhibits GC when calling cb */
  struct gc_tmp_frame vf = new_tmp_frame(v7);

  if (!v7_is_object(this_obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Array expected");
    goto clean;
  } else {
    a_prep1(v7, this_obj, &arg0, &arg1);
    *res = v7_mk_dense_array(v7);
    len = v7_array_length(v7, this_obj);

    tmp_stack_push(&vf, &arg0);
    tmp_stack_push(&vf, &arg1);
    tmp_stack_push(&vf, &v);

    for (i = 0; i < len; i++) {
      v = v7_array_get2(v7, this_obj, i, &has);
      if (!has) continue;
      rcode = a_prep2(v7, arg0, v, v7_mk_number(v7, i), arg1, &el);
      if (rcode != V7_OK) {
        goto clean;
      }

      rcode = v7_array_set_throwing(v7, *res, i, el, NULL);
      if (rcode != V7_OK) {
        goto clean;
      }
    }
  }

clean:
  tmp_frame_cleanup(&vf);
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_every(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t arg0, arg1, el, v;
  unsigned long i, len;
  int has;
  /* a_prep2 uninhibits GC when calling cb */
  struct gc_tmp_frame vf = new_tmp_frame(v7);

  *res = v7_mk_boolean(v7, 0);

  if (!v7_is_object(this_obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Array expected");
    goto clean;
  } else {
    a_prep1(v7, this_obj, &arg0, &arg1);

    tmp_stack_push(&vf, &arg0);
    tmp_stack_push(&vf, &arg1);
    tmp_stack_push(&vf, &v);

    len = v7_array_length(v7, this_obj);
    for (i = 0; i < len; i++) {
      v = v7_array_get2(v7, this_obj, i, &has);
      if (!has) continue;
      rcode = a_prep2(v7, arg0, v, v7_mk_number(v7, i), arg1, &el);
      if (rcode != V7_OK) {
        goto clean;
      }
      if (!v7_is_truthy(v7, el)) {
        *res = v7_mk_boolean(v7, 0);
        goto clean;
      }
    }
  }

  *res = v7_mk_boolean(v7, 1);

clean:
  tmp_frame_cleanup(&vf);
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_some(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t arg0, arg1, el, v;
  unsigned long i, len;
  int has;
  /* a_prep2 uninhibits GC when calling cb */
  struct gc_tmp_frame vf = new_tmp_frame(v7);

  *res = v7_mk_boolean(v7, 1);

  if (!v7_is_object(this_obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Array expected");
    goto clean;
  } else {
    a_prep1(v7, this_obj, &arg0, &arg1);

    tmp_stack_push(&vf, &arg0);
    tmp_stack_push(&vf, &arg1);
    tmp_stack_push(&vf, &v);

    len = v7_array_length(v7, this_obj);
    for (i = 0; i < len; i++) {
      v = v7_array_get2(v7, this_obj, i, &has);
      if (!has) continue;
      rcode = a_prep2(v7, arg0, v, v7_mk_number(v7, i), arg1, &el);
      if (rcode != V7_OK) {
        goto clean;
      }
      if (v7_is_truthy(v7, el)) {
        *res = v7_mk_boolean(v7, 1);
        goto clean;
      }
    }
  }

  *res = v7_mk_boolean(v7, 0);

clean:
  tmp_frame_cleanup(&vf);
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_filter(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  val_t arg0, arg1, el, v;
  unsigned long len, i;
  int has;
  /* a_prep2 uninhibits GC when calling cb */
  struct gc_tmp_frame vf = new_tmp_frame(v7);

  if (!v7_is_object(this_obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Array expected");
    goto clean;
  } else {
    a_prep1(v7, this_obj, &arg0, &arg1);
    *res = v7_mk_dense_array(v7);
    len = v7_array_length(v7, this_obj);

    tmp_stack_push(&vf, &arg0);
    tmp_stack_push(&vf, &arg1);
    tmp_stack_push(&vf, &v);

    for (i = 0; i < len; i++) {
      v = v7_array_get2(v7, this_obj, i, &has);
      if (!has) continue;
      rcode = a_prep2(v7, arg0, v, v7_mk_number(v7, i), arg1, &el);
      if (rcode != V7_OK) {
        goto clean;
      }
      if (v7_is_truthy(v7, el)) {
        rcode = v7_array_push_throwing(v7, *res, v, NULL);
        if (rcode != V7_OK) {
          goto clean;
        }
      }
    }
  }

clean:
  tmp_frame_cleanup(&vf);
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_concat(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  val_t this_obj = v7_get_this(v7);
  size_t i, j, len;
  val_t saved_args;

  if (!v7_is_array(v7, this_obj)) {
    rcode = v7_throwf(v7, TYPE_ERROR, "Array expected");
    goto clean;
  }

  len = v7_argc(v7);

  /*
   * reuse a_splice but override it's arguments. a_splice
   * internally uses a lot of helpers that fetch arguments
   * from the v7 context.
   * TODO(mkm): we need a better helper call another cfunction
   * from a cfunction.
   */
  saved_args = v7->vals.arguments;
  v7->vals.arguments = V7_UNDEFINED;
  rcode = a_splice(v7, 1, res);
  if (rcode != V7_OK) {
    goto clean;
  }
  v7->vals.arguments = saved_args;

  for (i = 0; i < len; i++) {
    val_t a = v7_arg(v7, i);
    if (!v7_is_array(v7, a)) {
      rcode = v7_array_push_throwing(v7, *res, a, NULL);
      if (rcode != V7_OK) {
        goto clean;
      }
    } else {
      size_t alen = v7_array_length(v7, a);
      for (j = 0; j < alen; j++) {
        rcode = v7_array_push_throwing(v7, *res, v7_array_get(v7, a, j), NULL);
        if (rcode != V7_OK) {
          goto clean;
        }
      }
    }
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Array_isArray(struct v7 *v7, v7_val_t *res) {
  val_t arg0 = v7_arg(v7, 0);
  *res = v7_mk_boolean(v7, v7_is_array(v7, arg0));
  return V7_OK;
}

V7_PRIVATE void init_array(struct v7 *v7) {
  val_t ctor = mk_cfunction_obj(v7, Array_ctor, 1);
  val_t length = v7_mk_dense_array(v7);

  v7_set(v7, ctor, "prototype", 9, v7->vals.array_prototype);
  set_method(v7, ctor, "isArray", Array_isArray, 1);
  v7_set(v7, v7->vals.global_object, "Array", 5, ctor);
  v7_def(v7, v7->vals.array_prototype, "constructor", ~0, _V7_DESC_HIDDEN(1),
         ctor);
  v7_set(v7, ctor, "name", 4, v7_mk_string(v7, "Array", ~0, 1));

  set_method(v7, v7->vals.array_prototype, "concat", Array_concat, 1);
  set_method(v7, v7->vals.array_prototype, "every", Array_every, 1);
  set_method(v7, v7->vals.array_prototype, "filter", Array_filter, 1);
  set_method(v7, v7->vals.array_prototype, "forEach", Array_forEach, 1);
  set_method(v7, v7->vals.array_prototype, "join", Array_join, 1);
  set_method(v7, v7->vals.array_prototype, "map", Array_map, 1);
  set_method(v7, v7->vals.array_prototype, "push", Array_push, 1);
  set_method(v7, v7->vals.array_prototype, "reverse", Array_reverse, 0);
  set_method(v7, v7->vals.array_prototype, "slice", Array_slice, 2);
  set_method(v7, v7->vals.array_prototype, "some", Array_some, 1);
  set_method(v7, v7->vals.array_prototype, "sort", Array_sort, 1);
  set_method(v7, v7->vals.array_prototype, "splice", Array_splice, 2);
  set_method(v7, v7->vals.array_prototype, "toString", Array_toString, 0);

  v7_array_set(v7, length, 0, v7_mk_cfunction(Array_get_length));
  v7_array_set(v7, length, 1, v7_mk_cfunction(Array_set_length));
  v7_def(v7, v7->vals.array_prototype, "length", 6,
         V7_DESC_ENUMERABLE(0) | V7_DESC_GETTER(1) | V7_DESC_SETTER(1), length);
}

#if defined(__cplusplus)
}
#endif /* __cplusplus */
