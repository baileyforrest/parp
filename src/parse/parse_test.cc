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
      new expr::Symbol("hello"),
      expr::True(),
      new expr::Int(1),
      new expr::Char('c'),
      new expr::String("world"),
  };
  // clang-format on

  VerifyExprs(kExpected, Read(kStr));
}

TEST_F(ParserTest, ReadVector) {
  const std::string kStr = "#(a b c d e)";
  // clang-format off
  const ExprVec kExpected = {
      new expr::Vector({
          new expr::Symbol("a"),
          new expr::Symbol("b"),
          new expr::Symbol("c"),
          new expr::Symbol("d"),
          new expr::Symbol("e"),
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

  auto tail = expr::Cons(new expr::Symbol("a"), expr::Nil());

  const ExprVec kExpected = {
      expr::Cons(new expr::Symbol("quote"), tail),
      expr::Cons(new expr::Symbol("quasiquote"), tail),
      expr::Cons(new expr::Symbol("unquote"), tail),
      expr::Cons(new expr::Symbol("unquote-splicing"), tail),
  };

  VerifyExprs(kExpected, Read(kStr));
}

TEST_F(ParserTest, ReadList) {
  const std::string kStr = "(a b c d e)";
  const ExprVec kExpected = {
      expr::Cons(
          new expr::Symbol("a"),
          expr::Cons(new expr::Symbol("b"),
                     expr::Cons(new expr::Symbol("c"),
                                expr::Cons(new expr::Symbol("d"),
                                           expr::Cons(new expr::Symbol("e"),
                                                      expr::Nil()))))),
  };

  VerifyExprs(kExpected, Read(kStr));
}

TEST_F(ParserTest, ReadListDot) {
  const std::string kStr =
      "(a b c d . e)\n"
      "(f . g)";

  const ExprVec kExpected = {
      expr::Cons(new expr::Symbol("a"),
                 expr::Cons(new expr::Symbol("b"),
                            expr::Cons(new expr::Symbol("c"),
                                       expr::Cons(new expr::Symbol("d"),
                                                  new expr::Symbol("e"))))),

      expr::Cons(new expr::Symbol("f"), new expr::Symbol("g")),
  };

  VerifyExprs(kExpected, Read(kStr));
}

}  // namespace parse
