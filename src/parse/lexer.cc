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

std::ostream& operator<<(std::ostream& stream, Token::Type type) {
#define CASE_TYPE(type) \
  case Token::Type::type: stream << "Token::Type::" #type; break

  switch (type) {
    CASE_TYPE(INVAL);
    CASE_TYPE(TOK_EOF);
    CASE_TYPE(ID);
    CASE_TYPE(BOOL);
    CASE_TYPE(NUMBER);
    CASE_TYPE(CHAR);
    CASE_TYPE(STRING);
    CASE_TYPE(LPAREN);
    CASE_TYPE(RPAREN);
    CASE_TYPE(POUND_PAREN);
    CASE_TYPE(QUOTE);
    CASE_TYPE(BACKTICK);
    CASE_TYPE(COMMA);
    CASE_TYPE(COMMA_AT);
    CASE_TYPE(DOT);
    default:
      assert(false);
  }

  return stream;
#undef CASE_TYPE
}

void Lexer::GetUntilDelim() {
  while (!stream_.Eof()) {
    int next = stream_.Peek();
    if (util::IsDelim(next)) {
      break;
    }
    lexbuf_.push_back(stream_.Get());
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
  assert(stream_.Peek() == '\\');
  GetUntilDelim();

  if (lexbuf_ == "space") {
    lexbuf_ = "\\ ";
  } else if (lexbuf_ == "newline") {
    lexbuf_ = "\\\n";
  }

  if (lexbuf_.size() != 2) {  // 1 for \, one for the character
    std::string msg = "Invalid character literal: " + lexbuf_;
    throw util::SyntaxException(msg, token_.mark);
  }

  token_.type = Token::Type::CHAR;
  token_.char_val = lexbuf_[1];
}

// Lex string after getting '"'
void Lexer::LexString() {
  for (int c; !stream_.Eof() && (c = stream_.Get()) != '"';) {
    // Handle escape
    if (c == '\\') {
      c = stream_.Get();
    }
    lexbuf_.push_back(c);
  }
  if (stream_.Eof()) {
    throw util::SyntaxException("Unterminated string literal", token_.mark);
  }

  token_.type = Token::Type::STRING;
  token_.str_val = &lexbuf_;
}

const Token &Lexer::NextToken() {
  token_.type = Token::Type::INVAL;
  lexbuf_.clear();

  // Handle spaces and comments
  while (!stream_.Eof()) {
    while (!stream_.Eof() && std::isspace(stream_.Peek())) {  // Skip spaces
      stream_.Get();
    }

    if (stream_.Eof() || stream_.Peek() != ';') {  // Non comment character
      break;
    }

    // Handle comment
    while (!stream_.Eof() && stream_.Get() != '\n') {
      continue;
    }
    // Newline is skipped in top of loop
  }

  if (stream_.Eof()) {
    token_.type = Token::Type::TOK_EOF;
    return token_;
  }

  // Set the location of the token.
  token_.mark = stream_.mark();
  int c = stream_.Get();

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
      if (stream_.Peek() == '@') {
        stream_.Get();
        token_.type = Token::Type::COMMA_AT;
      } else {
        token_.type = Token::Type::COMMA;
      }
      break;
    case '.':
      if (util::IsDelim(stream_.Peek())) {
        token_.type = Token::Type::DOT;
        break;
      }
      lexbuf_.push_back(c);
      if (std::isdigit(stream_.Peek())) {
        LexNum();
        break;
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
      switch (stream_.Peek()) {
        case 't': case 'T': case 'f': case 'F':
          c = stream_.Get();
          token_.type = Token::Type::BOOL;
          token_.bool_val = (c == 't' || c == 'T');
          break;
        case '\\':
          LexChar();
          break;
        case 'b': case 'o': case 'd': case 'x':
        case 'e': case 'i':
          lexbuf_.push_back(c);
          LexNum();
          break;
        default:
          std::string msg = std::string("Invalid token: ") + '#' +
            static_cast<char>(stream_.Peek());
          throw util::SyntaxException(msg, token_.mark);
      }
      break;

    case '+':
    case '-':
      if (util::IsDelim(stream_.Peek())) {
        lexbuf_.push_back(c);
        token_.type = Token::Type::ID;
        token_.id_val = &lexbuf_;
        break;
      }

      // FALL THROUGH
    case ASCII_DIGIT:
      lexbuf_.push_back(c);
      LexNum();
      break;

    case '"':
      LexString();
      break;
    default:
      GetUntilDelim();
      throw util::SyntaxException("Invalid token: " + lexbuf_, token_.mark);
  }

  assert(token_.type != Token::Type::INVAL);
  return token_;
}

}  // namespace parse
