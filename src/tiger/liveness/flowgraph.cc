#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  auto instr_list = instr_list_->GetList();
  for (auto it = instr_list.begin(); it != instr_list.end(); ++it) {
    if (typeid(*(*it)) == typeid(assem::LabelInstr)) { // is label instr
      auto cur = static_cast<assem::LabelInstr *>(*it);
      label_map_->Enter(cur->label_, flowgraph_->NewNode(cur));
    }
  }

  assem::Instr *cur = nullptr, *next = nullptr;
  FNodePtr from, to;
  auto it = instr_list.begin();
  if (typeid(*(*it)) == typeid(assem::LabelInstr)) { // had mapped
    auto li = static_cast<assem::LabelInstr *>(*it);
    from = label_map_->Look(li->label_);
    assert(from);
  } else {
    from = flowgraph_->NewNode(*it);
  }
  cur = (*it);
  it++; // next = (*it);
  while (it != instr_list.end()) {
    next = (*it);
    if (typeid(*next) == typeid(assem::LabelInstr)) {
      auto li = static_cast<assem::LabelInstr *>(next);
      to = label_map_->Look(li->label_);
      assert(to);
    } else {
      to = flowgraph_->NewNode(next);
    }

    if (typeid(*cur) == typeid(assem::OperInstr)) {
      auto opi = static_cast<assem::OperInstr *>(cur);
      if (opi->jumps_ && opi->jumps_->labels_) {
        auto labels = opi->jumps_->labels_;
        for (auto label : (*labels)) {
          assert(label_map_->Look(label));
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
  }
  assert(typeid(*cur) == typeid(assem::OperInstr));
  assert(static_cast<assem::OperInstr *>(cur)->assem_ == "");
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  // label instr no defs
  return new temp::TempList;
}

temp::TempList *MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
  if (dst_)
    return dst_;
  return new temp::TempList;
}

temp::TempList *OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  if (dst_)
    return dst_;
  return new temp::TempList;
}

temp::TempList *LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList;
}

temp::TempList *MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  if (src_)
    return src_;
  return new temp::TempList;
}

temp::TempList *OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  if (src_)
    return src_;
  return new temp::TempList;
}
} // namespace assem
