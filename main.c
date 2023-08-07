// #include "cgc.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
  // void * a = GC_malloc(14);
  // uint64_t ff = 1;
  // uint64_t* fff = &ff;

  // void* af = gc_malloc(1024);
  // void* aff = gc_malloc(1024);
  // uint64_t* aa = af + 1;
  // aa = fff;
  uint64_t stack_top;
  asm("movl %%rbp, %0" : "=r" (stack_top));
  printf("%llu", stack_top);
  printf("%llu", argv);
  return 0;
}
