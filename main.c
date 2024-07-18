#include "ycc.h"
#include <assert.h>
#include <endian.h>
#include <stdbool.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "%s:invalid number of arguments\n", argv[0]);
    return 1;
  }

  Token *tok = tokenize(argv[1]);
  Node *node = parse(tok);
  codegen(node);
  return 0;
}
