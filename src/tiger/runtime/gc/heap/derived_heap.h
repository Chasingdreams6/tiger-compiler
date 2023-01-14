#pragma once

#include "heap.h"
#include <vector>
#include <list>
#include <map>
#include <set>

namespace gc {

class Segment {
public:
  int start_, end_;
  int size() {return end_ - start_;}
  Segment() {start_ = end_ = -1;}
  Segment(int start, int end) : start_(start), end_(end) {}
  bool operator == (const Segment &t) {
    return start_ == t.start_ && end_ == t.end_;
  }
};

class SegList {
public:
  std::list<Segment> freelist;
  Segment merge(Segment x) {
    Segment z = x;
    auto it = freelist.begin();
    for (;it != freelist.end(); it++) {
      auto y = *it;
      if (y.end_ == x.start_ || x.end_ == y.start_) {
        if (y.end_ == x.start_) {
          z.start_ = y.start_;
          z.end_ = x.end_;
        } else {
          z.start_ = x.start_;
          z.end_ = y.end_;
        }
        freelist.erase(it);
        break;
      }
    }
    return z;
  }
  void insert(Segment x) {
    Segment z = merge(x);
    while (!(z == x)) {
      x = z;
      z = merge(x);
    }
    freelist.push_back(z);
  }
  void erase(Segment t) {
    auto it = freelist.begin();
    for (; it != freelist.end(); ++it) {
      if (*it == t) {
        freelist.erase(it);
        break;
      }
    }
  }
  Segment find(int size) {
    Segment res(-1, -1);
    auto it = freelist.begin();
    for (; it != freelist.end(); ++it) {
      //printf("find (%d,%d)\n", (*it).start_, (*it).end_);
      if (it->size() >= size) {
        res = (*it);
        break;
      }
    }
    return res;
  }
  int maxFree() const{
    int res = 0;
    for (auto x : freelist) {
      res = std::max(res, x.size());
    }
    return res;
  }
};
class DerivedHeap : public TigerHeap {
  // TODO(lab7): You need to implement those interfaces inherited from TigerHeap correctly according to your design.
  char* Allocate(uint64_t size) override;
  uint64_t Used() const override;
  uint64_t MaxFree() const override;
  void Initialize(uint64_t size) override;
  void GC() override;

public:
  void initGraph();
  void mark_and_sweep();
  void dfs(int x);
private:
  uint64_t heap_size, used_size;
  char* datas;
  SegList freeList, usedList;
  std::map<uint64_t, Segment> used_mapper; // map address to segment
  std::map<uint64_t, int> graph_mapper; // map address to node_id
  std::set<uint64_t> all_address;
  int visited[255];
  std::vector<int> starter;
  int head[255], v[255], next[255], tot = 0, totSeg = 0;
  uint64_t address[255];
  int add_mapping(uint64_t address_) {
    address[++totSeg] = address_;
    return totSeg;
  }
  void add_edge(int x, int y) {
    v[++tot] = y;
    next[tot] = head[x];
    head[x] = tot;
  }
};

} // namespace gc

