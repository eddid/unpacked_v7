/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/cs_strtod.h"
#include "common/mbuf.h"

#include "v7/src/internal.h"
#include "v7/src/varint.h"
#include "v7/src/ast.h"
#include "v7/src/core.h"
#include "v7/src/string.h"
#include "common/str_util.h"

#ifdef V7_LARGE_AST
typedef uint32_t ast_skip_t;
#else
typedef uint16_t ast_skip_t;
#define AST_SKIP_MAX UINT16_MAX
#endif

#ifndef V7_DISABLE_AST_TAG_NAMES
#define AST_ENTRY(name, has_varint, has_inlined, num_skips, num_subtrees) \
  { (name), (has_varint), (has_inlined), (num_skips), (num_subtrees) }
#else
#define AST_ENTRY(name, has_varint, has_inlined, num_skips, num_subtrees) \
  { (has_varint), (has_inlined), (num_skips), (num_subtrees) }
#endif

/*
 * The structure of AST nodes cannot be described in portable ANSI C,
 * since they are variable length and packed (unaligned).
 *
 * Here each node's body is described with a pseudo-C structure notation.
 * The pseudo type `child` represents a variable length byte sequence
 * representing a fully serialized child node.
 *
 * `child body[]` represents a sequence of such subtrees.
 *
 * Pseudo-labels, such as `end:` represent the targets of skip fields
 * with the same name (e.g. `ast_skip_t end`).
 *
 * Skips allow skipping a subtree or sequence of subtrees.
 *
 * Sequences of subtrees (i.e. `child []`) have to be terminated by a skip:
 * they don't have a termination tag; all nodes whose position is before the
 * skip are part of the sequence.
 *
 * Skips are encoded as network-byte-order 16-bit offsets counted from the
 * first byte of the node body (i.e. not counting the tag itself).
 * This currently limits the the maximum size of a function body to 64k.
 *
 * Notes:
 *
 * - Some nodes contain skips just for performance or because it simplifies
 * the implementation of the interpreter. For example, technically, the FOR
 * node doesn't need the `body` skip in order to be correctly traversed.
 * However, being able to quickly skip the `iter` expression is useful
 * also because it allows the interpreter to avoid traversing the expression
 * subtree without evaluating it, just in order to find the next subtree.
 *
 * - The name `skip` was chosen because `offset` was too overloaded in general
 * and label` is part of our domain model (i.e. JS has a label AST node type).
 *
 *
 * So, each node has a mandatory field: *tag* (see `enum ast_tag`), and a
 * number of optional fields. Whether the node has one or another optional
 * field is determined by the *node descriptor*: `struct ast_node_def`. For
 * each node type (i.e. for each element of `enum ast_tag`) there is a
 * corresponding descriptor: see `ast_node_defs`.
 *
 * Optional fields are:
 *
 * - *varint*: a varint-encoded number. At the moment, this field is only used
 *   together with the next field: inlined data, and a varint number determines
 *   the inlined data length.
 * - *inlined data*: a node-specific data. Size of it is determined by the
 *   previous field: varint.
 * - *skips*: as explained above, these are integer offsets, encoded in
 *   big-endian. The number of skips is determined by the node descriptor
 *   (`struct ast_node_def`). The size of each skip is either 16 or 32 bits,
 *   depending on whether the macro `V7_LARGE_AST` is set. The order of skips
 *   is determined by the `enum ast_which_skip`. See examples below for
 *   clarity.
 * - *subtrees*: child nodes. Some nodes have fixed number of child nodes; in
 *   this case, the descriptor has non-zero field `num_subtrees`.  Otherwise,
 *   `num_subtrees` is zero, and consumer handles child nodes one by one, until
 *   the end of the node is reached (end of the node is determined by the `end`
 *   skip)
 *
 *
 * Examples:
 *
 * Let's start from the very easy example script: "300;"
 *
 * Tree looks as follows:
 *
 *    $ ./v7 -e "300;" -t
 *      SCRIPT
 *        /- [...] -/
 *        NUM 300
 *
 * Binary data is:
 *
 *    $ ./v7 -e "300;" -b | od -A n -t x1
 *    56 07 41 53 54 56 31 30 00 01 00 09 00 00 13 03
 *    33 30 30
 *
 * Let's break it down and examine:
 *
 *    - 56 07 41 53 54 56 31 30 00
 *        Just a format prefix:
 *        Null-terminated string: `"V\007ASTV10"` (see `BIN_AST_SIGNATURE`)
 *    - 01
 *        AST tag: `AST_SCRIPT`. As you see in `ast_node_defs` below, node of
 *        this type has neither *varint* nor *inlined data* fields, but it has
 *        2 skips: `end` and `next`. `end` is a skip to the end of the current
 *        node (`SCRIPT`), and `next` will be explained below.
 *
 *        The size of each skip depends on whether `V7_LARGE_AST` is defined.
 *        If it is, then size is 32 bit, otherwise it's 16 bit. In this
 *        example, we have 16-bit skips.
 *
 *        The order of skips is determined by the `enum ast_which_skip`. If you
 *        check, you'll see that `AST_END_SKIP` is 0, and `AST_VAR_NEXT_SKIP`
 *        is 1. So, `end` skip fill be the first, and `next` will be the second:
 *    - 00 09
 *        `end` skip: 9 bytes. It's the size of the whole `SCRIPT` data. So, if
 *        we have an index of the `ASC_SCRIPT` tag, we can just add this skip
 *        (9) to this index, and therefore skip over the whole node.
 *    - 00 00
 *        `next` skip. `next` actually means "next variable node": since
 *        variables are hoisted in JavaScript, when the interpreter starts
 *        executing a top-level code or any function, it needs to get a list of
 *        all defined variables. The `SCRIPT` node has a "skip" to the first
 *        `var` or `function` declaration, which, in turn, has a "skip" to the
 *        next one, etc. If there is no next `var` declaration, then 0 is
 *        stored.
 *
 *        In our super-simple script, we have no `var` neither `function`
 *        declarations, so, this skip is 0.
 *
 *        Now, the body of our SCRIPT node goes, which contains child nodes:
 *
 *    - 13
 *        AST tag: `AST_NUM`. Look at the `ast_node_defs`, and we'll see that
 *        nodes of this type don't have any skips, but they do have the varint
 *        field and the inlined data. Here we go:
 *    - 03
 *        Varint value: 3
 *    - 33 30 30
 *        UTF-8 string "300"
 *
 * ---------------
 *
 * The next example is a bit more interesting:
 *
 *    var foo,
 *        bar = 1;
 *    foo = 3;
 *    var baz = 4;
 *
 * Tree:
 *
 *    $ ./v7 -e 'var foo, bar=1; foo=3; var baz = 4;' -t
 *    SCRIPT
 *      /- [...] -/
 *      VAR
 *        /- [...] -/
 *        VAR_DECL foo
 *          NOP
 *        VAR_DECL bar
 *          NUM 1
 *      ASSIGN
 *        IDENT foo
 *        NUM 3
 *      VAR
 *        /- [...] -/
 *        VAR_DECL baz
 *          NUM 4
 *
 * Binary:
 *
 *    $ ./v7 -e 'var foo, bar=1; foo=3; var baz = 4;' -b | od -A n -t x1
 *    56 07 41 53 54 56 31 30 00 01 00 2d 00 05 02 00
 *    12 00 1c 03 03 66 6f 6f 00 03 03 62 61 72 13 01
 *    31 07 14 03 66 6f 6f 13 01 33 02 00 0c 00 00 03
 *    03 62 61 7a 13 01 34
 *
 * Break it down:
 *
 *    - 56 07 41 53 54 56 31 30 00
 *        `"V\007ASTV10"`
 *    - 01:       AST tag: `AST_SCRIPT`
 *    - 00 2d:    `end` skip: 0x2d = 45 bytes
 *    - 00 05:    `next` skip: an offset from `AST_SCRIPT` byte to the first
 *                `var` declaration.
 *
 *        Now, body of the SCRIPT node begins, which contains child nodes,
 *        and the first node is the var declaration `var foo, bar=1;`:
 *
 *        SCRIPT node body: {{{
 *    - 02:       AST tag: `AST_VAR`
 *    - 00 12:    `end` skip: 18 bytes from tag byte to the end of current node
 *    - 00 1c:    `next` skip: 28 bytes from tag byte to the next `var` node
 *
 *        The VAR node contains arbitrary number of child nodes, so, consumer
 *        takes advantage of the `end` skip.
 *
 *        VAR node body: {{{
 *    - 03:       AST tag: `AST_VAR_DECL`
 *    - 03:       Varint value: 3 (the length of the inlined data: a variable
 *name)
 *    - 66 6f 6f: UTF-8 string: "foo"
 *    - 00:       AST tag: `AST_NOP`
 *                Since we haven't provided any value to store into `foo`, NOP
 *                without any additional data is stored in AST.
 *
 *    - 03:       AST tag: `AST_VAR_DECL`
 *    - 03:       Varint value: 3 (the length of the inlined data: a variable
 *name)
 *    - 62 61 72: UTF-8 string: "bar"
 *    - 13:       AST tag: `AST_NUM`
 *    - 01:       Varint value: 1
 *    - 31:       UTF-8 string "1"
 *        VAR body end }}}
 *
 *    - 07:       AST tag: `AST_ASSIGN`
 *
 *        The ASSIGN node has fixed number of subrees: 2 (lvalue and rvalue),
 *        so there's no `end` skip.
 *
 *        ASSIGN node body: {{{
 *    - 14:       AST tag: `AST_IDENT`
 *    - 03:       Varint value: 3
 *    - 66 6f 6f: UTF-8 string: "foo"
 *
 *    - 13:       AST tag: `AST_NUM`
 *    - 01:       Varint value: 1
 *    - 33:       UTF-8 string: "3"
 *        ASSIGN body end }}}
 *
 *    - 02:       AST tag: `AST_VAR`
 *    - 00 0c:    `end` skip: 12 bytes from tag byte to the end of current node
 *    - 00 00:    `next` skip: no more `var` nodes
 *
 *        VAR node body: {{{
 *    - 03:       AST tag: `AST_VAR_DECL`
 *    - 03:       Varint value: 3 (the length of the inlined data: a variable
 *name)
 *    - 62 61 7a: UTF-8 string: "baz"
 *    - 13:       AST tag: `AST_NUM`
 *    - 01:       Varint value: 1
 *    - 34:       UTF-8 string "4"
 *        VAR body end }}}
 *        SCRIPT body end }}}
 *
 * --------------------------
 */

const struct ast_node_def ast_node_defs[] = {
    AST_ENTRY("NOP", 0, 0, 0, 0), /* struct {} */

    /*
     * struct {
     *   ast_skip_t end;
     *   ast_skip_t first_var;
     *   child body[];
     * end:
     * }
     */
    AST_ENTRY("SCRIPT", 0, 0, 2, 0),
    /*
     * struct {
     *   ast_skip_t end;
     *   ast_skip_t next;
     *   child decls[];
     * end:
     * }
     */
    AST_ENTRY("VAR", 0, 0, 2, 0),
    /*
     * struct {
     *   varint len;
     *   char name[len];
     *   child expr;
     * }
     */
    AST_ENTRY("VAR_DECL", 1, 1, 0, 1),
    /*
     * struct {
     *   varint len;
     *   char name[len];
     *   child expr;
     * }
     */
    AST_ENTRY("FUNC_DECL", 1, 1, 0, 1),
    /*
     * struct {
     *   ast_skip_t end;
     *   ast_skip_t end_true;
     *   child cond;
     *   child iftrue[];
     * end_true:
     *   child iffalse[];
     * end:
     * }
     */
    AST_ENTRY("IF", 0, 0, 2, 1),
    /*
     * TODO(mkm) distinguish function expressions
     * from function statements.
     * Function statements behave like vars and need a
     * next field for hoisting.
     * We can also ignore the name for function expressions
     * if it's only needed for debugging.
     *
     * struct {
     *   ast_skip_t end;
     *   ast_skip_t first_var;
     *   ast_skip_t body;
     *   child name;
     *   child params[];
     * body:
     *   child body[];
     * end:
     * }
     */
    AST_ENTRY("FUNC", 0, 0, 3, 1),
    AST_ENTRY("ASSIGN", 0, 0, 0, 2),         /* struct { child left, right; } */
    AST_ENTRY("REM_ASSIGN", 0, 0, 0, 2),     /* struct { child left, right; } */
    AST_ENTRY("MUL_ASSIGN", 0, 0, 0, 2),     /* struct { child left, right; } */
    AST_ENTRY("DIV_ASSIGN", 0, 0, 0, 2),     /* struct { child left, right; } */
    AST_ENTRY("XOR_ASSIGN", 0, 0, 0, 2),     /* struct { child left, right; } */
    AST_ENTRY("PLUS_ASSIGN", 0, 0, 0, 2),    /* struct { child left, right; } */
    AST_ENTRY("MINUS_ASSIGN", 0, 0, 0, 2),   /* struct { child left, right; } */
    AST_ENTRY("OR_ASSIGN", 0, 0, 0, 2),      /* struct { child left, right; } */
    AST_ENTRY("AND_ASSIGN", 0, 0, 0, 2),     /* struct { child left, right; } */
    AST_ENTRY("LSHIFT_ASSIGN", 0, 0, 0, 2),  /* struct { child left, right; } */
    AST_ENTRY("RSHIFT_ASSIGN", 0, 0, 0, 2),  /* struct { child left, right; } */
    AST_ENTRY("URSHIFT_ASSIGN", 0, 0, 0, 2), /* struct { child left, right; } */
    AST_ENTRY("NUM", 1, 1, 0, 0),    /* struct { varint len, char s[len]; } */
    AST_ENTRY("IDENT", 1, 1, 0, 0),  /* struct { varint len, char s[len]; } */
    AST_ENTRY("STRING", 1, 1, 0, 0), /* struct { varint len, char s[len]; } */
    AST_ENTRY("REGEX", 1, 1, 0, 0),  /* struct { varint len, char s[len]; } */
    AST_ENTRY("LABEL", 1, 1, 0, 0),  /* struct { varint len, char s[len]; } */

    /*
     * struct {
     *   ast_skip_t end;
     *   child body[];
     * end:
     * }
     */
    AST_ENTRY("SEQ", 0, 0, 1, 0),
    /*
     * struct {
     *   ast_skip_t end;
     *   child cond;
     *   child body[];
     * end:
     * }
     */
    AST_ENTRY("WHILE", 0, 0, 1, 1),
    /*
     * struct {
     *   ast_skip_t end;
     *   ast_skip_t cond;
     *   child body[];
     * cond:
     *   child cond;
     * end:
     * }
     */
    AST_ENTRY("DOWHILE", 0, 0, 2, 0),
    /*
     * struct {
     *   ast_skip_t end;
     *   ast_skip_t body;
     *   child init;
     *   child cond;
     *   child iter;
     * body:
     *   child body[];
     * end:
     * }
     */
    AST_ENTRY("FOR", 0, 0, 2, 3),
    /*
     * struct {
     *   ast_skip_t end;
     *   ast_skip_t dummy; // allows to quickly promote a for to a for in
     *   child var;
     *   child expr;
     *   child dummy;
     *   child body[];
     * end:
     * }
     */
    AST_ENTRY("FOR_IN", 0, 0, 2, 3),
    AST_ENTRY("COND", 0, 0, 0, 3), /* struct { child cond, iftrue, iffalse; } */
    AST_ENTRY("DEBUGGER", 0, 0, 0, 0), /* struct {} */
    AST_ENTRY("BREAK", 0, 0, 0, 0),    /* struct {} */

    /*
     * struct {
     *   child label; // TODO(mkm): inline
     * }
     */
    AST_ENTRY("LAB_BREAK", 0, 0, 0, 1),
    AST_ENTRY("CONTINUE", 0, 0, 0, 0), /* struct {} */

    /*
     * struct {
     *   child label; // TODO(mkm): inline
     * }
     */
    AST_ENTRY("LAB_CONTINUE", 0, 0, 0, 1),
    AST_ENTRY("RETURN", 0, 0, 0, 0),     /* struct {} */
    AST_ENTRY("VAL_RETURN", 0, 0, 0, 1), /* struct { child expr; } */
    AST_ENTRY("THROW", 0, 0, 0, 1),      /* struct { child expr; } */

    /*
     * struct {
     *   ast_skip_t end;
     *   ast_skip_t catch;
     *   ast_skip_t finally;
     *   child try[];
     * catch:
     *   child var; // TODO(mkm): inline
     *   child catch[];
     * finally:
     *   child finally[];
     * end:
     * }
     */
    AST_ENTRY("TRY", 0, 0, 3, 1),
    /*
     * struct {
     *   ast_skip_t end;
     *   ast_skip_t def;
     *   child expr;
     *   child cases[];
     * def:
     *   child default?; // optional
     * end:
     * }
     */
    AST_ENTRY("SWITCH", 0, 0, 2, 1),
    /*
     * struct {
     *   ast_skip_t end;
     *   child val;
     *   child stmts[];
     * end:
     * }
     */
    AST_ENTRY("CASE", 0, 0, 1, 1),
    /*
     * struct {
     *   ast_skip_t end;
     *   child stmts[];
     * end:
     * }
     */
    AST_ENTRY("DEFAULT", 0, 0, 1, 0),
    /*
     * struct {
     *   ast_skip_t end;
     *   child expr;
     *   child body[];
     * end:
     * }
     */
    AST_ENTRY("WITH", 0, 0, 1, 1),
    AST_ENTRY("LOG_OR", 0, 0, 0, 2),      /* struct { child left, right; } */
    AST_ENTRY("LOG_AND", 0, 0, 0, 2),     /* struct { child left, right; } */
    AST_ENTRY("OR", 0, 0, 0, 2),          /* struct { child left, right; } */
    AST_ENTRY("XOR", 0, 0, 0, 2),         /* struct { child left, right; } */
    AST_ENTRY("AND", 0, 0, 0, 2),         /* struct { child left, right; } */
    AST_ENTRY("EQ", 0, 0, 0, 2),          /* struct { child left, right; } */
    AST_ENTRY("EQ_EQ", 0, 0, 0, 2),       /* struct { child left, right; } */
    AST_ENTRY("NE", 0, 0, 0, 2),          /* struct { child left, right; } */
    AST_ENTRY("NE_NE", 0, 0, 0, 2),       /* struct { child left, right; } */
    AST_ENTRY("LE", 0, 0, 0, 2),          /* struct { child left, right; } */
    AST_ENTRY("LT", 0, 0, 0, 2),          /* struct { child left, right; } */
    AST_ENTRY("GE", 0, 0, 0, 2),          /* struct { child left, right; } */
    AST_ENTRY("GT", 0, 0, 0, 2),          /* struct { child left, right; } */
    AST_ENTRY("IN", 0, 0, 0, 2),          /* struct { child left, right; } */
    AST_ENTRY("INSTANCEOF", 0, 0, 0, 2),  /* struct { child left, right; } */
    AST_ENTRY("LSHIFT", 0, 0, 0, 2),      /* struct { child left, right; } */
    AST_ENTRY("RSHIFT", 0, 0, 0, 2),      /* struct { child left, right; } */
    AST_ENTRY("URSHIFT", 0, 0, 0, 2),     /* struct { child left, right; } */
    AST_ENTRY("ADD", 0, 0, 0, 2),         /* struct { child left, right; } */
    AST_ENTRY("SUB", 0, 0, 0, 2),         /* struct { child left, right; } */
    AST_ENTRY("REM", 0, 0, 0, 2),         /* struct { child left, right; } */
    AST_ENTRY("MUL", 0, 0, 0, 2),         /* struct { child left, right; } */
    AST_ENTRY("DIV", 0, 0, 0, 2),         /* struct { child left, right; } */
    AST_ENTRY("POS", 0, 0, 0, 1),         /* struct { child expr; } */
    AST_ENTRY("NEG", 0, 0, 0, 1),         /* struct { child expr; } */
    AST_ENTRY("NOT", 0, 0, 0, 1),         /* struct { child expr; } */
    AST_ENTRY("LOGICAL_NOT", 0, 0, 0, 1), /* struct { child expr; } */
    AST_ENTRY("VOID", 0, 0, 0, 1),        /* struct { child expr; } */
    AST_ENTRY("DELETE", 0, 0, 0, 1),      /* struct { child expr; } */
    AST_ENTRY("TYPEOF", 0, 0, 0, 1),      /* struct { child expr; } */
    AST_ENTRY("PREINC", 0, 0, 0, 1),      /* struct { child expr; } */
    AST_ENTRY("PREDEC", 0, 0, 0, 1),      /* struct { child expr; } */
    AST_ENTRY("POSTINC", 0, 0, 0, 1),     /* struct { child expr; } */
    AST_ENTRY("POSTDEC", 0, 0, 0, 1),     /* struct { child expr; } */

    /*
     * struct {
     *   varint len;
     *   char ident[len];
     *   child expr;
     * }
     */
    AST_ENTRY("MEMBER", 1, 1, 0, 1),
    /*
     * struct {
     *   child expr;
     *   child index;
     * }
     */
    AST_ENTRY("INDEX", 0, 0, 0, 2),
    /*
     * struct {
     *   ast_skip_t end;
     *   child expr;
     *   child args[];
     * end:
     * }
     */
    AST_ENTRY("CALL", 0, 0, 1, 1),
    /*
     * struct {
     *   ast_skip_t end;
     *   child expr;
     *   child args[];
     * end:
     * }
     */
    AST_ENTRY("NEW", 0, 0, 1, 1),
    /*
     * struct {
     *   ast_skip_t end;
     *   child elements[];
     * end:
     * }
     */
    AST_ENTRY("ARRAY", 0, 0, 1, 0),
    /*
     * struct {
     *   ast_skip_t end;
     *   child props[];
     * end:
     * }
     */
    AST_ENTRY("OBJECT", 0, 0, 1, 0),
    /*
     * struct {
     *   varint len;
     *   char name[len];
     *   child expr;
     * }
     */
    AST_ENTRY("PROP", 1, 1, 0, 1),
    /*
     * struct {
     *   child func;
     * }
     */
    AST_ENTRY("GETTER", 0, 0, 0, 1),
    /*
     * struct {
     *   child func;
     * end:
     * }
     */
    AST_ENTRY("SETTER", 0, 0, 0, 1),
    AST_ENTRY("THIS", 0, 0, 0, 0),       /* struct {} */
    AST_ENTRY("TRUE", 0, 0, 0, 0),       /* struct {} */
    AST_ENTRY("FALSE", 0, 0, 0, 0),      /* struct {} */
    AST_ENTRY("NULL", 0, 0, 0, 0),       /* struct {} */
    AST_ENTRY("UNDEF", 0, 0, 0, 0),      /* struct {} */
    AST_ENTRY("USE_STRICT", 0, 0, 0, 0), /* struct {} */
};

/*
 * A flag which is used to mark node's tag byte if the node has line number
 * data encoded (varint after skips). See `ast_get_line_no()`.
 */
#define AST_TAG_LINENO_PRESENT 0x80

V7_STATIC_ASSERT(AST_MAX_TAG < 256, ast_tag_should_fit_in_char);
V7_STATIC_ASSERT(AST_MAX_TAG == ARRAY_SIZE(ast_node_defs), bad_node_defs);
V7_STATIC_ASSERT(AST_MAX_TAG <= AST_TAG_LINENO_PRESENT, bad_AST_LINE_NO);

#if V7_ENABLE_FOOTPRINT_REPORT
const size_t ast_node_defs_size = sizeof(ast_node_defs);
const size_t ast_node_defs_count = ARRAY_SIZE(ast_node_defs);
#endif

/*
 * Converts a given byte `t` (which should be read from the AST data buffer)
 * into `enum ast_tag`. This function is needed because tag might be marked
 * with the `AST_TAG_LINENO_PRESENT` flag; the returned tag is always unmarked,
 * and if the flag was indeed set, `lineno_present` is set to 1; otherwise
 * it is set to 0.
 *
 * `lineno_present` is allowed to be NULL, if the caller doesn't care of the
 * line number presence.
 */
static enum ast_tag uint8_to_tag(uint8_t t, uint8_t *lineno_present) {
  if (t & AST_TAG_LINENO_PRESENT) {
    t &= ~AST_TAG_LINENO_PRESENT;
    if (lineno_present != NULL) {
      *lineno_present = 1;
    }
  } else if (lineno_present != NULL) {
    *lineno_present = 0;
  }
  return (enum ast_tag) t;
}

V7_PRIVATE ast_off_t
ast_insert_node(struct ast *a, ast_off_t pos, enum ast_tag tag) {
  uint8_t t = (uint8_t) tag;
  const struct ast_node_def *d = &ast_node_defs[tag];
  ast_off_t cur = pos;

  assert(tag < AST_MAX_TAG);

  mbuf_insert(&a->mbuf, cur, (char *) &t, sizeof(t));
  cur += sizeof(t);

  mbuf_insert(&a->mbuf, cur, NULL, sizeof(ast_skip_t) * d->num_skips);
  cur += sizeof(ast_skip_t) * d->num_skips;

  if (d->num_skips) {
    ast_set_skip(a, pos + 1, AST_END_SKIP);
  }

  return pos + 1;
}

V7_PRIVATE void ast_modify_tag(struct ast *a, ast_off_t tag_off,
                               enum ast_tag tag) {
  a->mbuf.buf[tag_off] = tag | (a->mbuf.buf[tag_off] & 0x80);
}

#ifndef V7_DISABLE_LINE_NUMBERS
V7_PRIVATE void ast_add_line_no(struct ast *a, ast_off_t tag_off, int line_no) {
  ast_off_t ln_off = tag_off + 1 /* tag byte */;
  int llen = calc_llen(line_no);

  ast_move_to_inlined_data(a, &ln_off);
  mbuf_insert(&a->mbuf, ln_off, NULL, llen);
  encode_varint(line_no, (unsigned char *) (a->mbuf.buf + ln_off));

  assert(a->mbuf.buf[tag_off] < AST_MAX_TAG);
  a->mbuf.buf[tag_off] |= AST_TAG_LINENO_PRESENT;
}
#endif

V7_PRIVATE ast_off_t
ast_set_skip(struct ast *a, ast_off_t pos, enum ast_which_skip skip) {
  return ast_modify_skip(a, pos, a->mbuf.len, skip);
}

V7_PRIVATE ast_off_t ast_modify_skip(struct ast *a, ast_off_t pos,
                                     ast_off_t where,
                                     enum ast_which_skip skip) {
  uint8_t *p = (uint8_t *) a->mbuf.buf + pos + skip * sizeof(ast_skip_t);
  ast_skip_t delta = where - pos;
#ifndef NDEBUG
  enum ast_tag tag = uint8_to_tag(*(a->mbuf.buf + pos - 1), NULL);
  const struct ast_node_def *def = &ast_node_defs[tag];
#endif
  assert(pos <= where);

#ifndef V7_LARGE_AST
  /* the value of delta overflowed, therefore the ast is not useable */
  if (where - pos > AST_SKIP_MAX) {
    a->has_overflow = 1;
  }
#endif

  /* assertion, to be optimizable out */
  assert((int) skip < def->num_skips);

#ifdef V7_LARGE_AST
  p[0] = delta >> 24;
  p[1] = delta >> 16 & 0xff;
  p[2] = delta >> 8 & 0xff;
  p[3] = delta & 0xff;
#else
  p[0] = delta >> 8;
  p[1] = delta & 0xff;
#endif
  return where;
}

V7_PRIVATE ast_off_t
ast_get_skip(struct ast *a, ast_off_t pos, enum ast_which_skip skip) {
  uint8_t *p;
  assert(pos + skip * sizeof(ast_skip_t) < a->mbuf.len);

  p = (uint8_t *) a->mbuf.buf + pos + skip * sizeof(ast_skip_t);
#ifdef V7_LARGE_AST
  return pos + (p[3] | p[2] << 8 | p[1] << 16 | p[0] << 24);
#else
  return pos + (p[1] | p[0] << 8);
#endif
}

V7_PRIVATE enum ast_tag ast_fetch_tag(struct ast *a, ast_off_t *ppos) {
  enum ast_tag ret;
  assert(*ppos < a->mbuf.len);

  ret = uint8_to_tag(*(a->mbuf.buf + (*ppos)++), NULL);

  return ret;
}

V7_PRIVATE void ast_move_to_children(struct ast *a, ast_off_t *ppos) {
  enum ast_tag tag = uint8_to_tag(*(a->mbuf.buf + *ppos - 1), NULL);
  const struct ast_node_def *def = &ast_node_defs[tag];
  assert(*ppos - 1 < a->mbuf.len);

  ast_move_to_inlined_data(a, ppos);

  /* skip varint + inline data, if present */
  if (def->has_varint) {
    int llen;
    size_t slen = decode_varint((unsigned char *) a->mbuf.buf + *ppos, &llen);
    *ppos += llen;
    if (def->has_inlined) {
      *ppos += slen;
    }
  }
}

V7_PRIVATE ast_off_t ast_insert_inlined_node(struct ast *a, ast_off_t pos,
                                             enum ast_tag tag, const char *name,
                                             size_t len) {
  const struct ast_node_def *d = &ast_node_defs[tag];

  ast_off_t offset = ast_insert_node(a, pos, tag);

  assert(d->has_inlined);

  embed_string(&a->mbuf, offset + sizeof(ast_skip_t) * d->num_skips, name, len,
               EMBSTR_UNESCAPE);

  return offset;
}

V7_PRIVATE int ast_get_line_no(struct ast *a, ast_off_t pos) {
  /*
   * by default we'll return 0, meaning that the AST node does not contain line
   * number data
   */
  int ret = 0;

#ifndef V7_DISABLE_LINE_NUMBERS
  uint8_t lineno_present;
  enum ast_tag tag = uint8_to_tag(*(a->mbuf.buf + pos - 1), &lineno_present);

  if (lineno_present) {
    /* line number is present, so, let's decode it */
    int llen;

    /* skip skips */
    pos += ast_node_defs[tag].num_skips * sizeof(ast_skip_t);

    /* get line number */
    ret = decode_varint((unsigned char *) a->mbuf.buf + pos, &llen);
  }
#else
  (void) a;
  (void) pos;
#endif

  return ret;
}

V7_PRIVATE void ast_move_to_inlined_data(struct ast *a, ast_off_t *ppos) {
  uint8_t lineno_present = 0;
  enum ast_tag tag = uint8_to_tag(*(a->mbuf.buf + *ppos - 1), &lineno_present);
  const struct ast_node_def *def = &ast_node_defs[tag];
  assert(*ppos - 1 < a->mbuf.len);

  /* skip skips */
  *ppos += def->num_skips * sizeof(ast_skip_t);

  /* skip line_no, if present */
  if (lineno_present) {
    int llen;
    int line_no = decode_varint((unsigned char *) a->mbuf.buf + *ppos, &llen);
    *ppos += llen;

    (void) line_no;
  }
}

V7_PRIVATE char *ast_get_inlined_data(struct ast *a, ast_off_t pos, size_t *n) {
  int llen;
  assert(pos < a->mbuf.len);

  ast_move_to_inlined_data(a, &pos);

  *n = decode_varint((unsigned char *) a->mbuf.buf + pos, &llen);
  return a->mbuf.buf + pos + llen;
}

V7_PRIVATE double ast_get_num(struct ast *a, ast_off_t pos) {
  double ret;
  char *str;
  size_t str_len;
  char buf[12];
  char *p = buf;
  str = ast_get_inlined_data(a, pos, &str_len);
  assert(str + str_len <= a->mbuf.buf + a->mbuf.len);

  if (str_len > sizeof(buf) - 1) {
    p = (char *) malloc(str_len + 1);
  }
  strncpy(p, str, str_len);
  p[str_len] = '\0';
  ret = cs_strtod(p, NULL);
  if (p != buf) free(p);
  return ret;
}

#ifndef NO_LIBC
static void comment_at_depth(FILE *fp, const char *fmt, int depth, ...) {
  int i;
  STATIC char buf[256];
  va_list ap;
  va_start(ap, depth);

  c_vsnprintf(buf, sizeof(buf), fmt, ap);

  for (i = 0; i < depth; i++) {
    fprintf(fp, "  ");
  }
  fprintf(fp, "/* [%s] */\n", buf);
}
#endif

V7_PRIVATE void ast_skip_tree(struct ast *a, ast_off_t *ppos) {
  enum ast_tag tag = ast_fetch_tag(a, ppos);
  const struct ast_node_def *def = &ast_node_defs[tag];
  ast_off_t skips = *ppos;
  int i;
  ast_move_to_children(a, ppos);

  for (i = 0; i < def->num_subtrees; i++) {
    ast_skip_tree(a, ppos);
  }

  if (def->num_skips > AST_END_SKIP) {
    ast_off_t end = ast_get_skip(a, skips, AST_END_SKIP);

    while (*ppos < end) {
      ast_skip_tree(a, ppos);
    }
  }
}

#ifndef NO_LIBC
V7_PRIVATE void ast_dump_tree(FILE *fp, struct ast *a, ast_off_t *ppos,
                              int depth) {
  enum ast_tag tag = ast_fetch_tag(a, ppos);
  const struct ast_node_def *def = &ast_node_defs[tag];
  ast_off_t skips = *ppos;
  size_t slen;
  int i, llen;

  for (i = 0; i < depth; i++) {
    fprintf(fp, "  ");
  }

#ifndef V7_DISABLE_AST_TAG_NAMES
  fprintf(fp, "%s", def->name);
#else
  fprintf(fp, "TAG_%d", tag);
#endif

  if (def->has_inlined) {
    ast_off_t pos_tmp = *ppos;
    ast_move_to_inlined_data(a, &pos_tmp);

    slen = decode_varint((unsigned char *) a->mbuf.buf + pos_tmp, &llen);
    fprintf(fp, " %.*s\n", (int) slen, a->mbuf.buf + pos_tmp + llen);
  } else {
    fprintf(fp, "\n");
  }

  ast_move_to_children(a, ppos);

  for (i = 0; i < def->num_subtrees; i++) {
    ast_dump_tree(fp, a, ppos, depth + 1);
  }

  if (ast_node_defs[tag].num_skips) {
    /*
     * first skip always encodes end of the last children sequence.
     * so unless we care how the subtree sequences are grouped together
     * (and we currently don't) we can just read until the end of that skip.
     */
    ast_off_t end = ast_get_skip(a, skips, AST_END_SKIP);

    comment_at_depth(fp, "...", depth + 1);
    while (*ppos < end) {
      int s;
      for (s = ast_node_defs[tag].num_skips - 1; s > 0; s--) {
        if (*ppos == ast_get_skip(a, skips, (enum ast_which_skip) s)) {
          comment_at_depth(fp, "%d ->", depth + 1, s);
          break;
        }
      }
      ast_dump_tree(fp, a, ppos, depth + 1);
    }
  }
}
#endif

V7_PRIVATE void ast_init(struct ast *ast, size_t len) {
  mbuf_init(&ast->mbuf, len);
  ast->refcnt = 0;
  ast->has_overflow = 0;
}

V7_PRIVATE void ast_optimize(struct ast *ast) {
  /*
   * leave one trailing byte so that literals can be
   * null terminated on the fly.
   */
  mbuf_resize(&ast->mbuf, ast->mbuf.len + 1);
}

V7_PRIVATE void ast_free(struct ast *ast) {
  mbuf_free(&ast->mbuf);
  ast->refcnt = 0;
  ast->has_overflow = 0;
}

V7_PRIVATE void release_ast(struct v7 *v7, struct ast *a) {
  (void) v7;

  if (a->refcnt != 0) a->refcnt--;

  if (a->refcnt == 0) {
#if V7_ENABLE__Memory__stats
    v7->function_arena_ast_size -= a->mbuf.size;
#endif
    ast_free(a);
    free(a);
  }
}
