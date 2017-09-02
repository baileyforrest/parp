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

namespace eval {

namespace {

Expr* DoEval(Expr* expr, expr::Env* env) {
  switch (expr->type()) {
    case Expr::Type::ANALYZED:
      return expr->GetAsAnalyzed()->func()(env);
    case Expr::Type::SYMBOL:
      return env->Lookup(expr->GetAsSymbol());
    default:
      break;
  }

  return expr;
}

}  // namespace

Expr* Analyze(Expr* expr) {
  switch (expr->type()) {
    case Expr::Type::EMPTY_LIST:
    case Expr::Type::BOOL:
    case Expr::Type::NUMBER:
    case Expr::Type::CHAR:
    case Expr::Type::STRING:
    case Expr::Type::VECTOR:
    case Expr::Type::SYMBOL:
      return expr;

    case Expr::Type::PAIR:
      break;

    default:
      // Analyze called twice
      assert(false);
      break;
  }

  auto* pair = expr->GetAsPair();
  auto* op = Analyze(pair->car());
  auto args = ExprVecFromList(pair->cdr());
  auto refs = args;
  refs.push_back(op);

  return expr::Analyzed::Create(
      pair,
      [op, args](expr::Env* env) {
        auto* func = DoEval(op, env);
        if (auto* primitive = func->GetAsPrimitive()) {
          auto args_copy = args;
          return primitive->Eval(env, args_copy.data(), args_copy.size());
        }
        auto* lambda = func->GetAsLambda();
        if (!lambda) {
          throw new util::RuntimeException("Expected a procedure", lambda);
        }

        if (args.size() < lambda->required_args().size()) {
          std::ostringstream os;
          os << "Invalid number of arguments. expected ";
          if (lambda->variable_arg() != nullptr) {
            os << "at least ";
          }

          os << lambda->required_args().size();
          os << " given: " << args.size();
          throw new util::RuntimeException(os.str(), lambda);
        }
        auto arg_it = args.begin();
        auto* new_env = expr::Env::Create(lambda->env());

        for (auto* sym : lambda->required_args()) {
          new_env->DefineVar(sym, *arg_it++);
        }
        if (lambda->variable_arg() != nullptr) {
          auto* rest = ListFromIt(arg_it, args.end());
          new_env->DefineVar(lambda->variable_arg(), rest);
        }

        assert(lambda->body().size() >= 1);
        for (size_t i = 0; i < lambda->body().size() - 1; ++i) {
          DoEval(lambda->body()[i], new_env);
        }
        return DoEval(lambda->body().back(), new_env);
      },
      std::move(refs));
}

Expr* Eval(Expr* expr, expr::Env* env) {
  auto* e = Analyze(expr);
  return DoEval(e, env);
}

}  // namespace eval
