#include "tiger/codegen/codegen.h"

#include <cassert>
#include <memory>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

void CodeGen::Codegen() {
  fs_ = frame_->name_->Name() + "_framesize";
  // fs_ = frame_->Size()
  assem_instr_ = std::make_unique<AssemInstr>(new assem::InstrList);
  for (auto it = traces_->GetStmList()->GetList().begin();
       it != traces_->GetStmList()->GetList().end(); it++) {
    (*it)->Munch(*assem_instr_->GetInstrList(), fs_);
  }
  // TODO may need this?
  frame_->ProcEntryExit2(assem_instr_->GetInstrList());
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(
      new assem::LabelInstr(temp::LabelFactory::LabelString(label_), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::OperInstr("jmp `j0", nullptr, nullptr,
                                         new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  std::string ins;
  switch (op_) {
  case EQ_OP:
    ins = "je `j0";
    break;
  case NE_OP:
    ins = "jne `j0";
    break;
  case LT_OP:
    ins = "jl `j0";
    break;
  case LE_OP:
    ins = "jle `j0";
    break;
  case GT_OP:
    ins = "jg `j0";
    break;
  case GE_OP:
    ins = "jge `j0";
    break;
  }
  instr_list.Append(
      new assem::OperInstr("cmp `s1, `s0", nullptr,
                           new temp::TempList({left_->Munch(instr_list, fs),
                                               right_->Munch(instr_list, fs)}),
                           new assem::Targets(nullptr)));
  instr_list.Append(new assem::OperInstr(
      ins, nullptr, nullptr,
      new assem::Targets(new std::vector<temp::Label *>{true_label_})));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  if (typeid(*dst_) == typeid(tree::MemExp)) {
    auto *e1 = dynamic_cast<tree::MemExp *>(dst_);
    if (typeid(*e1->exp_) == typeid(tree::BinopExp)) {
      // case MOVE( MEM( BINOP(
      auto *e1_ = dynamic_cast<tree::BinopExp *>(e1->exp_);
      // MOVE( MEM(BINOP), e2)   MOVE(t, s)
      // movq s,t
      if (typeid(*e1_->right_) == typeid(tree::ConstExp)) {
        // case MOVE( MEM( BINOP( -, CONST() ) ), e2 )
        std::string ins =
            "movq `s0," +
            std::to_string(
                dynamic_cast<tree::ConstExp *>(e1_->right_)->consti_) +
            "(`s1)";
        instr_list.Append(new assem::OperInstr(
            ins, nullptr,
            new temp::TempList({src_->Munch(instr_list, fs),
                                e1_->left_->Munch(instr_list, fs)}),
            nullptr));
      } else if (typeid(*e1_->left_) == typeid(tree::ConstExp)) {
        // case MOVE( MEM( BINOP( CONST(), - ) ), e2 )
        std::string ins =
            "movq `s0," +
            std::to_string(
                dynamic_cast<tree::ConstExp *>(e1_->left_)->consti_) +
            "(`s1)";
        instr_list.Append(new assem::OperInstr(
            ins, nullptr,
            new temp::TempList({src_->Munch(instr_list, fs),
                                e1_->right_->Munch(instr_list, fs)}),
            nullptr));
      } else {
        // case MOVE( MEM( BINOP( -, - ) ), e2 )
        std::string ins = "movq `s0,(`s1)";
        instr_list.Append(new assem::OperInstr(
            ins, nullptr,
            new temp::TempList(
                {src_->Munch(instr_list, fs), e1->exp_->Munch(instr_list, fs)}),
            nullptr));
      }
    } else if (typeid(*src_) == typeid(tree::MemExp)) {
      // case MOVE( MEM(), MEM() )
      instr_list.Append(new assem::OperInstr(
          "movq (`s0), (`s1)", nullptr,
          new temp::TempList(
              {src_->Munch(instr_list, fs), dst_->Munch(instr_list, fs)}),
          nullptr));
    } else if (typeid(*e1->exp_) == typeid(tree::ConstExp)) {
      // case MOVE( MEM( CONST() ), e2 )
      std::string ins =
          "movq `s0, " +
          std::to_string(dynamic_cast<tree::ConstExp *>(e1->exp_)->consti_) +
          "(`r0)";
      instr_list.Append(new assem::OperInstr(
          ins, nullptr, new temp::TempList({src_->Munch(instr_list, fs)}),
          nullptr));
    } else {
      // case MOVE( MEM(e1), e2)
      std::string ins = "movq `s0, (`s1)";
      instr_list.Append(
          new assem::OperInstr(ins, nullptr,
                               new temp::TempList({src_->Munch(instr_list, fs),
                                                   e1->Munch(instr_list, fs)}),
                               nullptr));
    }
  } else if (typeid(*dst_) == typeid(tree::TempExp)) {
    if (typeid(*src_) == typeid(tree::ConstExp)) {
      tree::ConstExp *e2 = dynamic_cast<tree::ConstExp *>(src_);
      std::string imm = '$' + std::to_string(e2->consti_);
      instr_list.Append(new assem::OperInstr(
          "movq " + imm + ", `d0",
          new temp::TempList({dynamic_cast<tree::TempExp *>(dst_)->temp_}),
          nullptr, nullptr));
    } else {
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0",
          new temp::TempList({dynamic_cast<tree::TempExp *>(dst_)->temp_}),
          new temp::TempList({src_->Munch(instr_list, fs)})));
    }
  } else
    assert(0);
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *lv = left_->Munch(instr_list, fs);
  temp::Temp *rv = right_->Munch(instr_list, fs);
  temp::Temp *r = temp::TempFactory::NewTemp();
  std::string ins = "";
  switch (op_) {
  case PLUS_OP:
    ins = "addq";
    break;
  case MINUS_OP:
    ins = "subq";
    break;
  case MUL_OP:
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({frame::RAX()}),
                                           new temp::TempList({lv})));
    instr_list.Append(new assem::OperInstr(
        "cqto", new temp::TempList({frame::RDX(), frame::RAX()}),
        new temp::TempList({frame::RDX()}), nullptr));
    instr_list.Append(new assem::OperInstr(
        "imulq `s0", new temp::TempList({frame::RDX(), frame::RAX()}),
        new temp::TempList({rv, frame::RAX(), frame::RDX()}), nullptr));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({r}),
                                           new temp::TempList({frame::RAX()})));
    return r;
  case DIV_OP:
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({frame::RAX()}),
                                           new temp::TempList({lv})));
    instr_list.Append(new assem::OperInstr(
        "cqto", new temp::TempList({frame::RDX(), frame::RAX()}),
        new temp::TempList({frame::RDX()}), nullptr));
    instr_list.Append(new assem::OperInstr(
        "idivq `s0", new temp::TempList({frame::RDX(), frame::RAX()}),
        new temp::TempList({rv, frame::RAX(), frame::RDX()}), nullptr));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({r}),
                                           new temp::TempList({frame::RAX()})));
    return r;
  }
  instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                         new temp::TempList({frame::RAX()}),
                                         new temp::TempList({lv})));
  instr_list.Append(new assem::OperInstr(ins + " `s0, `d0",
                                         new temp::TempList({frame::RAX()}),
                                         new temp::TempList({rv}), nullptr));
  instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                         new temp::TempList({r}),
                                         new temp::TempList({frame::RAX()})));
  return r;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *r = temp::TempFactory::NewTemp();
  if (typeid(*exp_) == typeid(tree::BinopExp)) {
    tree::BinopExp *e1 = dynamic_cast<tree::BinopExp *>(exp_);
    if (typeid(*e1->left_) == typeid(tree::ConstExp)) {
      std::string ins =
          "movq " +
          std::to_string(dynamic_cast<tree::ConstExp *>(e1->left_)->consti_) +
          "(`s0), `d0";
      instr_list.Append(new assem::OperInstr(
          ins, new temp::TempList({r}),
          new temp::TempList({e1->right_->Munch(instr_list, fs)}), nullptr));
    } else if (typeid(*e1->right_) == typeid(tree::ConstExp)) {
      std::string ins =
          "movq " +
          std::to_string(dynamic_cast<tree::ConstExp *>(e1->right_)->consti_) +
          "(`s0), `d0";
      instr_list.Append(new assem::OperInstr(
          ins, new temp::TempList({r}),
          new temp::TempList({e1->left_->Munch(instr_list, fs)}), nullptr));
    } else {
      temp::Temp *lv = e1->left_->Munch(instr_list, fs);
      instr_list.Append(new assem::OperInstr(
          "addq `s0, `d0", new temp::TempList({lv}),
          new temp::TempList({e1->right_->Munch(instr_list, fs)}), nullptr));
      instr_list.Append(
          new assem::OperInstr("movq (`s0), `d0", new temp::TempList({r}),
                               new temp::TempList({lv}), nullptr));
    }
    return r;
  } else if (typeid(*exp_) == typeid(tree::ConstExp)) {
    // TODO need R0 reg to store 0 forever'
    std::string ins =
        "movq " +
        std::to_string(dynamic_cast<tree::ConstExp *>(exp_)->consti_) +
        "(`r0), `d0";
    instr_list.Append(
        new assem::OperInstr(ins, new temp::TempList({r}), nullptr, nullptr));
    return r;
  } else {
    instr_list.Append(new assem::OperInstr(
        "movq `s0, `d0", new temp::TempList({r}),
        new temp::TempList({exp_->Munch(instr_list, fs)}), nullptr));
    return r;
  }
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  if (temp_ == frame::FP()) { // use rsp to trans fp
    temp::Temp *r = temp::TempFactory::NewTemp();
    std::string ins = "leaq " + std::string(fs) + "(%rsp), `d0";
    instr_list.Append(
        new assem::OperInstr(ins, new temp::TempList({r}), nullptr, nullptr));
    return r;
  }
  return temp_;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *r = temp::TempFactory::NewTemp();
  std::string ins = "leaq " + name_->Name() + "(%rip), `d0";
  instr_list.Append(
      new assem::OperInstr(ins, new temp::TempList({r}), nullptr, nullptr));
  return r;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *r = temp::TempFactory::NewTemp();
  std::string ins = "movq $" + std::to_string(consti_) + ", `d0";
  instr_list.Append(
      new assem::MoveInstr(ins, new temp::TempList({r}), nullptr));
  return r;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  tree::NameExp *name = dynamic_cast<tree::NameExp *>(fun_);
  temp::Temp *r = frame::RAX();
  temp::TempList *args = args_->MunchArgs(instr_list, fs);
  instr_list.Append(new assem::OperInstr(
      "callq " + name->name_->Name(),
      new temp::TempList(
          {frame::RAX(), frame::RBX(), frame::R10(), frame::R11(), frame::RDI(),
           frame::RSI(), frame::RCX(), frame::RDX(), frame::R8(), frame::R9()}),
      args, nullptr));
  return r;
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  temp::TempList *args =
      new temp::TempList({frame::RDI(), frame::RSI(), frame::RCX(),
                          frame::RDX(), frame::R8(), frame::R9()});
  temp::TempList *used = new temp::TempList;
  auto it = exp_list_.begin();
  for (int i = 0; it != exp_list_.end() && i < args->GetList().size(); ++i) {
    std::string ins = "movq `s0, `d0";
    instr_list.Append(
        new assem::MoveInstr(ins, new temp::TempList({args->NthTemp(i)}),
                             new temp::TempList((*it)->Munch(instr_list, fs))));
    // used->Insert(args->NthTemp(i));
    used->Append(args->NthTemp(i));
    it++;
  }
  for (; it != exp_list_.end(); it++) {
    instr_list.Append(new assem::OperInstr(
        "pushq `s0", nullptr, new temp::TempList((*it)->Munch(instr_list, fs)),
        nullptr));
  }
  return used;
}

} // namespace tree
