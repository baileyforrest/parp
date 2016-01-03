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

// TODO: Add tests for invalid tokens

#include <sstream>

#include "gtest/gtest.h"

#include "parse/lexer.h"
#include "util/char_class.h"
#include "util/text_stream.h"

namespace parse {

struct TokTest {
  Token tok;
  const char *str_val;
};

namespace {

void VerifyTokens(Lexer &lexer, const TokTest *expected) {
  for (;expected->tok.type != Token::Type::INVAL; ++expected) {
    const auto &expect = expected->tok;
    const auto &got = lexer.NextToken();
    EXPECT_EQ(expect.type, got.type);
    EXPECT_EQ(expect.mark, got.mark);

    if (expect.type != got.type) {
      continue;
    }
    switch (expect.type) {
      case Token::Type::ID:
        EXPECT_EQ(expected->str_val, *got.id_val);
        break;
      case Token::Type::BOOL:
        EXPECT_EQ(expect.bool_val, got.bool_val);
        break;
      case Token::Type::NUMBER:
        EXPECT_EQ(expected->str_val, *got.num_str);
        break;
      case Token::Type::CHAR:
        EXPECT_EQ(expect.char_val, got.char_val);
        break;
      case Token::Type::STRING:
        EXPECT_EQ(expected->str_val, *got.str_val);
        break;
      default:
        break;
    }
  }

  EXPECT_EQ(Token::Type::TOK_EOF, lexer.NextToken().type);
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

  const TokTest kExpected[] = {
    {{ Token::Type::LPAREN, { &kFilename, 3, 3 }, {}, }, "" },
    {{ Token::Type::ID, { &kFilename, 3, 4 }, {}, }, "define"},
    {{ Token::Type::ID, { &kFilename, 3, 11 }, {}, }, "fact" },
    {{ Token::Type::LPAREN, { &kFilename, 4, 4 }, {}, }, "" },
    {{ Token::Type::ID, { &kFilename, 4, 5 }, {}, }, "lambda" },
    {{ Token::Type::LPAREN, { &kFilename, 4, 12 }, {}, }, "" },
    {{ Token::Type::ID, { &kFilename, 4, 13 }, {}, }, "n" },
    {{ Token::Type::RPAREN, { &kFilename, 4, 14 }, {}, }, "" },
    {{ Token::Type::LPAREN, { &kFilename, 5, 5 }, {}, }, "" },
    {{ Token::Type::ID, { &kFilename, 5, 6 }, {}, }, "if" },
    {{ Token::Type::LPAREN, { &kFilename, 5, 9 }, {}, }, "" },
    {{ Token::Type::ID, { &kFilename, 5, 10 }, {}, }, "=" },
    {{ Token::Type::ID, { &kFilename, 5, 12 }, {}, }, "n" },
    {{ Token::Type::NUMBER, { &kFilename, 5, 14 }, {}, }, "0" },
    {{ Token::Type::RPAREN, { &kFilename, 5, 15 }, {}, }, "" },
    {{ Token::Type::NUMBER, { &kFilename, 6, 6 }, {}, }, "1" },
    {{ Token::Type::LPAREN, { &kFilename, 7, 6 }, {}, }, "" },
    {{ Token::Type::ID, { &kFilename, 7, 7 }, {}, }, "*" },
    {{ Token::Type::ID, { &kFilename, 7, 9 }, {}, }, "n" },
    {{ Token::Type::LPAREN, { &kFilename, 7, 11 }, {}, }, "" },
    {{ Token::Type::ID, { &kFilename, 7, 12 }, {}, }, "fact" },
    {{ Token::Type::LPAREN, { &kFilename, 7, 17 }, {}, }, "" },
    {{ Token::Type::ID, { &kFilename, 7, 18 }, {}, }, "-" },
    {{ Token::Type::ID, { &kFilename, 7, 20 }, {}, }, "n" },
    {{ Token::Type::NUMBER, { &kFilename, 7, 22 }, {}, }, "1" },
    {{ Token::Type::RPAREN, { &kFilename, 7, 23 }, {}, }, "" },
    {{ Token::Type::RPAREN, { &kFilename, 7, 24 }, {}, }, "" },
    {{ Token::Type::RPAREN, { &kFilename, 7, 25 }, {}, }, "" },
    {{ Token::Type::RPAREN, { &kFilename, 7, 26 }, {}, }, "" },
    {{ Token::Type::RPAREN, { &kFilename, 7, 27 }, {}, }, "" },
    {{ Token::Type::RPAREN, { &kFilename, 7, 28 }, {}, }, "" },

    {{ Token::Type::INVAL, { &kFilename, -1, -1 }, {}, }, "" },
  };

  std::istringstream s(kStr);
  util::TextStream stream(s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(lexer, kExpected);
}

TEST(LexerTest, Empty) {
  const char *kStr = "";

  const std::string kFilename = "foo";

  const TokTest kExpected[] = {
    {{ Token::Type::INVAL, { &kFilename, -1, -1 }, {}, }, "" },
  };

  std::istringstream s(kStr);
  util::TextStream stream(s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(lexer, kExpected);
}

TEST(LexerTest, NoTrailingNewine) {
  const char *kStr = "abc";

  const std::string kFilename = "foo";

  const TokTest kExpected[] = {
    {{ Token::Type::ID, { &kFilename, 1, 1 }, {}, }, "abc"},

    {{ Token::Type::INVAL, { &kFilename, -1, -1 }, {}, }, "" },
  };

  std::istringstream s(kStr);
  util::TextStream stream(s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(lexer, kExpected);
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

  const TokTest kExpected[] = {
    {{ Token::Type::ID, { &kFilename, 1, 1 }, {}, }, "abc" },
    {{ Token::Type::ID, { &kFilename, 2, 1 }, {}, }, "!" },
    {{ Token::Type::ID, { &kFilename, 3, 1 }, {}, }, "$" },
    {{ Token::Type::ID, { &kFilename, 4, 1 }, {}, }, "%" },
    {{ Token::Type::ID, { &kFilename, 5, 1 }, {}, }, "&" },
    {{ Token::Type::ID, { &kFilename, 6, 1 }, {}, }, "*" },
    {{ Token::Type::ID, { &kFilename, 7, 1 }, {}, }, "/" },
    {{ Token::Type::ID, { &kFilename, 8, 1 }, {}, }, ":" },
    {{ Token::Type::ID, { &kFilename, 9, 1 }, {}, }, "<" },
    {{ Token::Type::ID, { &kFilename, 10, 1 }, {}, }, "=" },
    {{ Token::Type::ID, { &kFilename, 11, 1 }, {}, }, ">" },
    {{ Token::Type::ID, { &kFilename, 12, 1 }, {}, }, "?" },
    {{ Token::Type::ID, { &kFilename, 13, 1 }, {}, }, "^" },
    {{ Token::Type::ID, { &kFilename, 14, 1 }, {}, }, "_" },
    {{ Token::Type::ID, { &kFilename, 15, 1 }, {}, }, "~" },
    {{ Token::Type::ID, { &kFilename, 16, 1 }, {}, }, "~a" },
    {{ Token::Type::ID, { &kFilename, 17, 1 }, {}, }, "+" },
    {{ Token::Type::ID, { &kFilename, 18, 1 }, {}, }, "-" },
    {{ Token::Type::ID, { &kFilename, 19, 1 }, {}, }, "..." },
    {{ Token::Type::ID, { &kFilename, 20, 1 }, {}, }, "a+" },
    {{ Token::Type::ID, { &kFilename, 21, 1 }, {}, }, "b-" },
    {{ Token::Type::ID, { &kFilename, 22, 1 }, {}, }, "c." },
    {{ Token::Type::ID, { &kFilename, 23, 1 }, {}, }, "c@" },

    {{ Token::Type::INVAL, { &kFilename, -1, -1 }, {}, }, "" },
  };

  std::istringstream s(kStr);
  util::TextStream stream(s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(lexer, kExpected);
}

TEST(LexerTest, BoolTest) {
  const char *kStr =
    "#t\n"
    "#f\n"
    "#T\n"
    "#F\n";

  const std::string kFilename = "foo";

  const TokTest kExpected[] = {
    {{ Token::Type::BOOL, { &kFilename, 1, 1 }, { .bool_val = true }, }, "" },
    {{ Token::Type::BOOL, { &kFilename, 2, 1 }, { .bool_val = false }, }, "" },
    {{ Token::Type::BOOL, { &kFilename, 3, 1 }, { .bool_val = true }, }, "" },
    {{ Token::Type::BOOL, { &kFilename, 4, 1 }, { .bool_val = false }, }, "" },

    {{ Token::Type::INVAL, { &kFilename, -1, -1 }, {}, }, "" },
  };

  std::istringstream s(kStr);
  util::TextStream stream(s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(lexer, kExpected);
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

    "5@4\n"
    "10+7i\n"
    "10-7i\n"

    "+13i\n"
    "-14i\n"

    "+i\n"
    "-i\n"

    "3\n"
    "3/4\n"
    "+2\n"
    "-2\n"
    "4##\n"
    "5.7\n"
    "5##.##7\n"
    "7.2###\n"
    ".3###\n"

    "1s\n"
    "1f\n"
    "1d\n"
    "1l\n"
    "2e-10\n"
    "2e+10\n"
    "#i3###.##4e-27d@4##.#5e14\n";

  const std::string kFilename = "foo";

  const TokTest kExpected[] = {
    {{ Token::Type::NUMBER, { &kFilename, 1, 1 }, {}, }, "#b1" },
    {{ Token::Type::NUMBER, { &kFilename, 2, 1 }, {}, }, "#o1" },
    {{ Token::Type::NUMBER, { &kFilename, 3, 1 }, {}, }, "#d1" },
    {{ Token::Type::NUMBER, { &kFilename, 4, 1 }, {}, }, "#x1" },
    {{ Token::Type::NUMBER, { &kFilename, 5, 1 }, {}, }, "#i1" },
    {{ Token::Type::NUMBER, { &kFilename, 6, 1 }, {}, }, "#e1" },
    {{ Token::Type::NUMBER, { &kFilename, 7, 1 }, {}, }, "#i#b1" },
    {{ Token::Type::NUMBER, { &kFilename, 8, 1 }, {}, }, "#i#o1" },
    {{ Token::Type::NUMBER, { &kFilename, 9, 1 }, {}, }, "#e#d1" },
    {{ Token::Type::NUMBER, { &kFilename, 10, 1 }, {}, }, "#e#x1" },
    {{ Token::Type::NUMBER, { &kFilename, 11, 1 }, {}, }, "#b#e1" },
    {{ Token::Type::NUMBER, { &kFilename, 12, 1 }, {}, }, "#o#e1" },
    {{ Token::Type::NUMBER, { &kFilename, 13, 1 }, {}, }, "#d#i1" },
    {{ Token::Type::NUMBER, { &kFilename, 14, 1 }, {}, }, "#x#i1" },
    {{ Token::Type::NUMBER, { &kFilename, 15, 1 }, {}, }, "5@4" },
    {{ Token::Type::NUMBER, { &kFilename, 16, 1 }, {}, }, "10+7i" },
    {{ Token::Type::NUMBER, { &kFilename, 17, 1 }, {}, }, "10-7i" },
    {{ Token::Type::NUMBER, { &kFilename, 18, 1 }, {}, }, "+13i" },
    {{ Token::Type::NUMBER, { &kFilename, 19, 1 }, {}, }, "-14i" },
    {{ Token::Type::NUMBER, { &kFilename, 20, 1 }, {}, }, "+i" },
    {{ Token::Type::NUMBER, { &kFilename, 21, 1 }, {}, }, "-i" },
    {{ Token::Type::NUMBER, { &kFilename, 22, 1 }, {}, }, "3" },
    {{ Token::Type::NUMBER, { &kFilename, 23, 1 }, {}, }, "3/4" },
    {{ Token::Type::NUMBER, { &kFilename, 24, 1 }, {}, }, "+2" },
    {{ Token::Type::NUMBER, { &kFilename, 25, 1 }, {}, }, "-2" },
    {{ Token::Type::NUMBER, { &kFilename, 26, 1 }, {}, }, "4##" },
    {{ Token::Type::NUMBER, { &kFilename, 27, 1 }, {}, }, "5.7" },
    {{ Token::Type::NUMBER, { &kFilename, 28, 1 }, {}, }, "5##.##7" },
    {{ Token::Type::NUMBER, { &kFilename, 29, 1 }, {}, }, "7.2###" },
    {{ Token::Type::NUMBER, { &kFilename, 30, 1 }, {}, }, ".3###" },
    {{ Token::Type::NUMBER, { &kFilename, 31, 1 }, {}, }, "1s" },
    {{ Token::Type::NUMBER, { &kFilename, 32, 1 }, {}, }, "1f" },
    {{ Token::Type::NUMBER, { &kFilename, 33, 1 }, {}, }, "1d" },
    {{ Token::Type::NUMBER, { &kFilename, 34, 1 }, {}, }, "1l" },
    {{ Token::Type::NUMBER, { &kFilename, 35, 1 }, {}, }, "2e-10" },
    {{ Token::Type::NUMBER, { &kFilename, 36, 1 }, {}, }, "2e+10" },
    {{ Token::Type::NUMBER, { &kFilename, 37, 1 }, {}, },
       "#i3###.##4e-27d@4##.#5e14" },

    {{ Token::Type::INVAL, { &kFilename, -1, -1 }, {}, }, "" },
  };

  std::istringstream s(kStr);
  util::TextStream stream(s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(lexer, kExpected);
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

  std::vector<TokTest> expected;
  for (int i = 0, line = 0; i < kAsciiChars; ++i) {
    if (util::IsDelim(i)) {
      continue;
    }
    ++line;
    TokTest expect = {
      {
        Token::Type::CHAR,
        { &kFilename, line, 1 },
        { .char_val = static_cast<char>(i) },
      }, ""
    };
    expected.push_back(expect);
  }
  TokTest terminator = {{ Token::Type::INVAL, { &kFilename, -1, -1 }, {}, }, "" };
  expected.push_back(terminator);

  std::istringstream s(input);
  util::TextStream stream(s, &kFilename);
  Lexer lexer{stream};


  VerifyTokens(lexer, expected.data());
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

  const TokTest kExpected[] = {
    {{ Token::Type::STRING, { &kFilename, 1, 1 }, { }, }, "abc" },
    {{ Token::Type::STRING, { &kFilename, 2, 1 }, { }, }, "abc" },
    {{ Token::Type::STRING, { &kFilename, 3, 1 }, { }, }, "abc" },
    {{ Token::Type::STRING, { &kFilename, 4, 1 }, { }, }, "\\abc" },
    {{ Token::Type::STRING, { &kFilename, 5, 1 }, { }, }, "\"abc" },
    {{ Token::Type::STRING, { &kFilename, 6, 1 }, { }, }, "foo\\abc" },
    {{ Token::Type::STRING, { &kFilename, 7, 1 }, { }, }, "foo\"abc" },
    {{ Token::Type::STRING, { &kFilename, 8, 1 }, { }, }, "abc\\" },
    {{ Token::Type::STRING, { &kFilename, 9, 1 }, { }, }, "abc\"" },

    {{ Token::Type::INVAL, { &kFilename, -1, -1 }, {}, }, "" },
  };

  std::istringstream s(kStr);
  util::TextStream stream(s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(lexer, kExpected);
}

}  // namespace parse
