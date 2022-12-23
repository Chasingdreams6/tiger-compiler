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
void MoveList::clear() {
  move_list_.clear();
}
bool MoveList::isDuplicate(std::pair<INodePtr, INodePtr> pair) {
  return Contain(pair.first, pair.second) || Contain(pair.second, pair.first);
}

temp::TempList *list_union(temp::TempList *a, temp::TempList *b) {
  auto *res = new temp::TempList;
  std::set<temp::Temp *> used;
  if (a != nullptr) {
    for (auto ai : a->GetList()) {
      res->Append(ai);
      used.insert(ai);
    }
  }
  if (b != nullptr) {
    for (auto bi : b->GetList()) {
      if (!used.count(bi)) {
        res->Append(bi);
        used.insert(bi);
      }
    }
  }
  return res;
}
//  a - b
temp::TempList *list_difference(temp::TempList *a, temp::TempList *b) {
  auto *res = new temp::TempList;
  if (a != nullptr) {
    for (auto ai : a->GetList()) {
      bool flag = true;
      if (b != nullptr) {
        for (auto bi : b->GetList()) {
          if (ai == bi) {
            flag = false;
            break;
          }
        }
      }
      if (flag)
        res->Append(ai);
    }
  }
  return res;
}

INodePtr newNode(IGraphPtr g, temp::Temp *t, tab::Table<temp::Temp, INode> *m) {
  if (m->Look(t) == nullptr) {
    m->Enter(t, g->NewNode(t));
    //m->Set(t, g->NewNode(t));
  }
  return m->Look(t);
}

temp::TempList* copy_list(temp::TempList *ori) {
  auto* res = new temp::TempList;
  if (!ori) return res;
  for (auto item : ori->GetList()) {
    res->Append(item);
  }
  return res;
}
bool equal_list(temp::TempList *a, temp::TempList *b) {
  if (!a || !b) return false;
  if (a->GetList().size() != b->GetList().size()) return false;
  for (auto item : a->GetList()) {
    bool flag = false;
    for (auto item1 : b->GetList()) {
      if (item1->Int() == item->Int()) {
        flag = true;
        break;
      }
    }
    if (!flag) return false;
  }
  return true;
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  auto nodelist = flowgraph_->Nodes();
  if (nodelist != nullptr) {
    for (auto node : nodelist->GetList()) {
      in_->Enter(node, new temp::TempList);
      out_->Enter(node, new temp::TempList);
    }
  }
  bool finished = false;
  while (!finished) {
    finished = true;
    if (!nodelist) continue;
    for (auto nl : nodelist->GetList()) {
      auto old_inList = copy_list(in_->Look(nl));
      auto old_outList = copy_list(out_->Look(nl));
      auto use = nl->NodeInfo()->Use();
      auto def = nl->NodeInfo()->Def();
      auto succs = nl->Succ();
      // TODO
      out_->Set(nl, new temp::TempList); // clear
      if (succs != nullptr) {
        for (auto succ : succs->GetList()) {
          out_->Set(nl, list_union(out_->Look(nl), in_->Look(succ)));
        }
      }
      in_->Set(nl, list_union(use, list_difference(out_->Look(nl), def)));
      if (!equal_list(old_inList, in_->Look(nl)) || !equal_list(old_outList, out_->Look(nl))) {
        finished = false;
      }
      delete old_outList;
      delete old_inList;
    }
  }

  auto allocated_regs = reg_manager->Registers();

  // directed or undirected ?
  int size = allocated_regs->GetList().size();
  for (int i = 0; i < size; ++i) {
    for (int j = i + 1; j < size; ++j) {
      auto x = allocated_regs->NthTemp(i), y = allocated_regs->NthTemp(j);
      auto m = newNode(live_graph_.interf_graph, x, temp_node_map_);
      auto n = newNode(live_graph_.interf_graph, y, temp_node_map_);
      live_graph_.interf_graph->AddEdge(m, n);
    }
  }

}

void LiveGraphFactory::InterfGraph() { /* TODO: Put your lab6 code here */
  auto nodelist = flowgraph_->Nodes();
  if (!nodelist) return;
  for (auto nl : nodelist->GetList()) {
    auto def = nl->NodeInfo()->Def();
    int defi = 0;
    if (typeid(*(nl->NodeInfo())) == typeid(assem::MoveInstr)) {
      auto use = nl->NodeInfo()->Use();
      int usei = 0;
      auto outs = out_->Look(nl);
      if (outs != nullptr) {
        for (auto out : outs->GetList()) {
          if (out != use->NthTemp(usei)) {
            auto m = newNode(live_graph_.interf_graph, def->NthTemp(defi),
                             temp_node_map_);
            auto n = newNode(live_graph_.interf_graph, out, temp_node_map_);
            if (m != n)
              live_graph_.interf_graph->AddEdge(m, n);
          }
        }
      }
      live_graph_.moves->Append(
          newNode(live_graph_.interf_graph, use->NthTemp(usei), temp_node_map_),
          newNode(live_graph_.interf_graph, def->NthTemp(defi),
                  temp_node_map_));
    } else {
      if (def != nullptr) {
        while (defi < def->GetList().size()) {
          auto outs = out_->Look(nl);
          for (auto out : outs->GetList()) {
            auto m = newNode(live_graph_.interf_graph, def->NthTemp(defi),
                             temp_node_map_);
            auto n = newNode(live_graph_.interf_graph, out, temp_node_map_);
            if (m != n)
              live_graph_.interf_graph->AddEdge(m, n);
          }
          defi++;
        }
      }
    }
  }

//   //output the intergraph
//  auto nodes = live_graph_.interf_graph->Nodes();
//  if (nodes) {
//    for (auto node : nodes->GetList()) {
//      auto succs = node->Adj();
//      printf("--From node:%d--\n", node->NodeInfo()->Int());
//      if (succs) {
//        for (auto succ : succs->GetList()) {
//          if (succ->NodeInfo()->Int() > 115 && node->NodeInfo()->Int() > 115)
//            printf("%d---%d\n", node->NodeInfo()->Int(), succ->NodeInfo()->Int());
//        }
//      }
//    }
//  }

}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live
