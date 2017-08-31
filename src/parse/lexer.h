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

#ifndef PARSE_LEXER_H_
#define PARSE_LEXER_H_

#include <istream>
#include <ostream>
#include <string>

#include "expr/number.h"

#include "util/macros.h"
#include "util/mark.h"
#include "util/text_stream.h"

namespace parse {

// struct representing <token> in r5rs:7.1.1
struct Token {
  enum class Type {
    INVAL,    // Invalid token
    TOK_EOF,  // EOF token

    ID,      // identifier
    BOOL,    // boolean
    NUMBER,  // number
    CHAR,    // character
    STRING,  // string

    LPAREN,       // (
    RPAREN,       // )
    POUND_PAREN,  // #(
    QUOTE,        // '
    BACKTICK,     // `
    COMMA,        // ,
    COMMA_AT,     // ,@
    DOT,          // .
  };

  Type type;
  util::Mark mark;
  expr::Expr* expr;

  std::ostream& PrettyPrint(
      std::ostream& stream) const;  // NOLINT(runtime/references)
};

bool operator==(const Token& lhs, const Token& rhs);
inline bool operator!=(const Token& lhs, const Token& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& stream, Token::Type type);
std::ostream& operator<<(std::ostream& stream, const Token& token);

class Lexer {
 public:
  explicit Lexer(util::TextStream& stream) : stream_(stream) {}
  ~Lexer() = default;

  const Token& NextToken();

 private:
  DISALLOW_MOVE_COPY_AND_ASSIGN(Lexer);

  void GetUntilDelim();
  void LexId();
  void LexNum();
  void LexChar();
  void LexString();

  Token token_;
  std::string lexbuf_;
  util::TextStream& stream_;
};

}  // namespace parse

#endif  // PARSE_LEXER_H_
