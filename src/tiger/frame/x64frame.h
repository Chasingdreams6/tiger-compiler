//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
public:
//  temp::Temp *rax, *rbx, *rcx, *rdx;
//  temp::Temp *rdi, *rsi, *rbp, *rsp;
//  temp::Temp *r8, *r9, *r10, *r11;
//  temp::Temp *r12, *r13, *r14, *r15;
  X64RegManager() : RegManager() {
//    rax = temp::TempFactory::NewTemp();
//    rbx = temp::TempFactory::NewTemp();
//    rcx = temp::TempFactory::NewTemp();
//    rdx = temp::TempFactory::NewTemp();
//    rdi = temp::TempFactory::NewTemp();
//    rsi = temp::TempFactory::NewTemp();
//    rbp = temp::TempFactory::NewTemp();
//    rsp = temp::TempFactory::NewTemp();
//    r8  = temp::TempFactory::NewTemp();
//    r9  = temp::TempFactory::NewTemp();
//    r10 = temp::TempFactory::NewTemp();
//    r11 = temp::TempFactory::NewTemp();
//    r12 = temp::TempFactory::NewTemp();
//    r13 = temp::TempFactory::NewTemp();
//    r14 = temp::TempFactory::NewTemp();
//    r15 = temp::TempFactory::NewTemp();
    this->temp_map_->Enter(RAX(), new std::string("%rax")); // 100
    this->temp_map_->Enter(RBX(), new std::string("%rbx")); // 101
    this->temp_map_->Enter(RCX(), new std::string("%rcx")); // 102
    this->temp_map_->Enter(RDX(), new std::string("%rdx")); // 103
    this->temp_map_->Enter(RDI(), new std::string("%rdi")); // 104
    this->temp_map_->Enter(RSI(), new std::string("%rsi")); // 105
    this->temp_map_->Enter(FP(), new std::string("%rbp")); // 106
    this->temp_map_->Enter(RSP(), new std::string("%rsp")); // 107
    this->temp_map_->Enter(R8(), new std::string("%r8")); // 108
    this->temp_map_->Enter(R9(), new std::string("%r9"));
    this->temp_map_->Enter(R10(), new std::string("%r10"));
    this->temp_map_->Enter(R11(), new std::string("%r11"));
    this->temp_map_->Enter(R12(), new std::string("%r12"));
    this->temp_map_->Enter(R13(), new std::string("%r13"));
    this->temp_map_->Enter(R14(), new std::string("%r14"));
    this->temp_map_->Enter(R15(), new std::string("%r15"));
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
  int MaxArgs() override {return maxArgs;}
  std::list<frame::Access *> *Formals() const { return formals_; }
  std::string GetLabel() override { return name_->Name(); }
  tree::Stm *ProcEntryExit1(tree::Stm *stm);
  assem::InstrList *ProcEntryExit2(assem::InstrList *body);
  //assem::Proc *ProcEntryExit3(assem::InstrList *il);
};
} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
