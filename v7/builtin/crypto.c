/*
 * Copyright (c) 2015 Cesanta Software Limited
 * All rights reserved
 */

#include <stdlib.h>
#include <string.h>

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/primitive.h"
#include "v7/src/string.h"
#include "v7/src/object.h"
#include "common/md5.h"
#include "common/sha1.h"
#include "common/base64.h"

#ifdef V7_ENABLE_CRYPTO

typedef void (*b64_func_t)(const unsigned char *, int, char *);

WARN_UNUSED_RESULT
static enum v7_err b64_transform(struct v7 *v7, b64_func_t func, double mult,
                                 v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);
  *res = V7_UNDEFINED;

  if (v7_is_string(arg0)) {
    size_t n;
    const char *s = v7_get_string(v7, &arg0, &n);
    char *buf = (char *) malloc(n * mult + 4);
    if (buf != NULL) {
      func((const unsigned char *) s, (int) n, buf);
      *res = v7_mk_string(v7, buf, strlen(buf), 1);
      free(buf);
    }
  }

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Crypto_base64_decode(struct v7 *v7, v7_val_t *res) {
  return b64_transform(v7, (b64_func_t) cs_base64_decode, 0.75, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Crypto_base64_encode(struct v7 *v7, v7_val_t *res) {
  return b64_transform(v7, cs_base64_encode, 1.5, res);
}

static void v7_md5(const char *data, size_t len, char buf[16]) {
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, (unsigned char *) data, len);
  MD5_Final((unsigned char *) buf, &ctx);
}

static void v7_sha1(const char *data, size_t len, char buf[20]) {
  cs_sha1_ctx ctx;
  cs_sha1_init(&ctx);
  cs_sha1_update(&ctx, (unsigned char *) data, len);
  cs_sha1_final((unsigned char *) buf, &ctx);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Crypto_md5(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);

  if (v7_is_string(arg0)) {
    size_t len;
    const char *data = v7_get_string(v7, &arg0, &len);
    char buf[16];
    v7_md5(data, len, buf);
    *res = v7_mk_string(v7, buf, sizeof(buf), 1);
    goto clean;
  }

  *res = V7_NULL;

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Crypto_md5_hex(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);

  if (v7_is_string(arg0)) {
    size_t len;
    const char *data = v7_get_string(v7, &arg0, &len);
    char hash[16], buf[sizeof(hash) * 2 + 1];
    v7_md5(data, len, hash);
    cs_to_hex(buf, (unsigned char *) hash, sizeof(hash));
    *res = v7_mk_string(v7, buf, sizeof(buf) - 1, 1);
    goto clean;
  }
  *res = V7_NULL;

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Crypto_sha1(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);

  if (v7_is_string(arg0)) {
    size_t len;
    const char *data = v7_get_string(v7, &arg0, &len);
    char buf[20];
    v7_sha1(data, len, buf);
    *res = v7_mk_string(v7, buf, sizeof(buf), 1);
    goto clean;
  }
  *res = V7_NULL;

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Crypto_sha1_hex(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);

  if (v7_is_string(arg0)) {
    size_t len;
    const char *data = v7_get_string(v7, &arg0, &len);
    char hash[20], buf[sizeof(hash) * 2 + 1];
    v7_sha1(data, len, hash);
    cs_to_hex(buf, (unsigned char *) hash, sizeof(hash));
    *res = v7_mk_string(v7, buf, sizeof(buf) - 1, 1);
    goto clean;
  }
  *res = V7_NULL;

clean:
  return rcode;
}
#endif

void init_crypto(struct v7 *v7) {
#ifdef V7_ENABLE_CRYPTO
  v7_val_t obj = v7_mk_object(v7);
  v7_set(v7, v7_get_global(v7), "Crypto", 6, obj);
  v7_set_method(v7, obj, "md5", Crypto_md5);
  v7_set_method(v7, obj, "md5_hex", Crypto_md5_hex);
  v7_set_method(v7, obj, "sha1", Crypto_sha1);
  v7_set_method(v7, obj, "sha1_hex", Crypto_sha1_hex);
  v7_set_method(v7, obj, "base64_encode", Crypto_base64_encode);
  v7_set_method(v7, obj, "base64_decode", Crypto_base64_decode);
#else
  (void) v7;
#endif
}
