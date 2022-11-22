#include "tiger/absyn/absyn.h"
#include "tiger/semant/semant.h"

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(sym_);  // check if pre_declared
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    return (static_cast<env::VarEntry*>(entry))->ty_->ActualTy();
  }
  else {
    errormsg->Error(pos_, "undefined variable %s",
                    sym_->Name().data());
  }
  return type::IntTy::Instance();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*var_ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }
  type::FieldList *list = (((type::RecordTy *)var_ty)->fields_);
  for (type::Field *field : list->GetList()) {
     if (field->name_ == sym_) { // match
        return field->ty_->ActualTy();
     }
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return type::VoidTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *sub_ty = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*var_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(var_->pos_, "array type required");
    return type::VoidTy::Instance();
  }
  if (typeid(*sub_ty) != typeid(type::IntTy)) {
    errormsg->Error(subscript_->pos_, "exp should be an integer");
  }
  return (((type::ArrayTy *)var_ty)->ty_->ActualTy());
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *function = venv->Look(func_);
  if (function && typeid(*function) == typeid(env::FunEntry)) {
      if (args_->GetList().size() < ((env::FunEntry *)function)->formals_->GetList().size()) {
        errormsg->Error(pos_, "para type mismatch");
        return type::VoidTy::Instance();
      }
      if (args_->GetList().size() > ((env::FunEntry *)function)->formals_->GetList().size()) {
        errormsg->Error(pos_, "too many params in function %s", func_->Name().data());
        return type::VoidTy::Instance();
      }
      auto arg_it = args_->GetList().begin();
      auto formal_it = ((env::FunEntry *)function)->formals_->GetList().begin();
      for (; arg_it != args_->GetList().end() && formal_it != ((env::FunEntry *)function)->formals_->GetList().end(); arg_it++, formal_it++) {
        type::Ty *arg_ty = (*arg_it)->SemAnalyze(venv, tenv, labelcount, errormsg);
        if ((typeid(*arg_ty) != typeid(*((*formal_it)->ActualTy()))) && (!arg_ty->IsSameType(type::NilTy::Instance())))
          errormsg->Error((*arg_it)->pos_, "para type mismatch");
      }
    return ((env::FunEntry *)function)->result_->ActualTy();
  }
  else {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return type::VoidTy::Instance();
  }
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //printf("exectuting opex");
  type::Ty *left_ty = left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_ty = right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (oper_ == absyn::Oper::PLUS_OP || oper_ == absyn::Oper::MINUS_OP || 
      oper_ == absyn::Oper::TIMES_OP || oper_ == absyn::Oper::DIVIDE_OP) {
        if ((typeid(*left_ty) != typeid(type::IntTy)) && (!left_ty->IsSameType(type::NilTy::Instance()))) {
          errormsg->Error(left_->pos_, "integer required");
        }
        if ((typeid(*right_ty) != typeid(type::IntTy)) && (!right_ty->IsSameType(type::NilTy::Instance()))) {
          errormsg->Error(right_->pos_, "integer required");
        }
        return type::IntTy::Instance();
      }
  else {
    if (!left_ty->IsSameType(right_ty)) {
      errormsg->Error(pos_, "same type required");
      return type::IntTy::Instance();
    }
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty_ = tenv->Look(typ_);
  if (ty_) ty_ = ty_->ActualTy();
  if (!ty_ || (typeid(*ty_) != typeid(type::RecordTy))) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::VoidTy::Instance();
  }
  //if ()
  auto list_it = ((type::RecordTy *)ty_)->fields_->GetList().begin();
  auto elist_it = fields_->GetList().begin();
  for (; list_it != ((type::RecordTy *)ty_)->fields_->GetList().end(); list_it++, elist_it++) {
     if (!(*list_it)->ty_->IsSameType((*elist_it)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg))) {
        errormsg->Error(pos_, "record type don't match");
        return type::VoidTy::Instance();
     }
  }
  return ty_;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *res_ty = type::VoidTy::Instance();
  for(absyn::Exp * exp : seq_->GetList()) {
      res_ty = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return res_ty;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *exp_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*var_) == typeid(absyn::SimpleVar)) {
    env::VarEntry *var_entry = (env::VarEntry *)venv->Look((static_cast<absyn::SimpleVar*>(var_))->sym_);
    if (var_entry->readonly_) {
       errormsg->Error(pos_, "loop variable can't be assigned");
    }
  }
  if (typeid(*var_ty) != typeid(*exp_ty) && typeid(*var_ty) != typeid(type::VoidTy) && typeid(*exp_ty) != typeid(type::VoidTy)) {
    errormsg->Error(pos_, "unmatched assign exp");
    return type::VoidTy::Instance();
  }
  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *if_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *else_ty = nullptr;
  if (elsee_) {
    else_ty = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  }
  if (!elsee_) { // if-then
    if (typeid(*then_ty) != typeid(type::VoidTy))
      errormsg->Error(pos_, "if-then exp's body must produce no value");
    else 
      return type::VoidTy::Instance();
  }
  else { // if-then-else
    if (typeid(*then_ty) != typeid(*else_ty) && typeid(*then_ty) != typeid(type::NilTy) && typeid(*else_ty) != typeid(type::NilTy)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
    }
  }
  return then_ty;
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg)->ActualTy();
  if (typeid(*body_ty) != typeid(type::VoidTy)) {
    errormsg->Error(pos_, "while body must produce no value");
  }
  return body_ty;
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *lo_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *hi_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*lo_ty) != typeid(type::IntTy)) {
      errormsg->Error(lo_->pos_, "for exp's range type is not integer");
  }
  if (typeid(*hi_ty) != typeid(type::IntTy)) {
      errormsg->Error(hi_->pos_, "for exp's range type is not integer");
  }
  venv->BeginScope();
  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));
  type::Ty *body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  venv->EndScope();
  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if (labelcount < 1) {
    errormsg->Error(pos_, "break is not inside any loop");
  }
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope();
  tenv->BeginScope();
  for (Dec *dec : decs_->GetList()) 
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *result;
  if (!body_)
    result = type::VoidTy::Instance();
  else result = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  tenv->EndScope();
  venv->EndScope();
  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *size_ty = size_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *value_ty = tenv->Look(typ_)->ActualTy();
  type::Ty *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  //printf("t1:%s t2:%s std:%s\n", typeid(*(((type::ArrayTy*)value_ty)->ty_->ActualTy())).name(), typeid(*init_ty).name(), typeid(type::IntTy).name());
  if (typeid(*(((type::ArrayTy*)value_ty)->ty_->ActualTy())) != typeid(*init_ty)) {
    errormsg->Error(pos_, "type mismatch");
    return value_ty;
  }
  return value_ty;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  for (absyn::FunDec *function : functions_->GetList()) { // the first pass
    if (venv->Look(function->name_)) // exist
      errormsg->Error(function->pos_, "two functions have the same name");
    if (function->result_) {
      type::Ty *result_ty = tenv->Look(function->result_)->ActualTy();
      venv->Enter(function->name_,
                  new env::FunEntry(function->params_->MakeFormalTyList(tenv, errormsg), result_ty));
    }
    else { // void function
      venv->Enter(function->name_,
                  new env::FunEntry(function->params_->MakeFormalTyList(tenv, errormsg), type::VoidTy::Instance()));
    }
  }
  for (absyn::FunDec *function : functions_->GetList()) { // the second pass
    venv->BeginScope();
    type::TyList *formals = function->params_->MakeFormalTyList(tenv, errormsg);
    auto formal_it = formals->GetList().begin();
    auto param_it = function->params_->GetList().begin();
    for (; param_it != function->params_->GetList().end(); formal_it++, param_it++)
      venv->Enter((*param_it)->name_, new env::VarEntry(*formal_it));
    type::Ty *ty = function->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (function->result_) {
      type::Ty *result_ty = tenv->Look(function->result_)->ActualTy();
      if (!result_ty->IsSameType(ty))
        errormsg->Error(pos_, "function return type don't match");
    }
    else {
      if (typeid(*ty) != typeid(type::VoidTy))
        errormsg->Error(pos_, "procedure returns value");
    }
    venv->EndScope();
  }
  //free(new_venv);
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typ_) { // long form
    type::Ty *type = tenv->Look(typ_)->ActualTy();
    if (!type->IsSameType(init_ty)) {
        errormsg->Error(pos_, "type mismatch");
        return ;
    }
  }
  else { // short form
    if (typeid(*init_ty) == typeid(type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
      return ;
    }
  }
  venv->Enter(var_, new env::VarEntry(init_ty));
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  for (absyn::NameAndTy *type : types_->GetList()) { // the first pass
     //printf("typename: %s\n", type->name_->Name().data());
     if (tenv->Look(type->name_)) {
        errormsg->Error(pos_, "two types have the same name");
        return ;
     }
     tenv->Enter(type->name_, new type::NameTy(type->name_, nullptr));
  }
  for (absyn::NameAndTy *type : types_->GetList()) { // for each type
    type::Ty *ty = tenv->Look(type->name_);
    ((type::NameTy *)ty)->ty_ = type->ty_->SemAnalyze(tenv, errormsg);
  }
  for (absyn::NameAndTy *type : types_->GetList()) {
    type::Ty *cur = tenv->Look(type->name_); // should be
    type::Ty *ty = cur;
    while (typeid(*ty) == typeid(type::NameTy)) { // recusive translate
      ty = ((type::NameTy *)ty)->ty_;
      if (typeid(*cur) == typeid(*ty)) {
          errormsg->Error(pos_, "illegal type cycle");
          return ;
      }
    }
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return new type::NameTy(name_, tenv->Look(name_));
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return new type::ArrayTy(tenv->Look(array_));
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
