#include "../cgc.h"
#include <stdlib.h>
#include <stdio.h>

void foo(void) {
  void* bufC = gc_malloc(16);
}

int main(int argc, char* argv[]) {
  // Initialize GC.
  gc_init(&argc); 

  // Allocate memory.
  void* bufA = gc_malloc(16);
  void* bufB = gc_malloc(16);
  foo();

  // STW, free memory, "bufC" should be freed.
  gc_collect();
  return 0;
}
