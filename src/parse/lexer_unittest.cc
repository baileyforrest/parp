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

// TODO(bcf): Add tests for invalid tokens

#include <sstream>
#include <vector>

#include "gtest/gtest.h"

#include "parse/lexer.h"
#include "util/char_class.h"
#include "util/text_stream.h"

namespace parse {

namespace {

void VerifyTokens(Lexer *lexer, const std::vector<Token> &expected) {
  for (const auto &expect : expected) {
    const auto &got = lexer->NextToken();
    EXPECT_EQ(expect.type, got.type);
    EXPECT_EQ(expect.mark, got.mark);

    if (expect.type != got.type) {
      continue;
    }
    switch (expect.type) {
      case Token::Type::ID:
      case Token::Type::BOOL:
      case Token::Type::NUMBER:
      case Token::Type::CHAR:
      case Token::Type::STRING:
        EXPECT_TRUE(expect.expr->Equal(got.expr));
        break;
      default:
        break;
    }
  }

  EXPECT_EQ(Token::Type::TOK_EOF, lexer->NextToken().type);
}

}  // namespace

TEST(LexerTest, Basic) {
  const char *kStr =
    "  ;;; The FACT procedure computes the factorial\n"
    "  ;;; of a non-negative integer.\n"
    "  (define fact\n"
    "   (lambda (n)\n"
    "    (if (= n 0)\n"
    "     1 ;Base case: return 1\n"
    "     (* n (fact (- n 1))))))\n";

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
    { Token::Type::LPAREN, { &kFilename, 3, 3 }, nullptr, },
    { Token::Type::ID, { &kFilename, 3, 4 }, expr::Symbol::Create("define") },
    { Token::Type::ID, { &kFilename, 3, 11 }, expr::Symbol::Create("fact") },
    { Token::Type::LPAREN, { &kFilename, 4, 4 }, nullptr },
    { Token::Type::ID, { &kFilename, 4, 5 }, expr::Symbol::Create("lambda") },
    { Token::Type::LPAREN, { &kFilename, 4, 12 }, nullptr },
    { Token::Type::ID, { &kFilename, 4, 13 }, expr::Symbol::Create("n") },
    { Token::Type::RPAREN, { &kFilename, 4, 14 }, nullptr },
    { Token::Type::LPAREN, { &kFilename, 5, 5 }, nullptr },
    { Token::Type::ID, { &kFilename, 5, 6 }, expr::Symbol::Create("if") },
    { Token::Type::LPAREN, { &kFilename, 5, 9 }, nullptr },
    { Token::Type::ID, { &kFilename, 5, 10 }, expr::Symbol::Create("=") },
    { Token::Type::ID, { &kFilename, 5, 12 }, expr::Symbol::Create("n") },
    { Token::Type::NUMBER, { &kFilename, 5, 14 }, expr::NumReal::Create(0) },
    { Token::Type::RPAREN, { &kFilename, 5, 15 }, nullptr },
    { Token::Type::NUMBER, { &kFilename, 6, 6 }, expr::NumReal::Create(1) },
    { Token::Type::LPAREN, { &kFilename, 7, 6 }, nullptr },
    { Token::Type::ID, { &kFilename, 7, 7 }, expr::Symbol::Create("*") },
    { Token::Type::ID, { &kFilename, 7, 9 }, expr::Symbol::Create("n") },
    { Token::Type::LPAREN, { &kFilename, 7, 11 }, nullptr },
    { Token::Type::ID, { &kFilename, 7, 12 }, expr::Symbol::Create("fact") },
    { Token::Type::LPAREN, { &kFilename, 7, 17 }, nullptr },
    { Token::Type::ID, { &kFilename, 7, 18 }, expr::Symbol::Create("-") },
    { Token::Type::ID, { &kFilename, 7, 20 }, expr::Symbol::Create("n") },
    { Token::Type::NUMBER, { &kFilename, 7, 22 }, expr::NumReal::Create(1) },
    { Token::Type::RPAREN, { &kFilename, 7, 23 }, nullptr },
    { Token::Type::RPAREN, { &kFilename, 7, 24 }, nullptr },
    { Token::Type::RPAREN, { &kFilename, 7, 25 }, nullptr },
    { Token::Type::RPAREN, { &kFilename, 7, 26 }, nullptr },
    { Token::Type::RPAREN, { &kFilename, 7, 27 }, nullptr },
    { Token::Type::RPAREN, { &kFilename, 7, 28 }, nullptr },
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST(LexerTest, Empty) {
  const char *kStr = "";

  const std::string kFilename = "foo";
  const std::vector<Token> kExpected;

  std::istringstream s(kStr);
  util::TextStream stream(&s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST(LexerTest, NoTrailingNewine) {
  const char *kStr = "abc";

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
    { Token::Type::ID, { &kFilename, 1, 1 }, expr::Symbol::Create("abc") },
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST(LexerTest, IdTest) {
  const char *kStr =
    "abc\n"

    // special initial
    "!\n"
    "$\n"
    "%\n"
    "&\n"
    "*\n"
    "/\n"
    ":\n"
    "<\n"
    "=\n"
    ">\n"
    "?\n"
    "^\n"
    "_\n"
    "~\n"

    // special initial with letters
    "~a\n"

    // <peculiar identifier>
    "+\n"
    "-\n"
    "...\n"

    // <special subsequent>
    "a+\n"
    "b-\n"
    "c.\n"
    "c@\n";

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
    { Token::Type::ID, { &kFilename, 1, 1 }, expr::Symbol::Create("abc") },
    { Token::Type::ID, { &kFilename, 2, 1 }, expr::Symbol::Create("!") },
    { Token::Type::ID, { &kFilename, 3, 1 }, expr::Symbol::Create("$") },
    { Token::Type::ID, { &kFilename, 4, 1 }, expr::Symbol::Create("%") },
    { Token::Type::ID, { &kFilename, 5, 1 }, expr::Symbol::Create("&") },
    { Token::Type::ID, { &kFilename, 6, 1 }, expr::Symbol::Create("*") },
    { Token::Type::ID, { &kFilename, 7, 1 }, expr::Symbol::Create("/") },
    { Token::Type::ID, { &kFilename, 8, 1 }, expr::Symbol::Create(":") },
    { Token::Type::ID, { &kFilename, 9, 1 }, expr::Symbol::Create("<") },
    { Token::Type::ID, { &kFilename, 10, 1 }, expr::Symbol::Create("=") },
    { Token::Type::ID, { &kFilename, 11, 1 }, expr::Symbol::Create(">") },
    { Token::Type::ID, { &kFilename, 12, 1 }, expr::Symbol::Create("?") },
    { Token::Type::ID, { &kFilename, 13, 1 }, expr::Symbol::Create("^") },
    { Token::Type::ID, { &kFilename, 14, 1 }, expr::Symbol::Create("_") },
    { Token::Type::ID, { &kFilename, 15, 1 }, expr::Symbol::Create("~") },
    { Token::Type::ID, { &kFilename, 16, 1 }, expr::Symbol::Create("~a") },
    { Token::Type::ID, { &kFilename, 17, 1 }, expr::Symbol::Create("+") },
    { Token::Type::ID, { &kFilename, 18, 1 }, expr::Symbol::Create("-") },
    { Token::Type::ID, { &kFilename, 19, 1 }, expr::Symbol::Create("...") },
    { Token::Type::ID, { &kFilename, 20, 1 }, expr::Symbol::Create("a+") },
    { Token::Type::ID, { &kFilename, 21, 1 }, expr::Symbol::Create("b-") },
    { Token::Type::ID, { &kFilename, 22, 1 }, expr::Symbol::Create("c.") },
    { Token::Type::ID, { &kFilename, 23, 1 }, expr::Symbol::Create("c@") },
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST(LexerTest, BoolTest) {
  const char *kStr =
    "#t\n"
    "#f\n"
    "#T\n"
    "#F\n";

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
    { Token::Type::BOOL, { &kFilename, 1, 1 }, expr::Bool::Create(true) },
    { Token::Type::BOOL, { &kFilename, 2, 1 }, expr::Bool::Create(false) },
    { Token::Type::BOOL, { &kFilename, 3, 1 }, expr::Bool::Create(true) },
    { Token::Type::BOOL, { &kFilename, 4, 1 }, expr::Bool::Create(false) },
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST(LexerTest, NumTest) {
  const char *kStr =
    "#b1\n"
    "#o1\n"
    "#d1\n"
    "#x1\n"

    "#i1\n"
    "#e1\n"
    "#i#b1\n"
    "#i#o1\n"
    "#e#d1\n"
    "#e#x1\n"
    "#b#e1\n"
    "#o#e1\n"
    "#d#i1\n"
    "#x#i1\n"

    "3\n"
    "+2\n"
    "-2\n"
    "4##\n"
    "5.7\n"
    "5##.##7\n"
    "7.2###\n"
    ".3###\n"

    "1s0\n"
    "1f1\n"
    "1d2\n"
    "1l3\n";

  /* TODO(bcf): Enable when these are supported.
    "3/4\n"

    "5@4\n"
    "10+7i\n"
    "10-7i\n"

    "+13i\n"
    "-14i\n"

    "+i\n"
    "-i\n"

    "2e-10\n"
    "2e+10\n"
    "#i3###.##4e-27d@4##.#5e14\n";
  */

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
    { Token::Type::NUMBER, { &kFilename, 1, 1 }, expr::NumReal::Create(1) },
    { Token::Type::NUMBER, { &kFilename, 2, 1 }, expr::NumReal::Create(1) },
    { Token::Type::NUMBER, { &kFilename, 3, 1 }, expr::NumReal::Create(1) },
    { Token::Type::NUMBER, { &kFilename, 4, 1 }, expr::NumReal::Create(1) },
    { Token::Type::NUMBER, { &kFilename, 5, 1 }, expr::NumFloat::Create(1) },
    { Token::Type::NUMBER, { &kFilename, 6, 1 }, expr::NumReal::Create(1) },
    { Token::Type::NUMBER, { &kFilename, 7, 1 }, expr::NumFloat::Create(1.0) },
    { Token::Type::NUMBER, { &kFilename, 8, 1 }, expr::NumFloat::Create(1.0) },
    { Token::Type::NUMBER, { &kFilename, 9, 1 }, expr::NumReal::Create(1) },
    { Token::Type::NUMBER, { &kFilename, 10, 1 }, expr::NumReal::Create(1) },
    { Token::Type::NUMBER, { &kFilename, 11, 1 }, expr::NumReal::Create(1) },
    { Token::Type::NUMBER, { &kFilename, 12, 1 }, expr::NumReal::Create(1) },
    { Token::Type::NUMBER, { &kFilename, 13, 1 }, expr::NumFloat::Create(1.0) },
    { Token::Type::NUMBER, { &kFilename, 14, 1 }, expr::NumFloat::Create(1.0) },
    { Token::Type::NUMBER, { &kFilename, 15, 1 }, expr::NumReal::Create(3) },
    { Token::Type::NUMBER, { &kFilename, 16, 1 }, expr::NumReal::Create(2) },
    { Token::Type::NUMBER, { &kFilename, 17, 1 }, expr::NumReal::Create(-2) },
    { Token::Type::NUMBER, { &kFilename, 18, 1 }, expr::NumFloat::Create(400) },
    { Token::Type::NUMBER, { &kFilename, 19, 1 }, expr::NumFloat::Create(5.7) },
    { Token::Type::NUMBER, { &kFilename, 20, 1 },
      expr::NumFloat::Create(500.007) },
    { Token::Type::NUMBER, { &kFilename, 21, 1 }, expr::NumFloat::Create(7.2) },
    { Token::Type::NUMBER, { &kFilename, 22, 1 }, expr::NumFloat::Create(0.3) },
    { Token::Type::NUMBER, { &kFilename, 23, 1 }, expr::NumFloat::Create(1.0) },
    { Token::Type::NUMBER, { &kFilename, 24, 1 },
      expr::NumFloat::Create(10.0) },
    { Token::Type::NUMBER, { &kFilename, 25, 1 },
      expr::NumFloat::Create(100.0) },
    { Token::Type::NUMBER, { &kFilename, 26, 1 },
      expr::NumFloat::Create(1000.0) },
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST(LexerTest, CharTest) {
  std::string input;
  const int kAsciiChars = 127;
  for (int i = 0, line = 0; i < kAsciiChars; ++i) {
    if (util::IsDelim(i)) {
      continue;
    }
    ++line;
    input += "#\\";
    input.push_back(i);
    input += "\n";
  }
  const std::string kFilename = "foo";

  std::vector<Token> expected;
  for (int i = 0, line = 0; i < kAsciiChars; ++i) {
    if (util::IsDelim(i)) {
      continue;
    }
    ++line;
    Token expect = {
        Token::Type::CHAR,
        { &kFilename, line, 1 },
        expr::Char::Create(static_cast<char>(i))
    };
    expected.push_back(expect);
  }

  std::istringstream s(input);
  util::TextStream stream(&s, &kFilename);
  Lexer lexer{stream};


  VerifyTokens(&lexer, expected);
}

TEST(LexerTest, StringTest) {
  const char *kStr =
    "\"abc\"\n"
    "\"\\abc\"\n"
    "\"a\\bc\"\n"
    "\"\\\\abc\"\n"
    "\"\\\"abc\"\n"
    "\"foo\\\\abc\"\n"
    "\"foo\\\"abc\"\n"
    "\"abc\\\\\"\n"
    "\"abc\\\"\"\n";

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
    { Token::Type::STRING, { &kFilename, 1, 1 },
      expr::String::Create("abc", true) },
    { Token::Type::STRING, { &kFilename, 2, 1 },
      expr::String::Create("abc", true) },
    { Token::Type::STRING, { &kFilename, 3, 1 },
      expr::String::Create("abc", true) },
    { Token::Type::STRING, { &kFilename, 4, 1 },
      expr::String::Create("\\abc", true) },
    { Token::Type::STRING, { &kFilename, 5, 1 },
      expr::String::Create("\"abc", true) },
    { Token::Type::STRING, { &kFilename, 6, 1 },
      expr::String::Create("foo\\abc", true) },
    { Token::Type::STRING, { &kFilename, 7, 1 },
      expr::String::Create("foo\"abc", true) },
    { Token::Type::STRING, { &kFilename, 8, 1 },
      expr::String::Create("abc\\", true) },
    { Token::Type::STRING, { &kFilename, 9, 1 },
      expr::String::Create("abc\"", true) },
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

}  // namespace parse
