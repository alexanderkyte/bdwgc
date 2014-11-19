#include "read_types.h"

static int z;
int y = 400;

typedef struct {
  int x;
  void *next;
} Node;

int fact(int n) {
  if (n == 1) {
    return 1;
  } else {
    return n * fact(n - 1);
  }
}

int main(int argc, char **argv) {
  z = 100000;
  int x = 10;
  Node* v = calloc(sizeof(Node), 1);
  v->x = 100;
  v->next = NULL;

  if (argc <= 0) {
    perror("Argv does not contain executable name. Something went very wrong");
    return -1;
  }

  double testDouble[30];
  char *testStr = "hello world";

  void *test = (void *)999;

  printf("test location: %p\n", &test);

  GCContext *context;
  dwarf_read(argv[0], &context);
  printf("init done\n");
  CallStack *callStack;
  dwarf_backtrace(&callStack);
  Roots *roots;
  get_roots(callStack, context, &roots);
  printf("got roots\n");

  for(int i=0; i < roots->roots->count; i++){
    printf("location: %p, type: %d\n", roots->roots->contents[i]);
  }

  printf("name | contents | children\n");
  for(int i=0; i < context->functions->count; i++){
    Function *fun = context->functions->contents[i];
    printf("%s %p %p\n", fun->dieName, fun->topScope->contents, fun->topScope->children);
  }

  printf("done\n");

  freeContext(context);
  freeCallstack(callStack);
  freeRoots(roots);

  return y + z + x + fact(5) + 1;
}
