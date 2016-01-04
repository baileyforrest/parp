/*
 * Copyright (C) 2015 Bailey Forrest <baileycforrest@gmail.com>
 *
 * This file is part of parp.
 *
 * parp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * parp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with parp.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "expr/expr.h"

#include <cassert>

namespace expr {

bool Expr::IsDatum() const {
  switch (type()) {
    case Expr::Type::BOOL:
    case Expr::Type::NUMBER:
    case Expr::Type::CHAR:
    case Expr::Type::STRING:
    case Expr::Type::SYMBOL:
    case Expr::Type::PAIR:
    case Expr::Type::VECTOR:
      return true;
    default:
      return false;
  }
}

Bool *Expr::GetAsBool() {
  assert(false);
  return nullptr;
}

Number *Expr::GetAsNumber() {
  assert(false);
  return nullptr;
}

Char *Expr::GetAsChar() {
  assert(false);
  return nullptr;
}

String *Expr::GetAsString() {
  assert(false);
  return nullptr;
}

Symbol *Expr::GetAsSymbol() {
  assert(false);
  return nullptr;
}

Pair *Expr::GetAsPair() {
  assert(false);
  return nullptr;
}

Vector *Expr::GetAsVector() {
  assert(false);
  return nullptr;
}


Var *Expr::GetAsVar() {
  assert(false);
  return nullptr;
}

Literal *Expr::GetAsLiteral() {
  assert(false);
  return nullptr;
}


Apply *Expr::GetAsApply() {
  assert(false);
  return nullptr;
}

Lambda *Expr::GetAsLambda() {
  assert(false);
  return nullptr;
}

Cond *Expr::GetAsCond() {
  assert(false);
  return nullptr;
}

Assign *Expr::GetAsAssign() {
  assert(false);
  return nullptr;
}

LetSyntax *Expr::GetAsLetSyntax() {
  assert(false);
  return nullptr;
}

// static
Bool *Bool::Create(bool val) {
  return static_cast<Bool *>(
      gc::Gc::Get().Alloc(sizeof(Bool), [val](void *addr) {
        return new(addr) Bool(val);
      }));
}

Bool *Bool::GetAsBool() {
  return this;
}

// static
Char *Char::Create(char val) {
  return static_cast<Char *>(
      gc::Gc::Get().Alloc(sizeof(Char), [val](void *addr) {
        return new(addr) Char(val);
      }));
}

Char *Char::GetAsChar() {
  return this;
}

// static
String *String::Create(const std::string &val) {
  return static_cast<String *>(
      gc::Gc::Get().Alloc(sizeof(String), [val](void *addr) {
        return new(addr) String(val);
      }));
}

String::~String() {
}

String *String::GetAsString() {
  return this;
}

// static
Symbol *Symbol::Create(const std::string &val) {
  return static_cast<Symbol *>(
      gc::Gc::Get().Alloc(sizeof(Symbol), [val](void *addr) {
        return new(addr) Symbol(val);
      }));
}

Symbol::~Symbol() {
}

Symbol *Symbol::GetAsSymbol() {
  return this;
}

// static
Pair *Pair::Create(Expr *car, Expr *cdr) {
  return static_cast<Pair *>(
      gc::Gc::Get().Alloc(sizeof(Pair), [car, cdr](void *addr) {
        return new(addr) Pair(car, cdr);
      }));
}

Pair *Pair::GetAsPair() {
  return this;
}

// static
Vector *Vector::Create(const std::vector<Expr *> &vals) {
  return static_cast<Vector *>(
      gc::Gc::Get().Alloc(sizeof(Vector), [vals](void *addr) {
        return new(addr) Vector(vals);
      }));
}

Vector::~Vector() {
}

Vector *Vector::GetAsVector() {
  return this;
}


Var::~Var() {
}

Var *Var::GetAsVar() {
  return this;
}

Apply::Apply(const std::vector<Expr *> &exprs)
  : Expr(Type::APPLY) {
  assert(exprs.size() > 0);
  op_ = exprs[0];
  args_.reserve(exprs.size() - 1);
  for (std::size_t i = 1; i < exprs.size(); ++i) {
    args_.push_back(exprs[i]);
  }
}

Apply::~Apply() {
}

Apply *Apply::GetAsApply() {
  return this;
}

Lambda::Lambda(const std::vector<Var *> &required_args, Var *variable_arg,
    const std::vector<Expr *> &body)
  : Expr(Type::LAMBDA), required_args_(required_args),
  variable_arg_(variable_arg), body_(body) {
  // TODO(bcf): assert Error checking on body.
}

Lambda::~Lambda() {
}

Lambda *Lambda::GetAsLambda() {
  return this;
}

Cond *Cond::GetAsCond() {
  return this;
}

Assign *Assign::GetAsAssign() {
  return this;
}

LetSyntax::~LetSyntax() {
}

LetSyntax *LetSyntax::GetAsLetSyntax() {
  return this;
}

}  // namespace expr
