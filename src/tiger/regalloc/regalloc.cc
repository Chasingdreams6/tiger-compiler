#include "tiger/regalloc/regalloc.h"

#include <memory>

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {

RegAllocator::RegAllocator(frame::Frame *frame,
                           std::unique_ptr<cg::AssemInstr> assemInstr)
    : frame_(frame), assem_instr_(std::move(assemInstr)) {
  result = std::make_unique<Result>();

  frozenMoves = new live::MoveList;
  worklistMoves = new live::MoveList;
  coalescedMoves = new live::MoveList;
  constrainedMoves = new live::MoveList;
  activeMoves = new live::MoveList;
}

void RegAllocator::RegAlloc() {
  initWorkList();
  mainFlow();
  temp::Map *coloring = temp::Map::Empty();
  temp::Map *registers = reg_manager->temp_map_;
  for (auto it : color) {
    coloring->Enter(it.first, registers->Look(it.second));
  }
  auto il = assem_instr_->GetInstrList();
  auto it = il->GetList().begin();
  bool flag = false;
  while (!flag) {
    flag = true;
    for (auto instr_it : il->GetList()) {
      if (typeid(*instr_it) == typeid(assem::MoveInstr) &&
          !coloring
               ->Look(
                   static_cast<assem::MoveInstr *>(instr_it)->dst_->NthTemp(0))
               ->compare(*coloring->Look(
                   static_cast<assem::MoveInstr *>(instr_it)->src_->NthTemp(
                       0)))) {
        printf("delete Ins:\n");
        instr_it->Print(stdout, coloring);
        assem_instr_->GetInstrList()->Remove(instr_it);
        flag = false;
        break;
        // assem_instr_->GetInstrList()->Erase(instr_it);
      }
    }
  }
  // output Mapping
   coloring->DumpMap(stdout);

  result->coloring_ = coloring;
  result->il_ = assem_instr_->GetInstrList();
}

std::unique_ptr<ra::Result> RegAllocator::TransferResult() {
  return std::move(result);
}
// all can used colors
std::set<temp::Temp *> RegAllocator::getColors() {
  auto regsList = reg_manager->Registers();
  static std::set<temp::Temp *> *colors = nullptr;
  if (!colors) {
    colors = new std::set<temp::Temp *>;
    for (auto reg : regsList->GetList()) {
      colors->insert(reg);
    }
  }
  return *colors;
}
/*
 * clean all dataset for allocation
 * */
void RegAllocator::initWorkList() {
  precolored = getColors();
  initial.clear();
  simplifyWorklist.clear();
  freezeWorklist.clear();
  spillWorklist.clear();
  spilledNodes.clear();
  coalescedNodes.clear();
  coloredNodes.clear();
  selectStack.clear();
  selectStackSet.clear();
  coalescedMoves->clear();
  constrainedMoves->clear();
  frozenMoves->clear();
  worklistMoves->clear();
  activeMoves->clear();
  degree.clear();
  alias.clear();
  moveList.clear();
  adjSet.clear();
  adjList.clear();
  color.clear();
}
void RegAllocator::mainFlow() {
  auto flowgraphFac = new fg::FlowGraphFactory(assem_instr_->GetInstrList());
  flowgraphFac->AssemFlowGraph();
  fg::FGraphPtr flowgraph = flowgraphFac->GetFlowGraph();
  auto n = flowgraph->Nodes();
  auto *liveGraphFactory = new live::LiveGraphFactory(flowgraph);
  liveGraphFactory->Liveness();
  build(liveGraphFactory->GetLiveGraph(), assem_instr_->GetInstrList());
  makeWorkList();
  while (!simplifyWorklist.empty() || !worklistMoves->GetList().empty() ||
         !freezeWorklist.empty() || !spillWorklist.empty()) {
    if (!simplifyWorklist.empty())
      simplify();
    else if (!worklistMoves->GetList().empty())
      coalesce();
    else if (!freezeWorklist.empty())
      freeze();
    else if (!spillWorklist.empty())
      selectSpill();
  }
  assignColors();
  if (!spilledNodes.empty()) {
    printf("SpillCnt:%lu\n", spilledNodes.size());
    rewrite();
    initWorkList();
    mainFlow();
  }
}
void RegAllocator::build(live::LiveGraph *liveGraph, assem::InstrList *il) {
  auto it = liveGraph->moves->GetList().begin();
  auto nodes = liveGraph->interf_graph->Nodes();
  while (it != liveGraph->moves->GetList().end()) {
    auto m = *it;
    auto src = m.first->NodeInfo();
    auto dst = m.second->NodeInfo();
    assert(src);
    assert(dst);
    if (!moveList[src]) {
      moveList[src] = new live::MoveList;
    }
    if (!moveList[dst]) {
      moveList[dst] = new live::MoveList;
    }
    if (!worklistMoves->isDuplicate(m))
      worklistMoves->Append(m.first, m.second);
    if (!moveList[src]->isDuplicate(m))
      moveList[src]->Append(m.first, m.second);
    if (!moveList[dst]->isDuplicate(m))
      moveList[dst]->Append(m.first, m.second);
    it++; // TODO
  }

  for (auto node : nodes->GetList()) {
    auto adjs = node->Adj();
    for (auto adj : adjs->GetList()) {
      addEdge(node->NodeInfo(), adj->NodeInfo());
    }
  }
  for (auto node : nodes->GetList()) {
    if (precolored.find(node->NodeInfo()) != precolored.end()) {
      color[node->NodeInfo()] = *precolored.find(node->NodeInfo());
    } else {
      initial.insert(node->NodeInfo());
    }
    alias[node->NodeInfo()] = node->NodeInfo();
  }
}
void RegAllocator::addEdge(temp::Temp *src, temp::Temp *dst) {
  Edge e = Edge(src, dst);
  if (src != dst && adjSet.find(&e) == adjSet.end()) { // not found
    adjSet.insert(new Edge(src, dst));
    adjSet.insert(new Edge(dst, src));
    if (precolored.find(src) == precolored.end()) {
      adjList[src].insert(dst);
      degree[src]++;
    }
    if (precolored.find(dst) == precolored.end()) {
      adjList[dst].insert(src);
      degree[dst]++;
    }
  }
}
void RegAllocator::makeWorkList() {
  for (auto it : initial) {
    if (degree[it] >= K)
      spillWorklist.insert(it);
    else if (moveRelated(it))
      freezeWorklist.insert(it);
    else
      simplifyWorklist.insert(it);
  }
  initial.clear();
}

live::MoveList *RegAllocator::nodeMoves(temp::Temp *x) {
  if (!moveList[x])
    moveList[x] = new live::MoveList;
  return moveList[x]->Intersect(activeMoves->Union(worklistMoves));
}

bool RegAllocator::moveRelated(temp::Temp *x) {
  return !nodeMoves(x)->GetList().empty();
}

std::set<temp::Temp *> RegAllocator::adjacent(temp::Temp *x) {
  return set_difference(adjList[x], set_union(selectStackSet, coalescedNodes));
}
void RegAllocator::simplify() {
  if (!simplifyWorklist.empty()) {
    auto t = *simplifyWorklist.begin();
    simplifyWorklist.erase(t);
    selectStack.push_back(t);
    selectStackSet.insert(t);
    auto adj = adjacent(t);
    for (auto node : adj) {
      decrementDegree(node);
    }
  }
}
void RegAllocator::decrementDegree(temp::Temp *x) {
  degree[x]--;
  if (degree[x] == K - 1) {
    std::set<temp::Temp *> tmp;
    tmp.insert(x);
    enableMoves(set_union(adjacent(x), tmp));
    spillWorklist.erase(x);
    if (moveRelated(x))
      freezeWorklist.insert(x);
    else
      simplifyWorklist.insert(x);
  }
}
void RegAllocator::enableMoves(std::set<temp::Temp *> nodes) {
  for (auto node : nodes) {
    auto nodemoves = nodeMoves(node);
    for (auto nodemove : nodemoves->GetList()) {
      if (activeMoves->Contain(nodemove.first, nodemove.second)) {
        worklistMoves->Append(nodemove.first, nodemove.second);
        activeMoves->Delete(nodemove.first, nodemove.second);
      }
    }
  }
}
void RegAllocator::coalesce() {
  if (!worklistMoves->GetList().empty()) {
    auto m = *worklistMoves->GetList().begin();
    auto x = m.first->NodeInfo();
    auto y = m.second->NodeInfo();
    x = getAlias(x);
    y = getAlias(y);
    temp::Temp *t;                                // tmp for swap
    if (precolored.find(y) != precolored.end()) { // y is precolored
      std::swap(x, y);
    }
    worklistMoves->Delete(m.first, m.second);

    // George
    bool flag = true;
    auto adjs = adjacent(y);
    for (auto adj : adjs) {
      if (!ok(adj, x)) {
        flag = false;
        break;
      }
    }
    Edge e(x, y);
    if (x == y) {
      coalescedMoves->Append(m.first, m.second);
      addWorkList(x);
    } else if (precolored.find(y) != precolored.end() ||
               adjSet.find(&e) != adjSet.end()) {
      constrainedMoves->Append(m.first, m.second);
      addWorkList(x);
      addWorkList(y);
    } else if (precolored.find(x) != precolored.end() && flag ||
               precolored.find(x) != precolored.end() &&
                   conservative(set_union(adjacent(x), adjacent(y)))) {
      coalescedMoves->Append(m.first, m.second);
      combine(x, y);
      addWorkList(x);
    } else {
      activeMoves->Append(m.first, m.second);
    }
  }
}
void RegAllocator::addWorkList(temp::Temp *x) {
  if (precolored.find(x) == precolored.end() && !moveRelated(x) &&
      degree[x] < K) {
    freezeWorklist.erase(x);
    simplifyWorklist.insert(x);
  }
}
temp::Temp *RegAllocator::getAlias(temp::Temp *x) {
  if (coalescedNodes.find(x) != coalescedNodes.end()) {
    return getAlias(alias[x]);
  }
  return x;
}
void RegAllocator::combine(temp::Temp *x, temp::Temp *y) {
  if (freezeWorklist.find(y) != freezeWorklist.end()) {
    freezeWorklist.erase(y);
  } else {
    if (spillWorklist.find(y) != spillWorklist.end()) {
      spillWorklist.erase(y);
    }
  }
  coalescedNodes.insert(y);
  alias[y] = x;
  if (!moveList[x]) {
    moveList[x] = new live::MoveList;
  }
  moveList[x]->Union(moveList[y]);
  std::set<temp::Temp *> tmp;
  tmp.insert(y);
  enableMoves(tmp);
  auto adjs = adjacent(y);
  for (auto adj : adjs) {
    addEdge(adj, x);
    decrementDegree(adj);
  }
  if (degree[x] >= K && freezeWorklist.find(x) != freezeWorklist.end()) {
    freezeWorklist.erase(x);
    spillWorklist.insert(x);
  }
}
// for george
bool RegAllocator::ok(temp::Temp *x, temp::Temp *y) {
  Edge e = Edge(x, y);
  return degree[x] < K || precolored.find(x) != precolored.end() ||
         adjSet.find(&e) != adjSet.end();
}
// for brigg
bool RegAllocator::conservative(std::set<temp::Temp *> nodes) {
  int cnt = 0;
  for (auto node : nodes) {
    if (degree[node] >= K)
      cnt++;
  }
  return cnt < K;
}
void RegAllocator::freeze() {
  if (!freezeWorklist.empty()) {
    auto target = freezeWorklist.begin();
    freezeWorklist.erase(target);
    simplifyWorklist.insert(*target);
    freezeMoves(*target);
  }
}
void RegAllocator::freezeMoves(temp::Temp *x) {
  auto moves = nodeMoves(x);
  for (auto move : moves->GetList()) {
    auto s = move.first->NodeInfo(), t = move.second->NodeInfo();
    temp::Temp *v;
    if (getAlias(t) == getAlias(x)) {
      v = getAlias(x);
    } else
      v = getAlias(t);
    activeMoves->Delete(move.first, move.second);
    frozenMoves->Append(move.first, move.second);
    if (nodeMoves(v)->GetList().empty() && degree[v] < K) {
      freezeWorklist.erase(v);
      simplifyWorklist.insert(v);
    }
  }
}
void RegAllocator::selectSpill() {
  auto it = spillWorklist.begin();
  temp::Temp *target = *it;
  int max_degree = degree[*it];
  // find the max degree node as the target
  for (; it != spillWorklist.end(); ++it) {
    if (spilledNodes.find(*it) == spilledNodes.end() &&
        precolored.find(*it) == precolored.end()) {
      auto i = degree.find(*it);
      if (i->second > max_degree) {
        max_degree = i->second;
        target = *it;
      }
    }
  }
  spillWorklist.erase(target);
  simplifyWorklist.insert(target);
  freezeMoves(target);
}
void RegAllocator::assignColors() {
  while (!selectStack.empty()) {
    auto n = selectStack.back();
    selectStack.pop_back();
    if (selectStackSet.find(n) != selectStackSet.end())
      selectStackSet.erase(n);
    auto canUseColors = getColors();
    auto adjs = adjList[n];
    for (auto adj : adjs) {
      auto tmp = set_union(coloredNodes, precolored);
      if (tmp.find(getAlias(adj)) != tmp.end()) {
        canUseColors.erase(color[getAlias(adj)]);
      }
    }
    if (canUseColors.empty()) {
      spilledNodes.insert(n);
    } else {
      coloredNodes.insert(n);
      auto c = *(canUseColors.begin());
      for (auto candidate : canUseColors) {
        if (candidate->Int() == n->Int()) {
          c = candidate;
          break;
        }
      }
      color[n] = c;
    }
  }
  for (auto node : coalescedNodes) {
    color[node] = color[getAlias(node)];
  }
}
void RegAllocator::rewrite() {
  std::map<temp::Temp *, int> allocations;
  if (spilledNodes.empty())
    return;
  for (auto node : spilledNodes) {
    frame_->AllocLocal(true, false);  // TODO
    allocations[node] = frame_->Size();
    printf("spill t%d, on stack %d\n", node->Int(), frame_->Size());
  }
  std::string store = "movq `s0, (%s - %d)(%%rsp)";
  std::string load = "movq (%s - %d)(%%rsp), `d0";
  auto i = assem_instr_->GetInstrList()->GetList().begin();
  auto oldi = i;
  while (i != assem_instr_->GetInstrList()->GetList().end()) {
    auto def = (*i)->Def();
    auto use = (*i)->Use();
    int cnt = 0;
    temp::Temp *t = nullptr;
    for (auto def_it = def->GetList().begin(); def_it != def->GetList().end();
         def_it++, cnt++) {
      if (allocations[*def_it]) {
        if (!t) t = temp::TempFactory::NewTemp();
        char ins[128];
        sprintf(ins, store.c_str(), frame_->getFrameSizeStr().c_str(),
                allocations[*def_it]);
        assem::OperInstr *instr = new assem::OperInstr(
            ins, nullptr, new temp::TempList({t, frame::RSP()}), nullptr);
        assem_instr_->GetInstrList()->Insert(++i, instr); // insert after i
        i--;                                              // restore
        printf("deflist: ");
        for (auto t : def->GetList()) {
          printf("t%d ", t->Int());
        }
        printf("\n");
        printf("change def cnt=%d to=t%d\n", cnt, t->Int());
        printf("new: %s\n\n", ins);
        def->Change(cnt, t);
      }
    }
    cnt = 0;
    for (auto use_it = use->GetList().begin(); use_it != use->GetList().end();
         use_it++, cnt++) {
      if (allocations[*use_it]) {
        if (!t) t = temp::TempFactory::NewTemp();
        char ins[128];
        sprintf(ins, load.c_str(), frame_->getFrameSizeStr().c_str(),
                allocations[*use_it]);
        assem::OperInstr *instr =
            new assem::OperInstr(ins, new temp::TempList({t}),
                                 new temp::TempList({frame::RSP()}), nullptr);
        // assert(prev != std::nullopt_t);
        assem_instr_->GetInstrList()->Insert(oldi, instr); // insert before i
        printf("uselist: ");
        for (auto t : use->GetList()) {
          printf("t%d ", t->Int());
        }
        printf("\n");
        printf("change use cnt=%d to=t%d\n", cnt, t->Int());
        printf("new: %s\n\n", ins);
        use->Change(cnt, t);
      }
    }
    i++;
    oldi = i;
  }
}
RegAllocator::~RegAllocator() {
  delete frozenMoves;
  delete worklistMoves;
  delete coalescedMoves;
  delete constrainedMoves;
  delete activeMoves;
  for (auto it : moveList) {
    delete (it.second);
  }
}

Result::~Result() {}
} // namespace ra