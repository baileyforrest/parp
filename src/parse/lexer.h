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
#include <string>

#include "expr/number.h"
#include "util/macros.h"
#include "util/mark.h"

namespace parse {

// struct representing <token> in r5rs:7.1.1
struct Token {
  enum class Type {
    INVAL,       // Invalid token
    TOK_EOF,     // EOF token

    ID,          // identifier
    BOOL,        // boolean
    NUMBER,      // number
    CHAR,        // character
    STRING,      // string

    LPAREN,      // (
    RPAREN,      // )
    POUND_PAREN, // #(
    QUOTE,       // '
    BACKTICK,    // `
    COMMA,       // ,
    COMMA_AT,    // ,@
    DOT,         // .
  };

  Type type;
  util::Mark mark;
  union {
    const std::string *id_val;
    bool bool_val;
    const std::string *num_str;
    char char_val;
    const std::string *str_val;
  };
};

class Lexer {
public:
  Lexer(std::istream &istream, const std::string &mark_path);
  ~Lexer();

  const Token &NextToken();

private:
  DISALLOW_MOVE_COPY_AND_ASSIGN(Lexer);
  int Get();
  int Peek() { return istream_.peek(); }
  int Eof() { return istream_.eof(); }
  void GetUntilDelim();
  void LexId();
  void LexNum();
  void LexChar();
  void LexString();

  Token token_;
  std::string lexbuf_;
  util::Mark mark_;

  std::istream &istream_;
  std::ios_base::iostate istream_except_mask_;
};

}  // namespace parse

#endif  // PARSE_LEXER_H_
