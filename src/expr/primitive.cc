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

#include "expr/expr.h"
#include "expr/primitive.h"
#include "eval/eval.h"
#include "util/exceptions.h"
#include "util/util.h"

#define THROW_EXCEPTION(msg)                         \
  do {                                               \
    auto error = std::string(__func__) + ": " + msg; \
    throw util::RuntimeException(error);             \
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
    if (num_args <= expected) {                                      \
      std::ostringstream os;                                         \
      os << "Expected at most " #expected " args. Got " << num_args; \
      THROW_EXCEPTION(os.str());                                     \
    }                                                                \
  } while (0)

#define EXPECT_ARGS_GE(expected)                                      \
  do {                                                                \
    if (num_args >= expected) {                                       \
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

#define PRIM_IMPL(Name, eval_body)                                      \
  class Name : public Primitive {                                       \
    Expr* Eval(Env* env, Expr** args, size_t num_args) const override { \
      (void)env;                                                        \
      (void)args;                                                       \
      (void)num_args;                                                   \
      eval_body                                                         \
    }                                                                   \
    std::ostream& AppendStream(std::ostream& stream) const override;    \
  };

namespace expr {
namespace primitive {

namespace {

struct {
  Primitive* (*expr)();
  const char* name;
} kPrimitives[] = {
#define X(name, str) {name, str},
#include "expr/primitives.inc"  // NOLINT(build/include)
#undef X
};

}  // namespace

namespace impl {
// clang-format off

PRIM_IMPL(Quote,
  EXPECT_ARGS_NUM(1);
  return *args;
);

PRIM_IMPL(Lambda,
  EXPECT_ARGS_GE(2);
  return nullptr;
  // TODO(bcf): This
);

PRIM_IMPL(If,
  EXPECT_ARGS_GE(2);
  EXPECT_ARGS_LE(3);
  auto* cond = eval::Eval(args[0], env);
  if (cond == expr::False()) {
    if (num_args == 3) {
      return eval::Eval(args[2], env);
    } else {
      return Nil();
    }
  }
  return eval::Eval(args[1], env);
);

PRIM_IMPL(Set,
  EXPECT_ARGS_NUM(2);
  EXPECT_TYPE(SYMBOL, args[0]);
  env->SetVar(args[0]->GetAsSymbol(), args[1]);
  return expr::Nil();
);

PRIM_IMPL(Define,
  EXPECT_ARGS_NUM(2);
  EXPECT_TYPE(SYMBOL, args[0]);
  env->DefineVar(args[0]->GetAsSymbol(), args[1]);
  // TODO(bcf): handle define lambda.

  return expr::Nil();
);

// clang-format on
}  // namespace impl

// Declare AppendStream
#define X(name, str)                                                   \
  std::ostream& impl::name::AppendStream(std::ostream& stream) const { \
    return stream << str;                                              \
  }
#include "expr/primitives.inc"  // NOLINT(build/include)
#undef X

// Declare Factories
#define X(name, str)        \
  Primitive* name() {       \
    static impl::name impl; \
    return &impl;           \
  }
#include "expr/primitives.inc"  // NOLINT(build/include)
#undef X

void LoadPrimitives(Env* env) {
  for (const auto& primitive : kPrimitives) {
    env->DefineVar(Symbol::Create(primitive.name), primitive.expr());
  }
}

}  // namespace primitive
}  // namespace expr
