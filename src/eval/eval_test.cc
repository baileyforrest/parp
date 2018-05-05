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

using expr::Char;
using expr::Expr;
using expr::Env;
using expr::False;
using expr::Float;
using expr::Int;
using expr::Nil;
using expr::Symbol;
using expr::String;
using expr::True;
using expr::Vector;

namespace eval {

namespace {

expr::Expr* ParseExpr(const std::string& str) {
  auto exprs = parse::Read(str);
  assert(exprs.size() == 1);
  return exprs[0];
}

Expr* IntExpr(Int::ValType val) {
  return new Int(val);
}

Expr* FloatExpr(Float::ValType d) {
  return new Float(d);
}

Expr* SymExpr(std::string str) {
  return Symbol::New(std::move(str));
}

}  // namespace

class EvalTest : public test::TestBase {
 protected:
  virtual void TestSetUp() {
    env_ = new Env();
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

  Expr* character = new Char('a');
  EXPECT_EQ(character, Eval(character, env_));

  Expr* str = new String("abc");
  EXPECT_EQ(str, Eval(str, env_));

  Expr* vec = new Vector({num, character, str});
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
  Expr* e = Symbol::New("a");
  EXPECT_EQ(*e, *EvalStr("(quote a)"));
  EXPECT_EQ(*e, *EvalStr("'a"));
  e = new Vector({Symbol::New("a"), Symbol::New("b"), Symbol::New("c")});
  EXPECT_EQ(*e, *EvalStr("(quote #(a b c))"));
  EXPECT_EQ(*e, *EvalStr("'#(a b c)"));
  e = Cons(Symbol::New("+"), Cons(new Int(1), Cons(new Int(2), Nil())));
  EXPECT_EQ(*Nil(), *EvalStr("'()"));
  EXPECT_EQ(*e, *EvalStr("(quote (+ 1 2))"));
  EXPECT_EQ(*e, *EvalStr("'(+ 1 2)"));
  e = Cons(Symbol::New("quote"), Cons(Symbol::New("a"), Nil()));
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
  auto* sym = Symbol::New("foo");
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

TEST_F(EvalTest, And) {
  EXPECT_EQ(*True(), *EvalStr("(and (= 2 2) (> 2 1))"));
  EXPECT_EQ(*False(), *EvalStr("(and (= 2 2) (< 2 1))"));
  Expr* e = Cons(Symbol::New("f"), Cons(Symbol::New("g"), Nil()));
  EXPECT_EQ(*e, *EvalStr("(and 1 2 'c '(f g))"));
  EXPECT_EQ(*True(), *EvalStr("(and)"));
}

TEST_F(EvalTest, Or) {
  EXPECT_EQ(*True(), *EvalStr("(or (= 2 2) (> 2 1))"));
  EXPECT_EQ(*True(), *EvalStr("(or (= 2 2) (< 2 1))"));
  EXPECT_EQ(*False(), *EvalStr("(or #f #f #f)"));
#if 0  // TODO(bcf): Enable when memq implemented.
  Expr* e = Cons(Symbol::New("b"), Cons(Symbol::New("c"), Nil()));
  EXPECT_EQ(*e, *EvalStr("(or (memq 'b '(a b c)) (/ 3 0))"));
#endif
  EXPECT_EQ(*False(), *EvalStr("(or)"));
}

TEST_F(EvalTest, Let) {
  EXPECT_EQ(*IntExpr(6), *EvalStr("(let ((x 2) (y 3)) (* x y))"));
  // clang-format off
  EXPECT_EQ(*IntExpr(35), *EvalStr(
      "(let ((x 2) (y 3))"
      "  (let ((x 7)"
      "        (z (+ x y)))"
      "    (* z x)))"));
  // clang-format on
}

TEST_F(EvalTest, LetStar) {
  // clang-format off
  EXPECT_EQ(*IntExpr(70), *EvalStr(
      "(let ((x 2) (y 3))"
      "  (let* ((x 7)"
      "         (z (+ x y)))"
      "    (* z x)))"));
  // clang-format on
}

TEST_F(EvalTest, LetRec) {
  // clang-format off
  EXPECT_EQ(*True(), *EvalStr(
      "(letrec ((even?"
      "          (lambda (n)"
      "             (if (zero? n)"
      "                 #t"
      "                 (odd? (- n 1)))))"
      "         (odd?"
      "          (lambda (n)"
      "            (if (zero? n)"
      "                #f"
      "                (even? (- n 1))))))"
      "  (even? 88))"));
  // clang-format on
}

TEST_F(EvalTest, Begin) {
  EvalStr("(define x 0)");
  EXPECT_EQ(*IntExpr(6), *EvalStr("(begin (set! x 5) (+ x 1))"));
}

TEST_F(EvalTest, Define) {
  // TODO(bcf): Test define function
  EvalStr("(define foo 42)");
  EXPECT_EQ(*IntExpr(42), *env_->Lookup(Symbol::New("foo")));
}

TEST_F(EvalTest, IsEqv) {
  EXPECT_EQ(*True(), *EvalStr("(eqv? #t #t)"));
  EXPECT_EQ(*True(), *EvalStr("(eqv? #f #f)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? #f #t)"));

  EXPECT_EQ(*True(), *EvalStr("(eqv? 'x 'x)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? 'x 'y)"));

  EXPECT_EQ(*True(), *EvalStr("(eqv? 3 3)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? 3 4)"));
  EXPECT_EQ(*True(), *EvalStr("(eqv? 3.1 3.1)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? 3.1 3.2)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? 3.0 3)"));

  EXPECT_EQ(*True(), *EvalStr("(eqv? #\\c #\\c)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? #\\c #\\d)"));
  EXPECT_EQ(*True(), *EvalStr("(eqv? '() '())"));

#if 0  // TODO(bcf): Enable after cons implemented.
  EXPECT_EQ(*False(), *EvalStr("(eqv? (cons 1 2) (cons 1 2))"));
#endif
  EXPECT_EQ(*False(), *EvalStr("(eqv? (lambda () 1) (lambda () 2))"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? #f 'nil)"));
  EXPECT_EQ(*True(), *EvalStr("(let ((p (lambda (x) x))) (eqv? p p))"));
  // clang-format off
  EvalStr(
    "(define gen-counter"
    "  (lambda ()"
    "    (let ((n 0))"
    "      (lambda () (set! n (+ n 1)) n))))");
  // clang-format on

  EXPECT_EQ(*True(), *EvalStr("(let ((g (gen-counter))) (eqv? g g))"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? (gen-counter) (gen-counter))"));

  // clang-format off
  EvalStr(
    "(define gen-loser"
    "  (lambda ()"
    "    (let ((n 0))"
    "      (lambda () (set! n (+ n 1)) 27))))");
  // clang-format on
  EXPECT_EQ(*True(), *EvalStr("(let ((g (gen-loser))) (eqv? g g))"));
  EXPECT_EQ(*True(), *EvalStr("(let ((x '(a))) (eqv? x x))"));

  // TODO(bcf): obj1 and obj2 are pairs, vectors, or strings that denote
  // the same locations in the store (section 3.4).

  // TODO(bcf): obj1 and obj2 are procedures whose location tags are
  // equal (section 4.1.4).
}

TEST_F(EvalTest, IsEq) {
#if 0  // TODO(bcf): Symbols should be singleton.
  EXPECT_EQ(*True(), *EvalStr("(eq? 'a 'a)"));
#endif
  EXPECT_EQ(*False(), *EvalStr("(eq? '(a) '(a))"));
  EXPECT_EQ(*True(), *EvalStr("(eq? '() '())"));
  EXPECT_EQ(*True(), *EvalStr("(eq? car car)"));
  EXPECT_EQ(*True(), *EvalStr("(let ((x '(a))) (eq? x x))"));
  EXPECT_EQ(*True(), *EvalStr("(let ((x '#())) (eq? x x)) "));
  EXPECT_EQ(*True(), *EvalStr("(let ((p (lambda (x) x))) (eq? p p))"));
}

TEST_F(EvalTest, IsEqual) {
  EXPECT_EQ(*True(), *EvalStr("(equal? 'a 'a)"));
  EXPECT_EQ(*True(), *EvalStr("(equal? '(a) '(a))"));
  EXPECT_EQ(*True(), *EvalStr("(equal? '(a (b) c) '(a (b) c))"));
  EXPECT_EQ(*True(), *EvalStr("(equal? \"abc\" \"abc\")"));
  EXPECT_EQ(*True(), *EvalStr("(equal? 2 2)"));
  EXPECT_EQ(*True(), *EvalStr("(equal? '#(5 'a) '#(5 'a))"));
}

TEST_F(EvalTest, NumberPredicates) {
#if 0
  EXPECT_EQ(*True(), *EvalStr("(complex? 3+4i)"));
#endif
  EXPECT_EQ(*True(), *EvalStr("(complex? 3)"));
  EXPECT_EQ(*True(), *EvalStr("(real? 3)"));
#if 0
  EXPECT_EQ(*True(), *EvalStr("(real? -2.5+0.0i)"));
#endif
#if 0  // TODO(bcf): Support parsing this.
  EXPECT_EQ(*True(), *EvalStr("(real? #e1e10)"));
#endif
#if 0
  EXPECT_EQ(*True(), *EvalStr("(rational? 6/10)"));
  EXPECT_EQ(*True(), *EvalStr("(rational? 6/3)"));
  EXPECT_EQ(*True(), *EvalStr("(integer? 3+0i)"));
#endif
  EXPECT_EQ(*True(), *EvalStr("(integer? 3.0)"));
#if 0
  EXPECT_EQ(*True(), *EvalStr("(integer? 8/4)"));
#endif
  EXPECT_EQ(*True(), *EvalStr("(exact? 3)"));
  EXPECT_EQ(*False(), *EvalStr("(exact? 3.0)"));
}

TEST_F(EvalTest, OpEq) {
  EXPECT_EQ(*False(), *EvalStr("(= 3 4)"));
  EXPECT_EQ(*True(), *EvalStr("(= 3 3 3)"));
  EXPECT_EQ(*True(), *EvalStr("(= 3 3 (+ 2 1))"));
  EXPECT_EQ(*False(), *EvalStr("(= 3 3 3 1)"));

  EXPECT_EQ(*False(), *EvalStr("(= 3.0 4)"));
  EXPECT_EQ(*True(), *EvalStr("(= 3.0 3.0 3.0)"));
  EXPECT_EQ(*False(), *EvalStr("(= 3.0 3 3.0 1)"));
}

TEST_F(EvalTest, OpLt) {
  EXPECT_EQ(*False(), *EvalStr("(< 4 3)"));
  EXPECT_EQ(*False(), *EvalStr("(< 4 4)"));
  EXPECT_EQ(*True(), *EvalStr("(< 3 4)"));
  EXPECT_EQ(*True(), *EvalStr("(< 1 2 3 4)"));
  EXPECT_EQ(*False(), *EvalStr("(< 1 2 3 3)"));
  EXPECT_EQ(*True(), *EvalStr("(< 1 2 3 (+ 4 5))"));

  EXPECT_EQ(*False(), *EvalStr("(< 4.0 3)"));
  EXPECT_EQ(*False(), *EvalStr("(< 4.0 4.0)"));
  EXPECT_EQ(*True(), *EvalStr("(< 3.0 4)"));
  EXPECT_EQ(*True(), *EvalStr("(< 1.0 2.0 3.0 4.0)"));
  EXPECT_EQ(*False(), *EvalStr("(< 1 2.0 3 3.0)"));
  EXPECT_EQ(*True(), *EvalStr("(< 1.0 2 3.0 (+ 4 5.0))"));
}

TEST_F(EvalTest, OpGt) {
  EXPECT_EQ(*False(), *EvalStr("(> 3 4)"));
  EXPECT_EQ(*False(), *EvalStr("(> 3 3)"));
  EXPECT_EQ(*False(), *EvalStr("(> 4 4 3)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4 3)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4 3 1)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4 3 (+ 1 1) 1)"));

  EXPECT_EQ(*False(), *EvalStr("(> 3.0 4.0)"));
  EXPECT_EQ(*False(), *EvalStr("(> 3 3.0)"));
  EXPECT_EQ(*False(), *EvalStr("(> 4.0 4.0 3)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4 3.0)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4.0 3 1.0)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4.0 3 (+ 1 1.0) 1.0)"));
}

TEST_F(EvalTest, OpLe) {
  EXPECT_EQ(*False(), *EvalStr("(<= 5 4)"));
  EXPECT_EQ(*False(), *EvalStr("(<= 5 6 7 8 1)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5 6 7 8)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5 8)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5 8 (+ 5 13))"));

  EXPECT_EQ(*False(), *EvalStr("(<= 5.0 4.0)"));
  EXPECT_EQ(*False(), *EvalStr("(<= 5.0 6 7.0 8 1.0)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5.0 6 7.0 8)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5.0 8.0)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5.0 8.0 (+ 5.0 13.0))"));
}

TEST_F(EvalTest, OpGe) {
  EXPECT_EQ(*False(), *EvalStr("(>= 4 5)"));
  EXPECT_EQ(*False(), *EvalStr("(>= (- 8 17) 5 5 3 3 2 2)"));
  EXPECT_EQ(*True(), *EvalStr("(>= 3 3 3 3 )"));
  EXPECT_EQ(*True(), *EvalStr("(>= 8 3 1 0 -5)"));

  EXPECT_EQ(*False(), *EvalStr("(>= 4.0 5)"));
  EXPECT_EQ(*False(), *EvalStr("(>= (- 8 17.0) 5.0 5 3.0 3.0 2.0 2)"));
  EXPECT_EQ(*True(), *EvalStr("(>= 3.0 3 3.0 3 )"));
  EXPECT_EQ(*True(), *EvalStr("(>= 8.0 3 1.0 0 -5.0)"));
}

TEST_F(EvalTest, IsZero) {
  EXPECT_EQ(*False(), *EvalStr("(zero? 1)"));
  EXPECT_EQ(*False(), *EvalStr("(zero? 0.1)"));
  EXPECT_EQ(*True(), *EvalStr("(zero? 0)"));
  EXPECT_EQ(*True(), *EvalStr("(zero? (- 1.1 1.1))"));
}

TEST_F(EvalTest, IsPositive) {
  EXPECT_EQ(*False(), *EvalStr("(positive? 0)"));
  EXPECT_EQ(*False(), *EvalStr("(positive? -1)"));
  EXPECT_EQ(*False(), *EvalStr("(positive? (- 1.1 1.1))"));
  EXPECT_EQ(*False(), *EvalStr("(positive? -0.1)"));

  EXPECT_EQ(*True(), *EvalStr("(positive? 1)"));
  EXPECT_EQ(*True(), *EvalStr("(positive? (- 1.1 1.0))"));
  EXPECT_EQ(*True(), *EvalStr("(positive? 0.1)"));
}

TEST_F(EvalTest, IsNegative) {
  EXPECT_EQ(*False(), *EvalStr("(negative? 0)"));
  EXPECT_EQ(*False(), *EvalStr("(negative? 1)"));
  EXPECT_EQ(*False(), *EvalStr("(negative? (- 1.1 1.1))"));
  EXPECT_EQ(*False(), *EvalStr("(negative? 0.1)"));

  EXPECT_EQ(*True(), *EvalStr("(negative? -1)"));
  EXPECT_EQ(*True(), *EvalStr("(negative? (- 1.0 1.1))"));
  EXPECT_EQ(*True(), *EvalStr("(negative? -0.1)"));
}

TEST_F(EvalTest, IsOdd) {
  EXPECT_EQ(*False(), *EvalStr("(odd? 2)"));
  EXPECT_EQ(*True(), *EvalStr("(odd? 3)"));
}

TEST_F(EvalTest, IsEven) {
  EXPECT_EQ(*True(), *EvalStr("(even? 2)"));
  EXPECT_EQ(*False(), *EvalStr("(even? 3)"));
}

TEST_F(EvalTest, Min) {
  EXPECT_EQ(*IntExpr(42), *EvalStr("(min 42 43 44)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(min 100 42)"));
  EXPECT_EQ(*FloatExpr(2.0), *EvalStr("(min 3 2.0 10)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(min 100 42 42.1)"));
}

TEST_F(EvalTest, Max) {
  EXPECT_EQ(*IntExpr(4), *EvalStr("(max 3 4)"));
  EXPECT_EQ(*FloatExpr(4), *EvalStr("(max 3.9 4)"));
}

TEST_F(EvalTest, Plus) {
  EXPECT_EQ(*IntExpr(0), *EvalStr("(+)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(+ 22 20)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(+ 22 12 3 5)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(+ 60 -18)"));
  EXPECT_EQ(*FloatExpr(2.0), *EvalStr("(+ 1 1.0)"));
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

TEST_F(EvalTest, Abs) {
  EXPECT_EQ(*IntExpr(7), *EvalStr("(abs -7)"));
  EXPECT_EQ(*FloatExpr(42.0), *EvalStr("(abs -42.0)"));
}

TEST_F(EvalTest, Quotient) {
  EXPECT_EQ(*IntExpr(3), *EvalStr("(quotient 13 4)"));
  EXPECT_EQ(*IntExpr(-3), *EvalStr("(quotient -13 4)"));
}

TEST_F(EvalTest, Remainder) {
  EXPECT_EQ(*IntExpr(1), *EvalStr("(remainder 13 4)"));
  EXPECT_EQ(*IntExpr(-1), *EvalStr("(remainder -13 4)"));
  EXPECT_EQ(*IntExpr(1), *EvalStr("(remainder 13 -4)"));
  EXPECT_EQ(*IntExpr(-1), *EvalStr("(remainder -13 -4)"));
  EXPECT_EQ(*FloatExpr(-1), *EvalStr("(remainder -13 -4.0)"));
}

TEST_F(EvalTest, Modulo) {
  EXPECT_EQ(*IntExpr(1), *EvalStr("(modulo 13 4)"));
  EXPECT_EQ(*IntExpr(3), *EvalStr("(modulo -13 4)"));
  EXPECT_EQ(*IntExpr(-3), *EvalStr("(modulo 13 -4)"));
  EXPECT_EQ(*IntExpr(-1), *EvalStr("(modulo -13 -4)"));
}

TEST_F(EvalTest, Floor) {
  EXPECT_EQ(*FloatExpr(-5.0), *EvalStr("(floor -4.3)"));
  EXPECT_EQ(*FloatExpr(3.0), *EvalStr("(floor 3.5)"));
}

TEST_F(EvalTest, Ceiling) {
  EXPECT_EQ(*FloatExpr(-4.0), *EvalStr("(ceiling -4.3)"));
  EXPECT_EQ(*FloatExpr(4.0), *EvalStr("(ceiling 3.5)"));
}

TEST_F(EvalTest, Truncate) {
  EXPECT_EQ(*FloatExpr(-4.0), *EvalStr("(truncate -4.3)"));
  EXPECT_EQ(*FloatExpr(3.0), *EvalStr("(truncate 3.5)"));
}

TEST_F(EvalTest, Round) {
  EXPECT_EQ(*FloatExpr(-4.0), *EvalStr("(round -4.3)"));
  EXPECT_EQ(*FloatExpr(4.0), *EvalStr("(round 3.5)"));
#if 0
  EXPECT_EQ(*IntExpr(4), *EvalStr("(round 7/4)"));
#endif
  EXPECT_EQ(*IntExpr(7), *EvalStr("(round 7)"));
}

}  // namespace eval
