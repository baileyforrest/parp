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

#include "expr/number.h"
#include "util/char_class.h"
#include "util/exceptions.h"

using expr::Char;
using expr::Float;
using expr::Int;
using expr::String;

namespace parse {

namespace {

class NumLexer {
 public:
  NumLexer(const std::string& str, const util::Mark* mark, int radix = -1)
      : mark_(mark), str_(str), it_(str.begin()), radix_(radix) {
    if (radix_ == -1) {
      radix_ = 10;
    } else {
      has_radix_ = true;
    }
    assert(radix_ == 2 || radix_ == 8 || radix_ == 10 || radix_ == 16);
  }

  expr::Number* LexNum();

 private:
  bool Eof() { return it_ == str_.end(); }
  void ThrowException(const std::string& msg) {
    const std::string full_msg =
        "Invalid number literal \"" + str_ + "\": " + msg;
    throw util::SyntaxException(full_msg, mark_);
  }

  void ParsePrefix();
  std::string ExtractDigitStr(bool* has_dot);
  expr::Number* ParseReal();

  const util::Mark* mark_;
  const std::string& str_;
  std::string::const_iterator it_;

  bool has_exact_ = false;
  bool exact_ = true;
  bool has_radix_ = false;
  int radix_ = -1;
  bool has_exp_ = false;
};

expr::Number* NumLexer::LexNum() {
  ParsePrefix();

  if (!Eof() && (*it_ == '+' || *it_ == '-') && it_ + 1 != str_.end() &&
      (*(it_ + 1) == 'i')) {
    // TODO(bcf): Remove when complex supported
    ThrowException("No support for complex numbers");
  }

  expr::Number* real_part = ParseReal();
  if (Eof())
    return real_part;

  expr::Number* imag_part = nullptr;

  switch (*it_) {
    case 'i':
    case 'I':
      imag_part = real_part;
      real_part = nullptr;
      break;
    case '@':
      ++it_;
      imag_part = ParseReal();
      break;
    case '+':
    case '-':
      imag_part = ParseReal();
      if (Eof() || (*it_ != 'i' && *it_ != 'I'))
        ThrowException("Expected 'i' in complex literal");
      ++it_;
      break;
    default:
      ThrowException(std::string("Unexpected junk on number literal: ") + *it_);
  }

  // TODO(bcf): Remove when complex supported
  (void)imag_part;
  ThrowException("No support for complex numbers");
  return nullptr;
}

void NumLexer::ParsePrefix() {
  while (!Eof() && *it_ == '#') {
    ++it_;
    if (Eof())
      ThrowException("Trailing '#'");

    char c = *(it_++);
    switch (c) {
      case EXACT_SPECIFIER:
        if (has_exact_)
          ThrowException("Multiple exactness specifiers");

        has_exact_ = true;
        switch (c) {
          case 'i':
          case 'I':
            exact_ = false;
            break;
          case 'e':
          case 'E':
            exact_ = true;
            break;
        }
        break;
      case RADIX_SPECIFIER:
        if (has_radix_)
          ThrowException("Multiple radix_ specifiers");

        has_radix_ = true;
        switch (c) {
          case 'b':
          case 'B':
            radix_ = 2;
            break;
          case 'o':
          case 'O':
            radix_ = 8;
            break;
          case 'd':
          case 'D':
            // Radix already set
            break;
          case 'x':
          case 'X':
            radix_ = 16;
            break;
        }
        break;
      default:
        ThrowException(std::string("Unknown prefix: '#'") + c);
    }
  }
}

std::string NumLexer::ExtractDigitStr(bool* has_dot) {
  *has_dot = false;
  std::string out;

  if (!Eof() && (*it_ == '+' || *it_ == '-'))
    out.push_back(*(it_++));

  if (Eof())
    ThrowException("No digits");

  for (; !Eof(); ++it_) {
    char c = *it_;
    bool hex_char = false;

    switch (c) {
      case 'e':
      case 'E':
      case 'f':
      case 'F':
      case 'd':
      case 'D':
        hex_char = true;
      case 's':
      case 'S':
      case 'l':
      case 'L':
        if (radix_ == 10) {
          if (!has_exact_)
            exact_ = false;
          c = 'e';
          break;
        }
        if (!hex_char)
          return out;

      // FALL THROUGH

      case 'a':
      case 'A':
      case 'b':
      case 'B':
      case 'c':
      case 'C':
        if (radix_ < 16)
          ThrowException(std::string("Invalid digit for non hex number: ") +
                         *it_);

      // FALL THROUGH
      case '9':
      case '8':
        if (radix_ <= 8)
          ThrowException(std::string("Invalid digit for non decimal number: ") +
                         *it_);

      case '7':
      case '6':
      case '5':
      case '4':
      case '3':
      case '2':
        if (radix_ <= 2)
          ThrowException(std::string("Invalid digit for non binary number: ") +
                         *it_);

      case '1':
      case '0':
        break;
      case '#':
        if (!has_exact_)
          exact_ = false;

        c = '0';
        break;
      case '.':
        if (!has_exact_)
          exact_ = false;
        *has_dot = true;
        break;

      default:
        return out;
    }

    out.push_back(c);
  }

  return out;
}

expr::Number* NumLexer::ParseReal() {
  bool neum_has_dot;
  std::string neum_str = ExtractDigitStr(&neum_has_dot);
  if (Eof() || *it_ != '/') {
    try {
      if (exact_) {
        return Int::Parse(neum_str, radix_);
      } else {
        return Float::Parse(neum_str, radix_);
      }
    } catch (std::exception& e) {
      ThrowException(e.what());
    }
  }
  if (neum_has_dot)
    ThrowException("Decimal point in neumerator of rational");
  ++it_;

  bool denom_has_dot;
  std::string denom_str = ExtractDigitStr(&denom_has_dot);

  if (denom_has_dot)
    ThrowException("Decimal point in denominator of rational");

  // TODO(bcf): Remove when rational numbers supported
  ThrowException("Rational numbers not supported");
  return nullptr;
}

}  // namespace

std::ostream& Token::PrettyPrint(std::ostream& stream) const {
  switch (type) {
    case Token::Type::TOK_EOF:
      stream << "EOF";
      break;

    case Token::Type::ID:
    case Token::Type::BOOL:
    case Token::Type::NUMBER:
    case Token::Type::CHAR:
    case Token::Type::STRING:
      stream << *expr;
      break;

    case Token::Type::LPAREN:
      stream << "(";
      break;
    case Token::Type::RPAREN:
      stream << ")";
      break;
    case Token::Type::POUND_PAREN:
      stream << "#(";
      break;
    case Token::Type::QUOTE:
      stream << "'";
      break;
    case Token::Type::BACKTICK:
      stream << "`";
      break;
    case Token::Type::COMMA:
      stream << ",";
      break;
    case Token::Type::COMMA_AT:
      stream << ",@";
      break;
    case Token::Type::DOT:
      stream << ".";
      break;
    default:
      assert(false);
  }

  return stream;
}

bool operator==(const Token& lhs, const Token& rhs) {
  if (lhs.type != rhs.type || lhs.mark != rhs.mark)
    return false;

  switch (lhs.type) {
    case Token::Type::ID:
    case Token::Type::BOOL:
    case Token::Type::NUMBER:
    case Token::Type::CHAR:
    case Token::Type::STRING:
      return lhs.expr->Equal(rhs.expr);
    default:
      return true;
  }
}

std::ostream& operator<<(std::ostream& stream, Token::Type type) {
#define CASE_TYPE(type)              \
  case Token::Type::type:            \
    stream << "Token::Type::" #type; \
    break

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

std::ostream& operator<<(std::ostream& stream, const Token& token) {
  stream << "Token{" << token.type << ", " << token.mark << ", ";
  token.PrettyPrint(stream);
  return stream << "}";
}

// static
expr::Number* Lexer::LexNum(const std::string& str, int radix) {
  NumLexer num_lexer(str, nullptr /* mark */, radix);
  return num_lexer.LexNum();
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
    throw util::SyntaxException(msg, &token_.mark);
  }

  token_.type = Token::Type::ID;
  token_.expr = expr::Symbol::New(lexbuf_);
}

void Lexer::LexNum() {
  GetUntilDelim();
  NumLexer num_lexer(lexbuf_, &token_.mark);

  token_.type = Token::Type::NUMBER;
  token_.expr = num_lexer.LexNum();
}

void Lexer::LexChar() {
  assert(stream_.Peek() == '\\');
  GetUntilDelim();

  if (lexbuf_ == "\\space") {
    lexbuf_ = "\\ ";
  } else if (lexbuf_ == "\\newline") {
    lexbuf_ = "\\\n";
  }

  if (lexbuf_.size() != 2) {  // 1 for \, one for the character
    std::string msg = "Invalid character literal: " + lexbuf_;
    throw util::SyntaxException(msg, &token_.mark);
  }

  token_.type = Token::Type::CHAR;
  token_.expr = new Char(lexbuf_[1]);
}

// Lex string after getting '"'
void Lexer::LexString() {
  while (true) {
    if (stream_.Eof())
      throw util::SyntaxException("Unterminated string literal", &token_.mark);
    int c = stream_.Get();
    if (c == '"')
      break;

    if (c == '\\')  // Handle escape
      c = stream_.Get();

    lexbuf_.push_back(c);
  }

  token_.type = Token::Type::STRING;
  token_.expr = new String(lexbuf_, true);
}

const Token& Lexer::NextToken() {
  token_.type = Token::Type::INVAL;
  token_.expr = nullptr;
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
        case 't':
        case 'T':
        case 'f':
        case 'F':
          c = stream_.Get();
          token_.type = Token::Type::BOOL;
          token_.expr = (c == 't' || c == 'T') ? expr::True() : expr::False();
          break;
        case '\\':
          LexChar();
          break;
        case 'b':
        case 'o':
        case 'd':
        case 'x':
        case 'e':
        case 'i':
          lexbuf_.push_back(c);
          LexNum();
          break;
        case '(':
          stream_.Get();  // Consume the (
          token_.type = Token::Type::POUND_PAREN;
          break;
        default:
          std::string msg = std::string("Invalid token: ") + '#' +
                            static_cast<char>(stream_.Peek());
          throw util::SyntaxException(msg, &token_.mark);
      }
      break;

    case '+':
    case '-':
      if (util::IsDelim(stream_.Peek())) {
        lexbuf_.push_back(c);
        token_.type = Token::Type::ID;
        token_.expr = expr::Symbol::New(lexbuf_);
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
      throw util::SyntaxException("Invalid token: " + lexbuf_, &token_.mark);
  }

  assert(token_.type != Token::Type::INVAL);
  return token_;
}

}  // namespace parse
