#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef enum { TK_PUNCT, TK_NUM, TK_EOF, TK_IDENT, TK_KEYWORD } TokenKind;
typedef struct Token Token;
typedef struct Node Node;
typedef struct Obj Obj;
typedef struct Function Function;
typedef struct Type Type;

struct Obj {
  Obj *next;
  char *name;
  Type *ty;
  int offset; // Offset from RBP
};

struct Function {
  Node *body;
  Obj *locals;
  int stack_size;
};

struct Token {
  TokenKind kind;
  Token *next;
  int val;
  char *loc;
  int len;
};
void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
bool equal(Token *tok, char *op);
bool consume(Token **rest, Token *tok, char *str);
Token *skip(Token *tok, char *op);

typedef enum {
  ND_ADD,
  ND_SUB,
  ND_MUL,
  ND_DIV,
  ND_NUM,
  ND_NEG,
  ND_EQ,
  ND_NE,
  ND_LT,
  ND_LE,
  ND_EXPR_STMT,
  ND_ASSIGN,
  ND_VAR,
  ND_BLOCK,
  ND_RETURN,
  ND_IF,
  ND_FOR, // for or while
  ND_ADDR,
  ND_DEREF,
  ND_FUNCALL,
} NodeKind;

// AST Node
struct Node {
  NodeKind kind;
  Node *next;
  Node *lhs;
  Node *rhs;
  int val;
  Obj *var;
  Type *ty;

  // if statement
  Node *cond;
  Node *then;
  Node *els;

  // for statement
  Node *init;
  Node *inc;

  Token *tok;
  // block
  Node *body;

  char *funcname;
};

typedef enum {
  TY_INT,
  TY_PTR,
} TypeKind;

struct Type {
  TypeKind kind;

  // Pointer
  Type *base;

  // Declaration
  Token *name;
};

extern Type *ty_int;
bool is_integer(Type *type);
Type *pointer_to(Type *base);
void add_type(Node *node);

Token *tokenize(char *input);
Function *parse(Token *tok);
void codegen(Function *node);
