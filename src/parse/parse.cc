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

#include "parse/parse.h"

#include <cassert>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

#include "parse/lexer.h"
#include "util/exceptions.h"

using expr::Vector;

namespace parse {

namespace {

class DatumParser {
 public:
  explicit DatumParser(util::TextStream& stream) : lexer_(stream) {}

  ExprVec Read();

 private:
  const Token& Tok() { return *cur_token_; }
  void AdvTok() { cur_token_ = &lexer_.NextToken(); }
  void ThrowException(const std::string& msg) {
    throw util::SyntaxException("Parse error: " + msg, &Tok().mark);
  }

  expr::Expr* ParseExpr();
  expr::Expr* ParseList();
  expr::Expr* ParseVector();

  Lexer lexer_;
  const Token* cur_token_ = nullptr;
};

ExprVec DatumParser::Read() {
  AdvTok();
  ExprVec result;

  while (Tok().type != Token::Type::TOK_EOF) {
    result.push_back(ParseExpr());
  }

  return result;
}

expr::Expr* DatumParser::ParseExpr() {
  switch (Tok().type) {
    case Token::Type::ID:
    case Token::Type::BOOL:
    case Token::Type::NUMBER:
    case Token::Type::CHAR:
    case Token::Type::STRING: {
      assert(Tok().expr);
      expr::Expr* result = Tok().expr.get();
      AdvTok();
      return result;
    }

    case Token::Type::LPAREN:
    case Token::Type::QUOTE:
    case Token::Type::BACKTICK:
    case Token::Type::COMMA:
    case Token::Type::COMMA_AT:
      return ParseList();

    case Token::Type::POUND_PAREN:
      return ParseVector();

    default: {
      std::ostringstream ss;
      Tok().PrettyPrint(ss);
      ThrowException("Unexpected token: " + ss.str());
    }
  }

  assert(false);
  return nullptr;
}

expr::Expr* DatumParser::ParseList() {
  const char* header = nullptr;

  switch (Tok().type) {
    case Token::Type::LPAREN: {
      AdvTok();  // Skip LPAREN

      bool has_dot = false;
      ExprVec exprs;
      while (Tok().type != Token::Type::RPAREN) {
        if (Tok().type == Token::Type::DOT) {
          if (exprs.size() == 0)
            ThrowException("Expected expression before '.'");

          if (has_dot)
            ThrowException("Unexpected token: '.'");

          has_dot = true;
          AdvTok();  // Skip DOT

          if (Tok().type == Token::Type::RPAREN)
            ThrowException("Expected expression after '.'");

          continue;
        }

        exprs.push_back(ParseExpr());
      }
      AdvTok();  // Skip RPAREN

      if (exprs.size() == 0)  // empty list
        return expr::Nil();

      expr::Pair* end = nullptr;
      for (auto it = exprs.rbegin(); it != exprs.rend(); ++it) {
        if (end == nullptr) {
          // Dot case has no '()
          if (has_dot) {
            assert(it + 1 != exprs.rend());
            auto last = *it;
            auto second_last = *(++it);
            end = expr::Cons(second_last, last);
            continue;
          }

          end = expr::Cons(*it, expr::Nil());
          continue;
        }

        end = expr::Cons(*it, end);
      }

      return end;
    }

    case Token::Type::QUOTE:
      header = "quote";
      break;
    case Token::Type::BACKTICK:
      header = "quasiquote";
      break;
    case Token::Type::COMMA:
      header = "unquote";
      break;
    case Token::Type::COMMA_AT:
      header = "unquote-splicing";
      break;
    default:
      assert(false);
  }

  // Code path for regular list is handled above
  assert(header != nullptr);
  AdvTok();
  auto expr = ParseExpr();

  return expr::Cons(expr::Symbol::New(header), expr::Cons(expr, expr::Nil()));
}

expr::Expr* DatumParser::ParseVector() {
  assert(Tok().type == Token::Type::POUND_PAREN);
  AdvTok();
  ExprVec exprs;

  while (Tok().type != Token::Type::RPAREN) {
    exprs.push_back(ParseExpr());
  }
  AdvTok();  // Skip RPAREN

  return new Vector(std::move(exprs));
}

}  // namespace

ExprVec Read(util::TextStream& stream) {  // NOLINT(runtime/references)
  DatumParser parser(stream);

  return parser.Read();
}

ExprVec Read(const std::string& str, const std::string& filename) {
  std::istringstream is(str);
  util::TextStream stream(&is, &filename);

  return Read(stream);
}

}  // namespace parse
