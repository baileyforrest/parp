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
  if (auto* analyzed = expr->GetAsAnalyzed()) {
    return analyzed->func()(env);
  }

  return expr;
}

Expr* Analyze(Expr* expr);
Expr* AnalyzeApplication(expr::Pair* pair);

Expr* Analyze(Expr* expr) {
  switch (expr->type()) {
    case Expr::Type::EMPTY_LIST:
    case Expr::Type::BOOL:
    case Expr::Type::NUMBER:
    case Expr::Type::CHAR:
    case Expr::Type::STRING:
    case Expr::Type::VECTOR:
      return expr;

    case Expr::Type::SYMBOL: {
      auto* sym = expr->GetAsSymbol();
      return expr::Analyzed::Create(
          expr, [sym](expr::Env* env) { return env->Lookup(sym); },
          {expr});  // NOLINT(whitespace/newline)
    }
    case Expr::Type::PAIR:
      return AnalyzeApplication(expr->GetAsPair());

    case Expr::Type::LAMBDA:
    case Expr::Type::ENV:
    case Expr::Type::PRIMITIVE:
    case Expr::Type::ANALYZED:
      // Analyze called twice
      assert(false);
      break;
  }

  return nullptr;
}

Expr* AnalyzeApplication(expr::Pair* pair) {
  auto* op = Analyze(pair->car());
  auto args = ExprVecFromList(pair->cdr());
  auto refs = args;
  refs.push_back(op);

  return expr::Analyzed::Create(
      pair,
      [op, args](expr::Env* env) {
        auto eargs = args;
        for (auto& e : eargs) {
          std::cout << *e << "\n";
          e = DoEval(e, env);
        }

        auto* func = DoEval(op, env);
        if (auto* primitive = func->GetAsPrimitive()) {
          return primitive->Eval(env, eargs.data(), args.size());
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
        auto arg_it = eargs.begin();
        std::vector<std::pair<expr::Symbol*, Expr*>> bindings;

        for (auto* sym : lambda->required_args()) {
          bindings.emplace_back(sym, *(arg_it++));
        }
        if (lambda->variable_arg() != nullptr) {
          auto* rest = ListFromIt(arg_it, eargs.end());
          bindings.emplace_back(lambda->variable_arg(), rest);
        }

        auto* new_env = expr::Env::Create(bindings, lambda->env());

        return DoEval(lambda->body(), new_env);
      },
      std::move(refs));
}

}  // namespace

Expr* Eval(Expr* expr, expr::Env* env) {
  auto* e = Analyze(expr);
  return DoEval(e, env);
}

}  // namespace eval
