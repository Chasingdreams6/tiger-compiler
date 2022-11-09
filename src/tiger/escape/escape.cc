#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  root_->Traverse(env, 0);
}

void setEscapeOne(esc::EscEnvPtr env, sym::Symbol *sym, int depth) {
  esc::EscapeEntry* entry = env->Look(sym);
  if (entry && depth > entry->depth_)
    *(entry->escape_) = true;
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  setEscapeOne(env, sym_, depth);
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
  setEscapeOne(env, sym_, depth);
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  subscript_->Traverse(env, depth);
  var_->Traverse(env, depth);
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  auto arg_it = args_->GetList().begin();
  for (; arg_it != args_->GetList().end(); arg_it++) {
    (*arg_it)->Traverse(env, depth);
  }
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  left_->Traverse(env, depth);
  right_->Traverse(env, depth);
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  auto elist_it = fields_->GetList().begin();
  for (; elist_it != fields_->GetList().end(); elist_it++) {
      (*elist_it)->exp_->Traverse(env, depth);
  }
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  for (absyn::Exp * exp : seq_->GetList()) {
    exp->Traverse(env, depth);
  }
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
  var_->Traverse(env, depth);
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  test_->Traverse(env, depth);
  then_->Traverse(env, depth);
  if (elsee_)
    elsee_->Traverse(env, depth);
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  test_->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  lo_->Traverse(env, depth);
  hi_->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  //depth += 1;
  env->BeginScope();
  for (Dec *dec : decs_->GetList())
    dec->Traverse(env, depth);
  body_->Traverse(env, depth);
  env->EndScope();
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  size_->Traverse(env, depth);
  init_->Traverse(env, depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  for (absyn::FunDec * function : functions_->GetList()) {
    auto param_it = function->params_->GetList().begin();
    env->BeginScope();
    for (; param_it != function->params_->GetList().end(); param_it++) {
      //setEscapeOne(env, (*param_it)->name_, depth);
      env->Enter((*param_it)->name_,
                 new esc::EscapeEntry(depth + 1, &((*param_it)->escape_)));
    }
    function->body_->Traverse(env, depth + 1);
    env->EndScope();
  }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  init_->Traverse(env, depth);
  env->Enter(var_, new esc::EscapeEntry(depth, &(escape_)));
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
  for (absyn::NameAndTy *nameAndTy : types_->GetList()) {
    if (typeid(*(*nameAndTy).ty_) == typeid(absyn::RecordTy)) {
      auto* type_ptr = (absyn::RecordTy*) ((*nameAndTy).ty_);
      auto reclist_it = type_ptr->record_->GetList().begin();
      for (; reclist_it != type_ptr->record_->GetList().end(); reclist_it++) {
//        env->Enter((*reclist_it)->name_,
//                   new esc::EscapeEntry(depth,
//                                        &((*reclist_it)->escape_)));
          (*reclist_it)->escape_ = true;
      }
    }
  }
}

} // namespace absyn
