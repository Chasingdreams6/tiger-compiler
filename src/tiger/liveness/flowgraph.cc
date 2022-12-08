#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  auto instr_list = instr_list_->GetList();
  for (auto it = instr_list.begin(); it != instr_list.end(); ++it) {
    if (typeid((*it)) == typeid(assem::LabelInstr *)) { // is label instr
      auto cur = static_cast<assem::LabelInstr *>(*it);
      label_map_->Enter(cur->label_, flowgraph_->NewNode(cur));
    }
  }

  assem::Instr *cur = nullptr, *next = nullptr;
  FNodePtr from, to;
  auto it = instr_list.begin();
  if (typeid((*it)) == typeid(assem::LabelInstr *)) { // had mapped
    auto li = static_cast<assem::LabelInstr *>(*it);
    from = label_map_->Look(li->label_);
  } else {
    from = flowgraph_->NewNode((*it));
  }
  cur = (*it);
  it++; next = (*it);
  while (next) {
    if (typeid(next) == typeid(assem::LabelInstr *)) {
      auto li = static_cast<assem::LabelInstr *>(next);
      to = label_map_->Look(li->label_);
    } else {
      to = flowgraph_->NewNode(next);
    }

    if (typeid(cur) == typeid(assem::OperInstr *)) {
      auto opi = static_cast<assem::OperInstr *> (cur);
      if (opi->jumps_) {
        auto labels = opi->jumps_->labels_;
        for (auto label : (*labels)) {
          flowgraph_->AddEdge(from, label_map_->Look(label));
        }
      } else {
        flowgraph_->AddEdge(from, to);
      }
    } else {
      flowgraph_->AddEdge(from, to);
    }
    from = to;
    cur = next;
    it++;
    next = (*it);
  }

}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  // label instr no defs
  return nullptr;
}

temp::TempList *MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_;
}

temp::TempList *OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_;
}

temp::TempList *LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return nullptr;
}

temp::TempList *MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_;
}

temp::TempList *OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_;
}
} // namespace assem
