#include "straightline/slp.h"

#include <iostream>

// #include "slp.h"
namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(stm1->MaxArgs(), stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  Table *tmp = stm1->Interp(t);
  return stm2->Interp(tmp);
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *tmp = exp->Interp(t);
  return tmp->t->Update(id, tmp->i);
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exps->getNum();
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  Table *res = exps->Interp(t);
  std::cout << std::endl;
  return res;
}

IntAndTable *A::IdExp::Interp(Table *t) const {
    return new IntAndTable(t->Lookup(id), t);
}

IntAndTable *A::NumExp::Interp(Table *t) const {
    return new IntAndTable(num, t);
}

IntAndTable *A::OpExp::Interp(Table *t) const {
  switch (oper)
  {
    case PLUS:
      return new IntAndTable(left->Interp(t)->i + right->Interp(t)->i, t);
    case MINUS:
      return new IntAndTable(left->Interp(t)->i - right->Interp(t)->i, t);
    case TIMES:
      return new IntAndTable(left->Interp(t)->i * right->Interp(t)->i, t);
    case DIV:
      return new IntAndTable(left->Interp(t)->i / right->Interp(t)->i, t);
  }
}

IntAndTable *A::EseqExp::Interp(Table *t) const {
  Table* tmp = stm->Interp(t);
  IntAndTable* tmp2 = exp->Interp(tmp);
  return new IntAndTable(tmp2->i, tmp2->t);
}

Table *A::PairExpList::Interp(Table *t) const {
  IntAndTable* tmp2 = exp->Interp(t);
  std::cout << tmp2->i << " ";
  return tail->Interp(tmp2->t);
}

Table *A::LastExpList::Interp(Table *t) const {
  IntAndTable* tmp2 = exp->Interp(t);
  std::cout << tmp2->i << " ";
  return tmp2->t;
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}


}  // namespace A
