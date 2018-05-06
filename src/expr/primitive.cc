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
#include "parse/lexer.h"
#include "util/exceptions.h"
#include "util/util.h"

#define EXPECT_ARGS_NUM(expected)                            \
  do {                                                       \
    if (num_args != expected) {                              \
      std::ostringstream os;                                 \
      os << "Expected " #expected " args. Got " << num_args; \
      throw RuntimeException(os.str(), this);                \
    }                                                        \
  } while (0)

#define EXPECT_ARGS_LE(expected)                                     \
  do {                                                               \
    if (num_args > expected) {                                       \
      std::ostringstream os;                                         \
      os << "Expected at most " #expected " args. Got " << num_args; \
      throw RuntimeException(os.str(), this);                        \
    }                                                                \
  } while (0)

#define EXPECT_ARGS_GE(expected)                                      \
  do {                                                                \
    if (num_args < expected) {                                        \
      std::ostringstream os;                                          \
      os << "Expected at least " #expected " args. Got " << num_args; \
      throw RuntimeException(os.str(), this);                         \
    }                                                                 \
  } while (0)

using eval::Eval;
using util::RuntimeException;

namespace expr {
namespace primitive {

namespace {

// Depth for car, cdr, caar etc
const int kCrDepth = 4;

const struct {
  Evals* (*expr)();
  const char* name;
} kPrimitives[] = {
#define X(name, str) {name, #str},
#include "expr/primitives.inc"  // NOLINT(build/include)
#undef X
};

void EvalArgs(Env* env, Expr** args, size_t num_args) {
  for (size_t i = 0; i < num_args; ++i) {
    args[i] = Eval(args[i], env);
  }
}

template <template <typename T> class Op>
Number* ArithOp(Env* env, Expr* initial, Expr** args, size_t num_args) {
  initial = Eval(initial, env);
  EvalArgs(env, args, num_args);
  Number* result = TryNumber(initial);
  for (size_t i = 0; i < num_args; ++i) {
    result = OpInPlace<Op>(result, TryNumber(args[i]));
  }

  return result;
}

template <template <typename T> class Op>
Expr* CmpOp(Env* env, Expr** args, size_t num_args) {
  EvalArgs(env, args, num_args);
  auto* last = TryNumber(args[0]);
  for (size_t i = 1; i < num_args; ++i) {
    auto* cur = TryNumber(args[i]);
    if (!OpCmp<Op>(last, cur)) {
      return False();
    }
    last = cur;
  }

  return True();
}

template <template <typename T> class Op>
Expr* TestOp(Env* env, Expr** args, size_t num_args) {
  EvalArgs(env, args, num_args);
  auto* num = TryNumber(args[0]);
  if (auto* as_int = num->AsInt()) {
    Op<Int::ValType> op;
    return op(as_int->val(), 0) ? True() : False();
  }

  Op<Float::ValType> op;
  return op(num->AsFloat()->val(), 0.0) ? True() : False();
}

template <template <typename T> class Op>
Expr* MostOp(Env* env, Expr** args, size_t num_args) {
  EvalArgs(env, args, num_args);
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
    return new Float(int_ret->val());
  }

  return ret;
}

Expr* ExecuteList(Env* env, Expr* cur) {
  Expr* last_eval = nullptr;
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

Expr* EvalArray(Env* env, Expr** args, size_t num_args) {
  Expr* last_eval = nullptr;
  for (size_t i = 0; i < num_args; ++i) {
    last_eval = Eval(args[i], env);
  }

  return last_eval;
}

class LambdaImpl : public Evals {
 public:
  static LambdaImpl* New(std::vector<Symbol*> required_args,
                         Symbol* variable_arg,
                         std::vector<Expr*> body,
                         Env* env) {
    required_args.shrink_to_fit();
    body.shrink_to_fit();
    return new LambdaImpl(std::move(required_args), variable_arg,
                          std::move(body), env);
  }

 private:
  // Evals implementation:
  std::ostream& AppendStream(std::ostream& stream) const override;
  Expr* DoEval(Env* env, Expr** args, size_t num_args) const override;

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

  const std::vector<Symbol*> required_args_;
  Symbol* variable_arg_;
  const std::vector<Expr*> body_;
  Env* env_;
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

Expr* LambdaImpl::DoEval(Env* env, Expr** args, size_t num_args) const {
  if (num_args < required_args_.size()) {
    std::ostringstream os;
    os << "Invalid number of arguments. expected ";
    if (variable_arg_ != nullptr) {
      os << "at least ";
    }

    os << required_args_.size();
    os << " given: " << num_args;
    throw RuntimeException(os.str(), this);
  }
  EvalArgs(env, args, num_args);

  auto arg_it = args;
  auto* new_env = new Env(env_);

  for (auto* sym : required_args_) {
    new_env->DefineVar(sym, *arg_it++);
  }
  if (variable_arg_ != nullptr) {
    auto* rest = ListFromIt(arg_it, args + num_args);
    new_env->DefineVar(variable_arg_, rest);
  }

  assert(body_.size() >= 1);
  for (size_t i = 0; i < body_.size() - 1; ++i) {
    Eval(body_[i], new_env);
  }
  return Eval(body_.back(), new_env);
}

class CrImpl : public Evals {
 public:
  static CrImpl* New(std::string cr) {
    cr.shrink_to_fit();
    return new CrImpl(std::move(cr));
  }

  // Evals implementation:
  std::ostream& AppendStream(std::ostream& stream) const override {
    return stream << "c" << cr_ << "r";
  }
  Expr* DoEval(Env* env, Expr** args, size_t num_args) const override {
    EXPECT_ARGS_NUM(1);
    EvalArgs(env, args, num_args);
    return TryPair(args[0])->Cr(cr_);
  }

  explicit CrImpl(std::string cr) : cr_(cr) {}
  ~CrImpl() override = default;

 private:
  std::string cr_;
};

void LoadCr(Env* env, size_t depth, std::string* cur) {
  if (depth == 0)
    return;
  // car and cdr are defined manually for performance reasons.
  if (cur->size() > 1) {
    auto sym_name = "c" + *cur + "r";
    env->DefineVar(Symbol::New(sym_name), CrImpl::New(*cur));
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

Number* ExactIfPossible(Float::ValType val) {
  Int::ValType int_val = std::trunc(val);
  if (int_val == val) {
    return new Int(int_val);
  }
  return new Float(val);
}

template <Float::ValType (*Op)(Float::ValType)>
Expr* EvalUnaryFloatOp(Env* env, Expr** args) {
  EvalArgs(env, args, 1);
  return ExactIfPossible(Op(TryGetFloatVal(args[0])));
}

template <Float::ValType (*Op)(Float::ValType, Float::ValType)>
Expr* EvalBinaryFloatOp(Env* env, Expr** args) {
  EvalArgs(env, args, 2);
  Float::ValType n1 = TryGetFloatVal(args[0]);
  Float::ValType n2 = TryGetFloatVal(args[1]);
  return ExactIfPossible(Op(n1, n2));
}

Expr* CopyList(Expr* list, Pair** last_link) {
  Expr* cur = list;
  Expr* ret = Nil();
  Pair* prev = nullptr;
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    Pair* copy = new Pair(list->car(), Nil());

    if (prev) {
      prev->set_cdr(copy);
    } else {
      ret = copy;
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
Expr* EvalCharOp(Env* env, Expr** args, size_t num_args) {
  EvalArgs(env, args, num_args);
  auto* c1 = TryChar(args[0]);
  auto* c2 = TryChar(args[1]);

  Op<Char::ValType> op;
  return op(c1->val(), c2->val()) ? True() : False();
}

template <template <typename T> class Op>
Expr* EvalCharCiOp(Env* env, Expr** args, size_t num_args) {
  EvalArgs(env, args, num_args);
  auto* c1 = TryChar(args[0]);
  auto* c2 = TryChar(args[1]);

  Op<Char::ValType> op;
  return op(std::tolower(c1->val()), std::tolower(c2->val())) ? True()
                                                              : False();
}

template <int (*Op)(int)>
Expr* CheckUnaryCharOp(Env* env, Expr** args, size_t num_args) {
  EvalArgs(env, args, num_args);
  return Op(TryChar(args[0])->val()) ? True() : False();
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
Expr* EvalStringOp(Env* env, Expr** args, size_t num_args) {
  EvalArgs(env, args, num_args);
  auto* s1 = TryString(args[0]);
  auto* s2 = TryString(args[1]);

  Op op;
  return op(s1->val(), s2->val()) ? True() : False();
}

template <template <typename T> class Op>
struct ICaseCmpStr {
  bool operator()(const std::string& s1, const std::string& s2) {
    Op<int> op;
    return op(strcasecmp(s1.c_str(), s2.c_str()), 0);
  }
};

template <bool need_return>
Expr* MapImpl(Env* env, Expr** args, size_t num_args) {
  static constexpr char kEqualSizeListErr[] =
      "Expected equal sized argument lists";
  EvalArgs(env, args, num_args);
  Evals* procedure = TryEvals(args[0]);

  Expr* ret = Nil();
  Pair* prev = nullptr;

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
        expr = new Pair(Symbol::New("quote"), new Pair(expr, Nil()));
      }
    }

    Expr* res = procedure->DoEval(env, new_args.data(), new_args.size());
    if (need_return) {
      Pair* new_link = new Pair(res, Nil());
      if (prev) {
        prev->set_cdr(new_link);
      } else {
        ret = new_link;
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
  Expr* DoEval(Env* /* env */,
               Expr** /* args */,
               size_t /* num_args */) const override {
    if (!forced_val_) {
      forced_val_ = Eval(expr_, env_);
      expr_ = nullptr;
      env_ = nullptr;
    }

    return forced_val_;
  }

 private:
  mutable Expr* expr_;
  mutable Env* env_;
  mutable Expr* forced_val_ = nullptr;
};

}  // namespace

namespace impl {

// Declare Class Base
#define X(Name, str)                                                     \
  class Name : public Evals {                                            \
    Expr* DoEval(Env* env, Expr** args, size_t num_args) const override; \
    std::ostream& AppendStream(std::ostream& stream) const {             \
      return stream << #str;                                             \
    }                                                                    \
  };
#include "expr/primitives.inc"  // NOLINT(build/include)
#undef X

Expr* Quote::DoEval(Env* /* env */, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return args[0];
}

Expr* Lambda::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  std::vector<Symbol*> req_args;
  Symbol* var_arg = nullptr;
  switch (args[0]->type()) {
    case Type::EMPTY_LIST:
      break;
    case Type::SYMBOL:
      var_arg = args[0]->AsSymbol();
      break;
    case Type::PAIR: {
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
      throw RuntimeException("Expected arguments", this);
  }

  // TODO(bcf): Analyze in other places as appropriate.
  for (size_t i = 1; i < num_args; ++i) {
    args[i] = eval::Analyze(args[i]);
  }

  return LambdaImpl::New(req_args, var_arg, {args + 1, args + num_args}, env);
}

Expr* If::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  EXPECT_ARGS_LE(3);
  auto* cond = Eval(args[0], env);
  if (cond == False()) {
    if (num_args == 3) {
      return Eval(args[2], env);
    } else {
      return Nil();
    }
  }
  return Eval(args[1], env);
}

Expr* Set::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  env->SetVar(TrySymbol(args[0]), Eval(args[1], env));
  return Nil();
}

Expr* Begin::DoEval(Env* env, Expr** args, size_t num_args) const {
  if (num_args == 0) {
    return Nil();
  }
  for (; num_args > 1; --num_args, ++args) {
    Eval(*args, env);
  }
  return Eval(*args, env);
}

Expr* Do::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Delay::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return new Promise(args[0], env);
}

Expr* Quasiquote::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* LetSyntax::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* LetRecSyntax::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* SyntaxRules::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Define::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  env->DefineVar(TrySymbol(args[0]), Eval(args[1], env));
  // TODO(bcf): handle define lambda.

  return Nil();
}

Expr* DefineSyntax::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsEqv::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  return args[0]->Eqv(args[1]) ? True() : False();
}

Expr* IsEq::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  return args[0]->Eq(args[1]) ? True() : False();
}

Expr* IsEqual::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  return args[0]->Equal(args[1]) ? True() : False();
}

Expr* IsNumber::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0]->type() == Expr::Type::NUMBER ? True() : False();
}

Expr* IsComplex::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  // We don't support complex.
  return True();
}

Expr* IsReal::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  // We don't support complex.
  return True();
}

Expr* IsRational::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  // We don't support rational.
  return False();
}

Expr* IsInteger::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto* num = args[0]->AsNumber();
  if (!num) {
    return False();
  }

  if (num->num_type() == Number::Type::INT) {
    return True();
  }

  auto* as_float = num->AsFloat();
  assert(as_float);

  return as_float->val() == std::floor(as_float->val()) ? True() : False();
}

Expr* IsExact::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0]->type() == Expr::Type::NUMBER && args[0]->AsNumber()->exact()
             ? True()
             : False();
}

Expr* IsInexact::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0]->type() == Expr::Type::NUMBER && args[0]->AsNumber()->exact()
             ? False()
             : True();
}

Expr* Min::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(1);
  return MostOp<std::less>(env, args, num_args);
}

Expr* Max::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(1);
  return MostOp<std::greater>(env, args, num_args);
}

Expr* Arrow::DoEval(Env* /* env */,
                    Expr** /* args */,
                    size_t /* num_args */) const {
  throw RuntimeException("Arrow expression cannot be called", this);
}

Expr* Else::DoEval(Env* /* env */,
                   Expr** /* args */,
                   size_t /* num_args */) const {
  throw RuntimeException("else expression cannot be called", this);
}

Expr* Cond::DoEval(Env* env, Expr** args, size_t num_args) const {
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
          env->Lookup(first_sym) == primitive::Else()) {
        return ExecuteList(env, pair->cdr());
      }
    }

    auto* test = Eval(first, env);
    if (test == False()) {
      continue;
    }

    if (pair->cdr() == Nil()) {
      return test;
    }
    auto* second = pair->Cr("ad");
    auto* second_sym = second ? second->AsSymbol() : nullptr;
    if (second_sym && second_sym->val() == "=>" &&
        env->Lookup(second_sym) == primitive::Arrow()) {
      auto* trailing = pair->Cr("ddd");
      if (trailing != Nil()) {
        throw RuntimeException("Unexpected expression", trailing);
      }

      auto* val = Eval(pair->Cr("add"), env);
      auto* func = val->AsEvals();
      if (!func) {
        throw RuntimeException("Expected procedure", val);
      }

      return func->DoEval(env, &test, 1);
    }

    return ExecuteList(env, pair->cdr());
  }

  return Nil();
}

Expr* Case::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(1);
  auto* key = Eval(args[0], env);
  for (size_t i = 1; i < num_args; ++i) {
    auto* clause = args[i]->AsPair();
    if (!clause) {
      throw RuntimeException("cond: bad syntax, expected clause", args[i]);
    }

    if (i == num_args - 1) {
      auto* first_sym = clause->car()->AsSymbol();
      if (first_sym && first_sym->val() == "else" &&
          env->Lookup(first_sym) == primitive::Else()) {
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

  return Nil();
}

Expr* And::DoEval(Env* env, Expr** args, size_t num_args) const {
  Expr* e = nullptr;
  for (size_t i = 0; i < num_args; ++i) {
    e = Eval(args[i], env);
    if (e == False()) {
      return e;
    }
  }

  return e ? e : True();
}

Expr* Or::DoEval(Env* env, Expr** args, size_t num_args) const {
  for (size_t i = 0; i < num_args; ++i) {
    auto* e = Eval(args[i], env);
    if (e != False()) {
      return e;
    }
  }

  return False();
}

// TODO(bcf): Named let.
Expr* Let::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  Expr* cur = TryPair(args[0]);
  auto* new_env = new Env(env);
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
    new_env->DefineVar(var, Eval(val, env));

    cur = pair->cdr();
  }
  if (cur != Nil()) {
    throw RuntimeException("let: Malformed binding list", cur);
  }

  return EvalArray(new_env, args + 1, num_args - 1);
}

Expr* LetStar::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  Expr* cur = TryPair(args[0]);
  auto* new_env = new Env(env);
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
    new_env->DefineVar(var, Eval(val, new_env));

    cur = pair->cdr();
  }
  if (cur != Nil()) {
    throw RuntimeException("let: Malformed binding list", cur);
  }

  return EvalArray(new_env, args + 1, num_args - 1);
}

Expr* LetRec::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  auto* new_env = new Env(env);
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
    new_env->SetVar(binding.first, Eval(binding.second, new_env));
  }

  return EvalArray(new_env, args + 1, num_args - 1);
}

Expr* OpEq::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  return CmpOp<std::equal_to>(env, args, num_args);
}

Expr* OpLt::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  return CmpOp<std::less>(env, args, num_args);
}

Expr* OpGt::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  return CmpOp<std::greater>(env, args, num_args);
}

Expr* OpLe::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  return CmpOp<std::less_equal>(env, args, num_args);
}

Expr* OpGe::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  return CmpOp<std::greater_equal>(env, args, num_args);
}

Expr* IsZero::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return TestOp<std::equal_to>(env, args, num_args);
}

Expr* IsPositive::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return TestOp<std::greater>(env, args, num_args);
}

Expr* IsNegative::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return TestOp<std::less>(env, args, num_args);
}

Expr* IsOdd::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto* num = TryNumber(args[0]);
  auto* as_int = num->AsInt();
  if (!as_int) {
    throw RuntimeException("expected integer, given float", num);
  }

  return as_int->val() % 2 == 1 ? True() : False();
}

Expr* IsEven::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto* num = TryNumber(args[0]);
  auto* as_int = num->AsInt();
  if (!as_int) {
    throw RuntimeException("expected integer, given float", num);
  }

  return as_int->val() % 2 == 0 ? True() : False();
}

Expr* Plus::DoEval(Env* env, Expr** args, size_t num_args) const {
  return ArithOp<std::plus>(env, new Int(0), args, num_args);
}

Expr* Star::DoEval(Env* env, Expr** args, size_t num_args) const {
  return ArithOp<std::multiplies>(env, new Int(1), args, num_args);
}

Expr* Minus::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(1);
  return ArithOp<std::minus>(env, *args, args + 1, num_args - 1);
}

Expr* Slash::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(1);
  return ArithOp<std::divides>(env, *args, args + 1, num_args - 1);
}

Expr* Abs::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto* num = TryNumber(args[0]);

  if (auto* as_int = num->AsInt()) {
    return as_int->val() >= 0 ? as_int : new Int(-as_int->val());
  }

  auto* as_float = num->AsFloat();
  assert(as_float);
  return as_float->val() >= 0.0 ? as_float : new Float(-as_float->val());
}

Expr* Quotient::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  bool is_exact = true;
  Int::ValType arg1 = TryGetIntValOrRound(args[0], &is_exact);
  Int::ValType arg2 = TryGetIntValOrRound(args[1], &is_exact);
  Int::ValType ret = arg1 / arg2;

  if (is_exact) {
    return new Int(ret);
  }
  return new Float(ret);
}

Expr* Remainder::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  bool is_exact = true;
  Int::ValType arg1 = TryGetIntValOrRound(args[0], &is_exact);
  Int::ValType arg2 = TryGetIntValOrRound(args[1], &is_exact);

  Int::ValType quotient = arg1 / arg2;
  Int::ValType ret = arg1 - arg2 * quotient;

  if (is_exact) {
    return new Int(ret);
  }
  return new Float(ret);
}

Expr* Modulo::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  bool is_exact = true;
  Int::ValType arg1 = TryGetIntValOrRound(args[0], &is_exact);
  Int::ValType arg2 = TryGetIntValOrRound(args[1], &is_exact);

  Int::ValType quotient = arg1 / arg2;
  Int::ValType ret = arg1 - arg2 * quotient;
  if ((ret < 0) ^ (arg2 < 0)) {
    ret += arg2;
  }

  if (is_exact) {
    return new Int(ret);
  }
  return new Float(ret);
}

// TODO(bcf): Implement in lisp
Expr* Gcd::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

// TODO(bcf): Implement in lisp
Expr* Lcm::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Numerator::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Denominator::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Floor::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto* num = TryNumber(args[0]);
  if (num->num_type() == Number::Type::INT) {
    return num;
  }

  assert(num->num_type() == Number::Type::FLOAT);
  return new Float(std::floor(num->AsFloat()->val()));
}

Expr* Ceiling::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto* num = TryNumber(args[0]);
  if (num->num_type() == Number::Type::INT) {
    return num;
  }

  assert(num->num_type() == Number::Type::FLOAT);
  return new Float(std::ceil(num->AsFloat()->val()));
}

Expr* Truncate::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto* num = TryNumber(args[0]);
  if (num->num_type() == Number::Type::INT) {
    return num;
  }

  assert(num->num_type() == Number::Type::FLOAT);
  return new Float(std::trunc(num->AsFloat()->val()));
}

Expr* Round::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto* num = TryNumber(args[0]);
  if (num->num_type() == Number::Type::INT) {
    return num;
  }

  assert(num->num_type() == Number::Type::FLOAT);
  return new Float(std::round(num->AsFloat()->val()));
}

Expr* Rationalize::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Exp::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return EvalUnaryFloatOp<std::exp>(env, args);
}

Expr* Log::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return EvalUnaryFloatOp<std::exp>(env, args);
}

Expr* Sin::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return EvalUnaryFloatOp<std::sin>(env, args);
}

Expr* Cos::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return EvalUnaryFloatOp<std::cos>(env, args);
}

Expr* Tan::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return EvalUnaryFloatOp<std::tan>(env, args);
}

Expr* Asin::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return EvalUnaryFloatOp<std::asin>(env, args);
}

Expr* ACos::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return EvalUnaryFloatOp<std::acos>(env, args);
}

Expr* ATan::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_LE(2);
  if (num_args == 1) {
    return EvalUnaryFloatOp<std::atan>(env, args);
  }

  return EvalBinaryFloatOp<std::atan2>(env, args);
}

Expr* Sqrt::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return EvalUnaryFloatOp<std::sqrt>(env, args);
}

Expr* Expt::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalBinaryFloatOp<std::pow>(env, args);
}

Expr* MakeRectangular::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* MakePolar::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* RealPart::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* ImagPart::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Magnitude::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Angle::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* ExactToInexact::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto* num = TryNumber(args[0]);
  if (auto* as_float = num->AsFloat()) {
    return as_float;
  }

  auto* as_int = num->AsInt();
  assert(as_int);
  return new Float(as_int->val());
}

Expr* InexactToExact::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto* num = TryNumber(args[0]);
  if (auto* as_int = num->AsInt()) {
    return as_int;
  }

  auto* as_float = num->AsFloat();
  assert(as_float);
  return new Int(as_float->val());
}

Expr* NumberToString::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_LE(2);
  EvalArgs(env, args, num_args);
  auto* num = TryNumber(args[0]);

  int radix = num_args == 2 ? TryInt(args[1])->val() : 10;
  if (auto* as_float = num->AsFloat()) {
    if (radix != 10) {
      throw RuntimeException("inexact numbers can only be printed in base 10",
                             this);
    }

    std::ostringstream oss;
    oss << as_float->val();

    return new expr::String(oss.str());
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
      throw RuntimeException("radix must be one of 2 8 10 16", this);
  }

  return new expr::String(ret);
}

Expr* StringToNumber::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_LE(2);
  EvalArgs(env, args, num_args);
  int radix = num_args == 2 ? TryInt(args[1])->val() : 10;
  const auto& str_val = TryString(args[0])->val();
  try {
    return parse::Lexer::LexNum(str_val, radix);
  } catch (const util::SyntaxException& e) {
    return False();
  }
}

Expr* Not::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0] == False() ? True() : False();
}

Expr* IsBoolean::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0]->type() == Expr::Type::BOOL ? True() : False();
}

Expr* IsPair::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0]->type() == Expr::Type::PAIR ? True() : False();
}

Expr* Cons::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  return new Pair(args[0], args[1]);
}

Expr* Car::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return TryPair(args[0])->car();
}

Expr* Cdr::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return TryPair(args[0])->cdr();
}

Expr* SetCar::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  TryPair(args[0])->set_car(args[1]);
  return Nil();
}

Expr* SetCdr::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  TryPair(args[0])->set_cdr(args[1]);
  return Nil();
}

Expr* IsNull::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0] == Nil() ? True() : False();
}

Expr* IsList::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  Expr* cur = args[0];

  std::set<Expr*> seen;
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    // If we found a loop, it's not a list.
    if (seen.find(list) != seen.end()) {
      return False();
    }
    seen.insert(list);
  }

  return cur == Nil() ? True() : False();
}

Expr* List::DoEval(Env* env, Expr** args, size_t num_args) const {
  EvalArgs(env, args, num_args);
  Expr* front = Nil();
  for (ssize_t i = num_args - 1; i >= 0; --i) {
    front = new Pair(args[i], front);
  }

  return front;
}

Expr* Length::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  Int::ValType ret = 0;
  Expr* cur = args[0];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    ++ret;
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  return new Int(ret);
}

Expr* Append::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(1);
  EvalArgs(env, args, num_args);
  Expr* ret = Nil();
  Pair* back = nullptr;

  for (size_t i = 0; i < num_args - 1; ++i) {
    Pair* cur_back = nullptr;
    Expr* copy = CopyList(args[i], &cur_back);
    if (copy == Nil()) {
      continue;
    }
    if (ret == Nil()) {
      ret = copy;
    } else {
      assert(cur_back);
      assert(cur_back->cdr() == Nil());
      cur_back->set_cdr(copy);
    }
    back = cur_back;
  }

  Expr* last = args[num_args - 1];
  if (ret == Nil()) {
    return last;
  }

  assert(back);
  assert(back->cdr() == Nil());
  back->set_cdr(last);
  return ret;
}

Expr* Reverse::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  Expr* ret = Nil();
  Expr* cur = args[0];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    ret = new Pair(list->car(), ret);
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  return ret;
}

Expr* ListTail::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  Int::ValType k = TryInt(args[1])->val();

  Expr* cur = args[0];
  for (; auto *list = cur->AsPair(); cur = list->cdr(), --k) {
    if (k == 0) {
      return cur;
    }
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  if (k != 0) {
    throw RuntimeException("index too large for list", this);
  }

  return Nil();
}

Expr* ListRef::DoEval(Env* env, Expr** args, size_t num_args) const {
  Expr* tail = primitive::ListTail()->DoEval(env, args, num_args);
  return TryPair(tail)->car();
}

Expr* Memq::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    if (args[0]->Eq(list->car())) {
      return list;
    }
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  return False();
}

Expr* Memv::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    if (args[0]->Eqv(list->car())) {
      return list;
    }
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  return False();
}

Expr* Member::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    if (args[0]->Equal(list->car())) {
      return list;
    }
  }

  if (cur != Nil()) {
    throw RuntimeException("Expected list", args[0]);
  }

  return False();
}

Expr* Assq::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    Pair* head = TryPair(list->car());
    if (head->car()->Eq(args[0])) {
      return head;
    }
  }

  return False();
}

Expr* Assv::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    Pair* head = TryPair(list->car());
    if (head->car()->Eqv(args[0])) {
      return head;
    }
  }

  return False();
}

Expr* Assoc::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  Expr* cur = args[1];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    Pair* head = TryPair(list->car());
    if (head->car()->Equal(args[0])) {
      return head;
    }
  }

  return False();
}

Expr* IsSymbol::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0]->type() == Expr::Type::SYMBOL ? True() : False();
}

Expr* SymbolToString::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return new expr::String(TrySymbol(args[0])->val(), true /* read_only */);
}

Expr* StringToSymbol::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return Symbol::New(TryString(args[0])->val());
}

Expr* IsChar::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0]->type() == Expr::Type::CHAR ? True() : False();
}

Expr* IsCharEq::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalCharOp<std::equal_to>(env, args, num_args);
}

Expr* IsCharLt::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalCharOp<std::less>(env, args, num_args);
}

Expr* IsCharGt::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalCharOp<std::greater>(env, args, num_args);
}

Expr* IsCharLe::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalCharOp<std::less_equal>(env, args, num_args);
}

Expr* IsCharGe::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalCharOp<std::greater_equal>(env, args, num_args);
}

Expr* IsCharCiEq::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalCharCiOp<std::equal_to>(env, args, num_args);
}

Expr* IsCharCiLt::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalCharCiOp<std::less>(env, args, num_args);
}

Expr* IsCharCiGt::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalCharCiOp<std::greater>(env, args, num_args);
}

Expr* IsCharCiLe::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalCharCiOp<std::less_equal>(env, args, num_args);
}

Expr* IsCharCiGe::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalCharCiOp<std::greater_equal>(env, args, num_args);
}

Expr* IsCharAlphabetic::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return CheckUnaryCharOp<std::isalpha>(env, args, num_args);
}

Expr* IsCharNumeric::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return CheckUnaryCharOp<std::isdigit>(env, args, num_args);
}

Expr* IsCharWhitespace::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return CheckUnaryCharOp<std::isspace>(env, args, num_args);
}

Expr* IsCharUpperCase::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return CheckUnaryCharOp<std::isupper>(env, args, num_args);
}

Expr* IsCharLowerCase::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return CheckUnaryCharOp<std::tolower>(env, args, num_args);
}

Expr* CharToInteger::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return new Int(TryChar(args[0])->val());
}

Expr* IntegerToChar::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto* as_int = TryInt(args[0]);
  if (as_int->val() > std::numeric_limits<Char::ValType>::max()) {
    throw RuntimeException("Value out of range", as_int);
  }
  return new Char(as_int->val());
}

Expr* CharUpCase::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto char_val = TryChar(args[0])->val();
  return std::isupper(char_val) ? args[0] : new Char(std::toupper(char_val));
}

Expr* CharDownCase::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto char_val = TryChar(args[0])->val();
  return std::islower(char_val) ? args[0] : new Char(std::tolower(char_val));
}

Expr* IsString::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0]->type() == Expr::Type::STRING ? True() : False();
}

Expr* MakeString::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_LE(2);
  EvalArgs(env, args, num_args);

  auto len = TryGetNonNegExactIntVal(args[0]);
  char init_value = ' ';
  if (num_args == 2) {
    init_value = TryChar(args[1])->val();
  }

  return new expr::String(std::string(len, init_value));
}

Expr* String::DoEval(Env* env, Expr** args, size_t num_args) const {
  EvalArgs(env, args, num_args);
  std::string val;
  for (size_t i = 0; i < num_args; ++i) {
    val.push_back(TryChar(args[i])->val());
  }

  return new expr::String(std::move(val));
}

Expr* StringLength::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return new Int(TryString(args[0])->val().size());
}

Expr* StringRef::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  auto& string_val = TryString(args[0])->val();
  auto idx = TryGetNonNegExactIntVal(args[1], string_val.size());
  return new Char(string_val[idx]);
}

Expr* StringSet::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(3);
  EvalArgs(env, args, num_args);
  auto* str = TryString(args[0]);
  if (str->read_only()) {
    throw RuntimeException("Attempt to write read only string", str);
  }

  auto idx = TryGetNonNegExactIntVal(args[1], str->val().size());
  str->set_val_idx(idx, TryChar(args[2])->val());
  return Nil();
}

Expr* IsStringEq::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalStringOp<std::equal_to<std::string>>(env, args, num_args);
}

Expr* IsStringEqCi::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalStringOp<ICaseCmpStr<std::equal_to>>(env, args, num_args);
}

Expr* IsStringLt::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalStringOp<std::less<std::string>>(env, args, num_args);
}

Expr* IsStringGt::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalStringOp<std::greater<std::string>>(env, args, num_args);
}

Expr* IsStringLe::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalStringOp<std::less_equal<std::string>>(env, args, num_args);
}

Expr* IsStringGe::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalStringOp<std::greater_equal<std::string>>(env, args, num_args);
}

Expr* IsStringLtCi::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalStringOp<ICaseCmpStr<std::less>>(env, args, num_args);
}

Expr* IsStringGtCi::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalStringOp<ICaseCmpStr<std::greater>>(env, args, num_args);
}

Expr* IsStringLeCi::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalStringOp<ICaseCmpStr<std::less_equal>>(env, args, num_args);
}

Expr* IsStringGeCi::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  return EvalStringOp<ICaseCmpStr<std::greater_equal>>(env, args, num_args);
}

Expr* Substring::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(3);
  EvalArgs(env, args, num_args);
  auto& str_val = TryString(args[0])->val();
  auto start = TryGetNonNegExactIntVal(args[1], str_val.size());
  auto end = TryGetNonNegExactIntVal(args[2], str_val.size());

  return new expr::String(str_val.substr(start, end - start));
}

Expr* StringAppend::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  return new expr::String(TryString(args[0])->val() +
                          TryString(args[1])->val());
}

Expr* StringToList::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  auto& str_val = TryString(args[0])->val();

  Expr* ret = Nil();
  for (ssize_t i = str_val.size() - 1; i >= 0; --i) {
    ret = new Pair(new Char(str_val[i]), ret);
  }

  return ret;
}

Expr* ListToString::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  std::string str;
  Expr* cur = args[0];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    str.push_back(TryChar(list->car())->val());
  }

  return new expr::String(str);
}

Expr* StringCopy::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return new expr::String(TryString(args[0])->val());
}

Expr* StringFill::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  auto* str = TryString(args[0]);
  if (str->read_only()) {
    throw RuntimeException("Attempt to write read only string", str);
  }
  auto char_val = TryChar(args[1])->val();
  for (size_t i = 0; i < str->val().size(); ++i) {
    str->set_val_idx(i, char_val);
  }
  return Nil();
}

Expr* IsVector::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0]->type() == Expr::Type::VECTOR ? True() : False();
}

Expr* MakeVector::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_LE(2);
  EvalArgs(env, args, num_args);

  auto count = TryGetNonNegExactIntVal(args[0]);
  Expr* init_val = num_args == 2 ? args[1] : Nil();
  return new expr::Vector(std::vector<Expr*>(count, init_val));
}

Expr* Vector::DoEval(Env* env, Expr** args, size_t num_args) const {
  EvalArgs(env, args, num_args);
  return new expr::Vector(std::vector<Expr*>(args, args + num_args));
}

Expr* VectorLength::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return new Int(TryVector(args[0])->vals().size());
}

Expr* VectorRef::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  auto& vec = TryVector(args[0])->vals();
  auto idx = TryGetNonNegExactIntVal(args[1], vec.size());
  return vec[idx];
}

Expr* VectorSet::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(3);
  EvalArgs(env, args, num_args);
  auto& vec = TryVector(args[0])->vals();
  auto idx = TryGetNonNegExactIntVal(args[1], vec.size());
  vec[idx] = args[2];
  return Nil();
}

Expr* VectorToList::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  Expr* ret = Nil();
  auto& vec_val = TryVector(args[0])->vals();
  for (auto it = vec_val.rbegin(); it != vec_val.rend(); ++it) {
    ret = new Pair(*it, ret);
  }

  return ret;
}

Expr* ListToVector::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);

  std::vector<Expr*> exprs;

  Expr* cur = args[0];
  for (; auto* list = cur->AsPair(); cur = list->cdr()) {
    exprs.push_back(list->car());
  }

  return new expr::Vector(std::move(exprs));
}

Expr* VectorFill::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EvalArgs(env, args, num_args);
  auto& vec_val = TryVector(args[0])->vals();

  for (auto& val : vec_val) {
    val = args[1];
  }

  return Nil();
}

Expr* IsProcedure::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return args[0]->type() == Expr::Type::EVALS ? True() : False();
}

Expr* Apply::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(1);
  EvalArgs(env, args, num_args);
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

Expr* Map::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  return MapImpl<true>(env, args, num_args);
}

Expr* ForEach::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  return MapImpl<false>(env, args, num_args);
}

Expr* Force::DoEval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  EvalArgs(env, args, num_args);
  return TryEvals(args[0])->DoEval(env, nullptr, 0);
}

Expr* CallWithCurrentContinuation::DoEval(Env* env,
                                          Expr** args,
                                          size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Values::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* CallWithValues::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* DynamicWind::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* EvalPrim::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* SchemeReportEnvironment::DoEval(Env* env,
                                      Expr** args,
                                      size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* NullEnvironment::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* InteractionEnvironment::DoEval(Env* env,
                                     Expr** args,
                                     size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* CallWithInputFile::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* CallWithOutputFile::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsInputPort::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsOutputPort::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* CurrentInputPort::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* CurrentOutputPort::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* WithInputFromFile::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* WithOutputToFile::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* OpenInputFile::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* OpenOutputFile::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* CloseInputPort::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* CloseOutputPort::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Read::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* ReadChar::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* PeekChar::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsEofObject::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharReady::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Write::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Display::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Newline::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* WriteChar::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Load::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* TranscriptOn::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* TranscriptOff::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

}  // namespace impl

// Declare Factories
#define X(Name, str)        \
  Evals* Name() {           \
    static impl::Name impl; \
    return &impl;           \
  }
#include "expr/primitives.inc"  // NOLINT(build/include)
#undef X

void LoadPrimitives(Env* env) {
  for (const auto& primitive : kPrimitives) {
    env->DefineVar(Symbol::New(primitive.name), primitive.expr());
  }

  std::string tmp;
  LoadCr(env, kCrDepth, &tmp);
}

}  // namespace primitive
}  // namespace expr
