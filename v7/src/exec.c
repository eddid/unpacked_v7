/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/* osdep.h must be included before `cs_file.h` TODO(dfrank) : fix this */
#include "common/cs_file.h"
#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/eval.h"
#include "v7/src/exec.h"
#include "v7/src/ast.h"
#include "v7/src/compiler.h"
#include "v7/src/exceptions.h"

enum v7_err v7_exec(struct v7 *v7, const char *js_code, v7_val_t *res) {
  return b_exec(v7, js_code, strlen(js_code), NULL, V7_UNDEFINED, V7_UNDEFINED,
                V7_UNDEFINED, 0, 0, 0, res);
}

enum v7_err v7_exec_opt(struct v7 *v7, const char *js_code,
                        const struct v7_exec_opts *opts, v7_val_t *res) {
  return b_exec(v7, js_code, strlen(js_code), opts->filename, V7_UNDEFINED,
                V7_UNDEFINED,
                (opts->this_obj == 0 ? V7_UNDEFINED : opts->this_obj),
                opts->is_json, 0, 0, res);
}

enum v7_err v7_parse_json(struct v7 *v7, const char *str, v7_val_t *res) {
  return b_exec(v7, str, strlen(str), NULL, V7_UNDEFINED, V7_UNDEFINED,
                V7_UNDEFINED, 1, 0, 0, res);
}

#ifndef V7_NO_FS
static enum v7_err exec_file(struct v7 *v7, const char *path, val_t *res,
                             int is_json) {
  enum v7_err rcode = V7_OK;
  char *p;
  size_t file_size;
  char *(*rd)(const char *, size_t *);

  rd = cs_read_file;
#ifdef V7_MMAP_EXEC
  rd = cs_mmap_file;
#ifdef V7_MMAP_EXEC_ONLY
#define I_STRINGIFY(x) #x
#define I_STRINGIFY2(x) I_STRINGIFY(x)

  /* use mmap only for .js files */
  if (strlen(path) <= 3 || strcmp(path + strlen(path) - 3, ".js") != 0) {
    rd = cs_read_file;
  }
#endif
#endif

  if ((p = rd(path, &file_size)) == NULL) {
    rcode = v7_throwf(v7, SYNTAX_ERROR, "cannot open [%s]", path);
    /*
     * In order to maintain compat with existing API, we should save the
     * current exception value into `*res`
     *
     * TODO(dfrank): probably change API: clients can use
     *`v7_get_thrown_value()` now.
     */
    if (res != NULL) *res = v7_get_thrown_value(v7, NULL);
    goto clean;
  } else {
#ifndef V7_MMAP_EXEC
    int fr = 1;
#else
    int fr = 0;
#endif
    rcode = b_exec(v7, p, file_size, path, V7_UNDEFINED, V7_UNDEFINED,
                   V7_UNDEFINED, is_json, fr, 0, res);
    if (rcode != V7_OK) {
      goto clean;
    }
  }

clean:
  return rcode;
}

enum v7_err v7_exec_file(struct v7 *v7, const char *path, val_t *res) {
  return exec_file(v7, path, res, 0);
}

enum v7_err v7_parse_json_file(struct v7 *v7, const char *path, v7_val_t *res) {
  return exec_file(v7, path, res, 1);
}
#endif /* V7_NO_FS */

enum v7_err v7_apply(struct v7 *v7, v7_val_t func, v7_val_t this_obj,
                     v7_val_t args, v7_val_t *res) {
  return b_apply(v7, func, this_obj, args, 0, res);
}

#ifndef NO_LIBC
enum v7_err _v7_compile(const char *src, size_t js_code_size, int binary,
                        int use_bcode, FILE *fp) {
  struct ast ast;
  struct v7 *v7 = v7_create();
  ast_off_t pos = 0;
  enum v7_err err;

  v7->is_precompiling = 1;

  ast_init(&ast, 0);
  err = parse(v7, &ast, src, js_code_size, 0);
  if (err == V7_OK) {
    if (use_bcode) {
      struct bcode bcode;
      /*
       * We don't set filename here, because the bcode will be just serialized
       * and then freed. We don't currently serialize filename. If we ever do,
       * we'll have to make `_v7_compile()` to also take a filename argument,
       * and use it here.
       */
      bcode_init(&bcode, 0, NULL, 0);
      err = compile_script(v7, &ast, &bcode);
      if (err != V7_OK) {
        goto cleanup_bcode;
      }

      if (binary) {
        bcode_serialize(v7, &bcode, fp);
      } else {
#ifdef V7_BCODE_DUMP
        dump_bcode(v7, fp, &bcode);
#else
        fprintf(stderr, "build flag V7_BCODE_DUMP not enabled\n");
#endif
      }
    cleanup_bcode:
      bcode_free(v7, &bcode);
    } else {
      if (binary) {
        fwrite(BIN_AST_SIGNATURE, sizeof(BIN_AST_SIGNATURE), 1, fp);
        fwrite(ast.mbuf.buf, ast.mbuf.len, 1, fp);
      } else {
        ast_dump_tree(fp, &ast, &pos, 0);
      }
    }
  }

  ast_free(&ast);
  v7_destroy(v7);
  return err;
}

enum v7_err v7_compile(const char *src, int binary, int use_bcode, FILE *fp) {
  return _v7_compile(src, strlen(src), binary, use_bcode, fp);
}
#endif
