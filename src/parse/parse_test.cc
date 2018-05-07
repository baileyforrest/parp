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

#include "expr/number.h"
#include "parse/parse.h"
#include "test/util.h"

using expr::Char;
using expr::Expr;
using expr::Int;
using expr::String;
using expr::Symbol;
using expr::Vector;

namespace parse {

namespace {

void VerifyExprs(const ExprVec& expect, const ExprVec& got) {
  ASSERT_EQ(expect.size(), got.size());

  for (auto eit = expect.begin(), git = got.begin(); eit != expect.end();
       ++eit, ++git) {
    EXPECT_EQ(**eit, **git);
  }
}

gc::Lock<expr::Expr> Cons(Expr* a, Expr* b) {
  return gc::Lock<expr::Expr>(new expr::Pair(a, b));
}

gc::Lock<expr::Expr> Cons(Expr* a, gc::Lock<Expr> b) {
  return Cons(a, b.get());
}

}  // namespace

class ParserTest : public test::TestBase {};

TEST_F(ParserTest, ReadSimpleDatum) {
  const std::string kStr =
      "hello\n"
      "#t\n"
      "1\n"
      "#\\c\n"
      "\"world\"\n";

  // clang-format off
  const ExprVec kExpected = {
      gc::Lock<expr::Expr>(Symbol::New("hello")),
      gc::Lock<expr::Expr>(expr::True()),
      gc::Lock<expr::Expr>(new Int(1)),
      gc::Lock<expr::Expr>(new Char('c')),
      gc::Lock<expr::Expr>(new String("world")),
  };
  // clang-format on

  VerifyExprs(kExpected, Read(kStr));
}

TEST_F(ParserTest, ReadVector) {
  const std::string kStr = "#(a b c d e)";
  // clang-format off
  const std::vector<gc::Lock<Symbol>> kSyms = {
      Symbol::NewLock("a"),
      Symbol::NewLock("b"),
      Symbol::NewLock("c"),
      Symbol::NewLock("d"),
      Symbol::NewLock("e"),
  };
  // clang-format on

  std::vector<Expr*> expr_list;
  for (const auto& sym : kSyms) {
    expr_list.push_back(sym.get());
  }
  ExprVec expected = {gc::Lock<Expr>(new Vector(expr_list))};
  VerifyExprs(expected, Read(kStr));
}

TEST_F(ParserTest, ReadListAbbreviation) {
  const std::string kStr =
      "'a\n"
      "`a\n"
      ",a\n"
      ",@a\n";

  auto tail = Cons(Symbol::NewLock("a").get(), expr::Nil());

  const ExprVec kExpected = {
      Cons(Symbol::NewLock("quote").get(), tail.get()),
      Cons(Symbol::NewLock("quasiquote").get(), tail.get()),
      Cons(Symbol::NewLock("unquote").get(), tail.get()),
      Cons(Symbol::NewLock("unquote-splicing").get(), tail.get()),
  };

  VerifyExprs(kExpected, Read(kStr));
}

TEST_F(ParserTest, ReadList) {
  const std::string kStr = "(a b c d e)";
  const ExprVec kExpected = {
      Cons(Symbol::NewLock("a").get(),
           Cons(Symbol::NewLock("b").get(),
                Cons(Symbol::NewLock("c").get(),
                     Cons(Symbol::NewLock("d").get(),
                          Cons(Symbol::NewLock("e").get(), expr::Nil()))))),
  };

  VerifyExprs(kExpected, Read(kStr));
}

TEST_F(ParserTest, ReadListDot) {
  const std::string kStr =
      "(a b c d . e)\n"
      "(f . g)";

  const ExprVec kExpected = {
      Cons(Symbol::NewLock("a").get(),
           Cons(Symbol::NewLock("b").get(),
                Cons(Symbol::NewLock("c").get(),
                     Cons(Symbol::NewLock("d").get(),
                          Symbol::NewLock("e").get())))),

      Cons(Symbol::NewLock("f").get(), Symbol::NewLock("g").get()),
  };

  VerifyExprs(kExpected, Read(kStr));
}

}  // namespace parse
