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
#include <utility>
#include <vector>

#include "util/exceptions.h"

using expr::Expr;
using expr::Env;

namespace eval {

namespace {

gc::Lock<Expr> DoEval(Expr* expr, expr::Env* env) {
  switch (expr->type()) {
    // If we are evaluating a EVALS directly, it must take no arguments.
    case Expr::Type::EVALS:
      return gc::Lock<Expr>(expr->AsEvals()->DoEval(env, nullptr, 0));
    case Expr::Type::SYMBOL:
      return gc::Lock<Expr>(env->Lookup(expr->AsSymbol()));
    default:
      break;
  }

  return gc::Lock<Expr>(expr);
}

class Apply : public expr::Evals {
 public:
  Apply(Expr* op, std::vector<Expr*> args) : op_(op), args_(args) {
    assert(op);
  }
  ~Apply() override = default;

  // Evals implementation:
  std::ostream& AppendStream(std::ostream& stream) const override;
  gc::Lock<Expr> DoEval(Env* env, Expr** exprs, size_t size) override;

 private:
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

gc::Lock<Expr> Apply::DoEval(Env* env, Expr** exprs, size_t size) {
  assert(!exprs);
  assert(size == 0);
  auto op = eval::DoEval(op_, env);
  auto* evals = expr::TryEvals(op.get());
  auto args_copy = args_;
  return evals->DoEval(env, args_copy.data(), args_copy.size());
}

}  // namespace

gc::Lock<Expr> Analyze(Expr* expr) {
  auto* pair = expr->AsPair();
  if (!pair) {
    return gc::Lock<Expr>(expr);
  }

  auto op = Analyze(pair->car());
  auto args = ExprVecFromList(pair->cdr());
  return gc::Lock<Expr>(new Apply(op.get(), std::move(args)));
}

gc::Lock<Expr> Eval(Expr* expr, expr::Env* env) {
  auto e = Analyze(expr);
  return DoEval(e.get(), env);
}

}  // namespace eval
