/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_V7_SRC_CORE_H_
#define CS_V7_SRC_CORE_H_

#include "v7/src/core_public.h"

#include "common/mbuf.h"
#include "v7/src/std_error.h"
#include "v7/src/mm.h"
#include "v7/src/parser.h"
#include "v7/src/object_public.h"
#include "v7/src/tokenizer.h"
#include "v7/src/opcodes.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef uint64_t val_t;

#if defined(V7_ENABLE_ENTITY_IDS)

typedef unsigned short entity_id_t;
typedef unsigned char entity_id_part_t;

/*
 * Magic numbers that are stored in various objects in order to identify their
 * type in runtime
 */
#define V7_ENTITY_ID_PROP 0xe9a1
#define V7_ENTITY_ID_PART_OBJ 0x57
#define V7_ENTITY_ID_PART_GEN_OBJ 0x31
#define V7_ENTITY_ID_PART_JS_FUNC 0x0d

#define V7_ENTITY_ID_NONE 0xa5a5
#define V7_ENTITY_ID_PART_NONE 0xa5

#endif

/*
 *  Double-precision floating-point number, IEEE 754
 *
 *  64 bit (8 bytes) in total
 *  1  bit sign
 *  11 bits exponent
 *  52 bits mantissa
 *      7         6        5        4        3        2        1        0
 *  seeeeeee|eeeemmmm|mmmmmmmm|mmmmmmmm|mmmmmmmm|mmmmmmmm|mmmmmmmm|mmmmmmmm
 *
 * If an exponent is all-1 and mantissa is all-0, then it is an INFINITY:
 *  11111111|11110000|00000000|00000000|00000000|00000000|00000000|00000000
 *
 * If an exponent is all-1 and mantissa's MSB is 1, it is a quiet NaN:
 *  11111111|11111000|00000000|00000000|00000000|00000000|00000000|00000000
 *
 *  V7 NaN-packing:
 *    sign and exponent is 0xfff
 *    4 bits specify type (tag), must be non-zero
 *    48 bits specify value
 *
 *  11111111|1111tttt|vvvvvvvv|vvvvvvvv|vvvvvvvv|vvvvvvvv|vvvvvvvv|vvvvvvvv
 *   NaN marker |type|  48-bit placeholder for values: pointers, strings
 *
 * On 64-bit platforms, pointers are really 48 bit only, so they can fit,
 * provided they are sign extended
 */

/*
 * A tag is made of the sign bit and the 4 lower order bits of byte 6.
 * So in total we have 32 possible tags.
 *
 * Tag (1,0) however cannot hold a zero payload otherwise it's interpreted as an
 * INFINITY; for simplicity we're just not going to use that combination.
 */
#define MAKE_TAG(s, t) \
  ((uint64_t)(s) << 63 | (uint64_t) 0x7ff0 << 48 | (uint64_t)(t) << 48)

#define V7_TAG_OBJECT MAKE_TAG(1, 0xF)
#define V7_TAG_FOREIGN MAKE_TAG(1, 0xE)
#define V7_TAG_UNDEFINED MAKE_TAG(1, 0xD)
#define V7_TAG_BOOLEAN MAKE_TAG(1, 0xC)
#define V7_TAG_NAN MAKE_TAG(1, 0xB)
#define V7_TAG_STRING_I MAKE_TAG(1, 0xA)  /* Inlined string len < 5 */
#define V7_TAG_STRING_5 MAKE_TAG(1, 0x9)  /* Inlined string len 5 */
#define V7_TAG_STRING_O MAKE_TAG(1, 0x8)  /* Owned string */
#define V7_TAG_STRING_F MAKE_TAG(1, 0x7)  /* Foreign string */
#define V7_TAG_STRING_C MAKE_TAG(1, 0x6)  /* String chunk */
#define V7_TAG_FUNCTION MAKE_TAG(1, 0x5)  /* JavaScript function */
#define V7_TAG_CFUNCTION MAKE_TAG(1, 0x4) /* C function */
#define V7_TAG_STRING_D MAKE_TAG(1, 0x3)  /* Dictionary string  */
#define V7_TAG_REGEXP MAKE_TAG(1, 0x2)    /* Regex */
#define V7_TAG_NOVALUE MAKE_TAG(1, 0x1)   /* Sentinel for no value */
#define V7_TAG_MASK MAKE_TAG(1, 0xF)

#define _V7_NULL V7_TAG_FOREIGN
#define _V7_UNDEFINED V7_TAG_UNDEFINED

V7_STATIC_ASSERT(_V7_NULL == V7_NULL, public_V7_NULL_is_wrong);
V7_STATIC_ASSERT(_V7_UNDEFINED == V7_UNDEFINED, public_V7_UNDEFINED_is_wrong);

/*
 * Object attributes bitmask
 */
typedef unsigned char v7_obj_attr_t;
#define V7_OBJ_NOT_EXTENSIBLE (1 << 0) /* TODO(lsm): store this in LSB */
#define V7_OBJ_DENSE_ARRAY (1 << 1)    /* TODO(mkm): store in some tag */
#define V7_OBJ_FUNCTION (1 << 2)       /* function object */
#define V7_OBJ_OFF_HEAP (1 << 3)       /* object not managed by V7 HEAP */
#define V7_OBJ_HAS_DESTRUCTOR (1 << 4) /* has user data */

/*
 * JavaScript value is either a primitive, or an object.
 * There are 5 primitive types: Undefined, Null, Boolean, Number, String.
 * Non-primitive type is an Object type. There are several classes of Objects,
 * see description of `struct v7_generic_object` below for more details.
 * This enumeration combines types and object classes in one enumeration.
 * NOTE(lsm): compile with `-fshort-enums` to reduce sizeof(enum v7_type) to 1.
 */
enum v7_type {
  /* Primitive types */
  V7_TYPE_UNDEFINED,
  V7_TYPE_NULL,
  V7_TYPE_BOOLEAN,
  V7_TYPE_NUMBER,
  V7_TYPE_STRING,
  V7_TYPE_FOREIGN,
  V7_TYPE_CFUNCTION,

  /* Different classes of Object type */
  V7_TYPE_GENERIC_OBJECT,
  V7_TYPE_BOOLEAN_OBJECT,
  V7_TYPE_STRING_OBJECT,
  V7_TYPE_NUMBER_OBJECT,
  V7_TYPE_FUNCTION_OBJECT,
  V7_TYPE_CFUNCTION_OBJECT,
  V7_TYPE_REGEXP_OBJECT,
  V7_TYPE_ARRAY_OBJECT,
  V7_TYPE_DATE_OBJECT,
  V7_TYPE_ERROR_OBJECT,
  V7_TYPE_MAX_OBJECT_TYPE,
  V7_NUM_TYPES
};

/*
 * Call frame type mask: we have a "class hierarchy" of the call frames, see
 * `struct v7_call_frame_base`, and the `type_mask` field represents the exact
 * frame type.
 *
 * Possible values are:
 *
 * - `V7_CALL_FRAME_MASK_PRIVATE | V7_CALL_FRAME_MASK_BCODE`: the most popular
 *   frame type: call frame for bcode execution, either top-level code or JS
 *   function.
 * - `V7_CALL_FRAME_MASK_PRIVATE`: used for `catch` clauses only: the variables
 *   we create in `catch` clause should not be visible from the outside of the
 *   clause, so we have to create a separate scope object for it.
 * - `V7_CALL_FRAME_MASK_CFUNC`: call frame for C function.
 */
typedef uint8_t v7_call_frame_mask_t;
#define V7_CALL_FRAME_MASK_BCODE (1 << 0)
#define V7_CALL_FRAME_MASK_PRIVATE (1 << 1)
#define V7_CALL_FRAME_MASK_CFUNC (1 << 2)

/*
 * Base of the call frame; includes the pointer to the previous frame,
 * and the frame type.
 *
 * In order to save memory, it also contains some bitfields which actually
 * belong to some "sub-structures".
 *
 * The hierarchy is as follows:
 *
 *   - v7_call_frame_base
 *     - v7_call_frame_private
 *       - v7_call_frame_bcode
 *     - v7_call_frame_cfunc
 */
struct v7_call_frame_base {
  struct v7_call_frame_base *prev;

  /* See comment for `v7_call_frame_mask_t` */
  v7_call_frame_mask_t type_mask : 3;

  /* Belongs to `struct v7_call_frame_private` */
  unsigned int line_no : 16;

  /* Belongs to `struct v7_call_frame_bcode` */
  unsigned is_constructor : 1;
};

/*
 * "private" call frame, used in `catch` blocks, merely for using a separate
 * scope object there. It is also a "base class" for the bcode call frame,
 * see `struct v7_call_frame_bcode`.
 *
 * TODO(dfrank): probably implement it differently, so that we can get rid of
 * the separate "private" frames whatsoever (and just include it into struct
 * v7_call_frame_bcode )
 */
struct v7_call_frame_private {
  struct v7_call_frame_base base;
  size_t stack_size;
  struct {
    /*
     * Current execution scope. Initially, it is equal to the `global_object`;
     * and at each function call, it is augmented by the new scope object, which
     * has the previous value as a prototype.
     */
    val_t scope;

    val_t try_stack;
  } vals;
};

/*
 * "bcode" call frame, augments "private" frame with `bcode` and the position
 * in it, and `this` object. It is the primary frame type, used when executing
 * a bcode script or calling a function.
 */
struct v7_call_frame_bcode {
  struct v7_call_frame_private base;
  struct {
    val_t this_obj;
  } vals;
  struct bcode *bcode;
  char *bcode_ops;
};

/*
 * "cfunc" call frame, used when calling cfunctions.
 */
struct v7_call_frame_cfunc {
  struct v7_call_frame_base base;

  struct {
    val_t this_obj;
  } vals;

  v7_cfunction_t *cfunc;
};

/*
 * This structure groups together all val_t logical members
 * of struct v7 so that GC and freeze logic can easily access all
 * of them together. This structure must contain only val_t members.
 */
struct v7_vals {
  val_t global_object;

  val_t arguments; /* arguments of current call */

  val_t object_prototype;
  val_t array_prototype;
  val_t boolean_prototype;
  val_t error_prototype;
  val_t string_prototype;
  val_t regexp_prototype;
  val_t number_prototype;
  val_t date_prototype;
  val_t function_prototype;

  /*
   * temporary register for `OP_STASH` and `OP_UNSTASH` instructions. Valid if
   * `v7->is_stashed` is non-zero
   */
  val_t stash;

  val_t error_objects[ERROR_CTOR_MAX];

  /*
   * Value that is being thrown. Valid if `is_thrown` is non-zero (see below)
   */
  val_t thrown_error;

  /*
   * value that is going to be returned. Needed when some `finally` block needs
   * to be executed after `return my_value;` was issued. Used in bcode.
   * See also `is_returned` below
   */
  val_t returned_value;

  val_t last_name[2]; /* used for error reporting */
  /* most recent OP_CHECK_CALL exceptions, to be thrown by OP_CALL|OP_NEW */
  val_t call_check_ex;
};

struct v7 {
  struct v7_vals vals;

  /*
   * Stack of call frames.
   *
   * Execution contexts are contained in two chains:
   * - Stack of call frames: to allow returning, throwing, and stack trace
   *   generation;
   * - In the lexical scope via their prototype chain (to allow variable
   *   lookup), see `struct v7_call_frame_private::scope`.
   *
   * Execution contexts should be allocated on heap, because they might not be
   * on a call stack but still referenced (closures).
   *
   * New call frame is created every time some top-level code is evaluated,
   * or some code is being `eval`-d, or some function is called, either JS
   * function or C function (although the call frame types are different for
   * JS functions and cfunctions, see `struct v7_call_frame_base` and its
   * sub-structures)
   *
   * When no code is being evaluated at the moment, `call_stack` is `NULL`.
   *
   * See comment for `struct v7_call_frame_base` for some more details.
   */
  struct v7_call_frame_base *call_stack;

  /*
   * Bcode executes until it reaches `bottom_call_frame`. When some top-level
   * or `eval`-d code starts execution, the `bottom_call_frame` is set to the
   * call frame which was just created for the execution.
   */
  struct v7_call_frame_base *bottom_call_frame;

  struct mbuf stack; /* value stack for bcode interpreter */

  struct mbuf owned_strings;   /* Sequence of (varint len, char data[]) */
  struct mbuf foreign_strings; /* Sequence of (varint len, char *data) */

  struct mbuf tmp_stack; /* Stack of val_t* elements, used as root set */
  int need_gc;           /* Set to true to trigger GC when safe */

  struct gc_arena generic_object_arena;
  struct gc_arena function_arena;
  struct gc_arena property_arena;
#if V7_ENABLE__Memory__stats
  size_t function_arena_ast_size;
  size_t bcode_ops_size;
  size_t bcode_lit_total_size;
  size_t bcode_lit_deser_size;
#endif
  struct mbuf owned_values; /* buffer for GC roots owned by C code */

  /*
   * Stack of the root bcodes being executed at the moment. Note that when some
   * regular JS function is called inside `eval_bcode()`, the function's bcode
   * is NOT added here. Buf if some cfunction is called, which in turn calls
   * `b_exec()` (or `b_apply()`) recursively, the new bcode is added to this
   * stack.
   */
  struct mbuf act_bcodes;

  char error_msg[80]; /* Exception message */

  struct mbuf json_visited_stack; /* Detecting cycle in to_json */

  /* Parser state */
  struct v7_pstate pstate; /* Parsing state */
  enum v7_tok cur_tok;     /* Current token */
  const char *tok;         /* Parsed terminal token (ident, number, string) */
  unsigned long tok_len;   /* Length of the parsed terminal token */
  size_t last_var_node;    /* Offset of last var node or function/script node */
  int after_newline;       /* True if the cur_tok starts a new line */
  double cur_tok_dbl;      /* When tokenizing, parser stores TOK_NUMBER here */

  /*
   * Current linenumber. Currently it is used by parser, compiler and bcode
   * evaluator.
   *
   * - Parser: it's the last line_no emitted to AST
   * - Compiler: it's the last line_no emitted to bcode
   */
  int line_no;

  /* singleton, pointer because of amalgamation */
  struct v7_property *cur_dense_prop;

  volatile int interrupted;
#ifdef V7_STACK_SIZE
  void *sp_limit;
  void *sp_lwm;
#endif

#if defined(V7_CYG_PROFILE_ON)
  /* linked list of v7 contexts, needed by cyg_profile hooks */
  struct v7 *next_v7;

#if defined(V7_ENABLE_STACK_TRACKING)
  /* linked list of stack tracking contexts */
  struct stack_track_ctx *stack_track_ctx;

  int stack_stat[V7_STACK_STATS_CNT];
#endif

#endif

#ifdef V7_MALLOC_GC
  struct mbuf malloc_trace;
#endif

/*
 * TODO(imax): remove V7_DISABLE_STR_ALLOC_SEQ knob after 2015/12/01 if there
 * are no issues.
 */
#ifndef V7_DISABLE_STR_ALLOC_SEQ
  uint16_t gc_next_asn; /* Next sequence number to use. */
  uint16_t gc_min_asn;  /* Minimal sequence number currently in use. */
#endif

#if defined(V7_TRACK_MAX_PARSER_STACK_SIZE)
  size_t parser_stack_data_max_size;
  size_t parser_stack_ret_max_size;
  size_t parser_stack_data_max_len;
  size_t parser_stack_ret_max_len;
#endif

#ifdef V7_FREEZE
  FILE *freeze_file;
#endif

  /*
   * true if exception is currently being created. Needed to avoid recursive
   * exception creation
   */
  unsigned int creating_exception : 1;
  /* while true, GC is inhibited */
  unsigned int inhibit_gc : 1;
  /* true if `thrown_error` is valid */
  unsigned int is_thrown : 1;
  /* true if `returned_value` is valid */
  unsigned int is_returned : 1;
  /* true if a finally block is executing while breaking */
  unsigned int is_breaking : 1;
  /* true when a continue OP is executed, reset by `OP_JMP_IF_CONTINUE` */
  unsigned int is_continuing : 1;
  /* true if some value is currently stashed (`v7->vals.stash`) */
  unsigned int is_stashed : 1;
  /* true if last emitted statement does not affect data stack */
  unsigned int is_stack_neutral : 1;
  /* true if precompiling; affects compiler bcode choices */
  unsigned int is_precompiling : 1;

  enum opcode last_ops[2]; /* trace of last ops, used for error reporting */
};

struct v7_property {
  struct v7_property *
      next; /* Linkage in struct v7_generic_object::properties */
  v7_prop_attr_t attributes;
#if defined(V7_ENABLE_ENTITY_IDS)
  entity_id_t entity_id;
#endif
  val_t name;  /* Property name (a string) */
  val_t value; /* Property value */
};

/*
 * "base object": structure which is shared between objects and functions.
 */
struct v7_object {
  /* First HIDDEN property in a chain is an internal object value */
  struct v7_property *properties;
  v7_obj_attr_t attributes;
#if defined(V7_ENABLE_ENTITY_IDS)
  entity_id_part_t entity_id_base;
  entity_id_part_t entity_id_spec;
#endif
};

/*
 * An object is an unordered collection of properties.
 * A function stored in a property of an object is called a method.
 * A property has a name, a value, and set of attributes.
 * Attributes are: ReadOnly, DontEnum, DontDelete, Internal.
 *
 * A constructor is a function that creates and initializes objects.
 * Each constructor has an associated prototype object that is used for
 * inheritance and shared properties. When a constructor creates an object,
 * the new object references the constructor’s prototype.
 *
 * Objects could be a "generic objects" which is a collection of properties,
 * or a "typed object" which also hold an internal value like String or Number.
 * Those values are implicit, unnamed properties of the respective types,
 * and can be coerced into primitive types by calling a respective constructor
 * as a function:
 *    var a = new Number(123);
 *    typeof(a) == 'object';
 *    typeof(Number(a)) == 'number';
 */
struct v7_generic_object {
  /*
   * This has to be the first field so that objects can be managed by the GC.
   */
  struct v7_object base;
  struct v7_object *prototype;
};

/*
 * Variables are function-scoped and are hoisted.
 * Lexical scoping & closures: each function has a chain of scopes, defined
 * by the lexicographic order of function definitions.
 * Scope is different from the execution context.
 * Execution context carries "variable object" which is variable/value
 * mapping for all variables defined in a function, and `this` object.
 * If function is not called as a method, then `this` is a global object.
 * Otherwise, `this` is an object that contains called method.
 * New execution context is created each time a function call is performed.
 * Passing arguments through recursion is done using execution context, e.g.
 *
 *    var factorial = function(num) {
 *      return num < 2 ? 1 : num * factorial(num - 1);
 *    };
 *
 * Here, recursion calls the same function `factorial` several times. Execution
 * contexts for each call form a stack. Each context has different variable
 * object, `vars`, with different values of `num`.
 */

struct v7_js_function {
  /*
   * Functions are objects. This has to be the first field so that function
   * objects can be managed by the GC.
   */
  struct v7_object base;
  struct v7_generic_object *scope; /* lexical scope of the closure */

  /* bytecode, might be shared between functions */
  struct bcode *bcode;
};

struct v7_regexp {
  val_t regexp_string;
  struct slre_prog *compiled_regexp;
  long lastIndex;
};

/* Vector, describes some memory location pointed by `p` with length `len` */
struct v7_vec {
  char *p;
  size_t len;
};

/*
 * Constant vector, describes some const memory location pointed by `p` with
 * length `len`
 */
struct v7_vec_const {
  const char *p;
  size_t len;
};

#define V7_VEC(str) \
  { (str), sizeof(str) - 1 }

/*
 * Returns current execution scope.
 *
 * See comment for `struct v7_call_frame_private::vals::scope`
 */
V7_PRIVATE v7_val_t get_scope(struct v7 *v7);

/*
 * Returns 1 if currently executing bcode in the "strict mode", 0 otherwise
 */
V7_PRIVATE uint8_t is_strict_mode(struct v7 *v7);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_CORE_H_ */
