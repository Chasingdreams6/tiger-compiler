#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <list>
#include <memory>
#include <string>

#include "tiger/frame/temp.h"
#include "tiger/translate/tree.h"
#include "tiger/codegen/assem.h"



namespace frame {

#define REGDEF { static temp::Temp *t = nullptr; if(!t) t = temp::TempFactory::NewTemp(); return t;}
    inline temp::Temp* RAX() REGDEF
    inline temp::Temp* RBX() REGDEF
    inline temp::Temp* RCX() REGDEF
    inline temp::Temp* RDX() REGDEF
    inline temp::Temp* R8() REGDEF
    inline temp::Temp* R9() REGDEF
    inline temp::Temp* R10() REGDEF
    inline temp::Temp* R11() REGDEF
    inline temp::Temp* R12() REGDEF
    inline temp::Temp* R13() REGDEF
    inline temp::Temp* R14() REGDEF
    inline temp::Temp* R15() REGDEF
    inline temp::Temp* RSP() REGDEF
    inline temp::Temp* FP() REGDEF
    inline temp::Temp* RDI() REGDEF
    inline temp::Temp* RSI() REGDEF

    //temp::TempList* CalleeSaves();
    //inline temp::TempList* callee
class RegManager {
public:
  RegManager() : temp_map_(temp::Map::Empty()) {}

  temp::Temp *GetRegister(int regno) { return regs_[regno]; }

  /**
   * Get general-purpose registers except RSI
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  [[nodiscard]] virtual temp::TempList *Registers() = 0;

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  [[nodiscard]] virtual temp::TempList *ArgRegs() = 0;

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CallerSaves() = 0;

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CalleeSaves() = 0;

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  [[nodiscard]] virtual temp::TempList *ReturnSink() = 0;

  /**
   * Get word size
   */
  [[nodiscard]] virtual int WordSize() = 0;

  [[nodiscard]] virtual temp::Temp *FramePointer() = 0;

  [[nodiscard]] virtual temp::Temp *StackPointer() = 0;

  [[nodiscard]] virtual temp::Temp *ReturnValue() = 0;

  temp::Map *temp_map_;
protected:
  std::vector<temp::Temp *> regs_;
};

class Access {
private:
  bool isPointer_;
public:
  /* TODO: Put your lab5 code here */
  bool getIsPointer() const {return isPointer_;}
  void setIsPointer(bool isPointer) {isPointer_ = isPointer;}
  virtual ~Access() = default;
  Access(bool isPointer) : isPointer_(isPointer) {}
  virtual tree::Exp *ToExp(tree::Exp *framePtr) const = 0;
};

/*
 * 1. The locations of all the formals
   2. Instructions required to implement the “view shift”
   3. The number of locals allocated so far
   4. And the label at which the function’s machine code is to begin
 * */
class Frame {
  /* TODO: Put your lab5 code here */
public:

  temp::Label *name_;
  int size_;
  int maxArgs = 0;
  std::list<frame::Access *> *formals_; // the locations of all the formals
  tree::Stm *viewShift;
  //std::vector<tree::Stm*> viewShift;
  temp::TempList *calleeRegs; // remain regs for passing argument
  temp::TempList *returnSink;
  int usedRegs = 0;

  virtual Access* AllocLocal(bool escape, bool isPointer) = 0;
  virtual std::list<frame::Access*>* Formals() const = 0;
  virtual int Size() = 0;
  virtual int MaxArgs() = 0;
  //static Frame* NewFrame(temp::Label *name, std::list<bool> formals);
  virtual std::string getFrameSizeStr() = 0;
  virtual std::string GetLabel() = 0;
  virtual tree::Stm* ProcEntryExit1(tree::Stm* stm) = 0;
  virtual assem::InstrList* ProcEntryExit2(assem::InstrList* body) = 0;
  //virtual assem::Proc* ProcEntryExit3(Frame *pFrame, assem::InstrList* il) = 0;
};

/**
 * Fragments
 */

class Frag {
public:
  virtual ~Frag() = default;

  enum OutputPhase {
    Proc,
    String,
    Data
  };

  /**
   *Generate assembly for main program
   * @param out FILE object for output assembly file
   */
  virtual void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const = 0;
};

class StringFrag : public Frag {
public:
  temp::Label *label_;
  std::string str_;

  StringFrag(temp::Label *label, std::string str)
      : label_(label), str_(std::move(str)) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class ProcFrag : public Frag {
public:
  tree::Stm *body_;
  Frame *frame_;

  ProcFrag(tree::Stm *body, Frame *frame) : body_(body), frame_(frame) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class DataFrag : public Frag {
public:
  temp::Label *label_;
  Frame *frame_;
  DataFrag(temp::Label *label, Frame *frame) : label_(label), frame_(frame) {}
  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class Frags {
public:
  Frags() = default;
  void PushBack(Frag *frag) { frags_.emplace_back(frag); }
  const std::list<Frag*> &GetList() { return frags_; }

private:
  std::list<Frag*> frags_;
};


} // namespace frame

#endif