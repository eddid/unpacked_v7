/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "common/cs_strtod.h"
#include "common/utf.h"

#include "v7/src/internal.h"
#include "v7/src/core.h"

/*
 * NOTE(lsm): Must be in the same order as enum for keywords. See comment
 * for function get_tok() for rationale for that.
 */
static const struct v7_vec_const s_keywords[] = {
    V7_VEC("break"),      V7_VEC("case"),     V7_VEC("catch"),
    V7_VEC("continue"),   V7_VEC("debugger"), V7_VEC("default"),
    V7_VEC("delete"),     V7_VEC("do"),       V7_VEC("else"),
    V7_VEC("false"),      V7_VEC("finally"),  V7_VEC("for"),
    V7_VEC("function"),   V7_VEC("if"),       V7_VEC("in"),
    V7_VEC("instanceof"), V7_VEC("new"),      V7_VEC("null"),
    V7_VEC("return"),     V7_VEC("switch"),   V7_VEC("this"),
    V7_VEC("throw"),      V7_VEC("true"),     V7_VEC("try"),
    V7_VEC("typeof"),     V7_VEC("var"),      V7_VEC("void"),
    V7_VEC("while"),      V7_VEC("with")};

V7_PRIVATE int is_reserved_word_token(enum v7_tok tok) {
  return tok >= TOK_BREAK && tok <= TOK_WITH;
}

/*
 * Move ptr to the next token, skipping comments and whitespaces.
 * Return number of new line characters detected.
 */
V7_PRIVATE int skip_to_next_tok(const char **ptr, const char *src_end) {
  const char *s = *ptr, *p = NULL;
  int num_lines = 0;

  while (s != p && s < src_end && *s != '\0' &&
         (isspace((unsigned char) *s) || *s == '/')) {
    p = s;
    while (s < src_end && *s != '\0' && isspace((unsigned char) *s)) {
      if (*s == '\n') num_lines++;
      s++;
    }
    if ((s + 1) < src_end && s[0] == '/' && s[1] == '/') {
      s += 2;
      while (s < src_end && s[0] != '\0' && s[0] != '\n') s++;
    }
    if ((s + 1) < src_end && s[0] == '/' && s[1] == '*') {
      s += 2;
      while (s < src_end && s[0] != '\0' && !(s[-1] == '/' && s[-2] == '*')) {
        if (s[0] == '\n') num_lines++;
        s++;
      }
    }
  }
  *ptr = s;

  return num_lines;
}

/* Advance `s` pointer to the end of identifier  */
static void ident(const char **s, const char *src_end) {
  const unsigned char *p = (unsigned char *) *s;
  int n;
  Rune r;

  while ((const char *) p < src_end && p[0] != '\0') {
    if (p[0] == '$' || p[0] == '_' || isalnum(p[0])) {
      /* $, _, or any alphanumeric are valid identifier characters */
      p++;
    } else if ((const char *) (p + 5) < src_end && p[0] == '\\' &&
               p[1] == 'u' && isxdigit(p[2]) && isxdigit(p[3]) &&
               isxdigit(p[4]) && isxdigit(p[5])) {
      /* Unicode escape, \uXXXX . Could be used like "var \u0078 = 1;" */
      p += 6;
    } else if ((n = chartorune(&r, (char *) p)) > 1 && isalpharune(r)) {
      /*
       * TODO(dfrank): the chartorune() call above can read `p` past the
       * src_end, so it might crash on incorrect code. The solution would be
       * to modify `chartorune()` to accept src_end argument as well.
       */
      /* Unicode alphanumeric character */
      p += n;
    } else {
      break;
    }
  }

  *s = (char *) p;
}

static enum v7_tok kw(const char *s, size_t len, int ntoks, enum v7_tok tok) {
  int i;

  for (i = 0; i < ntoks; i++) {
    if (s_keywords[(tok - TOK_BREAK) + i].len == len &&
        memcmp(s_keywords[(tok - TOK_BREAK) + i].p + 1, s + 1, len - 1) == 0)
      break;
  }

  return i == ntoks ? TOK_IDENTIFIER : (enum v7_tok)(tok + i);
}

static enum v7_tok punct1(const char **s, const char *src_end, int ch1,
                          enum v7_tok tok1, enum v7_tok tok2) {
  (*s)++;
  if (*s < src_end && **s == ch1) {
    (*s)++;
    return tok1;
  } else {
    return tok2;
  }
}

static enum v7_tok punct2(const char **s, const char *src_end, int ch1,
                          enum v7_tok tok1, int ch2, enum v7_tok tok2,
                          enum v7_tok tok3) {
  if ((*s + 2) < src_end && s[0][1] == ch1 && s[0][2] == ch2) {
    (*s) += 3;
    return tok2;
  }

  return punct1(s, src_end, ch1, tok1, tok3);
}

static enum v7_tok punct3(const char **s, const char *src_end, int ch1,
                          enum v7_tok tok1, int ch2, enum v7_tok tok2,
                          enum v7_tok tok3) {
  (*s)++;
  if (*s < src_end) {
    if (**s == ch1) {
      (*s)++;
      return tok1;
    } else if (**s == ch2) {
      (*s)++;
      return tok2;
    }
  }
  return tok3;
}

static void parse_number(const char *s, const char **end, double *num) {
  *num = cs_strtod(s, (char **) end);
}

static enum v7_tok parse_str_literal(const char **p, const char *src_end) {
  const char *s = *p;
  int quote = '\0';

  if (s < src_end) {
    quote = *s++;
  }

  /* Scan string literal, handle escape sequences */
  for (; s < src_end && *s != '\0' && *s != quote; s++) {
    if (*s == '\\') {
      switch (s[1]) {
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
        case 'v':
        case '\\':
          s++;
          break;
        default:
          if (s[1] == quote) s++;
          break;
      }
    }
  }

  if (s < src_end && *s == quote) {
    s++;
    *p = s;
    return TOK_STRING_LITERAL;
  } else {
    return TOK_END_OF_INPUT;
  }
}

/*
 * This function is the heart of the tokenizer.
 * Organized as a giant switch statement.
 *
 * Switch statement is by the first character of the input stream. If first
 * character begins with a letter, it could be either keyword or identifier.
 * get_tok() calls ident() which shifts `s` pointer to the end of the word.
 * Now, tokenizer knows that the word begins at `p` and ends at `s`.
 * It calls function kw() to scan over the keywords that start with `p[0]`
 * letter. Therefore, keyword tokens and keyword strings must be in the
 * same order, to let kw() function work properly.
 * If kw() finds a keyword match, it returns keyword token.
 * Otherwise, it returns TOK_IDENTIFIER.
 * NOTE(lsm): `prev_tok` is a previously parsed token. It is needed for
 * correctly parsing regex literals.
 */
V7_PRIVATE enum v7_tok get_tok(const char **s, const char *src_end, double *n,
                               enum v7_tok prev_tok) {
  const char *p = *s;

  if (p >= src_end) {
    return TOK_END_OF_INPUT;
  }

  switch (*p) {
    /* Letters */
    case 'a':
      ident(s, src_end);
      return TOK_IDENTIFIER;
    case 'b':
      ident(s, src_end);
      return kw(p, *s - p, 1, TOK_BREAK);
    case 'c':
      ident(s, src_end);
      return kw(p, *s - p, 3, TOK_CASE);
    case 'd':
      ident(s, src_end);
      return kw(p, *s - p, 4, TOK_DEBUGGER);
    case 'e':
      ident(s, src_end);
      return kw(p, *s - p, 1, TOK_ELSE);
    case 'f':
      ident(s, src_end);
      return kw(p, *s - p, 4, TOK_FALSE);
    case 'g':
    case 'h':
      ident(s, src_end);
      return TOK_IDENTIFIER;
    case 'i':
      ident(s, src_end);
      return kw(p, *s - p, 3, TOK_IF);
    case 'j':
    case 'k':
    case 'l':
    case 'm':
      ident(s, src_end);
      return TOK_IDENTIFIER;
    case 'n':
      ident(s, src_end);
      return kw(p, *s - p, 2, TOK_NEW);
    case 'o':
    case 'p':
    case 'q':
      ident(s, src_end);
      return TOK_IDENTIFIER;
    case 'r':
      ident(s, src_end);
      return kw(p, *s - p, 1, TOK_RETURN);
    case 's':
      ident(s, src_end);
      return kw(p, *s - p, 1, TOK_SWITCH);
    case 't':
      ident(s, src_end);
      return kw(p, *s - p, 5, TOK_THIS);
    case 'u':
      ident(s, src_end);
      return TOK_IDENTIFIER;
    case 'v':
      ident(s, src_end);
      return kw(p, *s - p, 2, TOK_VAR);
    case 'w':
      ident(s, src_end);
      return kw(p, *s - p, 2, TOK_WHILE);
    case 'x':
    case 'y':
    case 'z':
      ident(s, src_end);
      return TOK_IDENTIFIER;

    case '_':
    case '$':
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'H':
    case 'I':
    case 'J':
    case 'K':
    case 'L':
    case 'M':
    case 'N':
    case 'O':
    case 'P':
    case 'Q':
    case 'R':
    case 'S':
    case 'T':
    case 'U':
    case 'V':
    case 'W':
    case 'X':
    case 'Y':
    case 'Z':
    case '\\': /* Identifier may start with unicode escape sequence */
      ident(s, src_end);
      return TOK_IDENTIFIER;

    /* Numbers */
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      parse_number(p, s, n);
      return TOK_NUMBER;

    /* String literals */
    case '\'':
    case '"':
      return parse_str_literal(s, src_end);

    /* Punctuators */
    case '=':
      return punct2(s, src_end, '=', TOK_EQ, '=', TOK_EQ_EQ, TOK_ASSIGN);
    case '!':
      return punct2(s, src_end, '=', TOK_NE, '=', TOK_NE_NE, TOK_NOT);

    case '%':
      return punct1(s, src_end, '=', TOK_REM_ASSIGN, TOK_REM);
    case '*':
      return punct1(s, src_end, '=', TOK_MUL_ASSIGN, TOK_MUL);
    case '/':
      /*
       * TOK_DIV, TOK_DIV_ASSIGN, and TOK_REGEX_LITERAL start with `/` char.
       * Division can happen after an expression.
       * In expressions like this:
       *            a /= b; c /= d;
       * things between slashes is NOT a regex literal.
       * The switch below catches all cases where division happens.
       */
      switch (prev_tok) {
        case TOK_CLOSE_CURLY:
        case TOK_CLOSE_PAREN:
        case TOK_CLOSE_BRACKET:
        case TOK_IDENTIFIER:
        case TOK_NUMBER:
          return punct1(s, src_end, '=', TOK_DIV_ASSIGN, TOK_DIV);
        default:
          /* Not a division - this is a regex. Scan until closing slash */
          for (p++; p < src_end && *p != '\0' && *p != '\n'; p++) {
            if (*p == '\\') {
              /* Skip escape sequence */
              p++;
            } else if (*p == '/') {
              /* This is a closing slash */
              p++;
              /* Skip regex flags */
              while (*p == 'g' || *p == 'i' || *p == 'm') {
                p++;
              }
              *s = p;
              return TOK_REGEX_LITERAL;
            }
          }
          break;
      }
      return punct1(s, src_end, '=', TOK_DIV_ASSIGN, TOK_DIV);
    case '^':
      return punct1(s, src_end, '=', TOK_XOR_ASSIGN, TOK_XOR);

    case '+':
      return punct3(s, src_end, '+', TOK_PLUS_PLUS, '=', TOK_PLUS_ASSIGN,
                    TOK_PLUS);
    case '-':
      return punct3(s, src_end, '-', TOK_MINUS_MINUS, '=', TOK_MINUS_ASSIGN,
                    TOK_MINUS);
    case '&':
      return punct3(s, src_end, '&', TOK_LOGICAL_AND, '=', TOK_AND_ASSIGN,
                    TOK_AND);
    case '|':
      return punct3(s, src_end, '|', TOK_LOGICAL_OR, '=', TOK_OR_ASSIGN,
                    TOK_OR);

    case '<':
      if (*s + 1 < src_end && s[0][1] == '=') {
        (*s) += 2;
        return TOK_LE;
      }
      return punct2(s, src_end, '<', TOK_LSHIFT, '=', TOK_LSHIFT_ASSIGN,
                    TOK_LT);
    case '>':
      if (*s + 1 < src_end && s[0][1] == '=') {
        (*s) += 2;
        return TOK_GE;
      }
      if (*s + 3 < src_end && s[0][1] == '>' && s[0][2] == '>' &&
          s[0][3] == '=') {
        (*s) += 4;
        return TOK_URSHIFT_ASSIGN;
      }
      if (*s + 2 < src_end && s[0][1] == '>' && s[0][2] == '>') {
        (*s) += 3;
        return TOK_URSHIFT;
      }
      return punct2(s, src_end, '>', TOK_RSHIFT, '=', TOK_RSHIFT_ASSIGN,
                    TOK_GT);

    case '{':
      (*s)++;
      return TOK_OPEN_CURLY;
    case '}':
      (*s)++;
      return TOK_CLOSE_CURLY;
    case '(':
      (*s)++;
      return TOK_OPEN_PAREN;
    case ')':
      (*s)++;
      return TOK_CLOSE_PAREN;
    case '[':
      (*s)++;
      return TOK_OPEN_BRACKET;
    case ']':
      (*s)++;
      return TOK_CLOSE_BRACKET;
    case '.':
      switch (*(*s + 1)) {
        /* Numbers */
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          parse_number(p, s, n);
          return TOK_NUMBER;
      }
      (*s)++;
      return TOK_DOT;
    case ';':
      (*s)++;
      return TOK_SEMICOLON;
    case ':':
      (*s)++;
      return TOK_COLON;
    case '?':
      (*s)++;
      return TOK_QUESTION;
    case '~':
      (*s)++;
      return TOK_TILDA;
    case ',':
      (*s)++;
      return TOK_COMMA;

    default: {
      /* Handle unicode variables */
      Rune r;
      if (chartorune(&r, *s) > 1 && isalpharune(r)) {
        ident(s, src_end);
        return TOK_IDENTIFIER;
      }
      return TOK_END_OF_INPUT;
    }
  }
}

#ifdef TEST_RUN
int main(void) {
  const char *src =
      "for (var fo++ = -1; /= <= 1.17; x<<) { == <<=, 'x')} "
      "Infinity %=x<<=2";
  const char *src_end = src + strlen(src);
  enum v7_tok tok;
  double num;
  const char *p = src;

  skip_to_next_tok(&src, src_end);
  while ((tok = get_tok(&src, src_end, &num)) != TOK_END_OF_INPUT) {
    printf("%d [%.*s]\n", tok, (int) (src - p), p);
    skip_to_next_tok(&src, src_end);
    p = src;
  }
  printf("%d [%.*s]\n", tok, (int) (src - p), p);

  return 0;
}
#endif
