#include "derived_heap.h"
#include <cstring>
#include <stack>
#include <stdio.h>

namespace gc {
// TODO(lab7): You need to implement those interfaces inherited from TigerHeap
// correctly according to your design.

char *DerivedHeap::Allocate(uint64_t size) {
//  uint64_t *sp;
//  uint64_t *ptrmap;
//  GET_TIGER_STACK(sp);
//  GET_ROOT(ptrmap);
//  printf("Used: %ld, Allocate: %ld\n", used_size, size);
//  sp = sp + 1; // skip RA
//  printf("RSP: %x, value: %x \n", sp, *sp, *(uint64_t *)(*sp));
//  printf("RSP+8: %x, value: %x \n", sp + 1, *(sp + 1));
//  printf("RSP+16: %x, value: %x \n", sp + 2, *(sp + 2));
//  printf("RSP+24: %x, value: %x \n", sp + 3, *(sp + 3));
//  printf("RSP+32: %x, value: %x \n", sp + 4, *(sp + 4));
  Segment x = freeList.find(size);
  if (x.start_ == -1) { // no enough space
    return nullptr;
  }
  char *p = datas + x.start_;
  freeList.erase(x);
  Segment u = Segment(x.start_, x.start_ + size);
  //printf("alloc [%d,%d), pos=%lx\n", u.start_, u.end_, (uint64_t)p);
  usedList.insert(u);
  used_mapper[(uint64_t)p] = u;
  x.start_ = x.start_ + size;
  used_size += size;
  freeList.insert(x);
  all_address.insert((uint64_t)p);
  return p;
}

void DerivedHeap::Initialize(uint64_t size) {
  // printf("InitHeapSize: %ld\n", size);
  heap_size = size;
  used_size = 0;
  datas = (char*)malloc(size);
  freeList.insert(Segment(0, size));

}

void DerivedHeap::initGraph() {
  graph_mapper.clear();
  tot = totSeg = 0;
  memset(head, 0, sizeof head);
  memset(v, 0, sizeof v);
}
uint64_t DerivedHeap::MaxFree() const {
  // printf("Ask MaxFree, return %ld\n", heap_size);
  return freeList.maxFree();
}

uint64_t DerivedHeap::Used() const {
  // printf("Ask Used, return %ld\n", used_size);
  return used_size;
}

void DerivedHeap::GC() {
    //printf("Start GC\n");
    uint64_t *sp;
    uint64_t *ptrmap, *enddata;
    GET_TIGER_STACK(sp);
    GET_ROOT(ptrmap);
    GET_END(enddata);
    uint64_t *cur_ptmap = (uint64_t*)*enddata;
    initGraph();
    while (cur_ptmap != ptrmap) {
       if (*(cur_ptmap + 2) == *(sp)) { // cur
         sp = sp + 1; // skip RA
         uint64_t *cur = cur_ptmap + 3;
         while (*cur != -1) {
           uint64_t ptr = *(uint64_t *)( (char*)sp + *cur);
           // ptr -> *ptr
           //printf("%lx -> %lx -> %lx\n", ptr, *(uint64_t*)ptr, *((uint64_t*)*(uint64_t*)ptr));
           if (!graph_mapper[ptr]) {graph_mapper[ptr] = add_mapping(ptr);}
           if (!graph_mapper[*(uint64_t*)ptr]) {graph_mapper[*(uint64_t*)ptr] = add_mapping(*(uint64_t*)ptr);}
//           printf("add edge %d -> %d,(%d, %d)->(%d, %d)\n", graph_mapper[ptr], graph_mapper[*(uint64_t*)ptr],
//                  used_mapper[ptr].start_, used_mapper[ptr].end_,
//                  used_mapper[*(uint64_t*)ptr].start_, used_mapper[*(uint64_t*)ptr].end_);
           add_edge(graph_mapper[ptr], graph_mapper[*(uint64_t*)ptr]);
           starter.push_back(graph_mapper[ptr]);
           cur = cur + 1;
         }
         uint64_t size = *(cur_ptmap + 1);
         sp = (uint64_t *)((char*)sp + size); // use static link to skip up
       }
      cur_ptmap = (uint64_t *)*cur_ptmap;
    }
    mark_and_sweep();
    //printf("End GC\n");
}

void DerivedHeap::mark_and_sweep() {
  memset(visited, 0, sizeof visited);
  for (auto x : starter) { // mark
    if (!visited[x]) {
      visited[x] = 1;
      dfs(x);
    }
  }
  for (auto x : all_address) {
    int node_id = graph_mapper[x];
    Segment node_seg = used_mapper[x];
    int flag = 0;
    for (auto ss : starter) {
      if (node_id == ss) flag = 1;
    }
    if (!visited[node_id] && !flag) { // free
      //printf("free [%d, %d)\n", node_seg.start_, node_seg.end_);
      usedList.erase(node_seg);
      freeList.insert(node_seg);
      break;
    }
  }
  starter.clear();
}

void DerivedHeap::dfs(int x) {
  for (int i = head[x]; i; i = next[i]) {
    int y = v[i];
    if (!visited[y]) {
      visited[y] = 1;
      dfs(y);
    }
  }
}
} // namespace gc
