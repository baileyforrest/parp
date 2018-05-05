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

#include <cmath>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

#include "expr/expr.h"
#include "expr/number.h"
#include "expr/primitive.h"
#include "eval/eval.h"
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

struct {
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

Expr* LambdaImpl::DoEval(Env* /* env */, Expr** args, size_t num_args) const {
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
  EvalArgs(env_, args, num_args);

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
  Expr* DoEval(Env* /* env */, Expr** args, size_t num_args) const override {
    EXPECT_ARGS_NUM(1);
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
  if (!cur->empty()) {
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

Int::ValType TryIntOrRound(Expr* expr, bool* is_exact) {
  auto* num = TryNumber(expr);
  if (auto* as_int = num->AsInt()) {
    return as_int->val();
  }

  auto* as_float = num->AsFloat();
  assert(as_float);
  if (std::trunc(as_float->val()) != as_float->val()) {
    throw new RuntimeException("Expected integer", as_float);
  }

  *is_exact = false;
  return as_float->val();
}

Float::ValType TryGetFloat(Expr* expr) {
  auto* num = TryNumber(expr);
  if (auto* as_float = num->AsFloat()) {
    return as_float->val();
  }

  auto* as_int = num->AsInt();
  assert(as_int);
  return as_int->val();
}

template <Float::ValType (*Op)(Float::ValType)>
Expr* EvalUnaryFloatOp(Env* env, Expr** args) {
  EvalArgs(env, args, 1);
  return new Float(Op(TryGetFloat(args[0])));
}

template <Float::ValType (*Op)(Float::ValType, Float::ValType)>
Expr* EvalBinaryFloatOp(Env* env, Expr** args) {
  EvalArgs(env, args, 2);
  Float::ValType n1 = TryGetFloat(args[0]);
  Float::ValType n2 = TryGetFloat(args[1]);
  return new Float(Op(n1, n2));
}

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
  return *args;
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
  env->SetVar(TrySymbol(args[0]), args[1]);
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
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
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
  Expr* cur = TryPair(args[0]);
  auto* new_env = new Env(env);
  std::vector<std::pair<Symbol*, Expr*>> bindings;
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
    bindings.emplace_back(var, val);
    new_env->DefineVar(var, Nil());
    cur = pair->cdr();
  }
  if (cur != Nil()) {
    throw RuntimeException("let: Malformed binding list", cur);
  }

  for (const auto& binding : bindings) {
    new_env->SetVar(binding.first, Eval(binding.second, env));
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
  Int::ValType arg1 = TryIntOrRound(args[0], &is_exact);
  Int::ValType arg2 = TryIntOrRound(args[1], &is_exact);
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
  Int::ValType arg1 = TryIntOrRound(args[0], &is_exact);
  Int::ValType arg2 = TryIntOrRound(args[1], &is_exact);

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
  Int::ValType arg1 = TryIntOrRound(args[0], &is_exact);
  Int::ValType arg2 = TryIntOrRound(args[1], &is_exact);

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
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* StringToNumber::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Not::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsBoolean::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsPair::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Cons::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Car::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Cdr::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* SetCar::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* SetCdr::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsNull::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsList::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* List::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Length::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Append::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Reverse::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* ListTail::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* ListRef::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Memq::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Memv::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Member::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Assq::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Assv::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Assoc::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* SymbolPrim::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* SymbolToString::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* StringToSymbol::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsChar::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharEq::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharLt::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharGt::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharLe::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharGe::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharCiEq::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharCiLt::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharCiGt::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharCiLe::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharCiGe::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharAlphabetic::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharNumeric::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharWhitespace::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharUpperCase::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsCharLowerCase::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* CharToInteger::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IntegerToChar::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* CharUpCase::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* CharDownCase::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsString::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* MakeString::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* String::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* StringLength::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* StringRef::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* StringSet::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsStringEq::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsStringEqCi::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsStringLt::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsStringGt::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsStringLe::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsStringGe::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsStringLtCi::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsStringGtCi::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsStringLeCi::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsStringGeCi::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Substring::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* StringAppend::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* StringToList::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* ListToString::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* StringCopy::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* StringFill::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsVector::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* MakeVector::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Vector::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* VectorLength::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* VectorRef::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* VectorSet::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* VectorToList::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* ListToVector::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* VectorFill::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* IsProcedure::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Map::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* ForEach::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
}

Expr* Force::DoEval(Env* env, Expr** args, size_t num_args) const {
  throw util::RuntimeException("Not implemented", this);
  assert(false && env && args && num_args);
  return nullptr;
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

  // TODO(bcf): Have special case for car and cdr since they will be more
  // common.
  std::string tmp;
  LoadCr(env, kCrDepth, &tmp);
}

}  // namespace primitive
}  // namespace expr
