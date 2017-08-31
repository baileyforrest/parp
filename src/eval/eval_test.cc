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
#include "expr/expr.h"
#include "expr/number.h"
#include "test/util.h"

namespace eval {

class EvalTest : public test::TestBase {
 protected:
  virtual void TestSetUp() { env_ = expr::Env::Create({}, nullptr); }
  virtual void TestTearDown() { env_ = nullptr; }

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
  auto* symbol = expr::Symbol::Create("abc");
  env_->DefineVar(symbol, num);

  EXPECT_EQ(num, Eval(symbol, env_));
}

}  // namespace eval
