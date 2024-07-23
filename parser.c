#include "ycc.h"
#include <stdio.h>
#include <string.h>

static Obj *locals;
static Obj *globals;

static Node *new_node(NodeKind kind, Token *tok) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_num(int val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  return node;
}
static Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = expr;
  return node;
}

static Obj *find_var(Token *tok) {

  for (Obj *var = locals; var; var = var->next) {
    if (strlen(var->name) == tok->len &&
        !strncmp(tok->loc, var->name, tok->len))
      return var;
  }
  for (Obj *var = globals; var; var = var->next) {
    if (strlen(var->name) == tok->len &&
        !strncmp(tok->loc, var->name, tok->len))
      return var;
  }
  return NULL;
}
static Obj *new_var(char *name, Type *ty) {
  Obj *var = calloc(1, sizeof(Obj));
  var->name = name;
  var->ty = ty;
  return var;
}
static Obj *new_lvar(char *name, Type *ty) {
  Obj *var = new_var(name, ty);
  var->is_local = true;
  var->next = locals;
  locals = var;
  return var;
}

static Obj *new_gvar(char *name, Type *ty) {
  Obj *var = new_var(name, ty);
  var->next = globals;
  globals = var;
  return var;
}

static Node *new_var_node(Obj *var, Token *tok) {
  Node *node = new_node(ND_VAR, tok);
  node->var = var;
  return node;
}

static char *new_unique_name() {
  static int id = 0;
  return format(".L..%d", id++);
}

static Obj *new_anon_gvar(Type *ty) { return new_gvar(new_unique_name(), ty); }

static Obj *new_string_literal(char *p, Type *ty) {
  Obj *var = new_anon_gvar(ty);
  var->init_data = p;
  return var;
}

static Node *new_add(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    return new_binary(ND_ADD, lhs, rhs, tok);
  }
  if (lhs->ty->base && rhs->ty->base) {
    error_tok(tok, "invalid operands");
  }

  // 标准化 num+ptr 到 ptr+num
  if (!lhs->ty->base && rhs->ty->base) {
    Node *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  // ptr +num
  rhs = new_binary(ND_MUL, rhs, new_num(lhs->ty->base->size, tok), tok);
  return new_binary(ND_ADD, lhs, rhs, tok);
}

static Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);

  // num-num
  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    return new_binary(ND_SUB, lhs, rhs, tok);
  }

  // ptr-num
  if (lhs->ty->base && is_integer(rhs->ty)) {
    rhs = new_binary(ND_MUL, rhs, new_num(lhs->ty->base->size, tok), tok);
    add_type(rhs);
    Node *node = new_binary(ND_SUB, lhs, rhs, tok);
    node->ty = lhs->ty;
    return node;
  }

  // ptr-ptr
  if (lhs->ty->base && rhs->ty->base) {
    Node *node = new_binary(ND_SUB, lhs, rhs, tok);
    node->ty = ty_int;
    return new_binary(ND_DIV, node, new_num(lhs->ty->base->size, tok), tok);
  }
  error_tok(tok, "invalid operands");
}

static char *get_ident(Token *tok) {
  if (tok->kind != TK_IDENT) {
    error_tok(tok, "expected an identifier");
  }
  return strndup(tok->loc, tok->len);
}

static int get_number(Token *tok) {
  if (tok->kind != TK_NUM) {
    error_tok(tok, "expected a number");
  }
  return tok->val;
}

static bool is_typename(Token *tok) {
  return equal(tok, "char") || equal(tok, "int");
}

// stmt="return" expr ";"
//      | expr-stmt
//      | {" compound_stmt
//      | "if" "(" expr ")" stmt ("else" stmt)
//      | "for" "("expr-stmt expr-stmt expr?")" stmt
//      | "while" "(" expr ")" stmt
// declspec = "int"
// type-suffix=("("func-params)?
// declarator = "*"* ident type-suffix
// declaration=declspec(declarator("=" expr)? ("," declarator("="expr)?)*)?";"
// compound_stmt = (stmt | declaration )*"}"
// expr-stmt=expr? ";"
// expr=assign
// assign= equality("=" assign)
// equality=relational ("==" relational | "!=" relational)
// relational=add("<" add | "<=" add | ">" add | ">=" add)
// add=mul("+" mul | "-" mul)
// mul=unary("*" unary | "/" unary)
// postfix=primar ("[" expr"]")*
// unary=("+"|"-" |"&" | "*") unary | postfix
// funcall=ident "(" (assign ("," assign)* )?")"
// primary ="(" expr ")" | num | ident args?
// args="("")"

static Type *declspec(Token **rest, Token *tok);
static Type *declarator(Token **rest, Token *tok, Type *ty);
static Node *declaration(Token **rest, Token *tok);
static Node *compound_stmt(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *postfix(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

// func-params=(param ("," param)*)?")"
// param=declspec declarator
static Type *func_params(Token **rest, Token *tok, Type *ty) {
  Type head = {};
  Type *cur = &head;
  while (!equal(tok, ")")) {
    if (cur != &head) {
      tok = skip(tok, ",");
    }
    Type *basety = declspec(&tok, tok);
    Type *ty = declarator(&tok, tok, basety);
    cur = cur->next = copy_type(ty);
  }
  ty = func_type(ty);
  ty->params = head.next;
  *rest = tok->next;
  return ty;
}

// type-suffix="(" func-params | "[" num "]"  type-suffix | ε
static Type *type_suffix(Token **rest, Token *tok, Type *ty) {
  if (equal(tok, "(")) {
    return func_params(rest, tok->next, ty);
  }

  if (equal(tok, "[")) {
    int sz = get_number(tok->next);
    tok = skip(tok->next->next, "]");
    ty = type_suffix(rest, tok, ty);
    return array_of(ty, sz);
  }
  *rest = tok;
  return ty;
}

// declspec = "int" |"char"
static Type *declspec(Token **rest, Token *tok) {
  if (equal(tok, "char")) {
    *rest = tok->next;
    return ty_char;
  }
  *rest = skip(tok, "int");
  return ty_int;
}

// declarator = "*"* ident type-suffix
static Type *declarator(Token **rest, Token *tok, Type *ty) {
  while (consume(&tok, tok, "*")) {
    ty = pointer_to(ty);
  }
  if (tok->kind != TK_IDENT) {
    error_tok(tok, "expected an identifier");
  }
  ty = type_suffix(rest, tok->next, ty);
  ty->name = tok;
  return ty;
}

// declaration=declspec(declarator("=" expr)? ("," declarator("="expr)?)*)?";"
static Node *declaration(Token **rest, Token *tok) {
  Type *basety = declspec(&tok, tok);
  Node head = {};
  Node *cur = &head;
  int i = 0;
  while (!equal(tok, ";")) {
    if (i++ > 0) {
      tok = skip(tok, ",");
    }
    Type *ty = declarator(&tok, tok, basety);
    Obj *var = new_lvar(get_ident(ty->name), ty);
    if (!equal(tok, "=")) {
      continue;
    }

    Node *lhs = new_var_node(var, ty->name);
    Node *rhs = assign(&tok, tok->next);
    Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
    cur = cur->next = new_unary(ND_EXPR_STMT, node, tok);
  }
  Node *node = new_node(ND_BLOCK, tok);
  node->body = head.next;
  *rest = tok->next;
  return node;
}

// stmt="return" expr ";"
//      | expr-stmt
//      | {" compound_stmt
//      | "if" "(" expr ")" stmt ("else" stmt)
//      | "for" "("expr-stmt expr-stmt expr?")" stmt
static Node *stmt(Token **rest, Token *tok) {
  if (equal(tok, "return")) {
    Node *node = new_unary(ND_RETURN, expr(&tok, tok->next), tok);
    *rest = skip(tok, ";");
    return node;
  }
  if (equal(tok, "{")) {
    return compound_stmt(rest, tok->next);
  }
  if (equal(tok, "if")) {
    Node *node = new_node(ND_IF, tok);
    tok = skip(tok->next, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");
    node->then = stmt(&tok, tok);
    if (equal(tok, "else")) {
      node->els = stmt(&tok, tok->next);
    }
    *rest = tok;
    return node;
  }
  if (equal(tok, "for")) {
    Node *node = new_node(ND_FOR, tok);
    tok = skip(tok->next, "(");
    node->init = expr_stmt(&tok, tok);

    if (!equal(tok, ";"))
      node->cond = expr(&tok, tok);
    tok = skip(tok, ";");

    if (!equal(tok, ")"))
      node->inc = expr(&tok, tok);
    tok = skip(tok, ")");

    node->then = stmt(&tok, tok);
    *rest = tok;
    return node;
  }
  if (equal(tok, "while")) {
    Node *node = new_node(ND_FOR, tok);
    tok = skip(tok->next, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");
    node->then = stmt(&tok, tok);
    *rest = tok;
    return node;
  }
  return expr_stmt(rest, tok);
}

// compound_stmt = stmt* "}"
static Node *compound_stmt(Token **rest, Token *tok) {
  Node head = {};
  Node *cur = &head;
  while (!equal(tok, "}")) {
    if (is_typename(tok)) {
      cur = cur->next = declaration(&tok, tok);
    } else {
      cur = cur->next = stmt(&tok, tok);
    }
    add_type(cur);
  }
  Node *node = new_node(ND_BLOCK, tok);
  node->body = head.next;
  // skip "}"
  *rest = tok->next;
  return node;
}

// expr-stmt=expr? ";"
static Node *expr_stmt(Token **rest, Token *tok) {
  if (equal(tok, ";")) {
    *rest = tok->next;
    return new_node(ND_BLOCK, tok);
  }
  Node *node = new_unary(ND_EXPR_STMT, expr(&tok, tok), tok);
  *rest = skip(tok, ";");
  return node;
}

// expr=equality
static Node *expr(Token **rest, Token *tok) { return assign(rest, tok); }

// assign=equality("=" assign)
static Node *assign(Token **rest, Token *tok) {
  Node *node = equality(&tok, tok);
  while (true) {
    if (equal(tok, "=")) {
      node = new_binary(ND_ASSIGN, node, assign(&tok, tok->next), tok);
      continue;
    }
    *rest = tok;
    return node;
  }
}
// equality = relational("==" realational | "!=" relational)
static Node *equality(Token **rest, Token *tok) {
  Node *node = relational(&tok, tok);
  while (true) {
    Token *start = tok;
    if (equal(tok, "==")) {
      node = new_binary(ND_EQ, node, relational(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, "!=")) {
      node = new_binary(ND_NE, node, relational(&tok, tok->next), start);
      continue;
    }
    *rest = tok;
    return node;
  }
}

// relational=add("<" add | "<=" add | ">" add | ">=" add)
static Node *relational(Token **rest, Token *tok) {
  Node *node = add(&tok, tok);
  while (true) {
    Token *start = tok;
    if (equal(tok, "<")) {
      node = new_binary(ND_LT, node, add(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, "<=")) {
      node = new_binary(ND_LE, node, add(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, ">")) {
      node = new_binary(ND_LT, add(&tok, tok->next), node, start);
      continue;
    }
    if (equal(tok, ">=")) {
      node = new_binary(ND_LE, add(&tok, tok->next), node, start);
      continue;
    }
    *rest = tok;
    return node;
  }
}

// add=mul("+" mul|"-"mul)
static Node *add(Token **rest, Token *tok) {
  Node *node = mul(&tok, tok);
  while (true) {
    Token *start;
    if (equal(tok, "+")) {
      node = new_add(node, mul(&tok, tok->next), tok);
      continue;
    }
    if (equal(tok, "-")) {
      node = new_sub(node, mul(&tok, tok->next), tok);
      continue;
    }
    *rest = tok;
    return node;
  }
}

// mul=primary("*" primary | "/" primary)
static Node *mul(Token **rest, Token *tok) {
  Node *node = unary(&tok, tok);
  while (true) {
    Token *start = tok;
    if (equal(tok, "*")) {
      node = new_binary(ND_MUL, node, unary(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, "/")) {
      node = new_binary(ND_DIV, node, unary(&tok, tok->next), start);
      continue;
    }
    *rest = tok;
    return node;
  }
}
// unary= ("+"|”-" | "&" | "*") unary | postfix
static Node *unary(Token **rest, Token *tok) {
  if (equal(tok, "+")) {
    return unary(rest, tok->next);
  }
  if (equal(tok, "-")) {
    return new_unary(ND_NEG, unary(rest, tok->next), tok);
  }
  if (equal(tok, "&")) {
    return new_unary(ND_ADDR, unary(rest, tok->next), tok);
  }
  if (equal(tok, "*")) {
    return new_unary(ND_DEREF, unary(rest, tok->next), tok);
  }
  return postfix(rest, tok);
}

// postfix=primar ("[" expr"]")*
static Node *postfix(Token **rest, Token *tok) {
  Node *node = primary(&tok, tok);
  while (equal(tok, "[")) {
    Token *start = tok;
    Node *idx = expr(&tok, tok->next);

    tok = skip(tok, "]");
    node = new_unary(ND_DEREF, new_add(node, idx, start), start);
  }
  *rest = tok;
  return node;
}

// funcall=ident "(" (assign ("," assign)* )?")"
static Node *funcall(Token **rest, Token *tok) {
  Token *start = tok;
  tok = tok->next->next;
  Node head = {};
  Node *cur = &head;
  while (!equal(tok, ")")) {
    if (cur != &head) {
      tok = skip(tok, ",");
    }
    cur = cur->next = assign(&tok, tok);
  }
  *rest = skip(tok, ")");
  Node *node = new_node(ND_FUNCALL, start);
  node->funcname = strndup(start->loc, start->len);
  node->args = head.next;
  return node;
}

// primary ="(" expr ")" | num | ident args?  |num | str
// args="("")"
static Node *primary(Token **rest, Token *tok) {
  if (equal(tok, "(")) {
    Node *node = expr(&tok, tok->next);
    *rest = skip(tok, ")");
    return node;
  }
  if (tok->kind == TK_NUM) {
    Node *node = new_num(tok->val, tok);
    *rest = tok->next;
    return node;
  }
  if (tok->kind == TK_IDENT) {
    if (equal(tok->next, "(")) {
      return funcall(rest, tok);
    }
    Obj *var = find_var(tok);
    if (!var) {
      error_tok(tok, "undefined variable");
    }
    *rest = tok->next;
    return new_var_node(var, tok);
  }
  if (tok->kind == TK_STR) {
    Obj *var = new_string_literal(tok->str, tok->ty);
    *rest = tok->next;
    return new_var_node(var, tok);
  }
  if (equal(tok, "sizeof")) {
    Node *node = unary(rest, tok->next);
    add_type(node);
    return new_num(node->ty->size, tok);
  }
  error_tok(tok, "expected an expression");
  return NULL;
}

static void create_param_lvars(Type *param) {
  if (param) {
    create_param_lvars(param->next);
    new_lvar(get_ident(param->name), param);
  }
}

static Token *function(Token *tok, Type *basety) {
  Type *ty = declarator(&tok, tok, basety);
  Obj *fn = new_gvar(get_ident(ty->name), ty);
  fn->is_function = true;
  locals = NULL;
  create_param_lvars(ty->params);
  fn->params = locals;
  tok = skip(tok, "{");
  fn->body = compound_stmt(&tok, tok);
  fn->locals = locals;
  return tok;
}

static Token *global_variable(Token *tok, Type *basety) {
  bool first = true;
  while (!consume(&tok, tok, ";")) {
    if (!first) {
      tok = skip(tok, ",");
    }
    first = false;
    Type *ty = declarator(&tok, tok, basety);
    new_gvar(get_ident(ty->name), ty);
  }
  return tok;
}
static bool is_function(Token *tok) {
  if (equal(tok->next, ";")) {
    return false;
  }
  Type dummy = {};
  Type *ty = declarator(&tok, tok, &dummy);
  return ty->kind == TY_FUNC;
}

// program = (function-definition | global-variable)*
Obj *parse(Token *tok) {
  globals = NULL;
  while (tok->kind != TK_EOF) {
    Type *basety = declspec(&tok, tok);
    if (is_function(tok)) {
      tok = function(tok, basety);
      continue;
    }
    tok = global_variable(tok, basety);
  }
  return globals;
}
