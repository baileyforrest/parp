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

#include "util/exceptions.h"
#include "util/util.h"

namespace eval {

namespace {

void ThrowException [[ noreturn ]](expr::Expr *expr, const std::string &msg) {
  throw util::RuntimeException("Eval error: " + msg + " " +
      util::to_string(expr));
}

expr::Expr *EvalExpr(expr::Expr *expr, expr::Env *env) {
  return expr->type() == expr::Expr::Type::ANALYZED ?
    expr->GetAsAnalyzed()->func()(env) : expr;
}

class Analyzer {
public:
  expr::Expr *Analyze(expr::Expr *expr, expr::Env *env);

private:
  std::vector<expr::Expr *> AnalyzeList(expr::Expr *expr, expr::Env *env);
  expr::Expr *AnalyzePair(expr::Pair *pair, expr::Env *env);
  expr::Expr *AnalyzeQuote(expr::Pair *pair, expr::Env *env);
  expr::Expr *AnalyzeAssign(expr::Pair *pair, expr::Env *env);
  expr::Expr *AnalyzeDefine(expr::Pair *pair, expr::Env *env);
  expr::Expr *AnalyzeIf(expr::Pair *pair, expr::Env *env);
  expr::Expr *AnalyzeLambda(expr::Pair *pair, expr::Env *env);
  expr::Expr *AnalyzeSequence(expr::Expr *pair, expr::Env *env);
  expr::Expr *AnalyzeApplication(expr::Pair *pair, expr::Env *env);

};

expr::Expr *Analyzer::Analyze(expr::Expr *expr, expr::Env *env) {
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
      auto *sym = expr->GetAsSymbol();
      return expr::Analyzed::Create([sym](expr::Env *env) {
          return env->Lookup(sym);
      }, { expr });
    }
    case expr::Expr::Type::PAIR:
      return AnalyzePair(expr->GetAsPair(), env);

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

std::vector<expr::Expr *> Analyzer::AnalyzeList(
    expr::Expr *expr, expr::Env *env) {
  std::vector<expr::Expr *> analyzed;
  while (expr->type() == expr::Expr::Type::PAIR) {
    auto pair = expr->GetAsPair();
    analyzed.push_back(Analyze(pair->car(), env));
    expr = pair->cdr();
  }
  if (expr != expr::Nil())
    ThrowException(expr, "Expected '() terminated list of expressions");

  return analyzed;
}

expr::Expr *Analyzer::AnalyzePair(expr::Pair *pair, expr::Env *env) {
  if (pair->car()->type() != expr::Expr::Type::SYMBOL) {
    ThrowException(pair, "Expected Identifier");
  }

  auto sym = pair->car()->GetAsSymbol();

  if (sym->val() == "quote") {
    return AnalyzeQuote(pair, env);
  } else if (sym->val() == "set!") {
    return AnalyzeAssign(pair, env);
  } else if (sym->val() == "define") {
    return AnalyzeDefine(pair, env);
  } else if (sym->val() == "if") {
    return AnalyzeIf(pair, env);
  } else if (sym->val() == "lambda") {
    return AnalyzeLambda(pair, env);
  } else if (sym->val() == "begin") {
    return AnalyzeSequence(pair->cdr(), env);
  }

  return AnalyzeApplication(pair, env);
}

expr::Expr *Analyzer::AnalyzeQuote(expr::Pair *pair, expr::Env *env) {
  (void)env;
  if (pair->Cr("dd") != expr::Nil()) {
    ThrowException(pair, "Malformed quote expression");
  }
  return pair->Cr("ad");
}

expr::Expr *Analyzer::AnalyzeAssign(expr::Pair *pair, expr::Env *env) {
  expr::Expr *sym;
  if (pair->Cr("ddd") != expr::Nil() ||
      (sym = pair->Cr("ad"))->type() != expr::Expr::Type::SYMBOL) {
    ThrowException(pair, "Malformed assignment expression");
  }
  auto var = sym->GetAsSymbol();
  auto val = Analyze(pair->Cr("add"), env);
  return expr::Analyzed::Create([var, val](expr::Env *env) {
      env->SetVar(var, EvalExpr(val, env));
      return expr::True();
  }, { sym, val });
}

expr::Expr *Analyzer::AnalyzeDefine(expr::Pair *pair, expr::Env *env) {
  expr::Expr *sym;
  if (pair->Cr("ddd") != expr::Nil() ||
      (sym = pair->Cr("ad"))->type() != expr::Expr::Type::SYMBOL) {
    ThrowException(pair, "Malformed definition");
  }
  auto var = sym->GetAsSymbol();
  auto val = Analyze(pair->Cr("add"), env);
  return expr::Analyzed::Create([var, val](expr::Env *env) {
      env->DefineVar(var, EvalExpr(val, env));
      return expr::True();
  }, { sym, val });
}

expr::Expr *Analyzer::AnalyzeIf(expr::Pair *pair, expr::Env *env) {
  expr::Expr *raw_false_expr = nullptr;

  if (pair->Cr("dddd") == expr::Nil()) {
    raw_false_expr = pair->Cr("addd");
  } else if (pair->Cr("ddd") != expr::Nil()) {
    ThrowException(pair, "Malformed conditional");
  }
  auto cond = Analyze(pair->Cr("ad"), env);
  auto true_expr = Analyze(pair->Cr("add"), env);

  if (raw_false_expr == nullptr) {
    return expr::Analyzed::Create([cond, true_expr](expr::Env *env) {
        return EvalExpr(cond, env) == expr::False() ?
          expr::False() : EvalExpr(true_expr, env);
    }, { cond, true_expr });
  }

  auto false_expr = Analyze(raw_false_expr, env);

  return expr::Analyzed::Create(
      [cond, true_expr, false_expr](expr::Env *env) {
        return EvalExpr(cond, env) == expr::False() ?
          EvalExpr(false_expr, env) : EvalExpr(true_expr, env);
  }, { cond, true_expr, false_expr });
}

expr::Expr *Analyzer::AnalyzeLambda(expr::Pair *pair, expr::Env *env) {
  const expr::Symbol *var_arg = nullptr;
  std::vector<const expr::Symbol *> req_args;

  auto args = pair->Cr("ad");
  if (args->type() == expr::Expr::Type::SYMBOL) {
    var_arg = args->GetAsSymbol();
  } else {
    const char *kExpectSym = "Expected symbol as lambda argument";
    expr::Expr *cur = pair;
    while (cur->type() == expr::Expr::Type::PAIR) {
      auto as_pair = cur->GetAsPair();
      if (as_pair->car()->type() != expr::Expr::Type::SYMBOL)
        ThrowException(args, kExpectSym);

      req_args.push_back(as_pair->car()->GetAsSymbol());
      cur = as_pair->cdr();
    }

    if (cur != expr::Nil()) {
      if (cur->type() != expr::Expr::Type::SYMBOL)
        ThrowException(args, kExpectSym);

      var_arg = cur->GetAsSymbol();
    }
  }

  // TODO(bcf): Make sure in sequence, definitions must go first
  auto body = AnalyzeSequence(pair->Cr("dd"), env);

  // TODO(bcf): Handle memory refs for lambda
  return expr::Analyzed::Create([req_args, var_arg, body](expr::Env *env) {
      return expr::Lambda::Create(req_args, var_arg, body, env);
  }, { });
}

expr::Expr *Analyzer::AnalyzeSequence(expr::Expr *expr, expr::Env *env) {
  std::vector<expr::Expr *> analyzed = AnalyzeList(expr, env);
  if (analyzed.size() == 0)
    ThrowException(expr, "Empty sequence is not allowed");

  return expr::Analyzed::Create([analyzed](expr::Env *env) {
      expr::Expr *res = expr::Nil();
      for (auto e : analyzed) {
        res = EvalExpr(e, env);
      }
      return res;
  }, analyzed);
}

expr::Expr *Analyzer::AnalyzeApplication(expr::Pair *pair, expr::Env *env) {
  auto op = Analyze(pair->car(), env);
  auto args = AnalyzeList(pair->cdr(), env);

  auto refs = args;
  refs.push_back(op);

  return expr::Analyzed::Create([op, args](expr::Env *env) {
      auto proc = EvalExpr(op, env);
      if (proc->type() != expr::Expr::Type::LAMBDA)
        ThrowException(proc, "Expected a procedure");
      auto lambda = proc->GetAsLambda();
      if (args.size() < lambda->required_args().size()) {
        std::ostringstream os;
        os << "Invalid number of arguments. expected: ";
        if (lambda->variable_arg() != nullptr)
          os << "at least ";
        os << lambda->required_args().size();
        os << " given: " << args.size();
        ThrowException(proc, os.str());
      }
      auto eargs = args;
      for (auto &e : eargs) {
        e = EvalExpr(e, env);
      }
      auto arg_it = eargs.begin();
      std::vector<std::pair<const expr::Symbol *, expr::Expr *> > bindings;

      for (auto sym : lambda->required_args()) {
        bindings.emplace_back(sym, *(arg_it++));
      }
      if (lambda->variable_arg() != nullptr) {
        auto rest = ListFromIt(arg_it, eargs.end());
        bindings.emplace_back(lambda->variable_arg(), rest);
      }

      auto new_env = expr::Env::Create(bindings, lambda->env());

      return EvalExpr(lambda->body(), new_env);
  }, refs);
}

expr::Expr *DoEval(expr::Expr *expr, expr::Env *env) {
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

  assert(false);
  return nullptr;
}

}

expr::Expr *Eval(expr::Expr *expr, expr::Env *env) {
  Analyzer ana;
  return DoEval(ana.Analyze(expr, env), env);
}

}  // namespace eval
