#pragma once

#include "heap.h"
#include <vector>

namespace gc {

class DerivedHeap : public TigerHeap {
  // TODO(lab7): You need to implement those interfaces inherited from TigerHeap correctly according to your design.
  char* Allocate(uint64_t size) override;
  uint64_t Used() const override;
  uint64_t MaxFree() const override;
  void Initialize(uint64_t size) override;
  void GC() override;

private:
  uint64_t heap_size, used_size;
};

} // namespace gc

