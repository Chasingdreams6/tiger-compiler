//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
public:
  temp::Temp *rax, *rbx, *rcx, *rdx;
  temp::Temp *rdi, *rsi, *rbp, *rsp;
  temp::Temp *r8, *r9, *r10, *r11;
  temp::Temp *r12, *r13, *r14, *r15;
  X64RegManager() : RegManager() {
    rax = temp::TempFactory::NewTemp();
    rbx = temp::TempFactory::NewTemp();
    rcx = temp::TempFactory::NewTemp();
    rdx = temp::TempFactory::NewTemp();
    rdi = temp::TempFactory::NewTemp();
    rsi = temp::TempFactory::NewTemp();
    rbp = temp::TempFactory::NewTemp();
    rsp = temp::TempFactory::NewTemp();
    r8  = temp::TempFactory::NewTemp();
    r9  = temp::TempFactory::NewTemp();
    r10 = temp::TempFactory::NewTemp();
    r11 = temp::TempFactory::NewTemp();
    r12 = temp::TempFactory::NewTemp();
    r13 = temp::TempFactory::NewTemp();
    r14 = temp::TempFactory::NewTemp();
    r15 = temp::TempFactory::NewTemp();
  }
  temp::TempList *Registers() override;
  temp::TempList *ArgRegs() override;
  temp::TempList *CallerSaves() override;
  temp::TempList *CalleeSaves() override;
  temp::TempList *ReturnSink() override;
  int WordSize() override;
  temp::Temp *FramePointer() override;
  temp::Temp *StackPointer() override;
  temp::Temp *ReturnValue() override;
};

class X64Frame : public Frame {
public:
//  temp::Label *label_;
//  int size_;
//  std::list<frame::Access *> *formals_; // the locations of all the formals
//  tree::Stm *viewShift;
//  temp::TempList *calleeRegs; // remain regs for passing argument
//  temp::TempList *returnSink;
//  int usedRegs = 0;
  void ViewShift(tree::Stm *stm);
  X64Frame(temp::Label *name, std::list<bool> *formals);
  Access *AllocLocal(bool escape) override;
  int Size() override {return size_;}
  std::list<frame::Access *> *Formals() const { return formals_; }
  std::string GetLabel() override { return name_->Name(); }
  tree::Stm *ProcEntryExit1(tree::Stm *stm);
  assem::InstrList *ProcEntryExit2(assem::InstrList *body);
  //assem::Proc *ProcEntryExit3(assem::InstrList *il);
};
} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
