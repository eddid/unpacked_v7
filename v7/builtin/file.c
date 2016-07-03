/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/primitive.h"
#include "v7/src/string.h"
#include "v7/src/exceptions.h"
#include "v7/src/object.h"
#include "v7/src/exec.h"
#include "v7/src/array.h"
#include "common/mbuf.h"
#include "common/cs_file.h"
#include "v7/src/v7_features.h"
#include "common/cs_dirent.h"

#if defined(V7_ENABLE_FILE) && !defined(V7_NO_FS)

static const char s_fd_prop[] = "__fd";

#ifndef NO_LIBC
static FILE *v7_val_to_file(struct v7 *v7, v7_val_t val) {
  (void) v7;
  return (FILE *) v7_get_ptr(v7, val);
}

static v7_val_t v7_file_to_val(struct v7 *v7, FILE *file) {
  (void) v7;
  return v7_mk_foreign(v7, file);
}

static int v7_is_file_type(v7_val_t val) {
  return v7_is_foreign(val);
}
#else
FILE *v7_val_to_file(struct v7 *v7, v7_val_t val);
v7_val_t v7_file_to_val(struct v7 *v7, FILE *file);
int v7_is_file_type(v7_val_t val);
#endif

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_eval(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);

  *res = V7_UNDEFINED;

  if (v7_is_string(arg0)) {
    const char *s = v7_get_cstring(v7, &arg0);
    if (s == NULL) {
      rcode = v7_throwf(v7, "TypeError", "Invalid string");
      goto clean;
    }

    v7_set_gc_enabled(v7, 1);
    rcode = v7_exec_file(v7, s, res);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_exists(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);

  *res = v7_mk_boolean(v7, 0);

  if (v7_is_string(arg0)) {
    const char *fname = v7_get_cstring(v7, &arg0);
    if (fname != NULL) {
      struct stat st;
      if (stat(fname, &st) == 0) *res = v7_mk_boolean(v7, 1);
    }
  }

  return rcode;
}

WARN_UNUSED_RESULT
static enum v7_err f_read(struct v7 *v7, int all, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t this_obj = v7_get_this(v7);
  v7_val_t arg0 = v7_get(v7, this_obj, s_fd_prop, sizeof(s_fd_prop) - 1);

  if (v7_is_file_type(arg0)) {
    struct mbuf m;
    char buf[BUFSIZ];
    int n;
    FILE *fp = v7_val_to_file(v7, arg0);

    /* Read file contents into mbuf */
    mbuf_init(&m, 0);
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
      mbuf_append(&m, buf, n);
      if (!all) {
        break;
      }
    }

    if (m.len > 0) {
      *res = v7_mk_string(v7, m.buf, m.len, 1);
      mbuf_free(&m);
      goto clean;
    }
  }
  *res = v7_mk_string(v7, "", 0, 1);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_obj_read(struct v7 *v7, v7_val_t *res) {
  return f_read(v7, 0, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_obj_write(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t this_obj = v7_get_this(v7);
  v7_val_t arg0 = v7_get(v7, this_obj, s_fd_prop, sizeof(s_fd_prop) - 1);
  v7_val_t arg1 = v7_arg(v7, 0);
  size_t n, sent = 0, len = 0;

  if (v7_is_file_type(arg0) && v7_is_string(arg1)) {
    const char *s = v7_get_string(v7, &arg1, &len);
    FILE *fp = v7_val_to_file(v7, arg0);
    while (sent < len && (n = fwrite(s + sent, 1, len - sent, fp)) > 0) {
      sent += n;
    }
  }

  *res = v7_mk_number(v7, sent);

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_obj_close(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t this_obj = v7_get_this(v7);
  v7_val_t prop = v7_get(v7, this_obj, s_fd_prop, sizeof(s_fd_prop) - 1);
  int ires = -1;

  if (v7_is_file_type(prop)) {
    ires = fclose(v7_val_to_file(v7, prop));
  }

  *res = v7_mk_number(v7, ires);

  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_open(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);
  v7_val_t arg1 = v7_arg(v7, 1);
  FILE *fp = NULL;

  if (v7_is_string(arg0)) {
    const char *s1 = v7_get_cstring(v7, &arg0);
    const char *s2 = "rb"; /* Open files in read mode by default */

    if (v7_is_string(arg1)) {
      s2 = v7_get_cstring(v7, &arg1);
    }

    if (s1 == NULL || s2 == NULL) {
      *res = V7_NULL;
      goto clean;
    }

    fp = fopen(s1, s2);
    if (fp != NULL) {
      v7_val_t obj = v7_mk_object(v7);
      v7_val_t file_proto = v7_get(
          v7, v7_get(v7, v7_get_global(v7), "File", ~0), "prototype", ~0);
      v7_set_proto(v7, obj, file_proto);
      v7_def(v7, obj, s_fd_prop, sizeof(s_fd_prop) - 1, V7_DESC_ENUMERABLE(0),
             v7_file_to_val(v7, fp));
      *res = obj;
      goto clean;
    }
  }

  *res = V7_NULL;

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_read(struct v7 *v7, v7_val_t *res) {
  v7_val_t arg0 = v7_arg(v7, 0);

  if (v7_is_string(arg0)) {
    const char *path = v7_get_cstring(v7, &arg0);
    size_t size = 0;
    char *data = cs_read_file(path, &size);
    if (data != NULL) {
      *res = v7_mk_string(v7, data, size, 1);
      free(data);
    }
  }

  return V7_OK;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_write(struct v7 *v7, v7_val_t *res) {
  v7_val_t arg0 = v7_arg(v7, 0);
  v7_val_t arg1 = v7_arg(v7, 1);
  *res = v7_mk_boolean(v7, 0);

  if (v7_is_string(arg0) && v7_is_string(arg1)) {
    const char *path = v7_get_cstring(v7, &arg0);
    size_t len;
    const char *buf = v7_get_string(v7, &arg1, &len);
    FILE *fp = fopen(path, "wb+");
    if (fp != NULL) {
      if (fwrite(buf, 1, len, fp) == len) {
        *res = v7_mk_boolean(v7, 1);
      }
      fclose(fp);
    }
  }

  return V7_OK;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_rename(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);
  v7_val_t arg1 = v7_arg(v7, 1);
  int ires = -1;

  if (v7_is_string(arg0) && v7_is_string(arg1)) {
    const char *from = v7_get_cstring(v7, &arg0);
    const char *to = v7_get_cstring(v7, &arg1);
    if (from == NULL || to == NULL) {
      *res = v7_mk_number(v7, ENOENT);
      goto clean;
    }

    ires = rename(from, to);
  }

  *res = v7_mk_number(v7, ires == 0 ? 0 : errno);

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_loadJSON(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);

  *res = V7_UNDEFINED;

  if (v7_is_string(arg0)) {
    const char *file_name = v7_get_cstring(v7, &arg0);
    if (file_name == NULL) {
      goto clean;
    }

    rcode = v7_parse_json_file(v7, file_name, res);
    if (rcode != V7_OK) {
      /* swallow exception and return undefined */
      v7_clear_thrown_value(v7);
      rcode = V7_OK;
      *res = V7_UNDEFINED;
    }
  }

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_remove(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);
  int ires = -1;

  if (v7_is_string(arg0)) {
    const char *path = v7_get_cstring(v7, &arg0);
    if (path == NULL) {
      *res = v7_mk_number(v7, ENOENT);
      goto clean;
    }
    ires = remove(path);
  }
  *res = v7_mk_number(v7, ires == 0 ? 0 : errno);

clean:
  return rcode;
}

#if V7_ENABLE__File__list
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err File_list(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);

  *res = V7_UNDEFINED;

  if (v7_is_string(arg0)) {
    const char *path = v7_get_cstring(v7, &arg0);
    struct dirent *dp;
    DIR *dirp;

    if (path == NULL) {
      goto clean;
    }

    if ((dirp = (opendir(path))) != NULL) {
      *res = v7_mk_array(v7);
      while ((dp = readdir(dirp)) != NULL) {
        /* Do not show current and parent dirs */
        if (strcmp((const char *) dp->d_name, ".") == 0 ||
            strcmp((const char *) dp->d_name, "..") == 0) {
          continue;
        }
        /* Add file name to the list */
        v7_array_push(v7, *res,
                      v7_mk_string(v7, (const char *) dp->d_name,
                                   strlen((const char *) dp->d_name), 1));
      }
      closedir(dirp);
    }
  }

clean:
  return rcode;
}
#endif /* V7_ENABLE__File__list */

void init_file(struct v7 *v7) {
  v7_val_t file_obj = v7_mk_object(v7), file_proto = v7_mk_object(v7);
  v7_set(v7, v7_get_global(v7), "File", 4, file_obj);
  v7_set(v7, file_obj, "prototype", 9, file_proto);

  v7_set_method(v7, file_obj, "eval", File_eval);
  v7_set_method(v7, file_obj, "exists", File_exists);
  v7_set_method(v7, file_obj, "remove", File_remove);
  v7_set_method(v7, file_obj, "rename", File_rename);
  v7_set_method(v7, file_obj, "open", File_open);
  v7_set_method(v7, file_obj, "read", File_read);
  v7_set_method(v7, file_obj, "write", File_write);
  v7_set_method(v7, file_obj, "loadJSON", File_loadJSON);
#if V7_ENABLE__File__list
  v7_set_method(v7, file_obj, "list", File_list);
#endif

  v7_set_method(v7, file_proto, "close", File_obj_close);
  v7_set_method(v7, file_proto, "read", File_obj_read);
  v7_set_method(v7, file_proto, "write", File_obj_write);

#if V7_ENABLE__File__require
  v7_def(v7, v7_get_global(v7), "_modcache", ~0, 0, v7_mk_object(v7));
  if (v7_exec(v7,
              "function require(m) { "
              "  if (m in _modcache) { return _modcache[m]; }"
              "  var module = {exports:{}};"
              "  File.eval(m);"
              "  return (_modcache[m] = module.exports)"
              " }",
              NULL) != V7_OK) {
    /* TODO(mkm): percolate failure */
  }
#endif
}
#else
void init_file(struct v7 *v7) {
  (void) v7;
}
#endif /* NO_LIBC */
