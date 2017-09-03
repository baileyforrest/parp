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
#include <utility>

#include "eval/eval.h"
#include "expr/expr.h"
#include "expr/number.h"
#include "expr/primitive.h"
#include "parse/parse.h"
#include "test/util.h"

using expr::Expr;
using expr::Int;
using expr::Symbol;
using expr::Nil;

namespace eval {

namespace {

expr::Expr* ParseExpr(const std::string& str) {
  auto exprs = parse::Read(str);
  assert(exprs.size() == 1);
  return exprs[0];
}

Expr* IntExpr(int64_t val) {
  return new Int(val);
}

Expr* SymExpr(std::string str) {
  return new Symbol(std::move(str));
}

}  // namespace

class EvalTest : public test::TestBase {
 protected:
  virtual void TestSetUp() {
    env_ = new expr::Env;
    expr::primitive::LoadPrimitives(env_);
  }
  virtual void TestTearDown() { env_ = nullptr; }

  expr::Expr* EvalStr(const std::string& str) {
    return Eval(ParseExpr(str), env_);
  }

  expr::Env* env_ = nullptr;
};

TEST_F(EvalTest, SelfEvaluating) {
  EXPECT_EQ(Nil(), Eval(Nil(), env_));
  EXPECT_EQ(expr::True(), Eval(expr::True(), env_));
  EXPECT_EQ(expr::False(), Eval(expr::False(), env_));

  Expr* num = new Int(42);
  EXPECT_EQ(num, Eval(num, env_));

  Expr* character = new expr::Char('a');
  EXPECT_EQ(character, Eval(character, env_));

  Expr* str = new expr::String("abc");
  EXPECT_EQ(str, Eval(str, env_));

  Expr* vec = new expr::Vector({num, character, str});
  EXPECT_EQ(vec, Eval(vec, env_));
}

TEST_F(EvalTest, Symbol) {
  Expr* num = new Int(42);
  auto* symbol = ParseExpr("abc");
  env_->DefineVar(symbol->AsSymbol(), num);

  EXPECT_EQ(num, Eval(symbol, env_));
}

TEST_F(EvalTest, Quote) {
  EXPECT_EQ(*IntExpr(42), *EvalStr("(quote 42)"));
  Expr* e = new Symbol("a");
  EXPECT_EQ(*e, *EvalStr("(quote a)"));
  EXPECT_EQ(*e, *EvalStr("'a"));
  e = new expr::Vector({new Symbol("a"), new Symbol("b"), new Symbol("c")});
  EXPECT_EQ(*e, *EvalStr("(quote #(a b c))"));
  EXPECT_EQ(*e, *EvalStr("'#(a b c)"));
  e = Cons(new Symbol("+"), Cons(new Int(1), Cons(new Int(2), Nil())));
  EXPECT_EQ(*Nil(), *EvalStr("'()"));
  EXPECT_EQ(*e, *EvalStr("(quote (+ 1 2))"));
  EXPECT_EQ(*e, *EvalStr("'(+ 1 2)"));
  e = Cons(new Symbol("quote"), Cons(new Symbol("a"), Nil()));
  EXPECT_EQ(*e, *EvalStr("'(quote a)"));
  EXPECT_EQ(*e, *EvalStr("''a"));
}

TEST_F(EvalTest, Lambda) {
  EXPECT_EQ(*IntExpr(42), *EvalStr("((lambda (x) x) 42)"));
  EXPECT_EQ(*IntExpr(8), *EvalStr("((lambda (x) (+ x x)) 4)"));
  // Nested lambda
  EXPECT_EQ(*IntExpr(8), *EvalStr("((lambda (x) ((lambda (y) (+ x y)) 3)) 5)"));

  // Lamda returning lambda
  EXPECT_EQ(*IntExpr(12), *EvalStr("(((lambda () (lambda (x) (+ 5 x)))) 7)"));
}

TEST_F(EvalTest, If) {
  Expr* n42 = new Int(42);
  EXPECT_EQ(*n42, *EvalStr("(if #t 42)"));
  EXPECT_EQ(*Nil(), *EvalStr("(if #f 42)"));

  Expr* n43 = new Int(43);
  EXPECT_EQ(*n42, *EvalStr("(if #t 42 43)"));
  EXPECT_EQ(*n43, *EvalStr("(if #f 42 43)"));
  EXPECT_EQ(*IntExpr(12), *EvalStr("((if #f + *) 3 4)"));

  Expr* e = Cons(new Int(3),
                 Cons(new Int(4), Cons(new Int(5), Cons(new Int(6), Nil()))));
  EXPECT_EQ(*e, *EvalStr("((lambda x x) 3 4 5 6)"));

  e = Cons(new Int(5), Cons(new Int(6), Nil()));
  EXPECT_EQ(*e, *EvalStr("((lambda (x y . z) z) 3 4 5 6)"));
}

TEST_F(EvalTest, Set) {
  auto* sym = new Symbol("foo");
  env_->DefineVar(sym, Nil());
  EvalStr("(set! foo 42)");

  EXPECT_EQ(*IntExpr(42), *env_->Lookup(sym));
}

TEST_F(EvalTest, Cond) {
  EXPECT_EQ(*Nil(), *EvalStr("(cond)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(cond (#t 42))"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(cond (#f 3) (#t 42))"));
  EXPECT_EQ(*Nil(), *EvalStr("(cond (#f 3) (#f 42))"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(cond (#f 3) (else 42))"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(cond (else 42))"));
  EXPECT_EQ(
      *IntExpr(10),
      *EvalStr("(cond (#f 3) ((+ 4 3) => (lambda (x) (+ x 3))) (else 4))"));
}

TEST_F(EvalTest, Case) {
  EXPECT_EQ(*Nil(), *EvalStr("(case 3)"));
  // clang-format off
  EXPECT_EQ(*SymExpr("composite"), *EvalStr(
      "(case (* 2 3)"
      "  ((2 3 5 7) 'prime)"
      "  ((1 4 6 8 9) 'composite))"));
  EXPECT_EQ(*Nil(), *EvalStr(
      "(case (car '(c d))"
      "  ((a) 'a)"
      "  ((b) 'b))"));
  EXPECT_EQ(*SymExpr("consonant"), *EvalStr(
      "(case (car '(c d))"
      "  ((a e i o u) 'vowel)"
      "  ((w y) 'semivowel)"
      "  (else 'consonant))"));
  // clang-format on
}

TEST_F(EvalTest, Begin) {
  // TODO(bcf)
}

TEST_F(EvalTest, Define) {
  // TODO(bcf): Test define function
  EvalStr("(define foo 42)");
  EXPECT_EQ(*IntExpr(42), *env_->Lookup(new Symbol("foo")));
}

TEST_F(EvalTest, Plus) {
  EXPECT_EQ(*IntExpr(0), *EvalStr("(+)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(+ 22 20)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(+ 22 12 3 5)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(+ 60 -18)"));
}

TEST_F(EvalTest, Star) {
  EXPECT_EQ(*IntExpr(1), *EvalStr("(*)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(* 21 2)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(* 2 3 7)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(* 21 -2 -1)"));
}

TEST_F(EvalTest, Minus) {
  EXPECT_EQ(*IntExpr(42), *EvalStr("(- 84 42)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(- 84 20 22)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(- 22 -20)"));
}

TEST_F(EvalTest, Slash) {
  EXPECT_EQ(*IntExpr(42), *EvalStr("(/ 84 2)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(/ 252 2 3)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(/ 504 -6 -2)"));
}

}  // namespace eval
