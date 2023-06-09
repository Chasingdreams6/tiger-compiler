#include "tiger/output/output.h"

#include <cstdio>

#include "tiger/output/logger.h"
#include "tiger/frame/frame.h"

extern frame::RegManager *reg_manager;
extern frame::Frags *frags;
std::string lastDataLabel;

namespace output {
void AssemGen::GenAssem(bool need_ra) {
  frame::Frag::OutputPhase phase;

  // Output proc
  phase = frame::Frag::Proc;
  fprintf(out_, ".text\n");
  assert(frags);
  for (auto &&frag : frags->GetList())
    frag->OutputAssem(out_, phase, need_ra);

  // Output string
  phase = frame::Frag::String;
  fprintf(out_, ".section .rodata\n");
  for (auto &&frag : frags->GetList())
    frag->OutputAssem(out_, phase, need_ra);

  // GLOBAL ROOT
  phase = frame::Frag::Data;
  fprintf(out_, ".data\n");
  fprintf(out_, ".global GLOBAL_GC_ROOTS\n");
  fprintf(out_, "GLOBAL_GC_ROOTS:\n");
  lastDataLabel = "GLOBAL_GC_ROOTS";
  for (auto &&frag : frags->GetList()) {
    frag->OutputAssem(out_, phase, need_ra);
  }
  fprintf(out_, ".global EndData\n");
  fprintf(out_, "EndData:\n");
  fprintf(out_, (".quad " + lastDataLabel + "\n").c_str());
}

} // namespace output

namespace frame {

assem::Proc *ProcEntryExit3(Frame *pFrame, assem::InstrList *pList) {
  std::string prolog, epilog;
  int spill_size = std::max(pFrame->MaxArgs() - 6, 0) * reg_manager->WordSize();
  int stack_size = spill_size + pFrame->Size();
  std::string frame_size =
      ".set " + temp::LabelFactory::LabelString(pFrame->name_) + "_framesize, ";
  prolog = frame_size + std::to_string(stack_size) + "\n";
  prolog += temp::LabelFactory::LabelString(pFrame->name_) + ":\n";
  prolog += "subq $" + std::to_string(stack_size) + ", %rsp\n";

  epilog = "addq $" + std::to_string(stack_size) + ", %rsp\n";
  epilog += "retq\n";
  return new assem::Proc(prolog, pList, epilog);
}

void ProcFrag::OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const {
  std::unique_ptr<canon::Traces> traces;
  std::unique_ptr<cg::AssemInstr> assem_instr;
  std::unique_ptr<ra::Result> allocation;

  // When generating proc fragment, do not output string assembly
  if (phase != Proc)
    return;

  TigerLog("-------====IR tree=====-----\n");
  //TigerLog(body_);

  {
    // Canonicalize
    TigerLog("-------====Canonicalize=====-----\n");
    canon::Canon canon(body_);

    // Linearize to generate canonical trees
    TigerLog("-------====Linearlize=====-----\n");
    tree::StmList *stm_linearized = canon.Linearize();
    //TigerLog(stm_linearized);

    // Group list into basic blocks
    TigerLog("------====Basic block_=====-------\n");
    canon::StmListList *stm_lists = canon.BasicBlocks();
    //TigerLog(stm_lists);

    // Order basic blocks into traces_
    TigerLog("-------====Trace=====-----\n");
    tree::StmList *stm_traces = canon.TraceSchedule();
    //TigerLog(stm_traces);

    traces = canon.TransferTraces();
  }

  temp::Map *color =
      temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
  {
    // Lab 5: code generation
    TigerLog("-------====Code generate=====-----\n");
    cg::CodeGen code_gen(frame_, std::move(traces));
    code_gen.Codegen();
    assem_instr = code_gen.TransferAssemInstr();
    TigerLog(assem_instr.get(), color);
  }

  assem::InstrList *il = assem_instr.get()->GetInstrList();
  
  if (need_ra) {
    // Lab 6: register allocation
    TigerLog("----====Register allocate====-----\n");
    ra::RegAllocator reg_allocator(frame_, std::move(assem_instr));
    reg_allocator.RegAlloc();
    allocation = reg_allocator.TransferResult();
    il = allocation->il_;
    color = temp::Map::LayerMap(reg_manager->temp_map_, allocation->coloring_);
  }

  TigerLog("-------====Output assembly for %s=====-----\n",
           frame_->name_->Name().data());

  assem::Proc *proc = frame::ProcEntryExit3(frame_, il);

  std::string proc_name = frame_->GetLabel();

  fprintf(out, ".globl %s\n", proc_name.data());
  fprintf(out, ".type %s, @function\n", proc_name.data());
  // prologue
  fprintf(out, "%s", proc->prolog_.data());
  // body
  proc->body_->Print(out, color);
  // epilog_
  fprintf(out, "%s", proc->epilog_.data());
  fprintf(out, ".size %s, .-%s\n", proc_name.data(), proc_name.data());
}

void StringFrag::OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const {
  // When generating string fragment, do not output proc assembly
  if (phase != String)
    return;

  fprintf(out, "%s:\n", label_->Name().data());
  int length = static_cast<int>(str_.size());
  // It may contain zeros in the middle of string. To keep this work, we need
  // to print all the charactors instead of using fprintf(str)
  fprintf(out, ".long %d\n", length);
  fprintf(out, ".string \"");
  for (int i = 0; i < length; i++) {
    if (str_[i] == '\n') {
      fprintf(out, "\\n");
    } else if (str_[i] == '\t') {
      fprintf(out, "\\t");
    } else if (str_[i] == '\"') {
      fprintf(out, "\\\"");
    } else {
      fprintf(out, "%c", str_[i]);
    }
  }
  fprintf(out, "\"\n");
}
void DataFrag::OutputAssem(FILE *out, Frag::OutputPhase phase,
                           bool need_ra) const {
  if (phase != Data)
    return;
  // label
  fprintf(out, "L_map_%s:\n", label_->Name().data());
  // lastPointer
  fprintf(out, (".quad " + lastDataLabel + "\n").c_str());
  //size
  fprintf(out, ".quad 0x%x\n", frame_->Size());
  fprintf(out, ".quad %s\n", label_->Name().c_str());
  int size = frame_->Size();
  for (auto it : *frame_->Formals()) {
    if (it->getIsPointer()) {
      auto frame_access = (frame::InFrameAccess*)it;
      fprintf(out, ".quad 0x%x\n", frame_access->offset + size);
    }
  }
  //end
  fprintf(out, ".quad -1\n");

  lastDataLabel = "L_map_" + label_->Name();
}

} // namespace frame
