#include "read_types.h"

static int z;
int y = 400;

typedef struct {
  int x;
  void* next;
} Node;

int fact(int n){
  if(n == 1){
    return 1;
  } else {
    return n * fact(n-1);
  }
}

int main(int argc, char** argv){
  z = 100000;
  int x = 10;
  Node v = {.x = 100, .next = (void*)0};

  if(argc <= 0){
    perror("Argv does not contain executable name. Something went very wrong");
  }

  double testDouble[30];
  char *testStr = "hello world";
  
  types_init(argv[0]);
  printf("init done\n");
  TypedPointers roots;
  type_roots(&roots);
  printf("got roots\n");
  types_finalize();

  return y + z + x + fact(5) + 1;
}
