#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"
#include <map>
#include <set>

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result();
};

class Edge {
public:
  temp::Temp *src_, *dst_;
  Edge(temp::Temp *src, temp::Temp *dst) : src_(src), dst_(dst) {}
};
struct Edge_cmp {
  bool operator()(Edge *a, Edge *b) const {
    return a->src_->Int() < b->src_->Int() ||
           (a->src_->Int() == b->src_->Int() &&
            a->dst_->Int() < b->dst_->Int());
  }
};

template<typename T>
std::set<T> set_union(std::set<T> s1, std::set<T> s2) {
  typename std::set<T>::iterator it = s1.begin();
  std::set<T> res;
  for(; it != s1.end(); it++ ) {
    res.insert(*it);
  }
  for(it = s2.begin(); it != s2.end(); it++ ) {
    res.insert(*it);
  }
  return res;
}

template<typename T>
std::set<T> set_intersect(std::set<T> s1, std::set<T> s2) {
  typename std::set<T>::iterator it = s2.begin();
  std::set<T> res;
  for(; it != s2.end(); it++ ) {
    typename std::set<T>::iterator target = s1.find(*it);
    if (target != s1.end()) {
      res.insert(*it);
    }
  }
  return res;
}

template<typename T>
std::set<T> set_difference(std::set<T> s1, std::set<T> s2) {
  typename std::set<T>::iterator it = s1.begin();
  std::set<T> res;
  for(; it != s1.end(); it++ ) {
    typename std::set<T>::iterator target = s2.find(*it);
    if (target == s2.end()) {
      res.insert(*it);
    }
  }
  return res;
}

class RegAllocator {
  /* TODO: Put your lab6 code here */
private:
  int K = 16;
  std::set<temp::Temp *> precolored;
  std::set<temp::Temp *> initial;
  std::set<temp::Temp *> simplifyWorklist;
  std::set<temp::Temp *> freezeWorklist;
  std::set<temp::Temp *> spillWorklist;
  std::set<temp::Temp *> spilledNodes;
  std::list<temp::Temp *> selectStack;
  std::set<temp::Temp *> selectStackSet;
  std::set<temp::Temp *> coalescedNodes;
  std::set<temp::Temp *> coloredNodes;

  std::map<temp::Temp *, int> degree;
  std::map<temp::Temp *, temp::Temp *> alias;
  std::map<temp::Temp *, live::MoveList * > moveList;
  std::set<Edge *, Edge_cmp> adjSet;
  std::map<temp::Temp *, std::set<temp::Temp *> > adjList;
  std::map<temp::Temp *, temp::Temp *> color;

  live::MoveList * frozenMoves;
  live::MoveList * worklistMoves;
  live::MoveList * coalescedMoves;
  live::MoveList * constrainedMoves;
  live::MoveList * activeMoves;


  static std::set<temp::Temp *> getColors();
  void initWorkList();
  void mainFlow();
  void addEdge(temp::Temp *src, temp::Temp *dst);
  void makeWorkList();
  std::set<temp::Temp *> adjacent(temp::Temp *x);
  live::MoveList * nodeMoves(temp::Temp *x);
  bool moveRelated(temp::Temp *x);
  void build(live::LiveGraph *liveGraph, assem::InstrList *il);
  void simplify();
  void decrementDegree(temp::Temp *x);
  void enableMoves(std::set<temp::Temp *> nodes);
  void coalesce();
  void addWorkList(temp::Temp * x);
  // utility for bcj
  temp::Temp * getAlias(temp::Temp *x);
  void combine(temp::Temp *x, temp::Temp *y);
  std::unique_ptr<ra::Result> result;
  bool ok(temp::Temp *x, temp::Temp *y);
  bool conservative(std::set<temp::Temp *> nodes);
  void freeze();
  void freezeMoves(temp::Temp *x);
  void selectSpill();
  void assignColors();
  void rewrite();
  //temp::TempList *getDefs(assem::Instr *instr);
  //temp::TempList *getUses(assem::Instr *instr);
public:
  std::unique_ptr<cg::AssemInstr> assem_instr_;
  frame::Frame *frame_;
  RegAllocator(frame::Frame *, std::unique_ptr<cg::AssemInstr>);
  ~RegAllocator();
  void RegAlloc();
  std::unique_ptr<ra::Result> TransferResult();

};

} // namespace ra

#endif