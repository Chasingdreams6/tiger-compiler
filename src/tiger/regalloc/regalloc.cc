#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */

RegAllocator::RegAllocator(frame::Frame *frame,
                           std::unique_ptr<cg::AssemInstr> assemInstr)
    : frame_(frame), assem_instr_(std::move(assemInstr)) {}

void RegAllocator::RegAlloc() {}

std::unique_ptr<ra::Result> RegAllocator::TransferResult() {

  return std::unique_ptr<ra::Result>();
}

} // namespace ra