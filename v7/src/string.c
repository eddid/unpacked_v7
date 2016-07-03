/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/utf.h"
#include "v7/src/string.h"
#include "v7/src/exceptions.h"
#include "v7/src/conversion.h"
#include "v7/src/varint.h"
#include "v7/src/gc.h"
#include "v7/src/core.h"
#include "v7/src/primitive.h"
#include "v7/src/slre.h"
#include "v7/src/heapusage.h"

/* TODO(lsm): NaN payload location depends on endianness, make crossplatform */
#define GET_VAL_NAN_PAYLOAD(v) ((char *) &(v))

/*
 * Dictionary of read-only strings with length > 5.
 * NOTE(lsm): must be sorted lexicographically, because
 * v_find_string_in_dictionary performs binary search over this list.
 */
/* clang-format off */
static const struct v7_vec_const v_dictionary_strings[] = {
    V7_VEC(" is not a function"),
    V7_VEC("Boolean"),
    V7_VEC("Crypto"),
    V7_VEC("EvalError"),
    V7_VEC("Function"),
    V7_VEC("Infinity"),
    V7_VEC("InternalError"),
    V7_VEC("LOG10E"),
    V7_VEC("MAX_VALUE"),
    V7_VEC("MIN_VALUE"),
    V7_VEC("NEGATIVE_INFINITY"),
    V7_VEC("Number"),
    V7_VEC("Object"),
    V7_VEC("POSITIVE_INFINITY"),
    V7_VEC("RangeError"),
    V7_VEC("ReferenceError"),
    V7_VEC("RegExp"),
    V7_VEC("SQRT1_2"),
    V7_VEC("Socket"),
    V7_VEC("String"),
    V7_VEC("SyntaxError"),
    V7_VEC("TypeError"),
    V7_VEC("UBJSON"),
    V7_VEC("_modcache"),
    V7_VEC("accept"),
    V7_VEC("arguments"),
    V7_VEC("base64_decode"),
    V7_VEC("base64_encode"),
    V7_VEC("boolean"),
    V7_VEC("charAt"),
    V7_VEC("charCodeAt"),
    V7_VEC("concat"),
    V7_VEC("configurable"),
    V7_VEC("connect"),
    V7_VEC("constructor"),
    V7_VEC("create"),
    V7_VEC("defineProperties"),
    V7_VEC("defineProperty"),
    V7_VEC("every"),
    V7_VEC("exists"),
    V7_VEC("exports"),
    V7_VEC("filter"),
    V7_VEC("forEach"),
    V7_VEC("fromCharCode"),
    V7_VEC("function"),
    V7_VEC("getDate"),
    V7_VEC("getDay"),
    V7_VEC("getFullYear"),
    V7_VEC("getHours"),
    V7_VEC("getMilliseconds"),
    V7_VEC("getMinutes"),
    V7_VEC("getMonth"),
    V7_VEC("getOwnPropertyDescriptor"),
    V7_VEC("getOwnPropertyNames"),
    V7_VEC("getPrototypeOf"),
    V7_VEC("getSeconds"),
    V7_VEC("getTime"),
    V7_VEC("getTimezoneOffset"),
    V7_VEC("getUTCDate"),
    V7_VEC("getUTCDay"),
    V7_VEC("getUTCFullYear"),
    V7_VEC("getUTCHours"),
    V7_VEC("getUTCMilliseconds"),
    V7_VEC("getUTCMinutes"),
    V7_VEC("getUTCMonth"),
    V7_VEC("getUTCSeconds"),
    V7_VEC("global"),
    V7_VEC("hasOwnProperty"),
    V7_VEC("ignoreCase"),
    V7_VEC("indexOf"),
    V7_VEC("isArray"),
    V7_VEC("isExtensible"),
    V7_VEC("isFinite"),
    V7_VEC("isPrototypeOf"),
    V7_VEC("lastIndex"),
    V7_VEC("lastIndexOf"),
    V7_VEC("length"),
    V7_VEC("listen"),
    V7_VEC("loadJSON"),
    V7_VEC("localeCompare"),
    V7_VEC("md5_hex"),
    V7_VEC("module"),
    V7_VEC("multiline"),
    V7_VEC("number"),
    V7_VEC("parseFloat"),
    V7_VEC("parseInt"),
    V7_VEC("preventExtensions"),
    V7_VEC("propertyIsEnumerable"),
    V7_VEC("prototype"),
    V7_VEC("random"),
    V7_VEC("recvAll"),
    V7_VEC("reduce"),
    V7_VEC("remove"),
    V7_VEC("rename"),
    V7_VEC("render"),
    V7_VEC("replace"),
    V7_VEC("require"),
    V7_VEC("reverse"),
    V7_VEC("search"),
    V7_VEC("setDate"),
    V7_VEC("setFullYear"),
    V7_VEC("setHours"),
    V7_VEC("setMilliseconds"),
    V7_VEC("setMinutes"),
    V7_VEC("setMonth"),
    V7_VEC("setSeconds"),
    V7_VEC("setTime"),
    V7_VEC("setUTCDate"),
    V7_VEC("setUTCFullYear"),
    V7_VEC("setUTCHours"),
    V7_VEC("setUTCMilliseconds"),
    V7_VEC("setUTCMinutes"),
    V7_VEC("setUTCMonth"),
    V7_VEC("setUTCSeconds"),
    V7_VEC("sha1_hex"),
    V7_VEC("source"),
    V7_VEC("splice"),
    V7_VEC("string"),
    V7_VEC("stringify"),
    V7_VEC("substr"),
    V7_VEC("substring"),
    V7_VEC("toDateString"),
    V7_VEC("toExponential"),
    V7_VEC("toFixed"),
    V7_VEC("toISOString"),
    V7_VEC("toJSON"),
    V7_VEC("toLocaleDateString"),
    V7_VEC("toLocaleLowerCase"),
    V7_VEC("toLocaleString"),
    V7_VEC("toLocaleTimeString"),
    V7_VEC("toLocaleUpperCase"),
    V7_VEC("toLowerCase"),
    V7_VEC("toPrecision"),
    V7_VEC("toString"),
    V7_VEC("toTimeString"),
    V7_VEC("toUTCString"),
    V7_VEC("toUpperCase"),
    V7_VEC("valueOf"),
    V7_VEC("writable"),
};
/* clang-format on */

int nextesc(const char **p); /* from SLRE */
V7_PRIVATE size_t unescape(const char *s, size_t len, char *to) {
  const char *end = s + len;
  size_t n = 0;
  char tmp[4];
  Rune r;

  while (s < end) {
    s += chartorune(&r, s);
    if (r == '\\' && s < end) {
      switch (*s) {
        case '"':
          s++, r = '"';
          break;
        case '\'':
          s++, r = '\'';
          break;
        case '\n':
          s++, r = '\n';
          break;
        default: {
          const char *tmp_s = s;
          int i = nextesc(&s);
          switch (i) {
            case -SLRE_INVALID_ESC_CHAR:
              r = '\\';
              s = tmp_s;
              n += runetochar(to == NULL ? tmp : to + n, &r);
              s += chartorune(&r, s);
              break;
            case -SLRE_INVALID_HEX_DIGIT:
            default:
              r = i;
          }
        }
      }
    }
    n += runetochar(to == NULL ? tmp : to + n, &r);
  }

  return n;
}

static int v_find_string_in_dictionary(const char *s, size_t len) {
  size_t start = 0, end = ARRAY_SIZE(v_dictionary_strings);

  while (s != NULL && start < end) {
    size_t mid = start + (end - start) / 2;
    const struct v7_vec_const *v = &v_dictionary_strings[mid];
    size_t min_len = len < v->len ? len : v->len;
    int comparison_result = memcmp(s, v->p, min_len);
    if (comparison_result == 0) {
      comparison_result = len - v->len;
    }
    if (comparison_result < 0) {
      end = mid;
    } else if (comparison_result > 0) {
      start = mid + 1;
    } else {
      return mid;
    }
  }
  return -1;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err v7_char_code_at(struct v7 *v7, val_t obj, val_t arg,
                                       double *res) {
  enum v7_err rcode = V7_OK;
  size_t n;
  val_t s = V7_UNDEFINED;
  const char *p = NULL;
  double at = v7_get_double(v7, arg);

  *res = 0;

  rcode = to_string(v7, obj, &s, NULL, 0, NULL);
  if (rcode != V7_OK) {
    goto clean;
  }

  p = v7_get_string(v7, &s, &n);

  n = utfnlen(p, n);
  if (v7_is_number(arg) && at >= 0 && at < n) {
    Rune r = 0;
    p = utfnshift(p, at);
    chartorune(&r, (char *) p);
    *res = r;
    goto clean;
  } else {
    *res = NAN;
    goto clean;
  }

clean:
  return rcode;
}

V7_PRIVATE int s_cmp(struct v7 *v7, val_t a, val_t b) {
  size_t a_len, b_len;
  const char *a_ptr, *b_ptr;

  a_ptr = v7_get_string(v7, &a, &a_len);
  b_ptr = v7_get_string(v7, &b, &b_len);

  if (a_len == b_len) {
    return memcmp(a_ptr, b_ptr, a_len);
  }
  if (a_len > b_len) {
    return 1;
  } else if (a_len < b_len) {
    return -1;
  } else {
    return 0;
  }
}

V7_PRIVATE val_t s_concat(struct v7 *v7, val_t a, val_t b) {
  size_t a_len, b_len, res_len;
  const char *a_ptr, *b_ptr, *res_ptr;
  val_t res;

  /* Find out lengths of both srtings */
  a_ptr = v7_get_string(v7, &a, &a_len);
  b_ptr = v7_get_string(v7, &b, &b_len);

  /* Create an placeholder string */
  res = v7_mk_string(v7, NULL, a_len + b_len, 1);

  /* v7_mk_string() may have reallocated mbuf - revalidate pointers */
  a_ptr = v7_get_string(v7, &a, &a_len);
  b_ptr = v7_get_string(v7, &b, &b_len);

  /* Copy strings into the placeholder */
  res_ptr = v7_get_string(v7, &res, &res_len);
  memcpy((char *) res_ptr, a_ptr, a_len);
  memcpy((char *) res_ptr + a_len, b_ptr, b_len);

  return res;
}

V7_PRIVATE unsigned long cstr_to_ulong(const char *s, size_t len, int *ok) {
  char *e;
  unsigned long res = strtoul(s, &e, 10);
  *ok = (e == s + len) && len != 0;
  return res;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err str_to_ulong(struct v7 *v7, val_t v, int *ok,
                                    unsigned long *res) {
  enum v7_err rcode = V7_OK;
  char buf[100];
  size_t len = 0;

  V7_TRY(to_string(v7, v, NULL, buf, sizeof(buf), &len));

  *res = cstr_to_ulong(buf, len, ok);

clean:
  return rcode;
}

/* Insert a string into mbuf at specified offset */
V7_PRIVATE void embed_string(struct mbuf *m, size_t offset, const char *p,
                             size_t len, uint8_t /*enum embstr_flags*/ flags) {
  char *old_base = m->buf;
  uint8_t p_backed_by_mbuf = p >= old_base && p < old_base + m->len;
  size_t n = (flags & EMBSTR_UNESCAPE) ? unescape(p, len, NULL) : len;

  /* Calculate how many bytes length takes */
  int k = calc_llen(n);

  /* total length: varing length + string len + zero-term */
  size_t tot_len = k + n + !!(flags & EMBSTR_ZERO_TERM);

  /* Allocate buffer */
  heapusage_dont_count(1);
  mbuf_insert(m, offset, NULL, tot_len);
  heapusage_dont_count(0);

  /* Fixup p if it was relocated by mbuf_insert() above */
  if (p_backed_by_mbuf) {
    p += m->buf - old_base;
  }

  /* Write length */
  encode_varint(n, (unsigned char *) m->buf + offset);

  /* Write string */
  if (p != 0) {
    if (flags & EMBSTR_UNESCAPE) {
      unescape(p, len, m->buf + offset + k);
    } else {
      memcpy(m->buf + offset + k, p, len);
    }
  }

  /* add NULL-terminator if needed */
  if (flags & EMBSTR_ZERO_TERM) {
    m->buf[offset + tot_len - 1] = '\0';
  }
}

/* Create a string */
v7_val_t v7_mk_string(struct v7 *v7, const char *p, size_t len, int copy) {
  struct mbuf *m = copy ? &v7->owned_strings : &v7->foreign_strings;
  val_t offset = m->len, tag = V7_TAG_STRING_F;
  int dict_index;

#ifdef V7_GC_AFTER_STRING_ALLOC
  v7->need_gc = 1;
#endif

  if (len == ~((size_t) 0)) len = strlen(p);

  if (len <= 4) {
    char *s = GET_VAL_NAN_PAYLOAD(offset) + 1;
    offset = 0;
    if (p != 0) {
      memcpy(s, p, len);
    }
    s[-1] = len;
    tag = V7_TAG_STRING_I;
  } else if (len == 5) {
    char *s = GET_VAL_NAN_PAYLOAD(offset);
    offset = 0;
    if (p != 0) {
      memcpy(s, p, len);
    }
    tag = V7_TAG_STRING_5;
  } else if ((dict_index = v_find_string_in_dictionary(p, len)) >= 0) {
    offset = 0;
    GET_VAL_NAN_PAYLOAD(offset)[0] = dict_index;
    tag = V7_TAG_STRING_D;
  } else if (copy) {
    compute_need_gc(v7);

    /*
     * Before embedding new string, check if the reallocation is needed.  If
     * so, perform the reallocation by calling `mbuf_resize` manually, since we
     * need to preallocate some extra space (`_V7_STRING_BUF_RESERVE`)
     */
    if ((m->len + len) > m->size) {
      heapusage_dont_count(1);
      mbuf_resize(m, m->len + len + _V7_STRING_BUF_RESERVE);
      heapusage_dont_count(0);
    }
    embed_string(m, m->len, p, len, EMBSTR_ZERO_TERM);
    tag = V7_TAG_STRING_O;
#ifndef V7_DISABLE_STR_ALLOC_SEQ
    /* TODO(imax): panic if offset >= 2^32. */
    offset |= ((val_t) gc_next_allocation_seqn(v7, p, len)) << 32;
#endif
  } else {
    /* foreign string */
    if (sizeof(void *) <= 4 && len <= UINT16_MAX) {
      /* small foreign strings can fit length and ptr in the val_t */
      offset = (uint64_t) len << 32 | (uint64_t)(uintptr_t) p;
    } else {
      /* bigger strings need indirection that uses ram */
      size_t pos = m->len;
      int llen = calc_llen(len);

      /* allocate space for len and ptr */
      heapusage_dont_count(1);
      mbuf_insert(m, pos, NULL, llen + sizeof(p));
      heapusage_dont_count(0);

      encode_varint(len, (uint8_t *) (m->buf + pos));
      memcpy(m->buf + pos + llen, &p, sizeof(p));
    }
    tag = V7_TAG_STRING_F;
  }

  /* NOTE(lsm): don't use pointer_to_value, 32-bit ptrs will truncate */
  return (offset & ~V7_TAG_MASK) | tag;
}

int v7_is_string(val_t v) {
  uint64_t t = v & V7_TAG_MASK;
  return t == V7_TAG_STRING_I || t == V7_TAG_STRING_F || t == V7_TAG_STRING_O ||
         t == V7_TAG_STRING_5 || t == V7_TAG_STRING_D;
}

/* Get a pointer to string and string length. */
const char *v7_get_string(struct v7 *v7, val_t *v, size_t *sizep) {
  uint64_t tag = v[0] & V7_TAG_MASK;
  const char *p = NULL;
  int llen;
  size_t size = 0;

  if (!v7_is_string(*v)) {
    goto clean;
  }

  if (tag == V7_TAG_STRING_I) {
    p = GET_VAL_NAN_PAYLOAD(*v) + 1;
    size = p[-1];
  } else if (tag == V7_TAG_STRING_5) {
    p = GET_VAL_NAN_PAYLOAD(*v);
    size = 5;
  } else if (tag == V7_TAG_STRING_D) {
    int index = ((unsigned char *) GET_VAL_NAN_PAYLOAD(*v))[0];
    size = v_dictionary_strings[index].len;
    p = v_dictionary_strings[index].p;
  } else if (tag == V7_TAG_STRING_O) {
    size_t offset = (size_t) gc_string_val_to_offset(*v);
    char *s = v7->owned_strings.buf + offset;

#ifndef V7_DISABLE_STR_ALLOC_SEQ
    gc_check_valid_allocation_seqn(v7, (*v >> 32) & 0xFFFF);
#endif

    size = decode_varint((uint8_t *) s, &llen);
    p = s + llen;
  } else if (tag == V7_TAG_STRING_F) {
    /*
     * short foreign strings on <=32-bit machines can be encoded in a compact
     * form:
     *
     *     7         6        5        4        3        2        1        0
     *  11111111|1111tttt|llllllll|llllllll|ssssssss|ssssssss|ssssssss|ssssssss
     *
     * Strings longer than 2^26 will be indireceted through the foreign_strings
     * mbuf.
     *
     * We don't use a different tag to represent those two cases. Instead, all
     * foreign strings represented with the help of the foreign_strings mbuf
     * will have the upper 16-bits of the payload set to zero. This allows us to
     * represent up to 477 million foreign strings longer than 64k.
     */
    uint16_t len = (*v >> 32) & 0xFFFF;
    if (sizeof(void *) <= 4 && len != 0) {
      size = (size_t) len;
      p = (const char *) (uintptr_t) *v;
    } else {
      size_t offset = (size_t) gc_string_val_to_offset(*v);
      char *s = v7->foreign_strings.buf + offset;

      size = decode_varint((uint8_t *) s, &llen);
      memcpy(&p, s + llen, sizeof(p));
    }
  } else {
    assert(0);
  }

clean:
  if (sizep != NULL) {
    *sizep = size;
  }
  return p;
}

const char *v7_get_cstring(struct v7 *v7, v7_val_t *value) {
  size_t size;
  const char *s = v7_get_string(v7, value, &size);
  if (s == NULL) return NULL;
  if (s[size] != 0 || strlen(s) != size) {
    return NULL;
  }
  return s;
}
