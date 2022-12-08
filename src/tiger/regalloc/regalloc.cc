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
    for (auto instr : il->GetList()) {
      if (typeid(instr) == typeid(assem::MoveInstr *) &&
          !coloring
               ->Look(static_cast<assem::MoveInstr *>(instr)->dst_->NthTemp(0))
               ->compare(*coloring->Look(
                   static_cast<assem::MoveInstr *>(instr)->src_->NthTemp(0)))) {
        assem_instr_->GetInstrList()->Remove(instr);
        flag = false;
      }
    }
  }
  result->coloring_ = coloring;
  result->il_ = assem_instr_->GetInstrList();
}

std::unique_ptr<ra::Result> RegAllocator::TransferResult() {
    return std::move(result);
}
// precolored regs
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
    rewrite(flowgraph, frame_, std::move(assem_instr_));
    initWorkList();
    mainFlow();
  }
}
void RegAllocator::build(live::LiveGraph *liveGraph, assem::InstrList *il) {
  auto m = liveGraph->moves->GetList().front();
  auto nodes = liveGraph->interf_graph->Nodes();
  auto src = m.first->NodeInfo();
  auto dst = m.second->NodeInfo();
  if (!worklistMoves->isDuplicate(m))
    worklistMoves->Append(m.first, m.second);
  if (!moveList[src]->isDuplicate(m))
    moveList[src]->Append(m.first, m.second);
  if (!moveList[dst]->isDuplicate(m))
    moveList[dst]->Append(m.first, m.second);
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
      // TODO the meaning of find
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
    auto preColoreds = getColors();
    auto adjs = adjList[n];
    for (auto adj : adjs) {
      auto tmp = set_union(coloredNodes, precolored);
      if (tmp.find(getAlias(adj)) != tmp.end()) {
        preColoreds.erase(color[getAlias(adj)]);
      }
    }
    if (preColoreds.empty()) {
      spilledNodes.insert(n);
    } else {
      coloredNodes.insert(n);
      auto c = preColoreds.begin();
      color[n] = *c;
    }
  }
  for (auto node : coalescedNodes) {
    color[node] = color[getAlias(node)];
  }
}
void RegAllocator::rewrite(fg::FGraphPtr flowgraph, frame::Frame *f,
                           std::unique_ptr<cg::AssemInstr> assemInstr) {
  auto il = assemInstr->GetInstrList();
  std::map<temp::Temp *, int> allocations;
  for (auto node : spilledNodes) {
    f->AllocLocal(true);
    allocations[node] = f->Size();
  }
  std::string store = "\tmovq\t`s0, (%s - %d)(%%rsp)";
  std::string load = "\tmovq\t(%s - %d)(%%rsp), `d0";
  auto i = il->GetList().begin();
  while (i != il->GetList().end()) {
    auto def = getDefs(*i);
    auto use = getUses(*i);
    int cnt = 0;
    for (auto def_it = def->GetList().begin(); def_it != def->GetList().end();
         def_it++, cnt++) {
      if (allocations[*def_it]) {
        temp::Temp *t = temp::TempFactory::NewTemp();
        char ins[256];
        sprintf(ins, store.c_str(), f->getFrameSizeStr().c_str(),
                allocations[*def_it]);
        assem::OperInstr *instr = new assem::OperInstr(
            ins, nullptr, new temp::TempList({t, frame::RSP()}), nullptr);
        il->Insert(++i, instr); // insert after i
        def->Change(cnt, t);
      }
    }
    cnt = 0;
    for (auto use_it = use->GetList().begin(); use_it != use->GetList().end();
         use_it++, cnt++) {
      if (allocations[*use_it]) {
        temp::Temp *t = temp::TempFactory::NewTemp();
        char ins[256];
        sprintf(ins, load.c_str(), f->getFrameSizeStr().c_str(),
                allocations[*use_it]);
        assem::OperInstr *instr =
            new assem::OperInstr(ins, new temp::TempList({t}),
                                 new temp::TempList({frame::RSP()}), nullptr);
        // assert(prev != std::nullopt_t);
        il->Insert(i, instr); // insert before i
        use->Change(cnt, t);
      }
    }
    i++;
  }
}
temp::TempList *RegAllocator::getDefs(assem::Instr *instr) {
  if (typeid(instr) == typeid(assem::LabelInstr *))
    return nullptr;
  else if (typeid(instr) == typeid(assem::MoveInstr *)) {
    return static_cast<assem::MoveInstr *>(instr)->dst_;
  } else
    return static_cast<assem::MoveInstr *>(instr)->dst_;
}
temp::TempList *RegAllocator::getUses(assem::Instr *instr) {
  if (typeid(instr) == typeid(assem::LabelInstr *))
    return nullptr;
  else if (typeid(instr) == typeid(assem::MoveInstr *)) {
    return static_cast<assem::MoveInstr *>(instr)->src_;
  } else
    return static_cast<assem::MoveInstr *>(instr)->src_;
}

Result::~Result() {

}
} // namespace ra