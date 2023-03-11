#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

#include <list>
#include <memory>

#include "tiger/absyn/absyn.h"
#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/x64frame.h"
#include "tiger/semant/types.h"

namespace tr {

class Exp;
class ExpAndTy;
class Level;

class PatchList {
public:
  void DoPatch(temp::Label *label) {
    for (auto &patch : patch_list_)
      *patch = label;
  }

  static PatchList JoinPatch(const PatchList &first, const PatchList &second) {
    PatchList ret(first.GetList());
    for (auto &patch : second.patch_list_) {
      ret.patch_list_.push_back(patch);
    }
    return ret;
  }

  explicit PatchList(std::list<temp::Label **> patch_list)
      : patch_list_(patch_list) {}

  PatchList(temp::Label **one) {
    patch_list_.push_back(one);
  }
  PatchList() = default;

  [[nodiscard]] const std::list<temp::Label **> &GetList() const {
    return patch_list_;
  }

private:
  std::list<temp::Label **> patch_list_;
};



class Level {
public:
  frame::Frame *frame_;
  Level *parent_;
  Level() {}
  Level(Level* parent, frame::Frame *frame1) : parent_(parent), frame_(frame1){}
  static Level *NewLevel(Level* parent, temp::Label* label, std::list<bool>* formals) {
    if (!formals) {
      formals = new std::list<bool> { true};
    }
    else formals->push_front(true);
    return new Level(parent, new frame::X64Frame(label, formals));
  }
};

class ProgTr {
public:
  // TODO: Put your lab5 code here */
  ProgTr(std::unique_ptr<absyn::AbsynTree> absynTree,
         std::unique_ptr<err::ErrorMsg> errorMsg)
      : absyn_tree_(std::move(absynTree)),
        errormsg_(std::move(errorMsg)),
        tenv_(std::make_unique<env::TEnv>()),
        venv_(std::make_unique<env::VEnv>()),
        main_level_(std::make_unique<Level>()){
    //errormsg_->Error(255, "\nStart construct ProcTr\n");
  }
  /**
   * Translate IR tree
   */
  void Translate();

  /**
   * Transfer the ownership of errormsg to outer scope
   * @return unique pointer to errormsg
   */
  std::unique_ptr<err::ErrorMsg> TransferErrormsg() {
    return std::move(errormsg_);
  }

private:
  std::unique_ptr<absyn::AbsynTree> absyn_tree_;
  std::unique_ptr<err::ErrorMsg> errormsg_;
  std::unique_ptr<Level> main_level_;
  std::unique_ptr<env::TEnv> tenv_;
  std::unique_ptr<env::VEnv> venv_;

  // Fill base symbol for var env and type env
  void FillBaseVEnv();
  void FillBaseTEnv();
};

} // namespace tr

#endif
