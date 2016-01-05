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
#include <cstddef>
#include <ios>
#include <iostream>

#include "util/char_class.h"
#include "util/exceptions.h"

namespace parse {

namespace {

const std::string kInvalNumPrefix = "Invalid number literal";

struct LexNumState {
  LexNumState(const std::string &str, const util::Mark &mark)
    : str(str), it(str.begin()), mark(mark), has_exact(false), exact(true),
      has_radix(false), radix(10), real(nullptr), imag(nullptr) {}

  const std::string &str;
  std::string::const_iterator it;
  const util::Mark &mark;
  bool has_exact;
  bool exact;
  bool has_radix;
  int radix;
  bool has_exp;
  expr::Number *real;
  expr::Number *imag;

  bool Eof() { return it == str.end(); }
};

void LexNumPrefix(LexNumState *state) {
  state->radix = 10;

  while (!state->Eof() && *state->it == '#') {
    ++state->it;
    if (state->Eof()) {
      const std::string msg = kInvalNumPrefix + state->str + ". Trailing '#'";
      throw util::SyntaxException(msg, state->mark);
    }
    char c = *(state->it++);
    switch (c) {
      case EXACT_SPECIFIER:
        if (state->has_exact) {
          const std::string msg = kInvalNumPrefix + state->str +
            ". Multiple exactness specifiers";
          throw util::SyntaxException(msg, state->mark);
        }
        state->has_exact = true;
        switch (c) {
          case 'i':
          case 'I':
            state->exact = false;
            break;
          case 'e':
          case 'E':
            state->exact = true;
            break;
        }
        break;
      case RADIX_SPECIFIER:
        if (state->has_radix) {
          const std::string msg = kInvalNumPrefix + state->str +
            ". Multiple radix specifiers";
          throw util::SyntaxException(msg, state->mark);
        }
        state->has_radix = true;
        switch (c) {
          case 'b':
          case 'B':
            state->radix = 2;
            break;
          case 'o':
          case 'O':
            state->radix = 8;
            break;
          case 'd':
          case 'D':
            // Radix already set
            break;
          case 'x':
          case 'X':
            state->radix = 16;
            break;
        }
        break;
      default:
        const std::string msg = kInvalNumPrefix + state->str +
          "Unknown prefix: '#'" + c;
        throw util::SyntaxException(msg, state->mark);
    }
  }
}

void ExtractDigitStr(LexNumState *state, std::string *out, bool *has_dot) {
  if (!state->Eof() && (*state->it == '+' || *state->it == '-')) {
    out->push_back(*(state->it++));
  }
  if (state->Eof()) {
    const std::string msg = kInvalNumPrefix + state->str + ". No digits";
    throw util::SyntaxException(msg, state->mark);
  }

  for (; !state->Eof(); ++state->it) {
    char c = *state->it;
    bool hex_char = false;

    switch (c) {
      case 'e': case 'E': case 'f': case 'F': case 'd': case 'D':
        hex_char = true;
      case 's': case 'S':  case 'l': case 'L':
        if (state->radix == 10) {
          if (!state->has_exact)
            state->exact = false;
          c = 'e';
          break;
        }
        if (!hex_char)
          return;

        // FALL THROUGH

      case 'a': case 'A': case 'b': case 'B': case 'c': case 'C':
        if (state->radix < 16) {
          const std::string msg = kInvalNumPrefix + state->str +
            ". Invalid digit for non hex number: " + *state->it;
          throw util::SyntaxException(msg, state->mark);
        }

        // FALL THROUGH
      case '9':
      case '8':
        if (state->radix <= 8) {
          const std::string msg = kInvalNumPrefix + state->str +
            ". Invalid digit for non decimal number: " + *state->it;
          throw util::SyntaxException(msg, state->mark);
        }
      case '7':
      case '6':
      case '5':
      case '4':
      case '3':
      case '2':
        if (state->radix <= 2) {
          const std::string msg = kInvalNumPrefix + state->str +
            ". Invalid digit for non binary number: " + *state->it;
          throw util::SyntaxException(msg, state->mark);
        }
      case '1':
      case '0':
        break;
      case '#':
        if (!state->has_exact)
          state->exact = false;

        c = '0';
        break;
      case '.':
        if (!state->has_exact)
          state->exact = false;
        *has_dot = true;
        break;

      default:
        return;
    }

    out->push_back(c);
  }
}

void LexReal(LexNumState *state, expr::Number **output) {
  bool neum_has_dot;
  std::string neum_str;
  ExtractDigitStr(state, &neum_str, &neum_has_dot);
  if (state->Eof() || *state->it != '/') {
    try {
      if (state->exact) {
        *output = expr::NumReal::Create(neum_str, state->radix);
      } else {
        *output = expr::NumFloat::Create(neum_str, state->radix);
      }
    } catch (std::exception &e) {
      const std::string msg = kInvalNumPrefix + state->str +
         + " " + e.what();
      throw util::SyntaxException(msg, state->mark);
    }
    return;
  }
  if (neum_has_dot) {
    const std::string msg = kInvalNumPrefix + state->str +
      ". Decimal point in neumerator of rational";
    throw util::SyntaxException(msg, state->mark);
  }

  bool denom_has_dot;
  std::string denom_str;
  ExtractDigitStr(state, &denom_str, &denom_has_dot);
  if (denom_has_dot) {
    const std::string msg = kInvalNumPrefix + state->str +
      ". Decimal point in denominator of rational";
    throw util::SyntaxException(msg, state->mark);
  }

  // TODO(bcf): Remove when rational numbers supported
  throw util::SyntaxException("Rational numbers not supported", state->mark);
}


}  // namespace

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
  token_.expr = expr::Symbol::Create(lexbuf_);
}

void Lexer::LexNum() {
  GetUntilDelim();
  LexNumState state(lexbuf_, token_.mark);
  LexNumPrefix(&state);

  if (!state.Eof() && (*state.it == '+' || *state.it == '-') &&
      state.it + 1 != state.str.end() && (*(state.it + 1) == 'i')) {
    // TODO(bcf): Remove when complex supported
    throw util::SyntaxException("No support for complex numbers", token_.mark);
  }

  LexReal(&state, &state.real);
  if (state.Eof() || util::IsDelim(*state.it)) {
    token_.type = Token::Type::NUMBER;
    token_.expr = state.real;
    return;
  }

  switch (*state.it) {
    case 'i':
    case 'I':
      state.imag = state.real;
      state.real = nullptr;
      break;
    case '@':
      ++state.it;
      LexReal(&state, &state.imag);
      break;
    case '+':
    case '-':
      LexReal(&state, &state.imag);
      if (state.Eof() || (*state.it != 'i' && *state.it != 'I')) {
        throw util::SyntaxException("Expected 'i' in complex literal",
            token_.mark);
      }
      ++state.it;
      break;
    default:
      throw util::SyntaxException(
          std::string("Unexpected junk on number literal: ") +
          *state.it, token_.mark);
  }

  // TODO(bcf): Remove when complex supported
  throw util::SyntaxException("No support for complex numbers", token_.mark);
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
  token_.expr = expr::Char::Create(lexbuf_[1]);
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
  token_.expr = expr::String::Create(lexbuf_, true);
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
          token_.expr = expr::Bool::Create(c == 't' || c == 'T');
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
        token_.expr = expr::Symbol::Create(lexbuf_);
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
