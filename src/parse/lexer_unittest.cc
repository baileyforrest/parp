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

#include "gtest/gtest.h"

#include "parse/lexer.h"
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
    {{ Token::Type::LPAREN, { &kFilename, 3, 3 }, {}, }, ""},
    {{ Token::Type::ID, { &kFilename, 3, 4 }, {}, }, "define"},
    {{ Token::Type::ID, { &kFilename, 3, 11 }, {}, }, "fact" },
    {{ Token::Type::LPAREN, { &kFilename, 4, 4 }, {}, }, ""},
    {{ Token::Type::ID, { &kFilename, 4, 5 }, {}, }, "lambda" },
    {{ Token::Type::LPAREN, { &kFilename, 4, 12 }, {}, }, ""},
    {{ Token::Type::ID, { &kFilename, 4, 13 }, {}, }, "n" },
    {{ Token::Type::RPAREN, { &kFilename, 4, 14 }, {}, }, ""},
    {{ Token::Type::LPAREN, { &kFilename, 5, 5 }, {}, }, ""},
    {{ Token::Type::ID, { &kFilename, 5, 6 }, {}, }, "if" },
    {{ Token::Type::LPAREN, { &kFilename, 5, 9 }, {}, }, ""},
    {{ Token::Type::ID, { &kFilename, 5, 10 }, {}, }, "=" },
    {{ Token::Type::ID, { &kFilename, 5, 12 }, {}, }, "n" },
    {{ Token::Type::NUMBER, { &kFilename, 5, 14 }, {}, }, "0" },
    {{ Token::Type::RPAREN, { &kFilename, 5, 15 }, {}, }, ""},
    {{ Token::Type::NUMBER, { &kFilename, 6, 6 }, {}, }, "1" },
    {{ Token::Type::LPAREN, { &kFilename, 7, 6 }, {}, }, ""},
    {{ Token::Type::ID, { &kFilename, 7, 7 }, {}, }, "*" },
    {{ Token::Type::ID, { &kFilename, 7, 9 }, {}, }, "n" },
    {{ Token::Type::LPAREN, { &kFilename, 7, 11 }, {}, }, ""},
    {{ Token::Type::ID, { &kFilename, 7, 12 }, {}, }, "fact" },
    {{ Token::Type::LPAREN, { &kFilename, 7, 17 }, {}, }, ""},
    {{ Token::Type::ID, { &kFilename, 7, 18 }, {}, }, "-" },
    {{ Token::Type::ID, { &kFilename, 7, 20 }, {}, }, "n" },
    {{ Token::Type::NUMBER, { &kFilename, 7, 22 }, {}, }, "1" },
    {{ Token::Type::RPAREN, { &kFilename, 7, 23 }, {}, }, ""},
    {{ Token::Type::RPAREN, { &kFilename, 7, 24 }, {}, }, ""},
    {{ Token::Type::RPAREN, { &kFilename, 7, 25 }, {}, }, ""},
    {{ Token::Type::RPAREN, { &kFilename, 7, 26 }, {}, }, ""},
    {{ Token::Type::RPAREN, { &kFilename, 7, 27 }, {}, }, ""},
    {{ Token::Type::RPAREN, { &kFilename, 7, 28 }, {}, }, ""},
    {{ Token::Type::INVAL, { &kFilename, -1, -1 }, {}, }, ""},
  };

  std::istringstream s(kStr);
  util::TextStream stream(s, &kFilename);
  Lexer lexer{stream};

  VerifyTokens(lexer, kExpected);
}

}  // namespace parse
