#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
typedef enum { TK_PUNCT, TK_NUM, TK_EOF, TK_IDENT, TK_KEYWORD } TokenKind;
typedef struct Token Token;
typedef struct Node Node;
typedef struct Obj Obj;
typedef struct Function Function;

struct Obj {
  Obj *next;
  char *name;
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
Token *skip(Token *tok, char *op);
Token *tokenize(char *input);

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
  ND_FOR,
} NodeKind;

// AST Node
struct Node {
  NodeKind kind;
  Node *next;
  Node *lhs;
  Node *rhs;
  int val;
  Obj *var;

  // if statement
  Node *cond;
  Node *then;
  Node *els;

  // for statement
  Node *init;
  Node *inc;

  Node *body;
};

Function *parse(Token *tok);

void codegen(Function *node);
