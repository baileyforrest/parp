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

#include <sstream>
#include <string>
#include <vector>

#include "expr/expr.h"
#include "expr/primitive.h"
#include "eval/eval.h"
#include "util/exceptions.h"
#include "util/util.h"

#define THROW_EXCEPTION(msg)                         \
  do {                                               \
    auto error = std::string(__func__) + ": " + msg; \
    throw util::RuntimeException(error, nullptr);    \
  } while (0)

#define EXPECT_ARGS_NUM(expected)                            \
  do {                                                       \
    if (num_args != expected) {                              \
      std::ostringstream os;                                 \
      os << "Expected " #expected " args. Got " << num_args; \
      THROW_EXCEPTION(os.str());                             \
    }                                                        \
  } while (0)

#define EXPECT_ARGS_LE(expected)                                     \
  do {                                                               \
    if (num_args > expected) {                                       \
      std::ostringstream os;                                         \
      os << "Expected at most " #expected " args. Got " << num_args; \
      THROW_EXCEPTION(os.str());                                     \
    }                                                                \
  } while (0)

#define EXPECT_ARGS_GE(expected)                                      \
  do {                                                                \
    if (num_args < expected) {                                        \
      std::ostringstream os;                                          \
      os << "Expected at least " #expected " args. Got " << num_args; \
      THROW_EXCEPTION(os.str());                                      \
    }                                                                 \
  } while (0)

#define EXPECT_TYPE(expected, expr)                              \
  do {                                                           \
    if (Expr::Type::expected != expr->type()) {                  \
      std::ostringstream os;                                     \
      os << "Expected type " << Expr::Type::expected << ". Got " \
         << expr->type();                                        \
    }                                                            \
  } while (0)

namespace expr {
namespace primitive {

namespace {

struct {
  Evals* (*expr)();
  const char* name;
} kPrimitives[] = {
#define X(name, str) {name, #str},
#include "expr/primitives.inc"  // NOLINT(build/include)
#undef X
};

class LambdaImpl : public Evals {
 public:
  explicit LambdaImpl(const std::vector<Symbol*>& required_args,
                      Symbol* variable_arg,
                      const std::vector<Expr*>& body,
                      Env* env)
      : required_args_(required_args),
        variable_arg_(variable_arg),
        body_(body),
        env_(env) {
    assert(body_.size() > 0);
    assert(env);
  }

 private:
  // Evals implementation:
  std::ostream& AppendStream(std::ostream& stream) const override;
  Expr* Eval(Env* env, Expr** exprs, size_t size) const override;

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
    stream << *expr << "\n";
  }
  return stream << ")";
}

Expr* LambdaImpl::Eval(Env* /* env */, Expr** exprs, size_t size) const {
  if (size < required_args_.size()) {
    std::ostringstream os;
    os << "Invalid number of arguments. expected ";
    if (variable_arg_ != nullptr) {
      os << "at least ";
    }

    os << required_args_.size();
    os << " given: " << size;
    throw new util::RuntimeException(os.str(), this);
  }
  auto arg_it = exprs;
  auto* new_env = new expr::Env(env_);

  for (auto* sym : required_args_) {
    new_env->DefineVar(sym, *arg_it++);
  }
  if (variable_arg_ != nullptr) {
    auto* rest = ListFromIt(arg_it, exprs + size);
    new_env->DefineVar(variable_arg_, rest);
  }

  assert(body_.size() >= 1);
  for (size_t i = 0; i < body_.size() - 1; ++i) {
    eval::Eval(body_[i], new_env);
  }
  return eval::Eval(body_.back(), new_env);
}

}  // namespace

namespace impl {

// Declare Class Base
#define X(Name, str)                                                   \
  class Name : public Evals {                                          \
    Expr* Eval(Env* env, Expr** args, size_t num_args) const override; \
    std::ostream& AppendStream(std::ostream& stream) const {           \
      return stream << #str;                                           \
    }                                                                  \
  };
#include "expr/primitives.inc"  // NOLINT(build/include)
#undef X

Expr* Quote::Eval(Env* /* env */, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(1);
  return *args;
}

Expr* Lambda::Eval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  std::vector<Symbol*> req_args;
  Symbol* var_arg = nullptr;
  switch (args[0]->type()) {
    case Type::SYMBOL:
      req_args.push_back(args[0]->AsSymbol());
      break;
    case Type::PAIR: {
      Expr* cur_arg = args[0];
      while (auto* pair = cur_arg->AsPair()) {
        EXPECT_TYPE(SYMBOL, pair->car());
        auto* arg = pair->car()->AsSymbol();
        req_args.push_back(arg);
        cur_arg = pair->cdr();
      }
      if (cur_arg != expr::Nil()) {
        EXPECT_TYPE(SYMBOL, cur_arg);
        var_arg = cur_arg->AsSymbol();
      }
      break;
    };
    default:
      THROW_EXCEPTION("Expected arguments");
  }

  // TODO(bcf): Analyze in other places as appropriate.
  for (size_t i = 1; i < num_args; ++i) {
    args[i] = eval::Analyze(args[i]);
  }

  return new LambdaImpl(req_args, var_arg, {args + 1, args + num_args}, env);
}

Expr* If::Eval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_GE(2);
  EXPECT_ARGS_LE(3);
  auto* cond = eval::Eval(args[0], env);
  if (cond == False()) {
    if (num_args == 3) {
      return eval::Eval(args[2], env);
    } else {
      return Nil();
    }
  }
  return eval::Eval(args[1], env);
}

Expr* Set::Eval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EXPECT_TYPE(SYMBOL, args[0]);
  env->SetVar(args[0]->AsSymbol(), args[1]);
  return Nil();
}

Expr* Begin::Eval(Env* env, Expr** args, size_t num_args) const {
  if (num_args == 0) {
    return Nil();
  }
  for (; num_args > 1; --num_args, ++args) {
    eval::Eval(*args, env);
  }
  return eval::Eval(*args, env);
}

Expr* Define::Eval(Env* env, Expr** args, size_t num_args) const {
  EXPECT_ARGS_NUM(2);
  EXPECT_TYPE(SYMBOL, args[0]);
  env->DefineVar(args[0]->AsSymbol(), args[1]);
  // TODO(bcf): handle define lambda.

  return Nil();
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
    env->DefineVar(new Symbol(primitive.name), primitive.expr());
  }
}

}  // namespace primitive
}  // namespace expr
