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

#include <string>

#include "eval/eval.h"
#include "expr/expr.h"
#include "expr/number.h"
#include "expr/primitive.h"
#include "parse/parse.h"
#include "test/util.h"

namespace eval {

namespace {

expr::Expr* ParseExpr(const std::string& str) {
  auto exprs = parse::Read(str);
  assert(exprs.size() == 1);
  return exprs[0];
}

}  // namespace

class EvalTest : public test::TestBase {
 protected:
  virtual void TestSetUp() {
    env_ = expr::Env::Create({}, nullptr);
    expr::primitive::LoadPrimitives(env_);
  }
  virtual void TestTearDown() { env_ = nullptr; }

  expr::Expr* EvalStr(const std::string& str) {
    return Eval(ParseExpr(str), env_);
  }

  expr::Env* env_ = nullptr;
};

TEST_F(EvalTest, SelfEvaluating) {
  EXPECT_EQ(expr::Nil(), Eval(expr::Nil(), env_));
  EXPECT_EQ(expr::True(), Eval(expr::True(), env_));
  EXPECT_EQ(expr::False(), Eval(expr::False(), env_));

  auto* num = expr::NumReal::Create(42);
  EXPECT_EQ(num, Eval(num, env_));

  auto* character = expr::Char::Create('a');
  EXPECT_EQ(character, Eval(character, env_));

  auto* str = expr::String::Create("abc");
  EXPECT_EQ(str, Eval(str, env_));

  auto* vec = expr::Vector::Create({num, character, str});
  EXPECT_EQ(vec, Eval(vec, env_));
}

TEST_F(EvalTest, Symbol) {
  auto* num = expr::NumReal::Create(42);
  auto* symbol = ParseExpr("abc");
  env_->DefineVar(symbol->GetAsSymbol(), num);

  EXPECT_EQ(num, Eval(symbol, env_));
}

TEST_F(EvalTest, Quote) {
  auto* expected = static_cast<expr::Expr*>(expr::NumReal::Create(42));
  auto* expr = EvalStr("(quote 42)");

  EXPECT_EQ(*expected, *expr);
}

TEST_F(EvalTest, LambdaBasic) {
  auto* n42 = static_cast<expr::Expr*>(expr::NumReal::Create(42));
  EXPECT_EQ(*n42, *EvalStr("((lambda (x) x) 42)"));
}

TEST_F(EvalTest, If) {
  auto* n42 = static_cast<expr::Expr*>(expr::NumReal::Create(42));

  EXPECT_EQ(*n42, *EvalStr("(if #t 42)"));
  EXPECT_EQ(*expr::Nil(), *EvalStr("(if #f 42)"));

  auto* n43 = static_cast<expr::Expr*>(expr::NumReal::Create(43));
  EXPECT_EQ(*n42, *EvalStr("(if #t 42 43)"));
  EXPECT_EQ(*n43, *EvalStr("(if #f 42 43)"));
}

TEST_F(EvalTest, Set) {
  auto* expected = static_cast<expr::Expr*>(expr::NumReal::Create(42));
  auto* sym = expr::Symbol::Create("foo");
  env_->DefineVar(sym, expr::Nil());
  EvalStr("(set! foo 42)");

  EXPECT_EQ(*expected, *env_->Lookup(sym));
}

TEST_F(EvalTest, Begin) {
  // TODO(bcf)
}

TEST_F(EvalTest, Define) {
  // TODO(bcf): Test define function
  auto* expected = static_cast<expr::Expr*>(expr::NumReal::Create(42));
  auto* sym = expr::Symbol::Create("foo");
  EvalStr("(define foo 42)");

  EXPECT_EQ(*expected, *env_->Lookup(sym));
}

}  // namespace eval
