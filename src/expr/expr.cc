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

Var::~Var() {
}

Var *Var::GetAsVar() {
  return this;
}

Quote *Quote::GetAsQuote() {
  return this;
}

Bool *Bool::GetAsBool() {
  return this;
}

Char *Char::GetAsChar() {
  return this;
}

String::~String() {
}

String *String::GetAsString() {
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
