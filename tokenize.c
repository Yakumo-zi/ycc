#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ycc.h"

static char *current_input;

void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}
static void verror_at(char *loc, char *fmt, va_list ap) {
  int pos = loc - current_input;
  fprintf(stderr, "%s\n", current_input);
  fprintf(stderr, "%*s^ ", pos, "");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(-1);
}
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(loc, fmt, ap);
}

void error_tok(Token *tok, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  error_at(tok->loc, fmt, ap);
}

static bool startswith(char *p, char *q) {
  return strncmp(p, q, strlen(q)) == 0;
}

static int read_punct(char *p) {
  if (startswith(p, "==") || startswith(p, ">=") || startswith(p, "<=") ||
      startswith(p, "!=")) {
    return 2;
  }
  return ispunct(*p) ? 1 : 0;
}

bool equal(Token *tok, char *op) {
  return memcmp(tok->loc, op, tok->len) == 0 && op[tok->len] == '\0';
}

Token *skip(Token *tok, char *s) {
  if (!equal(tok, s))
    error_tok(tok, "expected '%s' ", s);
  return tok->next;
}

static int get_number(Token *tok) {
  if (tok->kind != TK_NUM)
    error_tok(tok, "expected a number");
  return tok->val;
}

static Token *new_token(TokenKind kind, char *start, char *end) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->loc = start;
  tok->len = end - start;
  return tok;
}

static bool is_indent1(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool is_indent2(char c) {
  return is_indent1(c) || ('0' <= c && c <= '9');
}

static bool is_keyword(Token *tok) {
  static char *kw[] = {"return", "if", "else", "for", "while"};
  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
    if (equal(tok, kw[i]))
      return true;
  }
  return false;
}

static void convert_keywords(Token *tok) {
  for (Token *t = tok; t->kind != TK_EOF; t = t->next) {
    if (is_keyword(tok)) {
      t->kind = TK_KEYWORD;
    }
  }
}

Token *tokenize(char *p) {
  current_input = p;
  Token head = {};
  Token *cur = &head;
  while (*p) {
    if (isspace(*p)) {
      p++;
      continue;
    }
    if (isdigit(*p)) {
      cur = cur->next = new_token(TK_NUM, p, p);
      char *q = p;
      cur->val = strtoul(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    int punct_len = read_punct(p);
    if (punct_len) {
      cur = cur->next = new_token(TK_PUNCT, p, p + punct_len);
      p += cur->len;
      continue;
    }

    // Identifier or Keyword
    if (is_indent1(*p)) {
      char *start = p;
      do {
        p++;
      } while (is_indent2(*p));
      cur = cur->next = new_token(TK_IDENT, start, p);
      continue;
    }
    error_at(p, "invalid token");
  }
  cur = cur->next = new_token(TK_EOF, p, p);
  convert_keywords(head.next);
  return head.next;
}
