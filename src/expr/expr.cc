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

#include "expr/expr.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <sstream>

#include "gc/gc.h"
#include "util/exceptions.h"
#include "util/util.h"

namespace expr {

namespace {
const char kErrorUnboundVar[] = "Attempt to reference unbound variable";
}  // namespace

const char* TypeToString(Expr::Type type) {
#define CASE_STR(x)   \
  case Expr::Type::x: \
    return #x
  switch (type) {
    CASE_STR(EMPTY_LIST);
    CASE_STR(BOOL);
    CASE_STR(NUMBER);
    CASE_STR(CHAR);
    CASE_STR(STRING);
    CASE_STR(SYMBOL);
    CASE_STR(PAIR);
    CASE_STR(VECTOR);
    CASE_STR(INPUT_PORT);
    CASE_STR(OUTPUT_PORT);
    CASE_STR(ENV);
    CASE_STR(EVALS);
  }
#undef CASE_STR
  assert(false);
  return "UNKNOWN";
}

std::ostream& Bool::AppendStream(std::ostream& stream) const {
  return val_ ? (stream << "#t") : (stream << "#f");
}

std::ostream& Char::AppendStream(std::ostream& stream) const {
  switch (val_) {
    case ' ':
      return stream << "#\\space";
    case '\n':
      return stream << "#\\newline";
    default:
      return stream << "#\\" << val_;
  }
}

expr::Expr* Pair::Cr(const std::string& str) const {
  expr::Expr* expr = nullptr;

  for (auto cit = str.rbegin(); cit != str.rend(); ++cit) {
    auto c = *cit;
    switch (c) {
      case 'a':
      case 'd': {
        auto* cexpr = expr == nullptr ? this : expr;
        if (cexpr->type() != Type::PAIR)
          return nullptr;

        auto* pair = cexpr->AsPair();
        expr = c == 'a' ? pair->car_ : pair->cdr_;
        break;
      }
      default:
        assert(false);
    }
  }

  return expr;
}

std::ostream& Vector::AppendStream(std::ostream& stream) const {
  stream << "#(";

  for (auto* e : vals_)
    stream << *e << " ";

  return stream << ")";
}

bool Vector::EqualImpl(const Expr* other) const {
  const auto& v1 = vals_;
  const auto& v2 = other->AsVector()->vals_;
  if (v1.size() != v2.size())
    return false;

  for (auto i1 = v1.begin(), i2 = v2.begin(); i1 != v1.end(); ++i1, ++i2) {
    if (!(*i1)->Equal(*i2))
      return false;
  }

  return true;
}

// static
InputPort* InputPort::Open(const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs) {
    throw util::RuntimeException(
        "Failed to open " + path + ": " + std::strerror(errno), nullptr);
  }

  return new InputPort(path, std::move(ifs));
}

// static
OutputPort* OutputPort::Open(const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs) {
    throw util::RuntimeException(
        "Failed to open " + path + ": " + std::strerror(errno), nullptr);
  }

  return new OutputPort(path, std::move(ifs));
}

std::ostream& Env::AppendStream(std::ostream& stream) const {
  stream << "{";
  for (const auto& pair : map_)
    stream << "(" << *pair.first << ", " << *pair.second << ")";

  return stream << "}";
}

Expr* Env::TryLookup(Symbol* var) const {
  auto* env = this;
  while (env != nullptr) {
    auto search = env->map_.find(var);
    if (search != env->map_.end())
      return search->second;

    env = env->enclosing_;
  }

  return nullptr;
}

Expr* Env::Lookup(Symbol* var) const {
  auto* ret = TryLookup(var);
  if (!ret) {
    throw util::RuntimeException(kErrorUnboundVar, var);
  }

  return ret;
}

void Env::SetVar(Symbol* var, Expr* expr) {
  auto* env = this;
  while (env != nullptr) {
    auto search = env->map_.find(var);
    if (search != env->map_.end()) {
      search->second = expr;
      return;
    }

    env = env->enclosing_;
  }

  throw util::RuntimeException(kErrorUnboundVar, var);
}

EmptyList* Nil() {
  static EmptyList empty_list;
  return &empty_list;
}

Bool* True() {
  static Bool true_val(true);
  return &true_val;
}

Bool* False() {
  static Bool false_val(false);
  return &false_val;
}

std::vector<Expr*> ExprVecFromList(Expr* expr) {
  std::vector<Expr*> ret;
  while (auto* pair = expr->AsPair()) {
    ret.push_back(pair->car());
    expr = pair->cdr();
  }
  if (expr != expr::Nil())
    throw util::RuntimeException("Expected '() terminated list of expressions",
                                 expr);

  return ret;
}

}  // namespace expr
