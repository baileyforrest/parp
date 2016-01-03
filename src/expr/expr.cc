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

Var *Expr::GetAsVar() {
  assert(false);
  return nullptr;
}

Quote *Expr::GetAsQuote() {
  assert(false);
  return nullptr;
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

Empty *Expr::GetAsEmpty() {
  assert(false);
  return nullptr;
}

Moved *Expr::GetAsMoved() {
  assert(false);
  return nullptr;
}


void Expr::Mark() {
}

Var::~Var() {
}

Var *Var::GetAsVar() {
  return this;
}

std::size_t Var::size() const {
  return sizeof(*this);
}

Quote *Quote::GetAsQuote() {
  return this;
}

std::size_t Quote::size() const {
  return sizeof(*this);
}

Bool *Bool::GetAsBool() {
  return this;
}

std::size_t Bool::size() const {
  return sizeof(*this);
}

Char *Char::GetAsChar() {
  return this;
}

std::size_t Char::size() const {
  return sizeof(*this);
}

String::~String() {
};

String *String::GetAsString() {
  return this;
}

std::size_t String::size() const {
  return sizeof(*this);
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
};

Apply *Apply::GetAsApply() {
  return this;
}

std::size_t Apply::size() const {
  return sizeof(*this);
}

Lambda::Lambda(const std::vector<Var *> &required_args, Var *variable_arg,
    std::vector<Expr *> &body)
  : Expr(Type::LAMBDA), required_args_(required_args),
  variable_arg_(variable_arg), body_(body) {
  // TODO: assert Error checking on body.
}

Lambda::~Lambda() {
};

Lambda *Lambda::GetAsLambda() {
  return this;
}

std::size_t Lambda::size() const {
  return sizeof(*this);
}

Cond *Cond::GetAsCond() {
  return this;
}

std::size_t Cond::size() const {
  return sizeof(*this);
}

Assign *Assign::GetAsAssign() {
  return this;
}

std::size_t Assign::size() const {
  return sizeof(*this);
}


LetSyntax::~LetSyntax() {
};

LetSyntax *LetSyntax::GetAsLetSyntax() {
  return this;
}

std::size_t LetSyntax::size() const {
  return sizeof(*this);
}

Empty *Empty::GetAsEmpty() {
  return this;
}

std::size_t Empty::size() const {
  return size_;
}

Moved *Moved::GetAsMoved() {
  return this;
}

std::size_t Moved::size() const {
  return sizeof(*this);
}

}  // namespace expr
