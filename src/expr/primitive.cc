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

#include <strings.h>

#include <cmath>
#include <cctype>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <set>
#include <vector>
#include <utility>

#include "expr/expr.h"
#include "expr/number.h"
#include "expr/primitive.h"
#include "eval/eval.h"
#include "gc/lock.h"
#include "parse/lexer.h"
#include "util/exceptions.h"
#include "util/util.h"

using eval::Eval;
using util::RuntimeException;

namespace expr {

namespace {

// Depth for car, cdr, caar etc
const int kCrDepth = 4;
const int kSchemeVersion = 5;

using PrimitiveFunc = gc::Lock<Expr> (*)(Env* env,
                                         Expr** args,
                                         size_t num_args);

void ExpectNumArgs(size_t num_args, size_t expected) {
  if (num_args != expected) {
    std::ostringstream os;
    os << "Expected " << expected << " args. Got " << num_args;
    throw RuntimeException(os.str(), nullptr);
  }
}

void ExpectNumArgsLe(size_t num_args, size_t expected) {
  if (num_args > expected) {
    std::ostringstream os;
    os << "Expected at most " << expected << " args. Got " << num_args;
    throw RuntimeException(os.str(), nullptr);
  }
}

void ExpectNumArgsGe(size_t num_args, size_t expected) {
  if (num_args < expected) {
    std::ostringstream os;
    os << "Expected at least " << expected << " args. Got " << num_args;
    throw RuntimeException(os.str(), nullptr);
  }
}

std::vector<gc::Lock<Expr>> EvalArgs(Env* env, Expr** args, size_t num_args) {
  std::vector<gc::Lock<Expr>> locks;
  locks.reserve(num_args);
  for (size_t i = 0; i < num_args; ++i) {
    auto lock = Eval(args[i], env);
    args[i] = lock.get();
    locks.emplace_back(lock);
  }

  return locks;
}

class PrimitiveImpl : public Evals {
 public:
  PrimitiveImpl(const char* name, PrimitiveFunc func, bool eval_args)
      : name_(name), func_(func), eval_args_(eval_args) {}

  // Evals implementation:
  std::ostream& AppendStream(std::ostream& stream) const override {
    return stream << name_;
  }

  gc::Lock<Expr> DoEval(Env* env, Expr** args, size_t num_args) override {
    std::vector<gc::Lock<Expr>> locks;
    if (eval_args_) {
      locks = EvalArgs(env, args, num_args);
    }

    try {
      return func_(env, args, num_args);
    } catch (RuntimeException& e) {
      throw RuntimeException(e.what(), nullptr);
    }
  }

 private:
  const char* const name_;
  const PrimitiveFunc func_;
  const bool eval_args_;
};

template <template <typename T> class Op>
gc::Lock<Expr> ArithOp(Env* env, Expr* initial, Expr** args, size_t num_args) {
  gc::Lock<Number> result(TryNumber(initial));
  for (size_t i = 0; i < num_args; ++i) {
    result = OpInPlace<Op>(result.get(), TryNumber(args[i]));
  }

  return gc::Lock<Expr>(result.get());
}

template <template <typename T> class Op>
gc::Lock<Expr> CmpOp(Env* env, Expr** args, size_t num_args) {
  auto* last = TryNumber(args[0]);
  for (size_t i = 1; i < num_args; ++i) {
    auto* cur = TryNumber(args[i]);
    if (!OpCmp<Op>(last, cur)) {
      return gc::Lock<Expr>(False());
    }
    last = cur;
  }

  return gc::Lock<Expr>(True());
}

template <template <typename T> class Op>
gc::Lock<Expr> TestOp(Expr* val) {
  auto* num = TryNumber(val);
  if (auto* as_int = num->AsInt()) {
    Op<Int::ValType> op;
    return gc::Lock<Expr>(op(as_int->val(), 0) ? True() : False());
  }

  Op<Float::ValType> op;
  return gc::Lock<Expr>(op(num->AsFloat()->val(), 0.0) ? True() : False());
}

template <template <typename T> class Op>
gc::Lock<Expr> MostOp(Expr** args, size_t num_args) {
  Number* ret = TryNumber(args[0]);
  bool has_inexact = !ret->exact();
  for (size_t i = 1; i < num_args; ++i) {
    auto* arg = TryNumber(args[i]);
    has_inexact |= !ret->exact();
    if (OpCmp<Op>(ret, arg)) {
      ret = arg;
    }
  }

  if (has_inexact && ret->exact()) {
    auto* int_ret = ret->AsInt();
    assert(int_ret);
    return gc::Lock<Expr>(new Float(int_ret->val()));
  }

  return gc::Lock<Expr>(ret);
}

gc::Lock<Expr> ExecuteList(Env* env, Expr* cur) {
  gc::Lock<Expr> last_eval;
  while (auto* link = cur->AsPair()) {
    last_eval = Eval(link->car(), env);
    cur = link->cdr();
  }

  if (cur != Nil()) {
    throw RuntimeException("Unexpected expression", cur);
  }

  if (!last_eval) {
    throw RuntimeException("Unexpected empty sequence", cur);
  }

  return last_eval;
}

gc::Lock<Expr> EvalArray(Env* env, Expr** args, size_t num_args) {
  gc::Lock<Expr> last_eval;
  for (size_t i = 0; i < num_args; ++i) {
    last_eval = Eval(args[i], env);
  }

  return last_eval;
}

class LambdaImpl : public Evals {
 public:
  explicit LambdaImpl(std::vector<Symbol*> required_args,
                      Symbol* variable_arg,
                      std::vector<Expr*> body,
                      Env* env)
      : required_args_(std::move(required_args)),
        variable_arg_(variable_arg),
        body_(std::move(body)),
        env_(env) {
    assert(body_.size() > 0);
    assert(env);
  }
  ~LambdaImpl() override = default;

  // Evals implementation:
  std::ostream& AppendStream(std::ostream& stream) const override;
  gc::Lock<Expr> DoEval(Env* env, Expr** args, size_t num_args) override;
  void MarkReferences() override {
    for (auto arg : required_args_) {
      arg->GcMark();
    }
    if (variable_arg_) {
      variable_arg_->GcMark();
    }

    for (auto expr : body_) {
      expr->GcMark();
    }

    env_->GcMark();
  }

 private:
  const std::vector<Symbol*> required_args_;
  Symbol* const variable_arg_;
  const std::vector<Expr*> body_;
  Env* const env_;
};

std::ostream& LambdaImpl::AppendStream(std::ostream& stream) const {
  stream << "(lambda ";
  if (required_args_.size() == 0 && variable_arg_ != nullptr) {
    stream << *variable_arg_;
  } else {
    stream << "(";
    for (auto* arg : required_args_)
      stream << *arg << " ";

    if (variable_arg_) {
      stream << ". " << *variable_arg_;
    }
    stream << ")";
  }

  for (auto* expr : body_) {
    stream << " " << *expr;
  }
  return stream << ")";
}

gc::Lock<Expr> LambdaImpl::DoEval(Env* env, Expr** args, size_t num_args) {
  if (num_args < required_args_.size()) {
    std::ostringstream os;
    os << "Invalid number of arguments. expected ";
    if (variable_arg_ != nullptr) {
      os << "at least ";
    }

    os << required_args_.size();
    os << " given: " << num_args;
    throw RuntimeException(os.str(), nullptr);
  }

  auto locks = EvalArgs(env, args, num_args);

  auto arg_it = args;
  auto new_env = gc::make_locked<Env>(env_);

  for (auto* sym : required_args_) {
    new_env->DefineVar(sym, *arg_it++);
  }
  if (variable_arg_ != nullptr) {
    gc::Lock<Expr> rest(Nil());
    for (Expr** temp = args + num_args - 1; temp >= arg_it; --temp) {
      rest.reset(new Pair(*temp, rest.get()));
    }
    new_env->DefineVar(variable_arg_, rest.get());
  }

  assert(body_.size() >= 1);
  for (size_t i = 0; i < body_.size() - 1; ++i) {
    Eval(body_[i], new_env.get());
  }
  return Eval(body_.back(), new_env.get());
}

class CrImpl : public Evals {
 public:
  explicit CrImpl(const std::string& cr) : cr_(cr) {}
  ~CrImpl() override = default;

  // Evals implementation:
  std::ostream& AppendStream(std::ostream& stream) const override {
    return stream << "c" << cr_ << "r";
  }
  gc::Lock<Expr> DoEval(Env* env, Expr** args, size_t num_args) override {
    ExpectNumArgs(num_args, 1);
    auto locks = EvalArgs(env, args, num_args);
    return gc::Lock<Expr>(TryPair(args[0])->Cr(cr_));
  }

 private:
  std::string cr_;
};

void LoadCr(Env* env, size_t depth, std::string* cur) {
  if (depth == 0)
    return;
  // car and cdr are defined manually for performance reasons.
  if (cur->size() > 1) {
    auto sym_name = "c" + *cur + "r";
    auto cr = gc::make_locked<CrImpl>(*cur);
    env->DefineVar(Symbol::New(sym_name), cr.get());
  }

  cur->push_back('a');
  LoadCr(env, depth - 1, cur);
  cur->pop_back();

  cur->push_back('d');
  LoadCr(env, depth - 1, cur);
  cur->pop_back();
}

Int::ValType TryGetIntValOrRound(Expr* expr, bool* is_exact) {
  auto* num = TryNumber(expr);
  if (auto* as_int = num->AsInt()) {
    return as_int->val();
  }

  auto* as_float = num->AsFloat();
  assert(as_float);
  if (std::trunc(as_float->val()) != as_float->val()) {
    throw RuntimeException("Expected integer", as_float);
  }

  *is_exact = false;
  return as_float->val();
}

Float::ValType TryGetFloatVal(Expr* expr) {
  auto* num = TryNumber(expr);
  if (auto* as_float = num->AsFloat()) {
    return as_float->val();
  }

  auto* as_int = num->AsInt();
  assert(as_int);
  return as_int->val();
}

gc::Lock<Expr> ExactIfPossible(Float::ValType val) {
  Int::ValType int_val = std::trunc(val);
  if (int_val == val) {
    return gc::Lock<Expr>(new Int(int_val));
  }
  return gc::Lock<Expr>(new Float(val));
}

template <Float::ValType (*Op)(Float::ValType)>
gc::Lock<Expr> EvalUnaryFloatOp(Expr* val) {
  return ExactIfPossible(Op(TryGetFloatVal(val)));
}

template <Float::ValType (*Op)(Float::ValType, Float::ValType)>
gc::Lock<Expr> EvalBinaryFloatOp(Expr* v1, Expr* v2) {
  Float::ValType n1 = TryGetFloatVal(v1);
  Float::ValType n2 = TryGetFloatVal(v2);
  return ExactIfPossible(Op(n1, n2));
}

gc::Lock<Expr> CopyList(Expr* list, Pair** last_link) {
  Expr* cur = list;
  gc::Lock<Expr> ret(Nil());
  Pair* prev = nullptr;
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    Pair* copy = new Pair(list->car(), Nil());

    if (prev) {
      prev->set_cdr(copy);
    } else {
      ret.reset(copy);
    }
    prev = copy;
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", list);
  }

  *last_link = prev;
  return ret;
}

template <template <typename T> class Op>
gc::Lock<Expr> EvalCharOp(Expr* v1, Expr* v2) {
  auto* c1 = TryChar(v1);
  auto* c2 = TryChar(v2);

  Op<Char::ValType> op;
  return gc::Lock<Expr>(op(c1->val(), c2->val()) ? True() : False());
}

template <template <typename T> class Op>
gc::Lock<Expr> EvalCharCiOp(Expr* v1, Expr* v2) {
  auto* c1 = TryChar(v1);
  auto* c2 = TryChar(v2);

  Op<Char::ValType> op;
  return gc::Lock<Expr>(
      op(std::tolower(c1->val()), std::tolower(c2->val())) ? True() : False());
}

template <int (*Op)(int)>
gc::Lock<Expr> CheckUnaryCharOp(Expr* val) {
  return gc::Lock<Expr>(Op(TryChar(val)->val()) ? True() : False());
}

Int::ValType TryGetNonNegExactIntVal(
    Expr* expr,
    Int::ValType max = std::numeric_limits<Int::ValType>::max()) {
  auto ret = TryInt(expr)->val();
  if (ret < 0) {
    throw RuntimeException("Expected exact positive integer", expr);
  }
  if (ret >= max) {
    auto msg = "index out of range, expected <= " + std::to_string(max);
    throw RuntimeException(std::move(msg), expr);
  }
  return ret;
}

template <typename Op>
gc::Lock<Expr> EvalStringOp(Expr* v1, Expr* v2) {
  auto* s1 = TryString(v1);
  auto* s2 = TryString(v2);

  Op op;
  return gc::Lock<Expr>(op(s1->val(), s2->val()) ? True() : False());
}

template <template <typename T> class Op>
struct ICaseCmpStr {
  bool operator()(const std::string& s1, const std::string& s2) {
    Op<int> op;
    return op(strcasecmp(s1.c_str(), s2.c_str()), 0);
  }
};

template <bool need_return>
gc::Lock<Expr> MapImpl(Env* env, Expr** args, size_t num_args) {
  static constexpr char kEqualSizeListErr[] =
      "Expected equal sized argument lists";
  Evals* procedure = TryEvals(args[0]);

  gc::Lock<Expr> ret(Nil());
  Pair* prev = nullptr;

  std::vector<gc::Lock<Pair>> locks;

  while (true) {
    bool done = false;
    std::vector<Expr*> new_args(num_args - 1);
    for (size_t i = 1; i < num_args; ++i) {
      if (auto* list = args[i]->AsPair()) {
        if (done) {
          throw RuntimeException(kEqualSizeListErr, args[i]);
        }
        new_args[i - 1] = list->car();
        args[i] = list->cdr();
      } else {
        if (args[i] != Nil()) {
          throw RuntimeException("Expected list", args[i]);
        }

        if (i == 1) {
          done = true;
        } else {
          if (!done) {
            throw RuntimeException(kEqualSizeListErr, args[i]);
          }
        }
      }
    }
    if (done) {
      break;
    }

    // Quote non-self evaluating types to make sure they aren't considered an
    // evaluation.
    for (auto& expr : new_args) {
      if (expr->type() == Expr::Type::PAIR ||
          expr->type() == Expr::Type::SYMBOL) {
        auto back = gc::make_locked<Pair>(expr, Nil());
        auto pair = gc::make_locked<Pair>(Symbol::New("quote"), back.get());
        expr = pair.get();
        locks.push_back(std::move(pair));
      }
    }

    auto res = procedure->DoEval(env, new_args.data(), new_args.size());
    if (need_return) {
      auto pair = gc::make_locked<Pair>(res.get(), Nil());
      Pair* new_link = pair.get();
      locks.push_back(std::move(pair));
      if (prev) {
        prev->set_cdr(new_link);
      } else {
        ret.reset(new_link);
      }
      prev = new_link;
    }
  }

  return ret;
}

class Promise : public Evals {
 public:
  Promise(Expr* expr, Env* env) : expr_(expr), env_(env) {}

  // Evals implementation:
  std::ostream& AppendStream(std::ostream& stream) const override {
    return stream << "promise";
  }
  gc::Lock<Expr> DoEval(Env* /* env */,
                        Expr** /* args */,
                        size_t /* num_args */) override {
    if (forced_val_) {
      return gc::Lock<Expr>(forced_val_);
    }

    auto ret = Eval(expr_, env_);
    forced_val_ = ret.get();
    expr_ = nullptr;
    env_ = nullptr;
    return ret;
  }
  void MarkReferences() override {
    if (expr_)
      expr_->GcMark();
    if (env_)
      env_->GcMark();
    if (forced_val_)
      forced_val_->GcMark();
  }

 private:
  Expr* expr_;
  Env* env_;
  Expr* forced_val_ = nullptr;
};

gc::Lock<Expr> Quote(Env* /* env */, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]);
}

gc::Lock<Expr> Lambda(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  std::vector<Symbol*> req_args;
  Symbol* var_arg = nullptr;
  switch (args[0]->type()) {
    case Expr::Type::EMPTY_LIST:
      break;
    case Expr::Type::SYMBOL:
      var_arg = args[0]->AsSymbol();
      break;
    case Expr::Type::PAIR: {
      Expr* cur_arg = args[0];
      while (auto* pair = cur_arg->AsPair()) {
        auto* arg = TrySymbol(pair->car());
        req_args.push_back(arg);
        cur_arg = pair->cdr();
      }
      if (cur_arg != expr::Nil()) {
        var_arg = TrySymbol(cur_arg);
      }
      break;
    };
    default:
      throw RuntimeException("Expected arguments", nullptr);
  }

  std::vector<gc::Lock<Expr>> locks;
  locks.reserve(num_args - 1);
  // TODO(bcf): Analyze in other places as appropriate.
  for (size_t i = 1; i < num_args; ++i) {
    auto lock = eval::Analyze(args[i]);
    args[i] = lock.get();
    locks.push_back(std::move(lock));
  }

  req_args.shrink_to_fit();
  return gc::Lock<Expr>(new LambdaImpl(std::move(req_args), var_arg,
                                       {args + 1, args + num_args}, env));
}

gc::Lock<Expr> If(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  ExpectNumArgsLe(num_args, 3);
  auto cond = Eval(args[0], env);
  if (cond.get() == False()) {
    if (num_args == 3) {
      return Eval(args[2], env);
    } else {
      return gc::Lock<Expr>(Nil());
    }
  }
  return Eval(args[1], env);
}

gc::Lock<Expr> Set(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  env->SetVar(TrySymbol(args[0]), Eval(args[1], env).get());
  return gc::Lock<Expr>(Nil());
}

gc::Lock<Expr> Cond(Env* env, Expr** args, size_t num_args) {
  for (size_t i = 0; i < num_args; ++i) {
    auto* pair = args[i]->AsPair();
    if (!pair) {
      throw RuntimeException(
          "cond: bad syntax (clause is not a test-value pair)", args[i]);
    }

    auto* first = pair->car();
    if (i == num_args - 1) {
      auto* first_sym = first->AsSymbol();
      if (first_sym && first_sym->val() == "else" &&
          env->TryLookup(first_sym) == nullptr) {
        return ExecuteList(env, pair->cdr());
      }
    }

    auto test = Eval(first, env);
    if (test.get() == False()) {
      continue;
    }

    if (pair->cdr() == Nil()) {
      return test;
    }
    auto* second = pair->Cr("ad");
    auto* second_sym = second ? second->AsSymbol() : nullptr;
    if (second_sym && second_sym->val() == "=>" &&
        env->TryLookup(second_sym) == nullptr) {
      auto* trailing = pair->Cr("ddd");
      if (trailing != Nil()) {
        throw RuntimeException("Unexpected expression", trailing);
      }

      auto val = Eval(pair->Cr("add"), env);
      auto* func = val->AsEvals();
      if (!func) {
        throw RuntimeException("Expected procedure", val.get());
      }

      Expr* exprs[] = {test.get()};
      return func->DoEval(env, exprs, 1);
    }

    return ExecuteList(env, pair->cdr());
  }

  return gc::Lock<Expr>(Nil());
}

gc::Lock<Expr> Case(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 1);
  auto key = Eval(args[0], env);
  for (size_t i = 1; i < num_args; ++i) {
    auto* clause = args[i]->AsPair();
    if (!clause) {
      throw RuntimeException("cond: bad syntax, expected clause", args[i]);
    }

    if (i == num_args - 1) {
      auto* first_sym = clause->car()->AsSymbol();
      if (first_sym && first_sym->val() == "else" &&
          env->TryLookup(first_sym) == nullptr) {
        return ExecuteList(env, clause->cdr());
      }
    }
    auto* test_list = clause->car()->AsPair();
    if (!test_list) {
      throw RuntimeException("case: bad syntax (not a datum sequence)",
                             args[i]);
    }

    Expr* cur = test_list;
    while (auto* pair = cur->AsPair()) {
      if (key->Eqv(pair->car())) {
        return ExecuteList(env, clause->cdr());
      }
      cur = pair->cdr();
    }

    if (cur != Nil()) {
      throw RuntimeException("case: bad syntax (malformed clause)", cur);
    }
  }

  return gc::Lock<Expr>(Nil());
}

gc::Lock<Expr> And(Env* env, Expr** args, size_t num_args) {
  gc::Lock<Expr> e;
  for (size_t i = 0; i < num_args; ++i) {
    e = Eval(args[i], env);
    if (e.get() == False()) {
      return e;
    }
  }

  return e ? e : gc::Lock<Expr>(True());
}

gc::Lock<Expr> Or(Env* env, Expr** args, size_t num_args) {
  for (size_t i = 0; i < num_args; ++i) {
    auto e = Eval(args[i], env);
    if (e.get() != False()) {
      return e;
    }
  }

  return gc::Lock<Expr>(False());
}

// TODO(bcf): Named let.
gc::Lock<Expr> Let(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  Expr* cur = TryPair(args[0]);
  auto new_env = gc::make_locked<Env>(env);
  while (auto* pair = cur->AsPair()) {
    static const char kErrMessage[] = "let: Expected binding: (var val)";
    auto* binding = pair->car()->AsPair();
    if (!binding) {
      throw RuntimeException(kErrMessage, pair->car());
    }
    auto* var = binding->car()->AsSymbol();
    if (!var) {
      throw RuntimeException("let: Expected symbol for binding",
                             binding->car());
    }
    auto* second_link = binding->cdr()->AsPair();
    if (!second_link || second_link->cdr() != Nil()) {
      throw RuntimeException(kErrMessage, binding->cdr());
    }
    auto* val = second_link->car();
    new_env->DefineVar(var, Eval(val, env).get());

    cur = pair->cdr();
  }
  if (cur != Nil()) {
    throw RuntimeException("let: Malformed binding list", cur);
  }

  return EvalArray(new_env.get(), args + 1, num_args - 1);
}

gc::Lock<Expr> LetStar(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  Expr* cur = TryPair(args[0]);
  auto new_env = gc::make_locked<Env>(env);
  while (auto* pair = cur->AsPair()) {
    static const char kErrMessage[] = "let*: Expected binding: (var val)";
    auto* binding = pair->car()->AsPair();
    if (!binding) {
      throw RuntimeException(kErrMessage, pair->car());
    }
    auto* var = binding->car()->AsSymbol();
    if (!var) {
      throw RuntimeException("let*: Expected symbol for binding",
                             binding->car());
    }
    auto* second_link = binding->cdr()->AsPair();
    if (!second_link || second_link->cdr() != Nil()) {
      throw RuntimeException(kErrMessage, binding->cdr());
    }
    auto* val = second_link->car();
    new_env->DefineVar(var, Eval(val, new_env.get()).get());

    cur = pair->cdr();
  }
  if (cur != Nil()) {
    throw RuntimeException("let: Malformed binding list", cur);
  }

  return EvalArray(new_env.get(), args + 1, num_args - 1);
}

gc::Lock<Expr> LetRec(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  auto new_env = gc::make_locked<Env>(env);
  std::vector<std::pair<Symbol*, Expr*>> bindings;

  Expr* cur = TryPair(args[0]);
  for (; auto* pair = cur->AsPair(); cur = pair->cdr()) {
    static const char kErrMessage[] = "let*: Expected binding: (var val)";
    auto* binding = pair->car()->AsPair();
    if (!binding) {
      throw RuntimeException(kErrMessage, pair->car());
    }
    auto* var = binding->car()->AsSymbol();
    if (!var) {
      throw RuntimeException("let*: Expected symbol for binding",
                             binding->car());
    }
    auto* second_link = binding->cdr()->AsPair();
    if (!second_link || second_link->cdr() != Nil()) {
      throw RuntimeException(kErrMessage, binding->cdr());
    }
    auto* val = second_link->car();
    bindings.emplace_back(var, val);
    new_env->DefineVar(var, Nil());
  }
  if (cur != Nil()) {
    throw RuntimeException("let: Malformed binding list", cur);
  }

  for (const auto& binding : bindings) {
    new_env->SetVar(binding.first, Eval(binding.second, new_env.get()).get());
  }

  return EvalArray(new_env.get(), args + 1, num_args - 1);
}

gc::Lock<Expr> Begin(Env* env, Expr** args, size_t num_args) {
  if (num_args == 0) {
    return gc::Lock<Expr>(Nil());
  }
  for (; num_args > 1; --num_args, ++args) {
    Eval(*args, env);
  }
  return Eval(*args, env);
}

gc::Lock<Expr> Do(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Delay(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(new Promise(args[0], env));
}

gc::Lock<Expr> Quasiquote(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> LetSyntax(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> LetRecSyntax(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> SyntaxRules(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Define(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  env->DefineVar(TrySymbol(args[0]), Eval(args[1], env).get());
  // TODO(bcf): handle define lambda.

  return gc::Lock<Expr>(Nil());
}

gc::Lock<Expr> DefineSyntax(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> IsEqv(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return gc::Lock<Expr>(args[0]->Eqv(args[1]) ? True() : False());
}

gc::Lock<Expr> IsEq(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return gc::Lock<Expr>(args[0]->Eq(args[1]) ? True() : False());
}

gc::Lock<Expr> IsEqual(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return gc::Lock<Expr>(args[0]->Equal(args[1]) ? True() : False());
}

gc::Lock<Expr> IsNumber(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::NUMBER ? True()
                                                              : False());
}

gc::Lock<Expr> IsComplex(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  // We don't support complex.
  return gc::Lock<Expr>(True());
}

gc::Lock<Expr> IsReal(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  // We don't support complex.
  return gc::Lock<Expr>(True());
}

gc::Lock<Expr> IsRational(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  // We don't support rational.
  return gc::Lock<Expr>(False());
}

gc::Lock<Expr> IsInteger(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto* num = args[0]->AsNumber();
  if (!num) {
    return gc::Lock<Expr>(False());
  }

  if (num->num_type() == Number::Type::INT) {
    return gc::Lock<Expr>(True());
  }

  auto* as_float = num->AsFloat();
  assert(as_float);

  return gc::Lock<Expr>(
      as_float->val() == std::floor(as_float->val()) ? True() : False());
}

gc::Lock<Expr> IsExact(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::NUMBER &&
                                args[0]->AsNumber()->exact()
                            ? True()
                            : False());
}

gc::Lock<Expr> IsInexact(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::NUMBER &&
                                args[0]->AsNumber()->exact()
                            ? False()
                            : True());
}

gc::Lock<Expr> Min(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 1);
  return MostOp<std::less>(args, num_args);
}

gc::Lock<Expr> Max(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 1);
  return MostOp<std::greater>(args, num_args);
}

gc::Lock<Expr> OpEq(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  return CmpOp<std::equal_to>(env, args, num_args);
}

gc::Lock<Expr> OpLt(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  return CmpOp<std::less>(env, args, num_args);
}

gc::Lock<Expr> OpGt(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  return CmpOp<std::greater>(env, args, num_args);
}

gc::Lock<Expr> OpLe(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  return CmpOp<std::less_equal>(env, args, num_args);
}

gc::Lock<Expr> OpGe(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  return CmpOp<std::greater_equal>(env, args, num_args);
}

gc::Lock<Expr> IsZero(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return TestOp<std::equal_to>(args[0]);
}

gc::Lock<Expr> IsPositive(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return TestOp<std::greater>(args[0]);
}

gc::Lock<Expr> IsNegative(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return TestOp<std::less>(args[0]);
}

gc::Lock<Expr> IsOdd(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto* num = TryNumber(args[0]);
  auto* as_int = num->AsInt();
  if (!as_int) {
    throw RuntimeException("expected integer, given float", num);
  }

  return gc::Lock<Expr>(as_int->val() % 2 == 1 ? True() : False());
}

gc::Lock<Expr> IsEven(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto* num = TryNumber(args[0]);
  auto* as_int = num->AsInt();
  if (!as_int) {
    throw RuntimeException("expected integer, given float", num);
  }

  return gc::Lock<Expr>(as_int->val() % 2 == 0 ? True() : False());
}

gc::Lock<Expr> Plus(Env* env, Expr** args, size_t num_args) {
  auto accum = gc::make_locked<Int>(0);
  return ArithOp<std::plus>(env, accum.get(), args, num_args);
}

gc::Lock<Expr> Star(Env* env, Expr** args, size_t num_args) {
  auto accum = gc::make_locked<Int>(1);
  return ArithOp<std::multiplies>(env, accum.get(), args, num_args);
}

gc::Lock<Expr> Minus(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 1);
  auto accum = TryNumber(args[0])->Clone();
  return ArithOp<std::minus>(env, accum.get(), args + 1, num_args - 1);
}

gc::Lock<Expr> Slash(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 1);
  auto accum = TryNumber(args[0])->Clone();
  return ArithOp<std::divides>(env, accum.get(), args + 1, num_args - 1);
}

gc::Lock<Expr> Abs(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto* num = TryNumber(args[0]);

  if (auto* as_int = num->AsInt()) {
    return gc::Lock<Expr>(as_int->val() >= 0 ? as_int
                                             : new Int(-as_int->val()));
  }

  auto* as_float = num->AsFloat();
  assert(as_float);
  return gc::Lock<Expr>(as_float->val() >= 0.0 ? as_float
                                               : new Float(-as_float->val()));
}

gc::Lock<Expr> Quotient(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  bool is_exact = true;
  Int::ValType arg1 = TryGetIntValOrRound(args[0], &is_exact);
  Int::ValType arg2 = TryGetIntValOrRound(args[1], &is_exact);
  Int::ValType ret = arg1 / arg2;

  return is_exact ? gc::Lock<Expr>(new Int(ret))
                  : gc::Lock<Expr>(new Float(ret));
}

gc::Lock<Expr> Remainder(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  bool is_exact = true;
  Int::ValType arg1 = TryGetIntValOrRound(args[0], &is_exact);
  Int::ValType arg2 = TryGetIntValOrRound(args[1], &is_exact);

  Int::ValType quotient = arg1 / arg2;
  Int::ValType ret = arg1 - arg2 * quotient;

  return is_exact ? gc::Lock<Expr>(new Int(ret))
                  : gc::Lock<Expr>(new Float(ret));
}

gc::Lock<Expr> Modulo(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  bool is_exact = true;
  Int::ValType arg1 = TryGetIntValOrRound(args[0], &is_exact);
  Int::ValType arg2 = TryGetIntValOrRound(args[1], &is_exact);

  Int::ValType quotient = arg1 / arg2;
  Int::ValType ret = arg1 - arg2 * quotient;
  if ((ret < 0) ^ (arg2 < 0)) {
    ret += arg2;
  }

  return is_exact ? gc::Lock<Expr>(new Int(ret))
                  : gc::Lock<Expr>(new Float(ret));
}

// TODO(bcf): Implement in lisp
gc::Lock<Expr> Gcd(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

// TODO(bcf): Implement in lisp
gc::Lock<Expr> Lcm(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Numerator(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Denominator(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Floor(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto* num = TryNumber(args[0]);
  if (num->num_type() == Number::Type::INT) {
    return gc::Lock<Expr>(num);
  }

  assert(num->num_type() == Number::Type::FLOAT);
  return gc::Lock<Expr>(new Float(std::floor(num->AsFloat()->val())));
}

gc::Lock<Expr> Ceiling(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto* num = TryNumber(args[0]);
  if (num->num_type() == Number::Type::INT) {
    return gc::Lock<Expr>(num);
  }

  assert(num->num_type() == Number::Type::FLOAT);
  return gc::Lock<Expr>(new Float(std::ceil(num->AsFloat()->val())));
}

gc::Lock<Expr> Truncate(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto* num = TryNumber(args[0]);
  if (num->num_type() == Number::Type::INT) {
    return gc::Lock<Expr>(num);
  }

  assert(num->num_type() == Number::Type::FLOAT);
  return gc::Lock<Expr>(new Float(std::trunc(num->AsFloat()->val())));
}

gc::Lock<Expr> Round(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto* num = TryNumber(args[0]);
  if (num->num_type() == Number::Type::INT) {
    return gc::Lock<Expr>(num);
  }

  assert(num->num_type() == Number::Type::FLOAT);
  return gc::Lock<Expr>(new Float(std::round(num->AsFloat()->val())));
}

gc::Lock<Expr> Rationalize(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Exp(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return EvalUnaryFloatOp<std::exp>(args[0]);
}

gc::Lock<Expr> Log(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return EvalUnaryFloatOp<std::exp>(args[0]);
}

gc::Lock<Expr> Sin(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return EvalUnaryFloatOp<std::sin>(args[0]);
}

gc::Lock<Expr> Cos(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return EvalUnaryFloatOp<std::cos>(args[0]);
}

gc::Lock<Expr> Tan(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return EvalUnaryFloatOp<std::tan>(args[0]);
}

gc::Lock<Expr> Asin(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return EvalUnaryFloatOp<std::asin>(args[0]);
}

gc::Lock<Expr> ACos(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return EvalUnaryFloatOp<std::acos>(args[0]);
}

gc::Lock<Expr> ATan(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsLe(num_args, 2);
  if (num_args == 1) {
    return EvalUnaryFloatOp<std::atan>(args[0]);
  }

  return EvalBinaryFloatOp<std::atan2>(args[0], args[1]);
}

gc::Lock<Expr> Sqrt(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return EvalUnaryFloatOp<std::sqrt>(args[0]);
}

gc::Lock<Expr> Expt(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalBinaryFloatOp<std::pow>(args[0], args[1]);
}

gc::Lock<Expr> MakeRectangular(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> MakePolar(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> RealPart(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> ImagPart(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Magnitude(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Angle(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> ExactToInexact(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto* num = TryNumber(args[0]);
  if (auto* as_float = num->AsFloat()) {
    return gc::Lock<Expr>(as_float);
  }

  auto* as_int = num->AsInt();
  assert(as_int);
  return gc::Lock<Expr>(new Float(as_int->val()));
}

gc::Lock<Expr> InexactToExact(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto* num = TryNumber(args[0]);
  if (auto* as_int = num->AsInt()) {
    return gc::Lock<Expr>(as_int);
  }

  auto* as_float = num->AsFloat();
  assert(as_float);
  return gc::Lock<Expr>(new Int(as_float->val()));
}

gc::Lock<Expr> NumberToString(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsLe(num_args, 2);
  auto* num = TryNumber(args[0]);

  int radix = num_args == 2 ? TryInt(args[1])->val() : 10;
  if (auto* as_float = num->AsFloat()) {
    if (radix != 10) {
      throw RuntimeException("inexact numbers can only be printed in base 10",
                             nullptr);
    }

    std::ostringstream oss;
    oss << as_float->val();

    return gc::Lock<Expr>(new expr::String(oss.str()));
  }

  auto* as_int = num->AsInt();
  assert(as_int);

  std::string ret;
  switch (radix) {
    case 2:
      for (Int::ValType cur = as_int->val(); cur; cur /= 10) {
        if (cur & 0x1) {
          ret.push_back('1');
        }
      }
      ret.reserve();
      break;
    case 8: {
      std::ostringstream oss;
      oss << std::oct << as_int->val();
      ret = oss.str();
      break;
    }
    case 10: {
      ret = std::to_string(as_int->val());
      break;
    }
    case 16: {
      std::ostringstream oss;
      oss << std::hex << as_int->val();
      ret = oss.str();
      break;
    }
    default:
      throw RuntimeException("radix must be one of 2 8 10 16", nullptr);
  }

  return gc::Lock<Expr>(new expr::String(ret));
}

gc::Lock<Expr> StringToNumber(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsLe(num_args, 2);
  int radix = num_args == 2 ? TryInt(args[1])->val() : 10;
  const auto& str_val = TryString(args[0])->val();
  try {
    return gc::Lock<Expr>(parse::Lexer::LexNum(str_val, radix).get());
  } catch (const util::SyntaxException& e) {
    return gc::Lock<Expr>(False());
  }
}

gc::Lock<Expr> Not(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0] == False() ? True() : False());
}

gc::Lock<Expr> IsBoolean(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::BOOL ? True() : False());
}

gc::Lock<Expr> IsPair(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::PAIR ? True() : False());
}

gc::Lock<Expr> Cons(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return gc::Lock<Expr>(new Pair(args[0], args[1]));
}

gc::Lock<Expr> Car(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(TryPair(args[0])->car());
}

gc::Lock<Expr> Cdr(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(TryPair(args[0])->cdr());
}

gc::Lock<Expr> SetCar(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  TryPair(args[0])->set_car(args[1]);
  return gc::Lock<Expr>(Nil());
}

gc::Lock<Expr> SetCdr(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  TryPair(args[0])->set_cdr(args[1]);
  return gc::Lock<Expr>(Nil());
}

gc::Lock<Expr> IsNull(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0] == Nil() ? True() : False());
}

gc::Lock<Expr> IsList(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  Expr* cur = args[0];

  std::set<Expr*> seen;
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    // If we found a loop, it's not a list.
    if (seen.find(list) != seen.end()) {
      return gc::Lock<Expr>(False());
    }
    seen.insert(list);
  }

  return gc::Lock<Expr>(cur == Nil() ? True() : False());
}

gc::Lock<Expr> List(Env* env, Expr** args, size_t num_args) {
  gc::Lock<Expr> front(Nil());
  for (ssize_t i = num_args - 1; i >= 0; --i) {
    front.reset(new Pair(args[i], front.get()));
  }

  return gc::Lock<Expr>(front.get());
}

gc::Lock<Expr> Length(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  Int::ValType ret = 0;
  Expr* cur = args[0];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    ++ret;
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  return gc::Lock<Expr>(new Int(ret));
}

gc::Lock<Expr> Append(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 1);
  gc::Lock<Expr> ret(Nil());
  Pair* back = nullptr;

  for (size_t i = 0; i < num_args - 1; ++i) {
    Pair* cur_back = nullptr;
    auto copy = CopyList(args[i], &cur_back);
    if (copy.get() == Nil()) {
      continue;
    }
    if (ret.get() == Nil()) {
      ret = copy;
    } else {
      assert(cur_back);
      assert(cur_back->cdr() == Nil());
      cur_back->set_cdr(copy.get());
    }
    back = cur_back;
  }

  Expr* last = args[num_args - 1];
  if (ret.get() == Nil()) {
    return gc::Lock<Expr>(last);
  }

  assert(back);
  assert(back->cdr() == Nil());
  back->set_cdr(last);
  return ret;
}

gc::Lock<Expr> Reverse(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  gc::Lock<Expr> ret(Nil());
  Expr* cur = args[0];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    ret.reset(new Pair(list->car(), ret.get()));
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  return ret;
}

gc::Lock<Expr> ListTail(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  Int::ValType k = TryInt(args[1])->val();

  Expr* cur = args[0];
  for (; auto *list = cur->AsPair(); cur = list->cdr(), --k) {
    if (k == 0) {
      return gc::Lock<Expr>(cur);
    }
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  if (k != 0) {
    throw RuntimeException("index too large for list", nullptr);
  }

  return gc::Lock<Expr>(Nil());
}

gc::Lock<Expr> ListRef(Env* env, Expr** args, size_t num_args) {
  auto tail = ListTail(env, args, num_args);
  return gc::Lock<Expr>(TryPair(tail.get())->car());
}

gc::Lock<Expr> Memq(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    if (args[0]->Eq(list->car())) {
      return gc::Lock<Expr>(list);
    }
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  return gc::Lock<Expr>(False());
}

gc::Lock<Expr> Memv(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    if (args[0]->Eqv(list->car())) {
      return gc::Lock<Expr>(list);
    }
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  return gc::Lock<Expr>(False());
}

gc::Lock<Expr> Member(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    if (args[0]->Equal(list->car())) {
      return gc::Lock<Expr>(list);
    }
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  return gc::Lock<Expr>(False());
}

gc::Lock<Expr> Assq(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    Pair* head = TryPair(list->car());
    if (head->car()->Eq(args[0])) {
      return gc::Lock<Expr>(head);
    }
  }

  return gc::Lock<Expr>(False());
}

gc::Lock<Expr> Assv(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    Pair* head = TryPair(list->car());
    if (head->car()->Eqv(args[0])) {
      return gc::Lock<Expr>(head);
    }
  }

  return gc::Lock<Expr>(False());
}

gc::Lock<Expr> Assoc(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    Pair* head = TryPair(list->car());
    if (head->car()->Equal(args[0])) {
      return gc::Lock<Expr>(head);
    }
  }

  return gc::Lock<Expr>(False());
}

gc::Lock<Expr> IsSymbol(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::SYMBOL ? True()
                                                              : False());
}

gc::Lock<Expr> SymbolToString(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(
      new expr::String(TrySymbol(args[0])->val(), true /* read_only */));
}

gc::Lock<Expr> StringToSymbol(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(Symbol::New(TryString(args[0])->val()));
}

gc::Lock<Expr> IsChar(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::CHAR ? True() : False());
}

gc::Lock<Expr> IsCharEq(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalCharOp<std::equal_to>(args[0], args[1]);
}

gc::Lock<Expr> IsCharLt(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalCharOp<std::less>(args[0], args[1]);
}

gc::Lock<Expr> IsCharGt(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalCharOp<std::greater>(args[0], args[1]);
}

gc::Lock<Expr> IsCharLe(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalCharOp<std::less_equal>(args[0], args[1]);
}

gc::Lock<Expr> IsCharGe(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalCharOp<std::greater_equal>(args[0], args[1]);
}

gc::Lock<Expr> IsCharCiEq(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalCharCiOp<std::equal_to>(args[0], args[1]);
}

gc::Lock<Expr> IsCharCiLt(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalCharCiOp<std::less>(args[0], args[1]);
}

gc::Lock<Expr> IsCharCiGt(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalCharCiOp<std::greater>(args[0], args[1]);
}

gc::Lock<Expr> IsCharCiLe(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalCharCiOp<std::less_equal>(args[0], args[1]);
}

gc::Lock<Expr> IsCharCiGe(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalCharCiOp<std::greater_equal>(args[0], args[1]);
}

gc::Lock<Expr> IsCharAlphabetic(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return CheckUnaryCharOp<std::isalpha>(args[0]);
}

gc::Lock<Expr> IsCharNumeric(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return CheckUnaryCharOp<std::isdigit>(args[0]);
}

gc::Lock<Expr> IsCharWhitespace(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return CheckUnaryCharOp<std::isspace>(args[0]);
}

gc::Lock<Expr> IsCharUpperCase(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return CheckUnaryCharOp<std::isupper>(args[0]);
}

gc::Lock<Expr> IsCharLowerCase(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return CheckUnaryCharOp<std::tolower>(args[0]);
}

gc::Lock<Expr> CharToInteger(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(new Int(TryChar(args[0])->val()));
}

gc::Lock<Expr> IntegerToChar(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto* as_int = TryInt(args[0]);
  if (as_int->val() > std::numeric_limits<Char::ValType>::max()) {
    throw RuntimeException("Value out of range", as_int);
  }
  return gc::Lock<Expr>(new Char(as_int->val()));
}

gc::Lock<Expr> CharUpCase(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto char_val = TryChar(args[0])->val();
  return gc::Lock<Expr>(
      std::isupper(char_val) ? args[0] : new Char(std::toupper(char_val)));
}

gc::Lock<Expr> CharDownCase(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto char_val = TryChar(args[0])->val();
  return gc::Lock<Expr>(
      std::islower(char_val) ? args[0] : new Char(std::tolower(char_val)));
}

gc::Lock<Expr> IsString(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::STRING ? True()
                                                              : False());
}

gc::Lock<Expr> MakeString(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsLe(num_args, 2);

  auto len = TryGetNonNegExactIntVal(args[0]);
  char init_value = ' ';
  if (num_args == 2) {
    init_value = TryChar(args[1])->val();
  }

  return gc::Lock<Expr>(new expr::String(std::string(len, init_value)));
}

gc::Lock<Expr> String(Env* env, Expr** args, size_t num_args) {
  std::string val;
  for (size_t i = 0; i < num_args; ++i) {
    val.push_back(TryChar(args[i])->val());
  }

  return gc::Lock<Expr>(new expr::String(std::move(val)));
}

gc::Lock<Expr> StringLength(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(new Int(TryString(args[0])->val().size()));
}

gc::Lock<Expr> StringRef(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  auto& string_val = TryString(args[0])->val();
  auto idx = TryGetNonNegExactIntVal(args[1], string_val.size());
  return gc::Lock<Expr>(new Char(string_val[idx]));
}

gc::Lock<Expr> StringSet(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 3);
  auto* str = TryString(args[0]);
  if (str->read_only()) {
    throw RuntimeException("Attempt to write read only string", str);
  }

  auto idx = TryGetNonNegExactIntVal(args[1], str->val().size());
  str->set_val_idx(idx, TryChar(args[2])->val());
  return gc::Lock<Expr>(Nil());
}

gc::Lock<Expr> IsStringEq(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalStringOp<std::equal_to<std::string>>(args[0], args[1]);
}

gc::Lock<Expr> IsStringEqCi(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalStringOp<ICaseCmpStr<std::equal_to>>(args[0], args[1]);
}

gc::Lock<Expr> IsStringLt(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalStringOp<std::less<std::string>>(args[0], args[1]);
}

gc::Lock<Expr> IsStringGt(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalStringOp<std::greater<std::string>>(args[0], args[1]);
}

gc::Lock<Expr> IsStringLe(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalStringOp<std::less_equal<std::string>>(args[0], args[1]);
}

gc::Lock<Expr> IsStringGe(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalStringOp<std::greater_equal<std::string>>(args[0], args[1]);
}

gc::Lock<Expr> IsStringLtCi(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalStringOp<ICaseCmpStr<std::less>>(args[0], args[1]);
}

gc::Lock<Expr> IsStringGtCi(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalStringOp<ICaseCmpStr<std::greater>>(args[0], args[1]);
}

gc::Lock<Expr> IsStringLeCi(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalStringOp<ICaseCmpStr<std::less_equal>>(args[0], args[1]);
}

gc::Lock<Expr> IsStringGeCi(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return EvalStringOp<ICaseCmpStr<std::greater_equal>>(args[0], args[1]);
}

gc::Lock<Expr> Substring(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 3);
  auto& str_val = TryString(args[0])->val();
  auto start = TryGetNonNegExactIntVal(args[1], str_val.size());
  auto end = TryGetNonNegExactIntVal(args[2], str_val.size());

  return gc::Lock<Expr>(new expr::String(str_val.substr(start, end - start)));
}

gc::Lock<Expr> StringAppend(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return gc::Lock<Expr>(
      new expr::String(TryString(args[0])->val() + TryString(args[1])->val()));
}

gc::Lock<Expr> StringToList(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  auto& str_val = TryString(args[0])->val();

  gc::Lock<Expr> ret(Nil());
  for (ssize_t i = str_val.size() - 1; i >= 0; --i) {
    ret.reset(new Pair(new Char(str_val[i]), ret.get()));
  }

  return ret;
}

gc::Lock<Expr> ListToString(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  std::string str;
  Expr* cur = args[0];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    str.push_back(TryChar(list->car())->val());
  }

  return gc::Lock<Expr>(new expr::String(str));
}

gc::Lock<Expr> StringCopy(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(new expr::String(TryString(args[0])->val()));
}

gc::Lock<Expr> StringFill(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  auto* str = TryString(args[0]);
  if (str->read_only()) {
    throw RuntimeException("Attempt to write read only string", str);
  }
  auto char_val = TryChar(args[1])->val();
  for (size_t i = 0; i < str->val().size(); ++i) {
    str->set_val_idx(i, char_val);
  }
  return gc::Lock<Expr>(Nil());
}

gc::Lock<Expr> IsVector(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::VECTOR ? True()
                                                              : False());
}

gc::Lock<Expr> MakeVector(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsLe(num_args, 2);

  auto count = TryGetNonNegExactIntVal(args[0]);
  Expr* init_val = num_args == 2 ? args[1] : Nil();
  return gc::Lock<Expr>(new expr::Vector(std::vector<Expr*>(count, init_val)));
}

gc::Lock<Expr> Vector(Env* env, Expr** args, size_t num_args) {
  return gc::Lock<Expr>(
      new expr::Vector(std::vector<Expr*>(args, args + num_args)));
}

gc::Lock<Expr> VectorLength(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(new Int(TryVector(args[0])->vals().size()));
}

gc::Lock<Expr> VectorRef(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  auto& vec = TryVector(args[0])->vals();
  auto idx = TryGetNonNegExactIntVal(args[1], vec.size());
  return gc::Lock<Expr>(vec[idx]);
}

gc::Lock<Expr> VectorSet(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 3);
  auto& vec = TryVector(args[0])->vals();
  auto idx = TryGetNonNegExactIntVal(args[1], vec.size());
  vec[idx] = args[2];
  return gc::Lock<Expr>(Nil());
}

gc::Lock<Expr> VectorToList(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  gc::Lock<Expr> ret(Nil());
  auto& vec_val = TryVector(args[0])->vals();
  for (auto it = vec_val.rbegin(); it != vec_val.rend(); ++it) {
    ret.reset(new Pair(*it, ret.get()));
  }

  return ret;
}

gc::Lock<Expr> ListToVector(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(new expr::Vector(ExprVecFromList(args[0])));
}

gc::Lock<Expr> VectorFill(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  auto& vec_val = TryVector(args[0])->vals();

  for (auto& val : vec_val) {
    val = args[1];
  }

  return gc::Lock<Expr>(Nil());
}

gc::Lock<Expr> IsProcedure(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::EVALS ? True()
                                                             : False());
}

gc::Lock<Expr> Apply(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 1);
  std::vector<Expr*> new_args(args + 1, args + 1 + num_args - 2);
  if (num_args > 1) {
    Expr* cur = args[num_args - 1];
    for (; auto* list = cur->AsPair(); cur = list->cdr()) {
      new_args.push_back(list->car());
    }
    if (cur != Nil()) {
      throw RuntimeException("Expected list", args[num_args - 1]);
    }
  }

  return TryEvals(args[0])->DoEval(env, new_args.data(), new_args.size());
}

gc::Lock<Expr> Map(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  return MapImpl<true>(env, args, num_args);
}

gc::Lock<Expr> ForEach(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgsGe(num_args, 2);
  return MapImpl<false>(env, args, num_args);
}

gc::Lock<Expr> Force(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return TryEvals(args[0])->DoEval(env, nullptr, 0);
}

gc::Lock<Expr> CallWithCurrentContinuation(Env* env,
                                           Expr** args,
                                           size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Values(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> CallWithValues(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> DynamicWind(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> EvalPrim(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 2);
  return Eval(args[0], TryEnv(args[1]));
}

gc::Lock<Expr> SchemeReportEnvironment(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  if (TryGetNonNegExactIntVal(args[0]) != kSchemeVersion) {
    throw RuntimeException("Unsupported version", args[0]);
  }
  auto ret = gc::make_locked<Env>(nullptr);
  LoadPrimitives(ret.get());
  return gc::Lock<Expr>(ret.get());
}

gc::Lock<Expr> NullEnvironment(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  if (TryGetNonNegExactIntVal(args[0]) != kSchemeVersion) {
    throw RuntimeException("Unsupported version", args[0]);
  }

  auto ret = gc::make_locked<Env>(nullptr);
  LoadSyntax(ret.get());
  return gc::Lock<Expr>(ret.get());
}

gc::Lock<Expr> InteractionEnvironment(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> CallWithInputFile(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> CallWithOutputFile(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> IsInputPort(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::INPUT_PORT ? True()
                                                                  : False());
}

gc::Lock<Expr> IsOutputPort(Env* env, Expr** args, size_t num_args) {
  ExpectNumArgs(num_args, 1);
  return gc::Lock<Expr>(args[0]->type() == Expr::Type::OUTPUT_PORT ? True()
                                                                   : False());
}

gc::Lock<Expr> CurrentInputPort(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> CurrentOutputPort(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> WithInputFromFile(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> WithOutputToFile(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> OpenInputFile(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> OpenOutputFile(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> CloseInputPort(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> CloseOutputPort(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Read(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> ReadChar(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> PeekChar(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> IsEofObject(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> IsCharReady(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Write(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Display(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Newline(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> WriteChar(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> Load(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> TranscriptOn(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

gc::Lock<Expr> TranscriptOff(Env* env, Expr** args, size_t num_args) {
  throw util::RuntimeException("Not implemented", nullptr);
  assert(false && env && args && num_args);
  return {};
}

const struct {
  PrimitiveFunc func;
  const char* name;
} kSyntax[] = {
#define X(name, str) {name, #str},
#include "expr/syntax.inc"  // NOLINT(build/include)
#undef X
};

const struct {
  PrimitiveFunc func;
  const char* name;
} kPrimitives[] = {
#define X(name, str) {name, #str},
#include "expr/primitives.inc"  // NOLINT(build/include)
#undef X
};

}  // namespace

void LoadSyntax(Env* env) {
  for (const auto& syntax : kSyntax) {
    env->DefineVar(Symbol::NewLock(syntax.name).get(),
                   gc::make_locked<PrimitiveImpl>(syntax.name, syntax.func,
                                                  false /* eval_args */)
                       .get());
  }
}

void LoadPrimitives(Env* env) {
  LoadSyntax(env);
  for (const auto& primitive : kPrimitives) {
    env->DefineVar(Symbol::NewLock(primitive.name).get(),
                   gc::make_locked<PrimitiveImpl>(
                       primitive.name, primitive.func, true /* eval_args */)
                       .get());
  }

  std::string tmp;
  LoadCr(env, kCrDepth, &tmp);
}

}  // namespace expr
