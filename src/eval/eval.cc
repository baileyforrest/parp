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

#include "eval/eval.h"

#include <cassert>
#include <sstream>
#include <string>
#include <vector>

#include "util/exceptions.h"

using expr::Expr;
using expr::Env;

namespace eval {

namespace {

Expr* DoEval(Expr* expr, expr::Env* env) {
  switch (expr->type()) {
    // If we are evaluating a EVALS directly, it must take no arguments.
    case Expr::Type::EVALS:
      return expr->AsEvals()->Eval(env, nullptr, 0);
    case Expr::Type::SYMBOL:
      return env->Lookup(expr->AsSymbol());
    default:
      break;
  }

  return expr;
}

class Apply : public expr::Evals {
 public:
  Apply(Expr* op, const std::vector<Expr*>& args) : op_(op), args_(args) {
    // We don't want to use std::move for args_ because we want to ensure it
    // will be shrunk to the minimum size.
    assert(op);
  }

 private:
  // Evals implementation:
  std::ostream& AppendStream(std::ostream& stream) const override;
  Expr* Eval(Env* env, Expr** exprs, size_t size) const override;

  ~Apply() override = default;

  Expr* op_;
  const std::vector<Expr*> args_;
};

std::ostream& Apply::AppendStream(std::ostream& stream) const {
  stream << "(" << *op_;
  for (const auto* expr : args_) {
    stream << " " << *expr;
  }
  stream << ")";

  return stream;
}

Expr* Apply::Eval(Env* env, Expr** exprs, size_t size) const {
  assert(!exprs);
  assert(size == 0);
  auto* val = DoEval(op_, env);
  auto* evals = val->AsEvals();
  if (!evals) {
    throw util::RuntimeException("Expected evaluating value", val);
  }
  auto args_copy = args_;
  return evals->Eval(env, args_copy.data(), args_copy.size());
}

}  // namespace

Expr* Analyze(Expr* expr) {
  auto* pair = expr->AsPair();
  if (!pair) {
    return expr;
  }

  auto* op = Analyze(pair->car());
  auto args = ExprVecFromList(pair->cdr());
  auto refs = args;
  refs.push_back(op);

  return new Apply(op, args);
}

Expr* Eval(Expr* expr, expr::Env* env) {
  auto* e = Analyze(expr);
  return DoEval(e, env);
}

}  // namespace eval
