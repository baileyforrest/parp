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

#include "parse/lexer.h"

#include <cassert>
#include <ios>
#include <iostream>

#include "util/char_class.h"
#include "util/exceptions.h"

namespace parse {

namespace {

bool IsDelim(int c) {
  if (std::isspace(c)) {
    return true;
  }

  switch (c) {
    case '(':
    case ')':
    case '"':
    case ',':
      return true;
  }

  return false;
}

}  // namespace

Lexer::Lexer(std::istream &istream, const std::string &mark_path)
  : istream_(istream),
    istream_except_mask_(istream_.exceptions()) {
  // Enable all the exceptions on istream
  istream_.exceptions(std::istream::badbit | std::istream::failbit);
  token_.mark.path = mark_path;
}

Lexer::~Lexer() {
  // Restore istream except mask
  istream_.exceptions(istream_except_mask_);
}

// We don't need the same logic on unget, because we'll only care about the
// mark's value in the front of the token.
int Lexer::Get() {
  int c;
  try {
    c = istream_.get();
  } catch (std::ios_base::failure &fail) {
    std::cerr << "I/O error reading " << mark_.Format();
    throw;
  }
  if (c == '\n') {
    ++mark_.line;
    mark_.col = 1;
  } else {
    ++mark_.col;
  }

  return c;
}

void Lexer::GetUntilDelim() {
  while (!Eof()) {
    int next = Peek();
    if (IsDelim(next)) {
      break;
    }
    lexbuf_.push_back(Get());
  }
}

void Lexer::LexId() {
  GetUntilDelim();
  bool inval = false;
  for (auto c : lexbuf_) {
    switch (c) {
      case ID_SUBSEQUENT:
        break;
      default:
        inval = true;
    }
  }

  if (inval) {
    std::string msg = "Invalid identifier: " + lexbuf_;
    throw util::SyntaxException(msg, token_.mark);
  }

  token_.type = Token::Type::ID;
  token_.id_val = &lexbuf_;
}

void Lexer::LexNum() {
  GetUntilDelim();
  token_.type = Token::Type::NUMBER;
  token_.num_str = &lexbuf_;
}

void Lexer::LexChar() {
  assert(Peek() == '\\');
  GetUntilDelim();

  if (lexbuf_ == "space") {
    lexbuf_ = "#\\ ";
  } else if (lexbuf_ == "newline") {
    lexbuf_ = "#\\\n";
  }

  if (lexbuf_.size() != 3) { // 2 for #\, one for the character
    std::string msg = "Invalid identifier: " + lexbuf_;
    throw util::SyntaxException(msg, token_.mark);
  }

  token_.type = Token::Type::CHAR;
  token_.char_val = lexbuf_[2];
}

// Lex string after getting '"'
void Lexer::LexString() {
  for (int c; (c = Get()) != '"' && !Eof();) {
    // Handle escape
    if (c == '\\') {
      c = Get();
    }
    lexbuf_.push_back(c);
  }
  if (Eof()) {
    throw util::SyntaxException("Unterminated string literal", token_.mark);
  }

  token_.type = Token::Type::STRING;
  token_.str_val = &lexbuf_;
}

const Token &Lexer::NextToken() {
  token_.type = Token::Type::INVAL;
  lexbuf_.clear();

  // Skip spaces
  while(std::isspace(Peek())) {
    Get();
  }

  // Set the location of the token.
  token_.mark.line = mark_.line;
  token_.mark.col = mark_.col;

  int c = Get();

  // Handle comment
  if (c == ';') {
    while ((c = Get()) != '\n' && !Eof()) {
      continue;
    }

    // Skip the newline too
    Get();
  }

  if (Eof()) {
    token_.type = Token::Type::TOK_EOF;
    return token_;
  }

  switch (c) {
    case '(':
      token_.type = Token::Type::LPAREN;
      break;
    case ')':
      token_.type = Token::Type::RPAREN;
      break;
    case '\'':
      token_.type = Token::Type::QUOTE;
      break;
    case '`':
      token_.type = Token::Type::BACKTICK;
      break;
    case ',':
      if (Peek() == '@') {
        Get();
        token_.type = Token::Type::COMMA_AT;
      } else {
        token_.type = Token::Type::COMMA;
      }
      break;
    case '.':
      if (IsDelim(Peek())) {
        token_.type = Token::Type::DOT;
        break;
      }
      lexbuf_.push_back(c);
      if (std::isdigit(Peek())) {
        LexNum();
      }
      LexId();
      break;

    // Identifiers
    case ASCII_LETTER:
    case SPECIAL_INITIAL:
      lexbuf_.push_back(c);
      LexId();
      break;

    case '#':
      switch (Peek()) {
        case 't':
        case 'f':
          c = Get();
          token_.type = Token::Type::BOOL;
          token_.bool_val = c == 't';
          break;
        case '\\':
          LexChar();
          break;
        default:
          LexNum();
          break;
      }
      break;

    case '"':
      LexString();
      break;
  }

  assert(token_.type != Token::Type::INVAL);
  return token_;
}

}  // namespace parse
