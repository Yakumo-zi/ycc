#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct Token Token;
typedef struct Node Node;
typedef struct Obj Obj;
typedef struct Function Function;
typedef struct Type Type;

typedef enum {
  TK_PUNCT,
  TK_NUM,
  TK_EOF,
  TK_IDENT,
  TK_KEYWORD,
  TK_STR,
} TokenKind;

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

struct Obj {
  Obj *next;
  char *name;
  Type *ty;
  int offset; // Offset from RBP
  bool is_local;

  // Global variable or function
  bool is_function;
  Obj *params;
  Node *body;
  Obj *locals;
  int stack_size;
  int val;
  char *init_data;
};

struct Token {
  TokenKind kind;
  Token *next;
  int val;
  char *loc;
  int len;
  Type *ty;
  char *str;
};
void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
bool equal(Token *tok, char *op);
bool consume(Token **rest, Token *tok, char *str);
Token *skip(Token *tok, char *op);

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
  Node *args;
};

typedef enum { TY_INT, TY_PTR, TY_FUNC, TY_ARRAY, TY_CHAR } TypeKind;

struct Type {
  TypeKind kind;

  // Pointer
  Type *base;

  // Declaration
  Token *name;

  Type *return_ty;
  Type *params;
  Type *next;

  int size;
  int array_len;
};

extern Type *ty_int;
extern Type *ty_char;

bool is_integer(Type *type);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *copy_type(Type *type);
Type *array_of(Type *base, int size);
void add_type(Node *node);

Token *tokenize(char *input);
Obj *parse(Token *tok);
void codegen(Obj *node);
