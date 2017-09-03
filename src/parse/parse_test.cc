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
      Symbol::New("hello"),
      expr::True(),
      Int::New(1),
      Char::New('c'),
      String::New("world"),
  };
  // clang-format on

  VerifyExprs(kExpected, Read(kStr));
}

TEST_F(ParserTest, ReadVector) {
  const std::string kStr = "#(a b c d e)";
  // clang-format off
  const ExprVec kExpected = {
    Vector::New({
          Symbol::New("a"),
          Symbol::New("b"),
          Symbol::New("c"),
          Symbol::New("d"),
          Symbol::New("e"),
      }),
  };
  // clang-format on

  VerifyExprs(kExpected, Read(kStr));
}

TEST_F(ParserTest, ReadListAbbreviation) {
  const std::string kStr =
      "'a\n"
      "`a\n"
      ",a\n"
      ",@a\n";

  auto tail = expr::Cons(Symbol::New("a"), expr::Nil());

  const ExprVec kExpected = {
      expr::Cons(Symbol::New("quote"), tail),
      expr::Cons(Symbol::New("quasiquote"), tail),
      expr::Cons(Symbol::New("unquote"), tail),
      expr::Cons(Symbol::New("unquote-splicing"), tail),
  };

  VerifyExprs(kExpected, Read(kStr));
}

TEST_F(ParserTest, ReadList) {
  const std::string kStr = "(a b c d e)";
  const ExprVec kExpected = {
      expr::Cons(Symbol::New("a"),
                 expr::Cons(Symbol::New("b"),
                            expr::Cons(Symbol::New("c"),
                                       expr::Cons(Symbol::New("d"),
                                                  expr::Cons(Symbol::New("e"),
                                                             expr::Nil()))))),
  };

  VerifyExprs(kExpected, Read(kStr));
}

TEST_F(ParserTest, ReadListDot) {
  const std::string kStr =
      "(a b c d . e)\n"
      "(f . g)";

  const ExprVec kExpected = {
      expr::Cons(Symbol::New("a"),
                 expr::Cons(Symbol::New("b"),
                            expr::Cons(Symbol::New("c"),
                                       expr::Cons(Symbol::New("d"),
                                                  Symbol::New("e"))))),

      expr::Cons(Symbol::New("f"), Symbol::New("g")),
  };

  VerifyExprs(kExpected, Read(kStr));
}

}  // namespace parse
