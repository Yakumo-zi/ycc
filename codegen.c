#include "ycc.h"
#include <assert.h>
#include <stdio.h>

static int depth = 0;
static char *argreg64[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
static char *argreg8[] = {"%dil", "%sil", "%dl", "%cl", "%r8b", "r9b"};
static Obj *current_fn;
static void gen_expr(Node *node);
static void gen_stmt(Node *node);
static int count() {
  static int i = 1;
  return i++;
}

static void push() {
  printf("    push %%rax\n");
  depth++;
}

static void pop(char *arg) {
  printf("    pop %s\n", arg);
  depth--;
}

static int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}
static void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local) {
      printf("    lea %d(%%rbp),%%rax\n", node->var->offset);
    } else {
      printf("    lea %s(%%rip),%%rax\n", node->var->name);
    }
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    return;
  }
  error_tok(node->tok, "not an lvalue");
}

static void load(Type *ty) {
  if (ty->kind == TY_ARRAY) {
    return;
  }
  if (ty->size == 1) {
    printf("    movsbq (%%rax),%%rax\n");
  } else {
    printf("    mov (%%rax),%%rax\n");
  }
}

static void store(Type *ty) {
  pop("%rdi");
  if (ty->size == 1) {
    printf("    mov %%al,(%%rdi)\n");
  } else {
    printf("    mov %%rax,(%%rdi)\n");
  }
}
static void gen_expr(Node *node) {
  switch (node->kind) {
  case ND_NUM:
    printf("    mov $%d,%%rax\n", node->val);
    return;
  case ND_NEG:
    gen_expr(node->lhs);
    printf("    neg %%rax\n");
    return;
  case ND_VAR:
    gen_addr(node);
    load(node->ty);
    return;
  case ND_ASSIGN:
    gen_addr(node->lhs);
    push();
    gen_expr(node->rhs);
    store(node->ty);
    return;
  case ND_STMT_EXPR:
    for (Node *n = node->body; n; n = n->next) {
      gen_stmt(n);
    }
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    load(node->ty);
    return;
  case ND_ADDR:
    gen_addr(node->lhs);
    return;
  case ND_FUNCALL:
    int nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      gen_expr(arg);
      push();
      nargs++;
    }

    for (int i = nargs - 1; i >= 0; i--) {
      pop(argreg64[i]);
    }
    printf("    mov $0,%%rax\n");
    printf("    call %s\n", node->funcname);
    return;
  }

  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop("%rdi");

  switch (node->kind) {
  case ND_ADD:
    printf("    add %%rdi,%%rax\n");
    return;
  case ND_SUB:
    printf("    sub %%rdi,%%rax\n");
    return;
  case ND_MUL:
    printf("    imul %%rdi,%%rax\n");
    return;
  case ND_DIV:
    printf("    cqo\n");
    printf("    idiv %%rdi\n");
    return;
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    printf("    cmp %%rdi,%%rax\n");
    if (node->kind == ND_EQ) {
      printf("    sete %%al\n");
    } else if (node->kind == ND_NE) {
      printf("    setne %%al\n");
    } else if (node->kind == ND_LT) {
      printf("    setl %%al\n");
    } else if (node->kind == ND_LE) {
      printf("    setle %%al\n");
    }
    printf("    movzb %%al,%%rax\n");
    return;
  }
  error_tok(node->tok, "invalid expression");
}
static void assign_lvar_offsets(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function) {
      continue;
    }
    int offset = 0;
    for (Obj *var = fn->locals; var; var = var->next) {
      offset += var->ty->size;
      var->offset = -offset;
    }
    fn->stack_size = align_to(offset, 16);
  }
}

void gen_stmt(Node *node) {
  int c = 0;
  switch (node->kind) {
  case ND_RETURN:
    gen_expr(node->lhs);
    printf("    jmp .L.return.%s\n", current_fn->name);
    return;
  case ND_EXPR_STMT:
    gen_expr(node->lhs);
    return;
  case ND_BLOCK:
    for (Node *n = node->body; n; n = n->next) {
      gen_stmt(n);
    }
    return;
  case ND_IF:
    c = count();
    gen_expr(node->cond);
    printf("    cmp $0,%%rax\n");
    printf("    je .L.else.%d\n", c);
    gen_stmt(node->then);
    printf("    jmp .L.end.%d\n", c);
    printf(".L.else.%d:\n", c);
    if (node->els) {
      gen_stmt(node->els);
    }
    printf(".L.end.%d:\n", c);
    return;
  case ND_FOR:
    c = count();
    if (node->init)
      gen_stmt(node->init);
    printf(".L.begin.%d:\n", c);
    if (node->cond) {
      gen_expr(node->cond);
      printf("    cmp $0,%%rax\n");
      printf("    je .L.end.%d\n", c);
    }
    gen_stmt(node->then);
    if (node->inc)
      gen_expr(node->inc);
    printf("    jmp .L.begin.%d\n", c);
    printf(".L.end.%d:\n", c);
    return;
  }
  error_tok(node->tok, "invalid statement");
}
static void emit_data(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (var->is_function)
      continue;
    printf("    .data\n");
    printf("    .global %s\n", var->name);
    printf("%s:\n", var->name);
    if (var->init_data) {
      for (int i = 0; i < var->ty->size; i++) {
        printf("    .byte %d\n", var->init_data[i]);
      }
    } else {
      printf("    .zero %d\n", var->ty->size);
    }
  }
}
static void emit_text(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function) {
      continue;
    }
    printf("    .globl %s\n", fn->name);
    printf("    .text\n");
    printf("%s:\n", fn->name);
    current_fn = fn;
    // Prologue
    printf("    push %%rbp\n");
    printf("    mov %%rsp, %%rbp\n");
    printf("    sub $%d, %%rsp\n", fn->stack_size);
    // Save passed-by-register arguments to the stack
    int i = 0;
    for (Obj *var = fn->params; var; var = var->next) {
      if (var->ty->size == 1) {
        printf("    mov %s,%d(%%rbp)\n", argreg8[i++], var->offset);
      } else {
        printf("    mov %s,%d(%%rbp)\n", argreg64[i++], var->offset);
      }
    }
    // Emit code
    gen_stmt(fn->body);
    assert(depth == 0);
    // Epilogue
    printf(".L.return.%s:\n", fn->name);
    printf("    mov %%rbp, %%rsp\n");
    printf("    pop %%rbp\n");
    printf("    ret\n");
  }
}
void codegen(Obj *prog) {
  assign_lvar_offsets(prog);
  emit_data(prog);
  emit_text(prog);
}
