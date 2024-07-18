#include <stdbool.h>
typedef enum { TK_PUNCT, TK_NUM, TK_EOF } TokenKind;

typedef struct Token Token;
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
} NodeKind;

// AST Node
typedef struct Node Node;
struct Node {
  NodeKind kind;
  Node *next;
  Node *lhs;
  Node *rhs;
  int val;
};

Node *parse(Token *tok);

void codegen(Node *node);
