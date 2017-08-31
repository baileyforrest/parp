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

// TODO(bcf): using namespace expr

namespace eval {

namespace {

void ThrowException[[noreturn]](const expr::Expr* expr,
                                const std::string& msg) {
  throw util::RuntimeException("Eval error: " + msg + " " +
                               util::to_string(expr));
}

expr::Expr* EvalExpr(expr::Expr* expr, expr::Env* env) {
  return expr->GetAsAnalyzed() ? expr->GetAsAnalyzed()->func()(env) : expr;
}

expr::Expr* Analyze(expr::Expr* expr);
std::vector<expr::Expr*> AnalyzeList(expr::Expr* expr);
expr::Expr* AnalyzePair(expr::Pair* pair);
expr::Expr* AnalyzeQuote(expr::Pair* pair);
expr::Expr* AnalyzeAssign(expr::Pair* pair);
expr::Expr* AnalyzeDefine(expr::Pair* pair);
expr::Expr* AnalyzeIf(expr::Pair* pair);
expr::Expr* AnalyzeLambda(expr::Pair* pair);
expr::Expr* AnalyzeSequence(expr::Expr* pair);
expr::Expr* AnalyzeApplication(expr::Pair* pair);

expr::Expr* Analyze(expr::Expr* expr) {
  switch (expr->type()) {
    case expr::Expr::Type::EMPTY_LIST:
    case expr::Expr::Type::BOOL:
    case expr::Expr::Type::NUMBER:
    case expr::Expr::Type::CHAR:
    case expr::Expr::Type::STRING:
    case expr::Expr::Type::VECTOR:
    case expr::Expr::Type::ENV:
      return expr;

    case expr::Expr::Type::SYMBOL: {
      auto* sym = expr->GetAsSymbol();
      return expr::Analyzed::Create(
          [sym](expr::Env* env) { return env->Lookup(sym); },
          {expr});  // NOLINT(whitespace/newline)
    }
    case expr::Expr::Type::PAIR:
      return AnalyzePair(expr->GetAsPair());

    case expr::Expr::Type::ANALYZED:
      // Analyze called twice
      assert(false);
      break;
    default:
      // TODO(bcf): remove after unused types removed
      assert(false);
  }

  assert(false);
  return nullptr;
}

std::vector<expr::Expr*> AnalyzeList(expr::Expr* expr) {
  std::vector<expr::Expr*> analyzed;
  while (auto* pair = expr->GetAsPair()) {
    analyzed.push_back(Analyze(pair->car()));
    expr = pair->cdr();
  }
  if (expr != expr::Nil())
    ThrowException(expr, "Expected '() terminated list of expressions");

  return analyzed;
}

expr::Expr* AnalyzePair(expr::Pair* pair) {
  if (auto* sym = pair->car()->GetAsSymbol()) {
    if (sym->val() == "quote") {
      return AnalyzeQuote(pair);
    } else if (sym->val() == "set!") {
      return AnalyzeAssign(pair);
    } else if (sym->val() == "define") {
      return AnalyzeDefine(pair);
    } else if (sym->val() == "if") {
      return AnalyzeIf(pair);
    } else if (sym->val() == "lambda") {
      return AnalyzeLambda(pair);
    } else if (sym->val() == "begin") {
      return AnalyzeSequence(pair->cdr());
    }
  }

  return AnalyzeApplication(pair);
}

expr::Expr* AnalyzeQuote(expr::Pair* pair) {
  if (pair->Cr("dd") != expr::Nil()) {
    ThrowException(pair, "Malformed quote expression");
  }
  return pair->Cr("ad");
}

expr::Expr* AnalyzeAssign(expr::Pair* pair) {
  if (pair->Cr("ddd") != expr::Nil()) {
    ThrowException(pair, "Malformed assignment expression");
  }
  auto* var = pair->Cr("ad")->GetAsSymbol();
  if (!var) {
    ThrowException(pair, "Expected Symbol");
  }
  auto* val = Analyze(pair->Cr("add"));
  return expr::Analyzed::Create(
      [var, val](expr::Env* env) {
        env->SetVar(var, EvalExpr(val, env));
        return expr::True();
      },
      {var, val});
}

expr::Expr* AnalyzeDefine(expr::Pair* pair) {
  if (pair->Cr("ddd") != expr::Nil()) {
    ThrowException(pair, "Malformed definition");
  }

  auto* var = pair->Cr("ad")->GetAsSymbol();
  if (!var) {
    ThrowException(pair, "Expected symbol");
  }

  auto* val = Analyze(pair->Cr("add"));
  return expr::Analyzed::Create(
      [var, val](expr::Env* env) {
        env->DefineVar(var, EvalExpr(val, env));
        return expr::True();
      },
      {var, val});
}

expr::Expr* AnalyzeIf(expr::Pair* pair) {
  expr::Expr* raw_false_expr = nullptr;

  if (pair->Cr("dddd") == expr::Nil()) {
    raw_false_expr = pair->Cr("addd");
  } else if (pair->Cr("ddd") != expr::Nil()) {
    ThrowException(pair, "Malformed if");
  }
  auto* cond = Analyze(pair->Cr("ad"));
  auto* true_expr = Analyze(pair->Cr("add"));

  if (raw_false_expr == nullptr) {
    return expr::Analyzed::Create(
        [cond, true_expr](expr::Env* env) {
          return EvalExpr(cond, env) == expr::False()
                     ? expr::False()
                     : EvalExpr(true_expr, env);
        },
        {cond, true_expr});
  }

  auto* false_expr = Analyze(raw_false_expr);

  return expr::Analyzed::Create(
      [cond, true_expr, false_expr](expr::Env* env) {
        return EvalExpr(cond, env) == expr::False() ? EvalExpr(false_expr, env)
                                                    : EvalExpr(true_expr, env);
      },
      {cond, true_expr, false_expr});
}

expr::Expr* AnalyzeLambda(expr::Pair* pair) {
  static const char kExpectSym[] = "Expected symbol as lambda argument";
  std::vector<const expr::Symbol*> req_args;
  std::vector<const expr::Expr*> refs;

  expr::Expr* cur_arg = pair->Cr("ad");
  while (auto* as_pair = cur_arg->GetAsPair()) {
    auto* arg = as_pair->car()->GetAsSymbol();
    if (!arg) {
      ThrowException(arg, kExpectSym);
    }

    req_args.push_back(arg);
    refs.push_back(arg);
    cur_arg = as_pair->cdr();
  }

  const expr::Symbol* var_arg = nullptr;
  if (cur_arg != expr::Nil()) {
    if (!(var_arg = cur_arg->GetAsSymbol())) {
      ThrowException(cur_arg, kExpectSym);
    }
    refs.push_back(var_arg);
  }

  // TODO(bcf): Make sure in sequence, definitions must go first
  auto* body = AnalyzeSequence(pair->Cr("dd"));
  refs.push_back(body);

  return expr::Analyzed::Create(
      [req_args, var_arg, body](expr::Env* env) {
        return expr::Lambda::Create(req_args, var_arg, body, env);
      },
      refs);
}

expr::Expr* AnalyzeSequence(expr::Expr* expr) {
  auto analyzed = AnalyzeList(expr);
  if (analyzed.size() == 0) {
    ThrowException(expr, "Empty sequence is not allowed");
  }

  std::vector<const expr::Expr*> const_analyzed;
  for (auto* e : analyzed) {
    const_analyzed.push_back(e);
  }

  return expr::Analyzed::Create(
      [analyzed](expr::Env* env) {
        expr::Expr* res = expr::Nil();
        for (auto* e : analyzed) {
          res = EvalExpr(e, env);
        }
        return res;
      },
      const_analyzed);
}

expr::Expr* AnalyzeApplication(expr::Pair* pair) {
  auto* op = Analyze(pair->car());
  auto args = AnalyzeList(pair->cdr());

  std::vector<const expr::Expr*> refs;
  refs.reserve(args.size() + 1);
  refs.push_back(op);
  for (auto* arg : args) {
    refs.push_back(arg);
  }

  return expr::Analyzed::Create(
      [op, args](expr::Env* env) {
        auto eargs = args;
        for (auto& e : eargs) {
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
        std::vector<std::pair<const expr::Symbol*, expr::Expr*>> bindings;

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
      refs);
}

expr::Expr* DoEval(expr::Expr* expr, expr::Env* env) {
  switch (expr->type()) {
    // Self evaluating
    case expr::Expr::Type::BOOL:
    case expr::Expr::Type::NUMBER:
    case expr::Expr::Type::CHAR:
    case expr::Expr::Type::STRING:
    case expr::Expr::Type::VECTOR:
      return expr;

    case expr::Expr::Type::SYMBOL:
      return env->Lookup(expr->GetAsSymbol());

    case expr::Expr::Type::ANALYZED:
      return expr->GetAsAnalyzed()->func()(env);

    default:
      ThrowException(expr, "Cannot evaluate");
  }
}

}  // namespace

expr::Expr* Eval(expr::Expr* expr, expr::Env* env) {
  auto* e = Analyze(expr);
  return DoEval(e, env);
}

}  // namespace eval
