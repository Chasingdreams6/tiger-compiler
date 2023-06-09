#include "tiger/frame/x64frame.h"
#include "frame.h"

#include <utility>

extern frame::RegManager *reg_manager;

namespace frame {


/* TODO: Put your lab5 code here */

Access *X64Frame::AllocLocal(bool escape, bool isPointer) {
  if (escape || isPointer) { // put all pointer to the stack
    size_ += reg_manager->WordSize();
    return new InFrameAccess(-size_, isPointer);
  } else {
    if (usedRegs < calleeRegs->GetList().size()) {
      Access *access = new InRegAccess(calleeRegs->NthTemp(usedRegs), false);
      usedRegs++;
      return access;
    }
    return new InRegAccess(temp::TempFactory::NewTemp(), false);
  }
}
void X64Frame::ViewShift(tree::Stm *stm) {
  // viewShift.push_back(stm);
  if (!viewShift) {
    viewShift = new tree::SeqStm(stm, nullptr);
    return;
  }
  auto *last_stm = static_cast<tree::SeqStm *>(viewShift);
  while (last_stm->right_)
    last_stm = static_cast<tree::SeqStm *>(last_stm->right_);
  last_stm->right_ = new tree::SeqStm(stm, nullptr);
}
X64Frame::X64Frame(temp::Label *name, std::list<bool> *formals, std::list<bool> *isPointers) {
  name_ = name;
  formals_ = new std::list<frame::Access *>;
  viewShift = nullptr;
  size_ = 0;
  calleeRegs = new temp::TempList(
      {frame::R12(), frame::R13(), frame::R14(), frame::R15()});
  returnSink = nullptr;
  int pos = 0;
  auto it_pointer = (*isPointers).begin();
  if (formals != nullptr)
    for (auto formal : *formals) {
      Access *acc = AllocLocal(formal, (*it_pointer));
      formals_->push_back(acc);
      switch (pos) {
      case 0: {
        tree::Stm *stm = new tree::MoveStm(acc->ToExp(new tree::TempExp(FP())),
                                           new tree::TempExp(RDI()));
        ViewShift(stm);
      } break;
      case 1: {
        tree::Stm *stm = new tree::MoveStm(acc->ToExp(new tree::TempExp(FP())),
                                           new tree::TempExp(RSI()));
        ViewShift(stm);
      } break;
      case 2: {
        tree::Stm *stm = new tree::MoveStm(acc->ToExp(new tree::TempExp(FP())),
                                           new tree::TempExp(RCX()));
        ViewShift(stm);
      } break;
      case 3: {
        tree::Stm *stm = new tree::MoveStm(acc->ToExp(new tree::TempExp(FP())),
                                           new tree::TempExp(RDX()));
        ViewShift(stm);
      } break;
      case 4: {
        tree::Stm *stm = new tree::MoveStm(acc->ToExp(new tree::TempExp(FP())),
                                           new tree::TempExp(R8()));
        ViewShift(stm);
      } break;
      case 5: {
        tree::Stm *stm = new tree::MoveStm(acc->ToExp(new tree::TempExp(FP())),
                                           new tree::TempExp(R9()));
        ViewShift(stm);
      } break;
      default: // spill, get value from stack
      {
        tree::Stm *stm = new tree::MoveStm(
            acc->ToExp(new tree::TempExp(FP())),
            new tree::MemExp(new tree::BinopExp(
                tree::PLUS_OP, new tree::TempExp(FP()),
                new tree::ConstExp((pos + 1 - 6) * reg_manager->WordSize()))));
        ViewShift(stm);
      }
      }
      pos++;
      it_pointer++;
    }
}
tree::Stm *X64Frame::ProcEntryExit1(tree::Stm *stm) {
  if (!viewShift)
    return stm;
  tree::SeqStm *last = dynamic_cast<tree::SeqStm *>(viewShift);
  while (last->right_) {
    last = dynamic_cast<tree::SeqStm *>(last->right_);
  }
  last->right_ = stm;
  return viewShift;
  // return new tree::SeqStm(viewShift, stm);
}
assem::InstrList *X64Frame::ProcEntryExit2(assem::InstrList *body) {
  if (!returnSink) {
    returnSink = reg_manager->ReturnSink();
  }
  temp::TempList *calleeSaved = reg_manager->CalleeSaves();
  temp::TempList *calleeSaveRestore =
      new temp::TempList({temp::TempFactory::NewTemp()});
  // save calleesaves
  for (int i = 0; i < calleeSaved->GetList().size(); ++i) {
    body->InsertF(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({calleeSaveRestore->NthTemp(i)}),
        new temp::TempList({calleeSaved->NthTemp(i)})));
    calleeSaveRestore->Append(temp::TempFactory::NewTemp());
  }
  // assem::InstrList *restore_ins = new assem::InstrList;
  //  restore calleesaves
  for (int i = 0; i < calleeSaved->GetList().size(); ++i) {
    body->Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList{calleeSaved->NthTemp(i)},
        new temp::TempList{calleeSaveRestore->NthTemp(i)}));
  }
  body->Append(new assem::OperInstr("", nullptr, returnSink, nullptr));
  return body;
}

temp::TempList *X64RegManager::Registers() {
  return new temp::TempList({RAX(), RBX(), RCX(), RDX(), RSI(), RDI(), RSP(),
                             FP(), R8(), R9(), R10(), R11(), R12(), R13(),
                             R14(), R15()});
}
temp::TempList *X64RegManager::ArgRegs() {
  return new temp::TempList({RDI(), RSI(), RCX(), RDX(), R8(), R9()});
}
temp::TempList *X64RegManager::CallerSaves() {
  return new temp::TempList({R10(), R11()});
}
temp::TempList *X64RegManager::CalleeSaves() {
  return new temp::TempList({RBX(), FP(), R12(), R13(), R14(), R15()});
}
temp::TempList *X64RegManager::ReturnSink() {
  return new temp::TempList({RAX(), RSP(), RBX(), FP(), R12(), R13(), R14(), R15()});
}
int X64RegManager::WordSize() { return 8; }
temp::Temp *X64RegManager::FramePointer() { return FP(); }
temp::Temp *X64RegManager::StackPointer() { return RSP(); }
temp::Temp *X64RegManager::ReturnValue() { return RAX(); }


} // namespace frame
