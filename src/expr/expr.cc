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
#include <sstream>

#include "gc/gc.h"
#include "util/exceptions.h"
#include "util/util.h"

namespace expr {

namespace {

void ThrowUnboundException(Symbol* var) {
  throw util::RuntimeException("Attempt to reference unbound variable: ", var);
}

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
    CASE_STR(LAMBDA);
    CASE_STR(ENV);
    CASE_STR(ANALYZED);
    CASE_STR(PRIMITIVE);
  }
#undef CASE_STR
  assert(false);
  return "UNKNOWN";
}

std::ostream& EmptyList::AppendStream(std::ostream& stream) const {
  return stream << "'()";
}

std::ostream& Bool::AppendStream(std::ostream& stream) const {
  return val_ ? (stream << "#t") : (stream << "#f");
}

// static
Char* Char::Create(char val) {
  return static_cast<Char*>(
      gc::Gc::Get().Alloc(sizeof(Char), [val](void* addr) {
        return new (addr) Char(val);
      }));  // NOLINT(whitespace/newline)
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

// static
String* String::Create(std::string val, bool read_only) {
  return static_cast<String*>(gc::Gc::Get().Alloc(
      sizeof(String), [val, &read_only](void* addr) mutable {
        return new (addr) String(val, std::move(read_only));
      }));
}

String::~String() = default;

std::ostream& String::AppendStream(std::ostream& stream) const {
  return stream << "\"" << val_ << "\"";
}

String::String(std::string val, bool read_only)
    : Expr(Type::STRING), val_(std::move(val)), read_only_(read_only) {}

bool String::EqualImpl(const Expr* other) const {
  return val_ == other->AsString()->val_;
}

// static
Symbol* Symbol::Create(std::string val) {
  return static_cast<Symbol*>(
      gc::Gc::Get().Alloc(sizeof(Symbol), [&val](void* addr) {
        return new (addr) Symbol(std::move(val));
      }));  // NOLINT(whitespace/newline)
}

Symbol::~Symbol() = default;

std::ostream& Symbol::AppendStream(std::ostream& stream) const {
  return stream << val_;
}

Symbol::Symbol(std::string val) : Expr(Type::SYMBOL), val_(std::move(val)) {}

bool Symbol::EqvImpl(const Expr* other) const {
  return val_ == other->AsSymbol()->val_;
}

// static
Pair* Pair::Create(Expr* car, Expr* cdr) {
  return static_cast<Pair*>(
      gc::Gc::Get().Alloc(sizeof(Pair), [car, cdr](void* addr) {
        return new (addr) Pair(car, cdr);
      }));  // NOLINT(whitespace/newline)
}

// TODO(bcf): Check if its a list, if so print as (a b c ...)
std::ostream& Pair::AppendStream(std::ostream& stream) const {
  return stream << "(" << *car_ << " . " << *cdr_ << ")";
}

bool Pair::EqvImpl(const Expr* other) const {
  return car_ == other->AsPair()->car_ && cdr_ == other->AsPair()->cdr_;
}

bool Pair::EqualImpl(const Expr* other) const {
  return car_->Equal(other->AsPair()->car_) &&
         cdr_->Equal(other->AsPair()->cdr_);
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

// static
Vector* Vector::Create(std::vector<Expr*> vals) {
  return static_cast<Vector*>(
      gc::Gc::Get().Alloc(sizeof(Vector), [&vals](void* addr) {
        return new (addr) Vector(std::move(vals));
      }));  // NOLINT(whitespace/newline)
}

Vector::~Vector() = default;

std::ostream& Vector::AppendStream(std::ostream& stream) const {
  stream << "#(";

  for (auto* e : vals_)
    stream << *e << " ";

  return stream << ")";
}

Vector::Vector(std::vector<Expr*> vals)
    : Expr(Type::VECTOR), vals_(std::move(vals)) {}

bool Vector::EqvImpl(const Expr* other) const {
  return vals_ == other->AsVector()->vals_;
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
Lambda* Lambda::Create(std::vector<Symbol*> required_args,
                       Symbol* variable_arg,
                       std::vector<Expr*> body,
                       Env* env) {
  return static_cast<Lambda*>(gc::Gc::Get().Alloc(
      sizeof(Lambda), [&required_args, variable_arg, &body, env](void* addr) {
        return new (addr) Lambda(std::move(required_args), variable_arg,
                                 std::move(body), env);
      }));
}

Lambda::~Lambda() = default;

std::ostream& Lambda::AppendStream(std::ostream& stream) const {
  stream << "(lambda ";
  if (required_args_.size() == 0 && variable_arg_ != nullptr) {
    stream << *variable_arg_;
  } else {
    stream << "(";
    for (auto* arg : required_args_)
      stream << *arg << " ";

    if (variable_arg_) {
      stream << ". " << *variable_arg_;
    }
    stream << ")";
  }

  for (auto* expr : body_) {
    stream << *expr << "\n";
  }
  return stream << ")";
}

Lambda::Lambda(std::vector<Symbol*> required_args,
               Symbol* variable_arg,
               std::vector<Expr*> body,
               Env* env)
    : Expr(Type::LAMBDA),
      required_args_(std::move(required_args)),
      variable_arg_(variable_arg),
      body_(std::move(body)),
      env_(env) {
  // TODO(bcf): assert Error checking on body.
}

// static
Env* Env::Create(Env* enclosing) {
  return static_cast<Env*>(
      gc::Gc::Get().Alloc(sizeof(Env), [enclosing](void* addr) {
        return new (addr) Env(enclosing);
      }));  // NOLINT(whitespace/newline)
}

Env::~Env() = default;

std::ostream& Env::AppendStream(std::ostream& stream) const {
  stream << "{";
  for (const auto& pair : map_)
    stream << "(" << *pair.first << ", " << *pair.second << ")";

  return stream << "}";
}

Env::Env(Env* enclosing) : Expr(Type::ENV), enclosing_(enclosing) {}

std::size_t Env::VarHash::operator()(Symbol* var) const {
  std::hash<std::string> hash;
  return hash(var->val());
}

Expr* Env::Lookup(Symbol* var) const {
  auto* env = this;
  while (env != nullptr) {
    auto search = env->map_.find(var);
    if (search != env->map_.end())
      return search->second;

    env = env->enclosing_;
  }

  ThrowUnboundException(var);
  return nullptr;
}

void Env::DefineVar(Symbol* var, Expr* expr) {
  map_[var] = expr;
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

  ThrowUnboundException(var);
}

// static
Analyzed* Analyzed::Create(Expr* orig_expr,
                           Evaluation func,
                           std::vector<Expr*> refs) {
  return static_cast<Analyzed*>(gc::Gc::Get().Alloc(
      sizeof(Analyzed), [orig_expr, &func, &refs](void* addr) {
        return new (addr) Analyzed(orig_expr, std::move(func), std::move(refs));
      }));  // NOLINT(whitespace/newline)
}

Analyzed::~Analyzed() = default;

std::ostream& Analyzed::AppendStream(std::ostream& stream) const {
  return stream << "Analyzed(" << *orig_expr_ << ")";
}

Analyzed::Analyzed(Expr* orig_expr, Evaluation func, std::vector<Expr*> refs)
    : Expr(Type::ANALYZED),
      orig_expr_(orig_expr),
      func_(std::move(func)),
      refs_(std::move(refs)) {}

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
