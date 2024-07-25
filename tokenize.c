#include "ycc.h"
#include <ctype.h>
#include <string.h>

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
  static char *kw[] = {"return", "if",  "else",   "for",
                       "while",  "int", "sizeof", "char"};
  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
    if (equal(tok, kw[i]))
      return true;
  }
  return false;
}

bool consume(Token **rest, Token *tok, char *str) {
  if (equal(tok, str)) {
    *rest = tok->next;
    return true;
  }
  *rest = tok;
  return false;
}

static void convert_keywords(Token *tok) {
  for (Token *t = tok; t->kind != TK_EOF; t = t->next) {
    if (is_keyword(t)) {
      t->kind = TK_KEYWORD;
    }
  }
}

static int from_hex(char c) {
  if ('0' <= c && c <= '9') {
    return c - '0';
  }
  if ('a' <= c && c <= 'f') {
    return c - 'a' + 10;
  }
  return c - 'A' + 10;
}

static int read_escaped_char(char **new_pos, char *p) {
  if ('0' <= *p && *p <= '7') {
    int c = *p++ - '0';
    if ('0' <= *p && *p <= '7') {
      c = (c << 3) + (*p++ - '0');
      if ('0' <= *p && *p <= '7')
        c = (c << 3) + (*p++ - '0');
    }
    *new_pos = p;
    return c;
  }
  if (*p == 'x') {
    p++;
    if (!isxdigit(*p)) {
      error_at(p, "invalid hex escape sequence");
    }
    int c = 0;
    for (; isxdigit(*p); p++) {
      c = (c << 4) + from_hex(*p);
      *new_pos = p;
    }
    return c;
  }

  *new_pos = p + 1;
  switch (*p) {
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 't':
    return '\t';
  case 'n':
    return '\n';
  case 'v':
    return '\v';
  case 'f':
    return '\f';
  case 'r':
    return '\r';
  case 'e':
    return 27;
  default:
    return *p;
  }
}
static char *string_literal_end(char *p) {
  char *start = p + 1;
  for (; *p != '"'; p++) {
    if (*p == '\n' || *p == '\0') {
      error_at(start, "unclosed string literal");
    }
    if (*p == '\\') {
      p++;
    }
  }
  return p;
}

static Token *read_string_literal(char *start) {
  char *end = string_literal_end(start + 1);
  char *buf = calloc(1, end - start);
  int len = 0;
  char *p = start + 1;
  for (char *p = start + 1; p < end;) {
    if (*p == '\\') {
      buf[len++] = read_escaped_char(&p, p + 1);
    } else {
      buf[len++] = *p++;
    }
  }
  Token *tok = new_token(TK_STR, start, end + 1);
  tok->ty = array_of(ty_char, len + 1);
  tok->str = buf;
  return tok;
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

    if (*p == '"') {
      cur = cur->next = read_string_literal(p);
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

    int punct_len = read_punct(p);
    if (punct_len) {
      cur = cur->next = new_token(TK_PUNCT, p, p + punct_len);
      p += cur->len;
      continue;
    }
    error_at(p, "invalid token");
  }
  cur = cur->next = new_token(TK_EOF, p, p);
  convert_keywords(head.next);
  return head.next;
}
