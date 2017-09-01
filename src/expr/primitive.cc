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

#include "expr/expr.h"
#include "expr/primitive.h"
#include "util/exceptions.h"
#include "util/util.h"

#define THROW_EXCEPTION(msg)                         \
  do {                                               \
    auto error = std::string(__func__) + ": " + msg; \
    throw util::RuntimeException(error);             \
  } while (0)

#define EXPECT_NUM_ARGS(expected)                        \
  do {                                                   \
    if (size != expected) {                              \
      std::ostringstream os;                             \
      os << "Expected " #expected " args. Got " << size; \
      THROW_EXCEPTION(os.str());                         \
    }                                                    \
  } while (0)

#define EXPECT_TYPE(expected, expr)                              \
  do {                                                           \
    if (Expr::Type::expected != expr->type()) {                  \
      std::ostringstream os;                                     \
      os << "Expected type " << Expr::Type::expected << ". Got " \
         << expr->type();                                        \
    }                                                            \
  } while (0)

#define PRIM_IMPL(Name, eval_body)                                   \
  class Name : public Primitive {                                    \
    Expr* Eval(Env* env, Expr** exprs, size_t size) const override { \
      (void)env;                                                     \
      (void)exprs;                                                   \
      (void)size;                                                    \
      eval_body                                                      \
    }                                                                \
    std::ostream& AppendStream(std::ostream& stream) const override; \
  };

namespace expr {
namespace primitive {

namespace {

struct {
  Primitive* (*expr)();
  const char* name;
} kPrimitives[] = {
#define X(name, str) {name, str},
#include "expr/primitives.inc"
#undef X
};

}  // namespace

namespace impl {
// clang-format off

PRIM_IMPL(Quote,
  EXPECT_NUM_ARGS(1);
  return *exprs;
);

PRIM_IMPL(Lambda,
  return nullptr;
);

PRIM_IMPL(If,
  return nullptr;
);

PRIM_IMPL(Set,
  EXPECT_NUM_ARGS(2);
  EXPECT_TYPE(SYMBOL, exprs[0]);
  env->SetVar(exprs[0]->GetAsSymbol(), exprs[1]);
  return expr::Nil();
);

PRIM_IMPL(Define,
  EXPECT_NUM_ARGS(2);
  EXPECT_TYPE(SYMBOL, exprs[0]);
  env->DefineVar(exprs[0]->GetAsSymbol(), exprs[1]);
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
#include "expr/primitives.inc"
#undef X

// Declare Factories
#define X(name, str)        \
  Primitive* name() {       \
    static impl::name impl; \
    return &impl;           \
  }
#include "expr/primitives.inc"
#undef X

void LoadPrimitives(Env* env) {
  for (const auto& primitive : kPrimitives) {
    env->DefineVar(Symbol::Create(primitive.name), primitive.expr());
  }
}

}  // namespace primitive
}  // namespace expr
