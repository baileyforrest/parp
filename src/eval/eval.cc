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
#include "util/util.h"

using expr::Expr;

namespace eval {

namespace {

void ThrowException[[noreturn]](const Expr* expr, const std::string& msg) {
  throw util::RuntimeException("Eval error: " + msg + " " +
                               util::to_string(expr));
}

Expr* EvalExpr(Expr* expr, expr::Env* env) {
  return expr->GetAsAnalyzed() ? expr->GetAsAnalyzed()->func()(env) : expr;
}

std::vector<Expr*> ExprVecFromList(Expr* expr) {
  std::vector<Expr*> ret;
  while (auto* pair = expr->GetAsPair()) {
    ret.push_back(pair->car());
    expr = pair->cdr();
  }
  if (expr != expr::Nil())
    ThrowException(expr, "Expected '() terminated list of expressions");

  return ret;
}

Expr* Analyze(Expr* expr);
std::vector<Expr*> AnalyzeList(Expr* expr);
Expr* AnalyzePair(expr::Pair* pair);
Expr* AnalyzeLambda(expr::Pair* pair);
Expr* AnalyzeSequence(Expr* pair);
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
      return AnalyzePair(expr->GetAsPair());

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

std::vector<Expr*> AnalyzeList(Expr* expr) {
  std::vector<Expr*> analyzed;
  while (auto* pair = expr->GetAsPair()) {
    analyzed.push_back(Analyze(pair->car()));
    expr = pair->cdr();
  }
  if (expr != expr::Nil())
    ThrowException(expr, "Expected '() terminated list of expressions");

  return analyzed;
}

// TODO(bcf): These shouldn't be hard coded here.
Expr* AnalyzePair(expr::Pair* pair) {
  if (auto* sym = pair->car()->GetAsSymbol()) {
    if (sym->val() == "lambda") {
      return AnalyzeLambda(pair);
    } else if (sym->val() == "begin") {
      return AnalyzeSequence(pair->cdr());
    }
  }

  return AnalyzeApplication(pair);
}

Expr* AnalyzeLambda(expr::Pair* pair) {
  static const char kExpectSym[] = "Expected symbol as lambda argument";
  std::vector<expr::Symbol*> req_args;
  std::vector<Expr*> refs;

  Expr* cur_arg = pair->Cr("ad");
  while (auto* as_pair = cur_arg->GetAsPair()) {
    auto* arg = as_pair->car()->GetAsSymbol();
    if (!arg) {
      ThrowException(arg, kExpectSym);
    }

    req_args.push_back(arg);
    refs.push_back(arg);
    cur_arg = as_pair->cdr();
  }

  expr::Symbol* var_arg = nullptr;
  if (cur_arg != expr::Nil()) {
    if (!(var_arg = cur_arg->GetAsSymbol())) {
      ThrowException(cur_arg, kExpectSym);
    }
    refs.push_back(var_arg);
  }

  // TODO(bcf): Make sure in sequence, definitions must go first
  auto* body = AnalyzeSequence(pair->Cr("dd"));
  refs.push_back(body);

  return expr::Analyzed::Create(pair,
                                [req_args, var_arg, body](expr::Env* env) {
                                  return expr::Lambda::Create(req_args, var_arg,
                                                              body, env);
                                },
                                std::move(refs));
}

Expr* AnalyzeSequence(Expr* expr) {
  auto analyzed = AnalyzeList(expr);
  if (analyzed.size() == 0) {
    ThrowException(expr, "Empty sequence is not allowed");
  }

  return expr::Analyzed::Create(expr,
                                [analyzed](expr::Env* env) {
                                  Expr* res = expr::Nil();
                                  for (auto* e : analyzed) {
                                    res = EvalExpr(e, env);
                                  }
                                  return res;
                                },
                                analyzed);
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
          e = EvalExpr(e, env);
        }

        auto* func = EvalExpr(op, env);
        if (auto* primitive = func->GetAsPrimitive()) {
          return primitive->Eval(env, eargs.data(), args.size());
        }
        auto* lambda = func->GetAsLambda();
        if (!lambda) {
          ThrowException(lambda, "Expected a procedure");
        }

        if (args.size() < lambda->required_args().size()) {
          std::ostringstream os;
          os << "Invalid number of arguments. expected ";
          if (lambda->variable_arg() != nullptr) {
            os << "at least ";
          }

          os << lambda->required_args().size();
          os << " given: " << args.size();
          ThrowException(lambda, os.str());
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

        return EvalExpr(lambda->body(), new_env);
      },
      std::move(refs));
}

Expr* DoEval(Expr* expr, expr::Env* env) {
  switch (expr->type()) {
    // Self evaluating
    case Expr::Type::EMPTY_LIST:
    case Expr::Type::BOOL:
    case Expr::Type::NUMBER:
    case Expr::Type::CHAR:
    case Expr::Type::STRING:
    case Expr::Type::VECTOR:
      return expr;

    case Expr::Type::SYMBOL:
      return env->Lookup(expr->GetAsSymbol());

    case Expr::Type::ANALYZED:
      return expr->GetAsAnalyzed()->func()(env);

    default:
      ThrowException(expr, "Cannot evaluate");
  }
}

}  // namespace

Expr* Eval(Expr* expr, expr::Env* env) {
  auto* e = Analyze(expr);
  return DoEval(e, env);
}

}  // namespace eval
