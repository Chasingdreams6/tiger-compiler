#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

#define DEBUG_TRANSLATE 0

namespace tr {

class Access {
public:
  Level *level_;
  frame::Access *access_;

  Access(Level *level, frame::Access *access)
      : level_(level), access_(access) {}
  static Access *AllocLocal(Level *level, bool escape) {
    frame::Access *access = level->frame_->AllocLocal(escape);
    return new Access(level, access);
  }
};

Level *OutMost() { // father
  static Level *level = nullptr;
  if (level)
    return level;
  level = Level::NewLevel(nullptr, temp::LabelFactory::NewLabel(), nullptr);
  return level;
}

void *AllocFrag(frame::Frag *frag) {
  if (!frags) {
    frags = new frame::Frags();
  }
  frags->PushBack(frag);
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;
  ExpAndTy() {}
  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() override { return exp_; }
  [[nodiscard]] tree::Stm *UnNx() override { return new tree::ExpStm(exp_); }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    tree::CjumpStm *stm = new tree::CjumpStm(tree::NE_OP, exp_,
                                             new tree::ConstExp(0), NULL, NULL);
    PatchList *trues = new PatchList(&(stm->true_label_));
    PatchList *falses = new PatchList(&(stm->false_label_));
    return Cx(*trues, *falses, stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() override { return stm_; }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    errormsg->Error(errormsg->GetTokPos(), "NxExp can't be condition");
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(t);
    cx_.falses_.DoPatch(f);
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
        new tree::EseqExp(
            cx_.stm_,
            new tree::EseqExp(
                new tree::LabelStm(f),
                new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r),
                                                    new tree::ConstExp(0)),
                                  new tree::EseqExp(new tree::LabelStm(t),
                                                    new tree::TempExp(r))))));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(t);
    cx_.falses_.DoPatch(f);
    return new tree::SeqStm(cx_.stm_, new tree::SeqStm(new tree::LabelStm(t),
                                                       new tree::LabelStm(f)));
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override { return cx_; }
};

void ProgTr::Translate() {
  temp::Label *main_label = temp::LabelFactory::NamedLabel("tigermain");
  // errormsg_->Error(255, "Before reset main_level");
  main_level_.reset(Level::NewLevel(OutMost(), main_label, nullptr));
  FillBaseTEnv();
  FillBaseVEnv();
  auto stm = absyn_tree_
                 ->Translate(venv_.get(), tenv_.get(), main_level_.get(),
                             temp::LabelFactory::NamedLabel("tigermain"),
                             errormsg_.get())
                 ->exp_->UnNx();
  frame::Frag *frag = new frame::ProcFrag(stm, main_level_->frame_);
  AllocFrag(frag);
  // errormsg_->Error(255, "End Translate");
}

} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  auto *res = new tr::ExpAndTy();
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    auto *varEntry = static_cast<env::VarEntry *>(entry);
    tree::Exp *fp = new tree::TempExp(frame::FP());
    res->ty_ = varEntry->ty_->ActualTy();
    tr::Level *dst_lv = varEntry->access_->level_, *cur_lv = level;
    while (cur_lv != dst_lv) {
      fp = cur_lv->frame_->Formals()->front()->ToExp(fp);
      cur_lv = cur_lv->parent_;
    }
    res->exp_ = new tr::ExExp(varEntry->access_->access_->ToExp(fp));
  } else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
    abort();
  }
  return res;
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *var_res = var_->Translate(venv, tenv, level, label, errormsg);
  if (typeid(*(var_res->ty_)) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    abort();
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::IntTy::Instance());
  }
  type::FieldList *list = (((type::RecordTy *)var_res->ty_)->fields_);
  int length = 0;
  for (type::Field *field : list->GetList()) {
    if (field->name_ == sym_) { // match
      return new tr::ExpAndTy(new tr::ExExp(new tree::MemExp(new tree::BinopExp(
                                  tree::PLUS_OP, var_res->exp_->UnEx(),
                                  new tree::ConstExp(length)))),
                              field->ty_->ActualTy());
    }
    length += reg_manager->WordSize();
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  abort();
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::VoidTy::Instance());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *sub_res =
      subscript_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *var_res = var_->Translate(venv, tenv, level, label, errormsg);
  if (typeid(*(var_res->ty_->ActualTy())) != typeid(type::ArrayTy)) {
    errormsg->Error(var_->pos_, "array type required");
    abort();
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }
  if (typeid(*sub_res->ty_->ActualTy()) != typeid(type::IntTy)) {
    errormsg->Error(subscript_->pos_, "exp should be an integer");
    abort();
  }
  // printf("type is %s\n", typeid(var_res->ty_->ActualTy()).name());

  return new tr::ExpAndTy(
      new tr::ExExp(new tree::MemExp(new tree::BinopExp(
          tree::PLUS_OP, var_res->exp_->UnEx(),
          new tree::BinopExp(tree::MUL_OP,
                             new tree::ConstExp(reg_manager->WordSize()),
                             sub_res->exp_->UnEx())))),
      dynamic_cast<type::ArrayTy *>(var_res->ty_)->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)),
                          type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  temp::Label *string_label = temp::LabelFactory::NewLabel();
  frame::StringFrag *string_frag = new frame::StringFrag(string_label, str_);
  tr::AllocFrag(string_frag);
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(string_label)),
                          type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(func_);
  if (DEBUG_TRANSLATE) {
    std::string output =
        "call " + func_->Name() + " " +
        std::string(
            typeid(*dynamic_cast<env::FunEntry *>(entry)->result_->ActualTy())
                .name());
    std::string flag = std::to_string(
        dynamic_cast<env::FunEntry *>(entry)->result_->IsSameType(
            type::IntTy::Instance()));
    errormsg->Error(pos_, output + " " + flag);
  }
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    abort();
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }
  if (args_->GetList().size() <
      ((env::FunEntry *)entry)->formals_->GetList().size()) {
    errormsg->Error(pos_, "para type mismatch, actual smaller size");
    abort();
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }
  if (args_->GetList().size() >
      ((env::FunEntry *)entry)->formals_->GetList().size()) {
    errormsg->Error(pos_, "too many params in function %s",
                    func_->Name().data());
    abort();
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }
  auto arg_it = args_->GetList().begin();
  auto formal_it = ((env::FunEntry *)entry)->formals_->GetList().begin();
  auto *arg_exps = new tree::ExpList;
  for (; arg_it != args_->GetList().end() &&
         formal_it != ((env::FunEntry *)entry)->formals_->GetList().end();
       arg_it++, formal_it++) {
    tr::ExpAndTy *arg_res =
        (*arg_it)->Translate(venv, tenv, level, label, errormsg);
    if (!arg_res->ty_->IsSameType((*formal_it)->ActualTy())) {
      errormsg->Error((*arg_it)->pos_, "para type mismatch");
      printf("actual is %s\n", typeid(*arg_res->ty_).name());
      printf("formal is %s\n", typeid(*(*formal_it)->ActualTy()).name());
      // printf("%d\n", typeid(*arg_res->ty_).name() == typeid())
      abort();
    }
    arg_exps->Append(arg_res->exp_->UnEx());
  }
  // add static link
  tr::Level *caller_level = level,
            *callee_level = static_cast<env::FunEntry *>(entry)->level_;
  tree::Exp *exp = new tree::TempExp(frame::FP());
  if (callee_level->parent_ != caller_level) {
    while (caller_level && caller_level->parent_ != callee_level->parent_) {
      exp = caller_level->frame_->Formals()->front()->ToExp(exp);
      caller_level = caller_level->parent_;
    }
    exp = caller_level->frame_->Formals()->front()->ToExp(exp);
  }
  // attention, runtime function shouldn't pass static link as first parameter
  if (callee_level->parent_ != tr::OutMost()) {
    arg_exps->Insert(exp);
  }
  // this is important for spill
  caller_level->frame_->maxArgs = std::max(caller_level->frame_->maxArgs, (int)arg_exps->GetList().size());
  //  printf(
  //      "End translation function %s, retty=%s\n", func_->Name().c_str(),
  //      typeid(dynamic_cast<env::FunEntry
  //      *>(entry)->result_->ActualTy()).name());
  tree::Exp *res_exp = new tree::CallExp(new tree::NameExp(func_), arg_exps);
  return new tr::ExpAndTy(
      new tr::ExExp(res_exp),
      dynamic_cast<env::FunEntry *>(entry)->result_->ActualTy());
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tree::CjumpStm *stm = nullptr, *stm2 = nullptr;
  tree::Stm *res = nullptr;
  tr::PatchList *trues = nullptr, *falses = nullptr;
  tr::ExpAndTy *left_res, *right_res;
  tree::Exp *left_exp, *right_exp;
  temp::Label *z;
  // TODO may need string cmp
  left_res = left_->Translate(venv, tenv, level, label, errormsg);
  right_res = right_->Translate(venv, tenv, level, label, errormsg);
  left_exp = left_res->exp_->UnEx();
  right_exp = right_res->exp_->UnEx();
  if (oper_ == PLUS_OP || oper_ == MINUS_OP || oper_ == TIMES_OP ||
      oper_ == DIVIDE_OP || oper_ == LE_OP || oper_ == LT_OP ||
      oper_ == GE_OP || oper_ == GT_OP || oper_ == EQ_OP || oper_ == NEQ_OP) {
    if (!left_res->ty_->IsSameType(right_res->ty_)) {
      errormsg->Error(pos_, "same type required");
      abort();
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                              type::IntTy::Instance());
    }
  }
    if (left_res->ty_->IsSameType(type::StringTy::Instance())) {
      left_exp = new tree::CallExp(
          new tree::NameExp(temp::LabelFactory::NamedLabel("string_equal")),
          new tree::ExpList({left_exp, right_exp}));
      right_exp = new tree::ConstExp(1);
    }
  //  if (typeid(*left_res->ty_) != typeid(type::IntTy) &&
  //      (!left_res->ty_->IsSameType(type::NilTy::Instance()))) {
  //    errormsg->Error(left_->pos_, "integer required");
  //    abort();
  //    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
  //                            type::IntTy::Instance());
  //  }
  //  if ((typeid(*right_res->ty_) != typeid(type::IntTy)) &&
  //      (!right_res->ty_->IsSameType(type::NilTy::Instance()))) {
  //    errormsg->Error(right_->pos_, "integer required");
  //    abort();
  //    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
  //                            type::IntTy::Instance());
  //  }
  switch (oper_) {
  case absyn::Oper::AND_OP:
    z = temp::LabelFactory::NewLabel();
    stm = new tree::CjumpStm(tree::NE_OP, left_exp, new tree::ConstExp(0),
                             z, nullptr);
    stm2 = new tree::CjumpStm(tree::NE_OP, right_exp, new tree::ConstExp(0),
                              nullptr, nullptr);
    res = new tree::SeqStm(stm, new tree::SeqStm(new tree::LabelStm(z), stm2));
    trues = new tr::PatchList(&stm2->true_label_);
    falses = new tr::PatchList({&stm->false_label_, &stm2->false_label_});
    return new tr::ExpAndTy(new tr::CxExp(*trues, *falses, res),
                            type::IntTy::Instance());
  case absyn::Oper::OR_OP:
    z = temp::LabelFactory::NewLabel();
    stm = new tree::CjumpStm(tree::EQ_OP, left_exp, new tree::ConstExp(1),
                             nullptr, z);
    stm2 = new tree::CjumpStm(tree::EQ_OP, right_exp, new tree::ConstExp(1),
                              nullptr, nullptr);
    res = new tree::SeqStm(stm, new tree::SeqStm(new tree::LabelStm(z), stm2));
    trues = new tr::PatchList({&stm->true_label_, &stm2->true_label_});
    falses = new tr::PatchList(&stm2->false_label_);
    return new tr::ExpAndTy(new tr::CxExp(*trues, *falses, res),
                            type::IntTy::Instance());
  }
  switch (oper_) {
  case absyn::Oper::PLUS_OP:
    return new ::tr::ExpAndTy(
        new tr::ExExp(new tree::BinopExp(tree::PLUS_OP, left_res->exp_->UnEx(),
                                         right_res->exp_->UnEx())),
        type::IntTy::Instance());
  case absyn::Oper::MINUS_OP:
    return new ::tr::ExpAndTy(
        new tr::ExExp(new tree::BinopExp(tree::MINUS_OP, left_res->exp_->UnEx(),
                                         right_res->exp_->UnEx())),
        type::IntTy::Instance());
  case absyn::Oper::TIMES_OP:
    return new ::tr::ExpAndTy(
        new tr::ExExp(new tree::BinopExp(tree::MUL_OP, left_res->exp_->UnEx(),
                                         right_res->exp_->UnEx())),
        type::IntTy::Instance());
  case absyn::Oper::DIVIDE_OP:
    return new ::tr::ExpAndTy(
        new tr::ExExp(new tree::BinopExp(tree::DIV_OP, left_res->exp_->UnEx(),
                                         right_res->exp_->UnEx())),
        type::IntTy::Instance());
  case absyn::LE_OP:
    stm =
        new tree::CjumpStm(tree::LE_OP, left_exp, right_exp, nullptr, nullptr);
    trues = new tr::PatchList(&stm->true_label_);
    falses = new tr::PatchList(&stm->false_label_);
    return new tr::ExpAndTy(new tr::CxExp(*trues, *falses, stm),
                            type::IntTy::Instance());
  case absyn::LT_OP:
    stm =
        new tree::CjumpStm(tree::LT_OP, left_exp, right_exp, nullptr, nullptr);
    trues = new tr::PatchList(&stm->true_label_);
    falses = new tr::PatchList(&stm->false_label_);
    return new tr::ExpAndTy(new tr::CxExp(*trues, *falses, stm),
                            type::IntTy::Instance());
  case absyn::GE_OP:
    stm =
        new tree::CjumpStm(tree::GE_OP, left_exp, right_exp, nullptr, nullptr);
    trues = new tr::PatchList(&stm->true_label_);
    falses = new tr::PatchList(&stm->false_label_);
    return new tr::ExpAndTy(new tr::CxExp(*trues, *falses, stm),
                            type::IntTy::Instance());
  case absyn::GT_OP:
    stm =
        new tree::CjumpStm(tree::GT_OP, left_exp, right_exp, nullptr, nullptr);
    trues = new tr::PatchList(&stm->true_label_);
    falses = new tr::PatchList(&stm->false_label_);
    return new tr::ExpAndTy(new tr::CxExp(*trues, *falses, stm),
                            type::IntTy::Instance());
  case absyn::EQ_OP:
    stm =
        new tree::CjumpStm(tree::EQ_OP, left_exp, right_exp, nullptr, nullptr);
    trues = new tr::PatchList(&stm->true_label_);
    falses = new tr::PatchList(&stm->false_label_);
    return new tr::ExpAndTy(new tr::CxExp(*trues, *falses, stm),
                            type::IntTy::Instance());
  case absyn::NEQ_OP:
    stm =
        new tree::CjumpStm(tree::NE_OP, left_exp, right_exp, nullptr, nullptr);
    trues = new tr::PatchList(&stm->true_label_);
    falses = new tr::PatchList(&stm->false_label_);
    return new tr::ExpAndTy(new tr::CxExp(*trues, *falses, stm),
                            type::IntTy::Instance());
  default:
    errormsg->Error(pos_, "unsupported arithmetic operation %d", oper_);
    abort();
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::IntTy::Instance());
  }
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(typ_);
  if (ty)
    ty = ty->ActualTy();
  if (!ty || (typeid(*ty) != typeid(type::RecordTy))) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    abort();
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }
  std::vector<tr::Exp *> record_exps;
  auto list_it =
      ((type::RecordTy *)ty)->fields_->GetList().begin(); // type's args
  auto elist_it = fields_->GetList().begin();             // actual args
  if (fields_->GetList().size() !=
      ((type::RecordTy *)ty)->fields_->GetList().size()) {
    errormsg->Error(pos_, "different number's of type");
    abort();
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }
  for (; list_it != ((type::RecordTy *)ty)->fields_->GetList().end();
       list_it++, elist_it++) {
    tr::ExpAndTy *exp_ty =
        (*elist_it)->exp_->Translate(venv, tenv, level, label, errormsg);
    if (list_it != ((type::RecordTy *)ty)->fields_->GetList().end() &&
        !(*list_it)->ty_->IsSameType(exp_ty->ty_)) {
      errormsg->Error(pos_, "record type don't match");
      abort();
      return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
    }
    record_exps.push_back(exp_ty->exp_);
  }
  // allocate memory for record
  tree::ExpList *runtime_args = new tree::ExpList({new tree::ConstExp(
      fields_->GetList().size() * reg_manager->WordSize())});
  temp::Temp *record = temp::TempFactory::NewTemp();
  auto *alloc_stm = new tree::MoveStm(
      new tree::TempExp(record),
      new tree::CallExp(
          new tree::NameExp(temp::LabelFactory::NamedLabel("alloc_record")),
          runtime_args));
  // init value to the record type
  tree::Stm *init_stm = nullptr, *init_tail = nullptr;
  for (int i = 0; i < fields_->GetList().size(); ++i) {
    tree::Stm *tmp_stm =
        new tree::MoveStm(new tree::MemExp(new tree::BinopExp(
                              tree::PLUS_OP, new tree::TempExp(record),
                              new tree::ConstExp(i * reg_manager->WordSize()))),
                          record_exps[i]->UnEx());
    if (!init_stm) {
      init_tail = init_stm = new tree::SeqStm(tmp_stm, init_stm);
    } else
      init_stm = new tree::SeqStm(tmp_stm, init_stm);
  }
  dynamic_cast<tree::SeqStm *>(init_tail)->right_ =
      new tree::ExpStm(new tree::ConstExp(0));
  return new tr::ExpAndTy(
      new tr::ExExp(new tree::EseqExp(new tree::SeqStm(alloc_stm, init_stm),
                                      new tree::TempExp(record))),
      ty);
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  tree::SeqStm *seqStm = nullptr, *seqLast = nullptr;
  for (auto i = seq_->GetList().begin(), j = --seq_->GetList().end(); i != j;
       ++i) {
    auto exp = *i;
    tr::ExpAndTy *exp_res = exp->Translate(venv, tenv, level, label, errormsg);
    if (seqStm) {
      seqLast->right_ = new tree::SeqStm(exp_res->exp_->UnNx(), nullptr);
      seqLast = static_cast<tree::SeqStm *>(seqLast->right_);
    } else { // first
      seqLast = seqStm = new tree::SeqStm(exp_res->exp_->UnNx(), nullptr);
    }
  }
  tr::ExpAndTy *last_one =
      (seq_->GetList().back())->Translate(venv, tenv, level, label, errormsg);
  if (seqStm) {
    seqLast->right_ = new tree::ExpStm(new tree::ConstExp(0));
  }
  if (seqStm)
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(seqStm, last_one->exp_->UnEx())),
        last_one->ty_);
  return new tr::ExpAndTy(new tr::ExExp(last_one->exp_->UnEx()), last_one->ty_);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *var_res = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp_res = exp_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *res_ty = type::VoidTy::Instance();
  // var in loop can't be assigned
  if (typeid(*var_) == typeid(absyn::SimpleVar)) {
    env::VarEntry *var_entry = (env::VarEntry *)venv->Look(
        (static_cast<absyn::SimpleVar *>(var_))->sym_);
    if (var_entry->readonly_) {
      errormsg->Error(pos_, "loop variable can't be assigned");
      abort();
      return new tr::ExpAndTy(nullptr, res_ty);
    }
  }
  return new tr::ExpAndTy(new tr::NxExp(new tree::MoveStm(
                              var_res->exp_->UnEx(), exp_res->exp_->UnEx())),
                          res_ty);
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *if_res = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then_res = then_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *else_res = nullptr;
  type::Ty *res_ty = type::VoidTy::Instance();
  if (elsee_) {
    else_res = elsee_->Translate(venv, tenv, level, label, errormsg);
    res_ty = then_res->ty_->ActualTy();
  }
  auto e1 = if_res->exp_->UnCx(errormsg);
  if (!elsee_) { // if-then
    if (typeid(*then_res->ty_) != typeid(type::VoidTy)) {
      errormsg->Error(pos_, "if-then exp's body must produce no value");
      abort();
      return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
    }
    temp::Label *then_label = temp::LabelFactory::NewLabel(),
                *end_label = temp::LabelFactory::NewLabel();
    e1.trues_.DoPatch(then_label);
    e1.falses_.DoPatch(end_label);
    auto else_seq = new tree::LabelStm(end_label);
    auto then_seq = new tree::SeqStm(
        new tree::LabelStm(then_label),
        new tree::SeqStm(
            then_res->exp_->UnNx(),
            new tree::JumpStm(new tree::NameExp(end_label),
                              new std::vector<temp::Label *>{end_label})));
    return new tr::ExpAndTy(new tr::NxExp(new tree::SeqStm(
                                e1.stm_, new tree::SeqStm(then_seq, else_seq))),
                            res_ty);
  } else { // if-then-else
    if (typeid(*then_res->ty_) != typeid(*else_res->ty_) &&
        typeid(*then_res->ty_) != typeid(type::NilTy) &&
        typeid(*else_res->ty_) != typeid(type::NilTy)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
      abort();
      return new tr::ExpAndTy(nullptr, res_ty);
    }
    temp::Label *then_label = temp::LabelFactory::NewLabel(),
                *end_label = temp::LabelFactory::NewLabel(),
                *else_label = temp::LabelFactory::NewLabel();
    temp::Temp *r = temp::TempFactory::NewTemp();
    e1.trues_.DoPatch(then_label);
    e1.falses_.DoPatch(else_label);
    auto else_seq = new tree::SeqStm(
        new tree::SeqStm(
            new tree::LabelStm(else_label),
            new tree::MoveStm(new tree::TempExp(r), else_res->exp_->UnEx())),
        new tree::LabelStm(end_label));
    auto then_seq = new tree::SeqStm(
        new tree::LabelStm(then_label),
        new tree::SeqStm(
            new tree::MoveStm(new tree::TempExp(r), then_res->exp_->UnEx()),
            new tree::JumpStm(new tree::NameExp(end_label),
                              new std::vector<temp::Label *>{end_label})));
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(
            new tree::SeqStm(e1.stm_, new tree::SeqStm(then_seq, else_seq)),
            new tree::TempExp(r))),
        res_ty);
  }
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *done_label = temp::LabelFactory::NewLabel();
  tr::ExpAndTy *test_res = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *body_res =
      body_->Translate(venv, tenv, level, done_label, errormsg);
  if (!body_res->ty_->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "while body must produce no value");
    abort();
  }
  tree::Stm *test_stm =
      new tree::CjumpStm(tree::EQ_OP, test_res->exp_->UnEx(),
                         new tree::ConstExp(0), done_label, body_label);
  return new tr::ExpAndTy(
      new tr::NxExp(new tree::SeqStm(
          new tree::SeqStm(new tree::LabelStm(test_label), test_stm),
          new tree::SeqStm(
              new tree::SeqStm(new tree::LabelStm(body_label),
                               body_res->exp_->UnNx()),
              new tree::SeqStm(
                  new tree::JumpStm(new tree::NameExp(test_label),
                                    new std::vector<temp::Label *>{test_label}),
                  new tree::LabelStm(done_label))))),
      type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *done_label = temp::LabelFactory::NewLabel();

  tr::ExpAndTy *lo_res = lo_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *hi_res = hi_->Translate(venv, tenv, level, label, errormsg);
  if (!lo_res->ty_->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
    abort();
  }
  if (!hi_res->ty_->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(hi_->pos_, "for exp's range type is not integer");
    abort();
  }
  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, lo_res->ty_, true));
  tr::ExpAndTy *body_res =
      body_->Translate(venv, tenv, level, done_label, errormsg);
  venv->EndScope();
  tree::Exp *i = access->access_->ToExp(new tree::TempExp(frame::FP()));
  return new tr::ExpAndTy(
      new tr::NxExp(new tree::SeqStm(
          new tree::MoveStm(i, lo_res->exp_->UnEx()),
          new tree::SeqStm(
              new tree::LabelStm(test_label),
              new tree::SeqStm(
                  new tree::CjumpStm(tree::LE_OP, i, hi_res->exp_->UnEx(),
                                     body_label, done_label),
                  new tree::SeqStm(
                      new tree::LabelStm(body_label),
                      new tree::SeqStm(
                          body_res->exp_->UnNx(),
                          new tree::SeqStm(
                              new tree::MoveStm(
                                  i, new tree::BinopExp(tree::PLUS_OP, i,
                                                        new tree::ConstExp(1))),
                              new tree::SeqStm(
                                  new tree::JumpStm(
                                      new tree::NameExp(test_label),
                                      new std::vector<temp::Label *>{
                                          test_label}),
                                  new tree::LabelStm(done_label))))))))),
      type::VoidTy::Instance());
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(
      new tr::NxExp(new tree::JumpStm(new tree::NameExp(label),
                                      new std::vector<temp::Label *>{label})),
      type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {

  if (DEBUG_TRANSLATE) {
    errormsg->Error(pos_, "Start translate Let");
  }

  tr::ExExp *res_exp = new tr::ExExp(new tree::EseqExp(nullptr, nullptr));
  tree::Stm **last_res = &(static_cast<tree::EseqExp *>(res_exp->exp_)->stm_);

  venv->BeginScope();
  tenv->BeginScope();
  for (Dec *dec : decs_->GetList()) {
    tr::Exp *dec_res = dec->Translate(venv, tenv, level, label, errormsg);
    *last_res = new tree::SeqStm(dec_res->UnNx(), nullptr);
    last_res = &static_cast<tree::SeqStm *>(*last_res)->right_;
  }
  *last_res = new tree::ExpStm(new tree::ConstExp(0));
  tr::ExpAndTy *body_res = body_->Translate(venv, tenv, level, label, errormsg);

  //  fill the value of res_exp
  static_cast<tree::EseqExp *>(res_exp->exp_)->exp_ = body_res->exp_->UnEx();
  tenv->EndScope();
  venv->EndScope();

  if (DEBUG_TRANSLATE) {
    std::string op =
        "End translate Let type=" + std::string(typeid(*body_res->ty_).name());
    errormsg->Error(pos_, op);
  }
  return new tr::ExpAndTy(res_exp, body_res->ty_);
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  type::Ty *res_ty = tenv->Look(typ_)->ActualTy();
  tr::ExpAndTy *size_res = size_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *init_res = init_->Translate(venv, tenv, level, label, errormsg);
  if (!((type::ArrayTy *)res_ty)->ty_->IsSameType(init_res->ty_->ActualTy())) {
    errormsg->Error(pos_, "type mismatch");
    abort();
    return new tr::ExpAndTy(nullptr, res_ty);
  }
  // alloc memory for array
  auto *runtime_args =
      new tree::ExpList({size_res->exp_->UnEx(), init_res->exp_->UnEx()});
  temp::Temp *arr = temp::TempFactory::NewTemp();
  tree::Stm *alloc_stm = new tree::MoveStm(
      new tree::TempExp(arr),
      new tree::CallExp(
          new tree::NameExp(temp::LabelFactory::NamedLabel("init_array")),
          runtime_args));
  return new tr::ExpAndTy(
      new tr::ExExp(new tree::EseqExp(alloc_stm, new tree::TempExp(arr))),
      res_ty);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  if (DEBUG_TRANSLATE) {
    errormsg->Error(pos_, "Start translate funcDec");
  }
  for (absyn::FunDec *function : functions_->GetList()) { // first pass
    type::Ty *ret_ty = type::VoidTy::Instance();
    if (function->result_) {
      ret_ty = tenv->Look(function->result_);
    }
    if (venv->Look(function->name_)) {
      errormsg->Error(function->pos_, "two functions have the same name");
      abort();
    }
    type::TyList *formals = function->params_->MakeFormalTyList(tenv, errormsg);
    // getEscapes
    auto *escapes = new std::list<bool>;
    for (auto formal_it : function->params_->GetList()) {
      escapes->push_back(formal_it->escape_);
    }
    auto *new_entry = new env::FunEntry(
        tr::Level::NewLevel(
            level, temp::LabelFactory::NamedLabel(function->name_->Name()),
            escapes),
        function->name_, formals, ret_ty);
    if (DEBUG_TRANSLATE) {
      std::string flag =
          std::to_string(ret_ty->IsSameType(type::IntTy::Instance()));
      std::string str =
          "fundec " + function->name_->Name() + " " + typeid(*ret_ty).name();
      errormsg->Error(pos_, str);
    }
    venv->Enter(function->name_, new_entry);
  }
  for (absyn::FunDec *function : functions_->GetList()) {
    venv->BeginScope();
    auto *fun_entry = static_cast<env::FunEntry *>(venv->Look(function->name_));
    FieldList *pf = function->params_;
    auto access_it = fun_entry->level_->frame_->Formals()->begin();
    access_it++; // Attention! Must pass the symbolic link
    for (auto param_it = pf->GetList().begin(); param_it != pf->GetList().end();
         param_it++, access_it++) { // for each params
      auto *new_acc = new tr::Access(fun_entry->level_, (*access_it));
      venv->Enter((*param_it)->name_,
                  new env::VarEntry(new_acc, tenv->Look((*param_it)->typ_)));
    }
    tr::ExpAndTy *body_res = function->body_->Translate(
        venv, tenv, fun_entry->level_, fun_entry->label_, errormsg);
    tree::Stm *fun_body =
        fun_entry->level_->frame_->ProcEntryExit1(new tree::MoveStm(
            new tree::TempExp(frame::RAX()), body_res->exp_->UnEx()));
    tr::AllocFrag(new frame::ProcFrag(fun_body, fun_entry->level_->frame_));
    venv->EndScope();
    if (function->result_) {
      type::Ty *result_ty = tenv->Look(function->result_)->ActualTy();
      if (!result_ty->IsSameType(body_res->ty_)) {
        errormsg->Error(pos_, "function return type don't match");
        abort();
        errormsg->Error(pos_, typeid(*body_res->ty_).name());
        errormsg->Error(pos_, typeid(*result_ty).name());
      }
    } else {
      if (typeid(*body_res->ty_) != typeid(type::VoidTy)) {
        errormsg->Error(pos_, "procedure returns value");
        abort();
      }
    }
  }
  return new tr::ExExp(new tree::ConstExp(0));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *init_res = init_->Translate(venv, tenv, level, label, errormsg);
  if (typ_) { // long form
    type::Ty *type = tenv->Look(typ_)->ActualTy();
    if (!type->IsSameType(init_res->ty_->ActualTy())) {
      errormsg->Error(pos_, "type mismatch");
      abort();
      return new tr::ExExp(new tree::ConstExp(0));
    }
  } else { // short form
    if (typeid(*init_res->ty_) == typeid(type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
      abort();
      return new tr::ExExp(new tree::ConstExp(0));
    }
  }
  tr::Access *acc = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(acc, init_res->ty_, false));
  return new tr::NxExp(
      new tree::MoveStm(acc->access_->ToExp(new tree::TempExp(frame::FP())),
                        init_res->exp_->UnEx()));
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  for (absyn::NameAndTy *type : types_->GetList()) {
    if (tenv->Look(type->name_)) {
      errormsg->Error(pos_, "two types have the same name");
      abort();
      return new tr::ExExp(new tree::ConstExp(0));
    }
    tenv->Enter(type->name_, new type::NameTy(type->name_, nullptr));
  }
  for (absyn::NameAndTy *type : types_->GetList()) {
    type::Ty *ty = tenv->Look(type->name_);
    ((type::NameTy *)ty)->ty_ = type->ty_->Translate(tenv, errormsg);
  }
  for (absyn::NameAndTy *type : types_->GetList()) {
    type::Ty *cur = tenv->Look(type->name_); // should be
    type::Ty *ty = cur;
    while (typeid(*ty) == typeid(type::NameTy)) { // recusive translate
      ty = ((type::NameTy *)ty)->ty_;
      if (typeid(*cur) == typeid(*ty)) {
        errormsg->Error(pos_, "illegal type cycle");
        abort();
        return new tr::ExExp(new tree::ConstExp(0));
      }
    }
  }
  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(name_);
  if (!ty) {
    errormsg->Error(pos_, "No such type");
    abort();
    return nullptr;
  }
  return new type::NameTy(name_, ty);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  return new type::ArrayTy(tenv->Look(array_));
}

} // namespace absyn
