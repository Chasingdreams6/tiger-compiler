#include "derived_heap.h"
#include <stdio.h>
#include <stack>
#include <cstring>

namespace gc {
// TODO(lab7): You need to implement those interfaces inherited from TigerHeap correctly according to your design.

  char* DerivedHeap::Allocate(uint64_t size) {
    uint64_t *sp;
    GET_TIGER_STACK(sp);
    printf("Used: %ld, Allocate: %ld\n", used_size, size);
    printf("RSP: %x, value: %x\n", sp, *sp);
    char *p = (char *)malloc(size);
    used_size += size;
    return p;
  }

  void DerivedHeap::Initialize(uint64_t size) {
      //printf("InitHeapSize: %ld\n", size);
      heap_size = size;
      used_size = 0;
  }

  uint64_t DerivedHeap::MaxFree() const {
      //printf("Ask MaxFree, return %ld\n", heap_size);
      return heap_size;
  }

  uint64_t DerivedHeap::Used() const {
    //printf("Ask Used, return %ld\n", used_size);
    return used_size;
  }

  void DerivedHeap::GC() {
    //printf("Start GC\n");
    //printf("End GC\n");
  }
} // namespace gc

