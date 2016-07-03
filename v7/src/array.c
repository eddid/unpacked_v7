/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/str_util.h"
#include "v7/src/internal.h"
#include "v7/src/array.h"
#include "v7/src/string.h"
#include "v7/src/object.h"
#include "v7/src/exceptions.h"
#include "v7/src/primitive.h"
#include "v7/src/core.h"

/* like c_snprintf but returns `size` if write is truncated */
static int v_sprintf_s(char *buf, size_t size, const char *fmt, ...) {
  size_t n;
  va_list ap;
  va_start(ap, fmt);
  n = c_vsnprintf(buf, size, fmt, ap);
  if (n > size) {
    return size;
  }
  return n;
}

v7_val_t v7_mk_array(struct v7 *v7) {
  val_t a = mk_object(v7, v7->vals.array_prototype);
#if 0
  v7_def(v7, a, "", 0, _V7_DESC_HIDDEN(1), V7_NULL);
#endif
  return a;
}

int v7_is_array(struct v7 *v7, val_t v) {
  return v7_is_generic_object(v) &&
         is_prototype_of(v7, v, v7->vals.array_prototype);
}

/*
 * Dense arrays are backed by mbuf. Currently the array can only grow by
 * appending (i.e. setting an element whose index == array.length)
 *
 * TODO(mkm): automatically promote dense arrays to normal objects
 *            when they are used as sparse arrays or to store arbitrary keys
 *            (perhaps a hybrid approach)
 * TODO(mkm): small sparsness doesn't have to promote the array,
 *            we can just fill empty slots with a tag. In JS missing array
 *            indices are subtly different from indices with an undefined value
 *            (key iteration).
 * TODO(mkm): change the interpreter so it can set elements in dense arrays
 */
V7_PRIVATE val_t v7_mk_dense_array(struct v7 *v7) {
  val_t a = v7_mk_array(v7);
#ifdef V7_ENABLE_DENSE_ARRAYS
  v7_own(v7, &a);
  v7_def(v7, a, "", 0, _V7_DESC_HIDDEN(1), V7_NULL);

  /*
   * Before setting a `V7_OBJ_DENSE_ARRAY` flag, make sure we don't have
   * `V7_OBJ_FUNCTION` flag set
   */
  assert(!(get_object_struct(a)->attributes & V7_OBJ_FUNCTION));
  get_object_struct(a)->attributes |= V7_OBJ_DENSE_ARRAY;

  v7_disown(v7, &a);
#endif
  return a;
}

/* TODO_V7_ERR */
val_t v7_array_get(struct v7 *v7, val_t arr, unsigned long index) {
  return v7_array_get2(v7, arr, index, NULL);
}

/* TODO_V7_ERR */
val_t v7_array_get2(struct v7 *v7, val_t arr, unsigned long index, int *has) {
  enum v7_err rcode = V7_OK;
  val_t res;

  if (has != NULL) {
    *has = 0;
  }
  if (v7_is_object(arr)) {
    if (get_object_struct(arr)->attributes & V7_OBJ_DENSE_ARRAY) {
      struct v7_property *p =
          v7_get_own_property2(v7, arr, "", 0, _V7_PROPERTY_HIDDEN);
      struct mbuf *abuf = NULL;
      unsigned long len;
      if (p != NULL) {
        abuf = (struct mbuf *) v7_get_ptr(v7, p->value);
      }
      if (abuf == NULL) {
        res = V7_UNDEFINED;
        goto clean;
      }
      len = abuf->len / sizeof(val_t);
      if (index >= len) {
        res = V7_UNDEFINED;
        goto clean;
      } else {
        memcpy(&res, abuf->buf + index * sizeof(val_t), sizeof(val_t));
        if (has != NULL && res != V7_TAG_NOVALUE) *has = 1;
        if (res == V7_TAG_NOVALUE) {
          res = V7_UNDEFINED;
        }
        goto clean;
      }
    } else {
      struct v7_property *p;
      char buf[20];
      int n = v_sprintf_s(buf, sizeof(buf), "%lu", index);
      p = v7_get_property(v7, arr, buf, n);
      if (has != NULL && p != NULL) *has = 1;
      V7_TRY(v7_property_value(v7, arr, p, &res));
      goto clean;
    }
  } else {
    res = V7_UNDEFINED;
    goto clean;
  }

clean:
  (void) rcode;
  return res;
}

#if V7_ENABLE_DENSE_ARRAYS

/* Create V7 strings for integers such as array indices */
static val_t ulong_to_str(struct v7 *v7, unsigned long n) {
  char buf[100];
  int len;
  len = c_snprintf(buf, sizeof(buf), "%lu", n);
  return v7_mk_string(v7, buf, len, 1);
}

/*
 * Pack 15-bit length and 15 bit index, leaving 2 bits for tag. the LSB has to
 * be set to distinguish it from a prop pointer.
 * In alternative we just fetch the length from obj at each call to v7_next_prop
 * and just stuff the index here (e.g. on 8/16-bit platforms).
 * TODO(mkm): conditional for 16-bit platforms
 */
#define PACK_ITER(len, idx) \
  ((struct v7_property *) ((len) << 17 | (idx) << 1 | 1))

#define UNPACK_ITER_LEN(p) (((uintptr_t) p) >> 17)
#define UNPACK_ITER_IDX(p) ((((uintptr_t) p) >> 1) & 0x7FFF)
#define IS_PACKED_ITER(p) ((uintptr_t) p & 1)

void *v7_next_prop(struct v7 *v7, val_t obj, void *h, val_t *name, val_t *val,
                   v7_prop_attr_t *attrs) {
  struct v7_property *p = (struct v7_property *) h;

  if (get_object_struct(obj)->attributes & V7_OBJ_DENSE_ARRAY) {
    /* This is a dense array. Find backing mbuf and fetch values from there */
    struct v7_property *hp =
        v7_get_own_property2(v7, obj, "", 0, _V7_PROPERTY_HIDDEN);
    struct mbuf *abuf = NULL;
    unsigned long len, idx;
    if (hp != NULL) {
      abuf = (struct mbuf *) v7_get_ptr(v7, hp->value);
    }
    if (abuf == NULL) return NULL;
    len = abuf->len / sizeof(val_t);
    if (len == 0) return NULL;
    idx = (p == NULL) ? 0 : UNPACK_ITER_IDX(p) + 1;
    p = (idx == len) ? get_object_struct(obj)->properties : PACK_ITER(len, idx);
    if (val != NULL) *val = ((val_t *) abuf->buf)[idx];
    if (attrs != NULL) *attrs = 0;
    if (name != NULL) {
      char buf[20];
      int n = v_sprintf_s(buf, sizeof(buf), "%lu", index);
      *name = v7_mk_string(v7, buf, n, 1);
    }
  } else {
    /* Ordinary object */
    p = (p == NULL) ? get_object_struct(obj)->properties : p->next;
    if (p != NULL) {
      if (name != NULL) *name = p->name;
      if (val != NULL) *val = p->value;
      if (attrs != NULL) *attrs = p->attributes;
    }
  }

  return p;
}

V7_PRIVATE val_t
v7_iter_get_value(struct v7 *v7, val_t obj, struct v7_property *p) {
  return IS_PACKED_ITER(p) ? v7_array_get(v7, obj, UNPACK_ITER_IDX(p))
                           : p->value;
}

V7_PRIVATE val_t v7_iter_get_name(struct v7 *v7, struct v7_property *p) {
  return IS_PACKED_ITER(p) ? ulong_to_str(v7, UNPACK_ITER_IDX(p)) : p->name;
}

V7_PRIVATE uint8_t v7_iter_get_attrs(struct v7_property *p) {
  return IS_PACKED_ITER(p) ? 0 : p->attributes;
}

/* return array index as number or undefined. works with iterators */
V7_PRIVATE enum v7_err v7_iter_get_index(struct v7 *v7, struct v7_property *p,
                                         val_t *res) {
  enum v7_err rcode = V7_OK;
  int ok;
  unsigned long res;
  if (IS_PACKED_ITER(p)) {
    *res = v7_mk_number(v7, UNPACK_ITER_IDX(p));
    goto clean;
  }
  V7_TRY(str_to_ulong(v7, p->name, &ok, &res));
  if (!ok || res >= UINT32_MAX) {
    goto clean;
  }
  *res = v7_mk_number(v7, res);
  goto clean;

clean:
  return rcode;
}
#endif

/* TODO_V7_ERR */
unsigned long v7_array_length(struct v7 *v7, val_t v) {
  enum v7_err rcode = V7_OK;
  struct v7_property *p;
  unsigned long len = 0;

  if (!v7_is_object(v)) {
    len = 0;
    goto clean;
  }

#if V7_ENABLE_DENSE_ARRAYS
  if (get_object_struct(v)->attributes & V7_OBJ_DENSE_ARRAY) {
    struct v7_property *p =
        v7_get_own_property2(v7, v, "", 0, _V7_PROPERTY_HIDDEN);
    struct mbuf *abuf;
    if (p == NULL) {
      len = 0;
      goto clean;
    }
    abuf = (struct mbuf *) v7_get_ptr(v7, p->value);
    if (abuf == NULL) {
      len = 0;
      goto clean;
    }
    len = abuf->len / sizeof(val_t);
    goto clean;
  }
#endif

  for (p = get_object_struct(v)->properties; p != NULL; p = p->next) {
    int ok = 0;
    unsigned long n = 0;
    V7_TRY(str_to_ulong(v7, p->name, &ok, &n));
    if (ok && n >= len && n < UINT32_MAX) {
      len = n + 1;
    }
  }

clean:
  (void) rcode;
  return len;
}

int v7_array_set(struct v7 *v7, val_t arr, unsigned long index, val_t v) {
  enum v7_err rcode = V7_OK;
  uint8_t saved_is_thrown = 0;
  val_t saved_thrown = v7_get_thrown_value(v7, &saved_is_thrown);
  int ret = -1;

  rcode = v7_array_set_throwing(v7, arr, index, v, &ret);
  if (rcode != V7_OK) {
    rcode = V7_OK;
    if (saved_is_thrown) {
      rcode = v7_throw(v7, saved_thrown);
    } else {
      v7_clear_thrown_value(v7);
    }
    ret = -1;
  }

  return ret;
}

enum v7_err v7_array_set_throwing(struct v7 *v7, val_t arr, unsigned long index,
                                  val_t v, int *res) {
  enum v7_err rcode = V7_OK;
  int ires = -1;

  if (v7_is_object(arr)) {
    if (get_object_struct(arr)->attributes & V7_OBJ_DENSE_ARRAY) {
      struct v7_property *p =
          v7_get_own_property2(v7, arr, "", 0, _V7_PROPERTY_HIDDEN);
      struct mbuf *abuf;
      unsigned long len;
      assert(p != NULL);
      abuf = (struct mbuf *) v7_get_ptr(v7, p->value);

      if (get_object_struct(arr)->attributes & V7_OBJ_NOT_EXTENSIBLE) {
        if (is_strict_mode(v7)) {
          rcode = v7_throwf(v7, TYPE_ERROR, "Object is not extensible");
          goto clean;
        }

        goto clean;
      }

      if (abuf == NULL) {
        abuf = (struct mbuf *) malloc(sizeof(*abuf));
        mbuf_init(abuf, sizeof(val_t) * (index + 1));
        p->value = v7_mk_foreign(v7, abuf);
      }
      len = abuf->len / sizeof(val_t);
      /* TODO(mkm): possibly promote to sparse array */
      if (index > len) {
        unsigned long i;
        val_t s = V7_TAG_NOVALUE;
        for (i = len; i < index; i++) {
          mbuf_append(abuf, (char *) &s, sizeof(val_t));
        }
        len = index;
      }

      if (index == len) {
        mbuf_append(abuf, (char *) &v, sizeof(val_t));
      } else {
        memcpy(abuf->buf + index * sizeof(val_t), &v, sizeof(val_t));
      }
    } else {
      char buf[20];
      int n = v_sprintf_s(buf, sizeof(buf), "%lu", index);
      {
        struct v7_property *tmp = NULL;
        rcode = set_property(v7, arr, buf, n, v, &tmp);
        ires = (tmp == NULL) ? -1 : 0;
      }
      if (rcode != V7_OK) {
        goto clean;
      }
    }
  }

clean:
  if (res != NULL) {
    *res = ires;
  }
  return rcode;
}

void v7_array_del(struct v7 *v7, val_t arr, unsigned long index) {
  char buf[20];
  int n = v_sprintf_s(buf, sizeof(buf), "%lu", index);
  v7_del(v7, arr, buf, n);
}

int v7_array_push(struct v7 *v7, v7_val_t arr, v7_val_t v) {
  return v7_array_set(v7, arr, v7_array_length(v7, arr), v);
}

WARN_UNUSED_RESULT
enum v7_err v7_array_push_throwing(struct v7 *v7, v7_val_t arr, v7_val_t v,
                                   int *res) {
  return v7_array_set_throwing(v7, arr, v7_array_length(v7, arr), v, res);
}
