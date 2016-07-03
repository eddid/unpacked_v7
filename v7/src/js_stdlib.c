/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/* clang-format off */
/* because clang-format would break JS code, e.g. === converted to == = ... */

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/exec.h"
#include "v7/src/util.h"

#define STRINGIFY(x) #x

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

static const char js_array_indexOf[] = STRINGIFY(
    Object.defineProperty(Array.prototype, "indexOf", {
      writable:true,
      configurable: true,
      value: function(a, x) {
        var i; var r = -1; var b = +x;
        if (!b || b < 0) b = 0;
        for (i in this) if (i >= b && (r < 0 || i < r) && this[i] === a) r = +i;
        return r;
    }}););

static const char js_array_lastIndexOf[] = STRINGIFY(
    Object.defineProperty(Array.prototype, "lastIndexOf", {
      writable:true,
      configurable: true,
      value: function(a, x) {
        var i; var r = -1; var b = +x;
        if (isNaN(b) || b < 0 || b >= this.length) b = this.length - 1;
        for (i in this) if (i <= b && (r < 0 || i > r) && this[i] === a) r = +i;
        return r;
    }}););

#if V7_ENABLE__Array__reduce
static const char js_array_reduce[] = STRINGIFY(
    Object.defineProperty(Array.prototype, "reduce", {
      writable:true,
      configurable: true,
      value: function(a, b) {
        var f = 0;
        if (typeof(a) != "function") {
          throw new TypeError(a + " is not a function");
        }
        for (var k in this) {
          if (k > this.length) break;
          if (f == 0 && b === undefined) {
            b = this[k];
            f = 1;
          } else {
            b = a(b, this[k], k, this);
          }
        }
        return b;
    }}););
#endif

static const char js_array_pop[] = STRINGIFY(
    Object.defineProperty(Array.prototype, "pop", {
      writable:true,
      configurable: true,
      value: function() {
      var i = this.length - 1;
        return this.splice(i, 1)[0];
    }}););

static const char js_array_shift[] = STRINGIFY(
    Object.defineProperty(Array.prototype, "shift", {
      writable:true,
      configurable: true,
      value: function() {
        return this.splice(0, 1)[0];
    }}););

#if V7_ENABLE__Function__call
static const char js_function_call[] = STRINGIFY(
    Object.defineProperty(Function.prototype, "call", {
      writable:true,
      configurable: true,
      value: function() {
        var t = arguments.splice(0, 1)[0];
        return this.apply(t, arguments);
    }}););
#endif

#if V7_ENABLE__Function__bind
static const char js_function_bind[] = STRINGIFY(
    Object.defineProperty(Function.prototype, "bind", {
      writable:true,
      configurable: true,
      value: function(t) {
        var f = this;
        return function() {
          return f.apply(t, arguments);
        };
    }}););
#endif

#if V7_ENABLE__Blob
static const char js_Blob[] = STRINGIFY(
    function Blob(a) {
      this.a = a;
    });
#endif

static const char * const js_functions[] = {
#if V7_ENABLE__Blob
  js_Blob,
#endif
#if V7_ENABLE__Function__call
  js_function_call,
#endif
#if V7_ENABLE__Function__bind
  js_function_bind,
#endif
#if V7_ENABLE__Array__reduce
  js_array_reduce,
#endif
  js_array_indexOf,
  js_array_lastIndexOf,
  js_array_pop,
  js_array_shift
};

 V7_PRIVATE void init_js_stdlib(struct v7 *v7) {
  val_t res;
  int i;

  for(i = 0; i < (int) ARRAY_SIZE(js_functions); i++) {
    if (v7_exec(v7, js_functions[i], &res) != V7_OK) {
      fprintf(stderr, "ex: %s:\n", js_functions[i]);
      v7_fprintln(stderr, v7, res);
    }
  }

  /* TODO(lsm): re-enable in a separate PR */
#if 0
  v7_exec(v7, &res, STRINGIFY(
    Array.prototype.unshift = function() {
      var a = new Array(0, 0);
      Array.prototype.push.apply(a, arguments);
      Array.prototype.splice.apply(this, a);
      return this.length;
    };));
#endif
}

#if defined(__cplusplus)
}
#endif /* __cplusplus */
