/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/gc.h"
#include "v7/src/freeze.h"
#include "v7/src/main.h"
#include "v7/src/primitive.h"
#include "v7/src/exec.h"
#include "v7/src/util.h"
#include "v7/src/conversion.h"
#include "common/platform.h"
#include "common/cs_file.h"

#if defined(_MSC_VER) && _MSC_VER >= 1800
#define fileno _fileno
#endif

#ifdef V7_EXE
#define V7_MAIN
#endif

#ifdef V7_MAIN

#include <sys/stat.h>

static void show_usage(char *argv[]) {
  fprintf(stderr, "V7 version %s (c) Cesanta Software, built on %s\n",
          V7_VERSION, __DATE__);
  fprintf(stderr, "Usage: %s [OPTIONS] js_file ...\n", argv[0]);
  fprintf(stderr, "%s\n", "OPTIONS:");
  fprintf(stderr, "%s\n", "  -e <expr>            execute expression");
  fprintf(stderr, "%s\n", "  -t                   dump generated text AST");
  fprintf(stderr, "%s\n", "  -b                   dump generated binary AST");
  fprintf(stderr, "%s\n", "  -c                   dump compiled binary bcode");
  fprintf(stderr, "%s\n", "  -mm                  dump memory stats");
  fprintf(stderr, "%s\n", "  -vo <n>              object arena size");
  fprintf(stderr, "%s\n", "  -vf <n>              function arena size");
  fprintf(stderr, "%s\n", "  -vp <n>              property arena size");
#ifdef V7_FREEZE
  fprintf(stderr, "%s\n", "  -freeze filename     dump JS heap into a file");
#endif
  exit(EXIT_FAILURE);
}

#if V7_ENABLE__Memory__stats
static void dump_mm_arena_stats(const char *msg, struct gc_arena *a) {
  printf("%s: total allocations %lu, total garbage %lu, max %" SIZE_T_FMT
         ", alive %lu\n",
         msg, a->allocations, a->garbage, gc_arena_size(a), a->alive);
  printf(
      "%s: (bytes: total allocations %lu, total garbage %lu, max %" SIZE_T_FMT
      ", alive %lu)\n",
      msg, a->allocations * a->cell_size, a->garbage * a->cell_size,
      gc_arena_size(a) * a->cell_size, a->alive * a->cell_size);
}

static void dump_mm_stats(struct v7 *v7) {
  dump_mm_arena_stats("object: ", &v7->generic_object_arena);
  dump_mm_arena_stats("function: ", &v7->function_arena);
  dump_mm_arena_stats("property: ", &v7->property_arena);
  printf("string arena len: %" SIZE_T_FMT "\n", v7->owned_strings.len);
  printf("Total heap size: %" SIZE_T_FMT "\n",
         v7->owned_strings.len +
             gc_arena_size(&v7->generic_object_arena) *
                 v7->generic_object_arena.cell_size +
             gc_arena_size(&v7->function_arena) * v7->function_arena.cell_size +
             gc_arena_size(&v7->property_arena) * v7->property_arena.cell_size);
}
#endif

int v7_main(int argc, char *argv[], void (*pre_freeze_init)(struct v7 *),
            void (*pre_init)(struct v7 *), void (*post_init)(struct v7 *)) {
  int exit_rcode = EXIT_SUCCESS;
  struct v7 *v7;
  struct v7_create_opts opts;
  int as_json = 0;
  int i, j, show_ast = 0, binary_ast = 0, dump_bcode = 0, dump_stats = 0;
  val_t res;
  int nexprs = 0;
  const char *exprs[16];

  memset(&opts, 0, sizeof(opts));

  /* Execute inline code */
  for (i = 1; i < argc && argv[i][0] == '-'; i++) {
    if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
      exprs[nexprs++] = argv[i + 1];
      i++;
    } else if (strcmp(argv[i], "-t") == 0) {
      show_ast = 1;
    } else if (strcmp(argv[i], "-b") == 0) {
      show_ast = 1;
      binary_ast = 1;
    } else if (strcmp(argv[i], "-c") == 0) {
      binary_ast = 1;
      dump_bcode = 1;
    } else if (strcmp(argv[i], "-h") == 0) {
      show_usage(argv);
    } else if (strcmp(argv[i], "-j") == 0) {
      as_json = 1;
#if V7_ENABLE__Memory__stats
    } else if (strcmp(argv[i], "-mm") == 0) {
      dump_stats = 1;
#endif
    } else if (strcmp(argv[i], "-vo") == 0 && i + 1 < argc) {
      opts.object_arena_size = atoi(argv[i + 1]);
      i++;
    } else if (strcmp(argv[i], "-vf") == 0 && i + 1 < argc) {
      opts.function_arena_size = atoi(argv[i + 1]);
      i++;
    } else if (strcmp(argv[i], "-vp") == 0 && i + 1 < argc) {
      opts.property_arena_size = atoi(argv[i + 1]);
      i++;
    }
#ifdef V7_FREEZE
    else if (strcmp(argv[i], "-freeze") == 0 && i + 1 < argc) {
      opts.freeze_file = argv[i + 1];
      i++;
    }
#endif
  }

#ifndef V7_ALLOW_ARGLESS_MAIN
  if (argc == 1) {
    show_usage(argv);
  }
#endif

  v7 = v7_create_opt(opts);
  res = V7_UNDEFINED;

  if (pre_freeze_init != NULL) {
    pre_freeze_init(v7);
  }

#ifdef V7_FREEZE
  /*
   * Skip pre_init if freezing, but still execute cmdline expressions.
   * This makes it easier to add custom code when freezing from cmdline.
   */
  if (opts.freeze_file == NULL) {
#endif

    if (pre_init != NULL) {
      pre_init(v7);
    }

#ifdef V7_FREEZE
  }
#endif

#if V7_ENABLE__Memory__stats > 0 && !defined(V7_DISABLE_GC)
  if (dump_stats) {
    printf("Memory stats during init:\n");
    dump_mm_stats(v7);
    v7_gc(v7, 0);
    printf("Memory stats before run:\n");
    dump_mm_stats(v7);
  }
#else
  (void) dump_stats;
#endif

  /* Execute inline expressions */
  for (j = 0; j < nexprs; j++) {
    enum v7_err (*exec)(struct v7 *, const char *, v7_val_t *);
    exec = v7_exec;

    if (show_ast || dump_bcode) {
      if (v7_compile(exprs[j], binary_ast, dump_bcode, stdout) != V7_OK) {
        exit_rcode = EXIT_FAILURE;
        fprintf(stderr, "%s\n", "parse error");
      }
    } else if (exec(v7, exprs[j], &res) != V7_OK) {
      v7_print_error(stderr, v7, exprs[j], res);
      exit_rcode = EXIT_FAILURE;
      res = V7_UNDEFINED;
    }
  }

  /* Execute files */
  for (; i < argc; i++) {
    if (show_ast || dump_bcode) {
      size_t size;
      char *source_code;
      if ((source_code = cs_read_file(argv[i], &size)) == NULL) {
        exit_rcode = EXIT_FAILURE;
        fprintf(stderr, "Cannot read [%s]\n", argv[i]);
      } else {
        if (_v7_compile(source_code, size, binary_ast, dump_bcode, stdout) !=
            V7_OK) {
          fprintf(stderr, "error: %s\n", v7->error_msg);
          exit_rcode = EXIT_FAILURE;
          exit(exit_rcode);
        }
        free(source_code);
      }
    } else if (v7_exec_file(v7, argv[i], &res) != V7_OK) {
      v7_print_error(stderr, v7, argv[i], res);
      res = V7_UNDEFINED;
    }
  }

#ifdef V7_FREEZE
  if (opts.freeze_file != NULL) {
    freeze(v7, opts.freeze_file);
    exit(0);
  }
#endif

  if (!(show_ast || dump_bcode)) {
    char buf[2000];
    char *s = v7_stringify(v7, res, buf, sizeof(buf),
                           as_json ? V7_STRINGIFY_JSON : V7_STRINGIFY_DEBUG);
    printf("%s\n", s);
    if (s != buf) {
      free(s);
    }
  }

  if (post_init != NULL) {
    post_init(v7);
  }

#if V7_ENABLE__Memory__stats
  if (dump_stats) {
    printf("Memory stats after run:\n");
    dump_mm_stats(v7);
  }
#else
  (void) dump_stats;
#endif

  v7_destroy(v7);
  return exit_rcode;
}
#endif

#if def V7_EXE
int main(int argc, char *argv[]) {
  return v7_main(argc, argv, NULL, NULL, NULL);
}
#elif defined(_WIN32)
#include <crtdbg.h>
#pragma comment(lib, "winmm.lib")
int main(int argc, char *argv[]) {
    int startTime;
    int endTime;
    
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc(67);
    
	startTime = timeGetTime();
    v7_main(argc, argv, NULL, NULL, NULL);
	endTime = timeGetTime();
	
    _CrtDumpMemoryLeaks();
    
    printf("%d\tms\n", endTime - startTime);
    return 0;
}
#endif
