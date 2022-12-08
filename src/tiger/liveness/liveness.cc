#include "tiger/liveness/liveness.h"
#include <map>
#include <set>

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

temp::TempList *list_union(temp::TempList *a, temp::TempList *b) {
  auto *res = new temp::TempList;
  std::set<temp::Temp *> used;
  for (auto ai : a->GetList()) {
    res->Append(ai);
    used.insert(ai);
  }
  for (auto bi : b->GetList()) {
    if (!used.count(bi)) {
      res->Append(bi);
      used.insert(bi);
    }
  }
  return res;
}
//  a - b
temp::TempList *list_difference(temp::TempList *a, temp::TempList *b) {
  auto *res = new temp::TempList;
  for (auto ai : a->GetList()) {
    bool flag = true;
    for (auto bi : b->GetList()) {
      if (ai == bi) {
        flag = false;
        break;
      }
    }
    if (flag)
      res->Append(ai);
  }
  return res;
}

INodePtr newNode(IGraphPtr g, temp::Temp *t, tab::Table<temp::Temp, INode> *m) {
  if (m->Look(t) == nullptr) {
    m->Set(t, g->NewNode(t));
  }
  return m->Look(t);
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  auto nodelist = flowgraph_->Nodes();
  bool finished = false;
  while (!finished) {
    finished = true;
    for (auto nl : nodelist->GetList()) {
      int old_inSize = in_->Look(nl)->GetList().size();
      int old_outSize = out_->Look(nl)->GetList().size();
      auto use = nl->NodeInfo()->Use();
      auto def = nl->NodeInfo()->Use();
      auto succs = nl->Succ();
      // TODO
      for (auto succ : succs->GetList()) {
        out_->Set(nl, list_union(out_->Look(nl), in_->Look(succ)));
      }
      in_->Set(nl, list_union(use, list_difference(out_->Look(nl), def)));
      if (old_inSize != in_->Look(nl)->GetList().size() ||
          old_outSize != out_->Look(nl)->GetList().size()) {
        finished = false;
      }
    }
  }

  auto allocated_regs = reg_manager->Registers();

  for (auto x : allocated_regs->GetList()) {
    for (auto y : allocated_regs->GetList()) {
      auto m = newNode(live_graph_.interf_graph, x, temp_node_map_);
      auto n = newNode(live_graph_.interf_graph, y, temp_node_map_);
      live_graph_.interf_graph->AddEdge(m, n);
    }
  }
}

void LiveGraphFactory::InterfGraph() { /* TODO: Put your lab6 code here */
  auto nodelist = flowgraph_->Nodes();
  for (auto nl : nodelist->GetList()) {
    auto def = nl->NodeInfo()->Def();
    int defi = 0;
    if (typeid(nl->NodeInfo()) == typeid(assem::MoveInstr *)) {
      auto use = nl->NodeInfo()->Use();
      int usei = 0;
      auto outs = out_->Look(nl);
      for (auto out : outs->GetList()) {
        if (out != use->NthTemp(usei)) {
          auto m = newNode(live_graph_.interf_graph, def->NthTemp(defi),
                           temp_node_map_);
          auto n = newNode(live_graph_.interf_graph, out, temp_node_map_);
          if (m != n)
            live_graph_.interf_graph->AddEdge(m, n);
        }
      }
      live_graph_.moves->Append(
          newNode(live_graph_.interf_graph, def->NthTemp(defi), temp_node_map_),
          newNode(live_graph_.interf_graph, use->NthTemp(usei),
                  temp_node_map_));
    } else {
      while (defi < def->GetList().size()) {
        auto outs = out_->Look(nl);
        for (auto out : outs->GetList()) {
          auto m = newNode(live_graph_.interf_graph, def->NthTemp(defi), temp_node_map_);
          auto n = newNode(live_graph_.interf_graph, out, temp_node_map_);
          if (m != n) live_graph_.interf_graph->AddEdge(m, n);
        }
        defi++;
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live
