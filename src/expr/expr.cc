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

namespace expr {

bool Expr::IsDatum() const {
  switch (type()) {
    case Expr::Type::BOOL:
    case Expr::Type::NUMBER:
    case Expr::Type::CHAR:
    case Expr::Type::STRING:
    case Expr::Type::SYMBOL:
    case Expr::Type::PAIR:
    case Expr::Type::VECTOR:
      return true;
    default:
      return false;
  }
}

bool Expr::Eqv(const Expr *other) const {
  return Eq(other) ||
    (type() == other->type() && EqvImpl(other));
}

bool Expr::Equal(const Expr *other) const {
  return Eq(other) ||
    (type() == other->type() && EqualImpl(other));
}

bool Expr::EqvImpl(const Expr *other) const {
  return Eq(other);
}

bool Expr::EqualImpl(const Expr *other) const {
  return Eqv(other);
}

const EmptyList *Expr::GetAsEmptyList() const {
  assert(false);
  return nullptr;
}

const Bool *Expr::GetAsBool() const {
  assert(false);
  return nullptr;
}

const Number *Expr::GetAsNumber() const {
  assert(false);
  return nullptr;
}

const Char *Expr::GetAsChar() const {
  assert(false);
  return nullptr;
}

const String *Expr::GetAsString() const {
  assert(false);
  return nullptr;
}

String *Expr::GetAsString() {
  assert(false);
  return nullptr;
}

const Symbol *Expr::GetAsSymbol() const {
  assert(false);
  return nullptr;
}

const Pair *Expr::GetAsPair() const {
  assert(false);
  return nullptr;
}

Pair *Expr::GetAsPair() {
  assert(false);
  return nullptr;
}

const Vector *Expr::GetAsVector() const {
  assert(false);
  return nullptr;
}

Vector *Expr::GetAsVector() {
  assert(false);
  return nullptr;
}

const Var *Expr::GetAsVar() const {
  assert(false);
  return nullptr;
}

const Apply *Expr::GetAsApply() const {
  assert(false);
  return nullptr;
}

const Lambda *Expr::GetAsLambda() const {
  assert(false);
  return nullptr;
}

const Cond *Expr::GetAsCond() const {
  assert(false);
  return nullptr;
}

const Assign *Expr::GetAsAssign() const {
  assert(false);
  return nullptr;
}

const LetSyntax *Expr::GetAsLetSyntax() const {
  assert(false);
  return nullptr;
}

bool Evals::EqvImpl(const Expr *other) const {
  (void)other;
  assert(false && "The evaluation of this expr should be compared instead");
}

EmptyList *EmptyList::Create() {
  static EmptyList empty_list;
  return &empty_list;
}

const EmptyList *EmptyList::GetAsEmptyList() const {
  return this;
}

std::ostream &EmptyList::AppendStream(std::ostream &stream) const {
  return stream << "'()";
}

// static
Bool *Bool::Create(bool val) {
  return static_cast<Bool *>(
      gc::Gc::Get().Alloc(sizeof(Bool), [val](void *addr) {
        return new(addr) Bool(val);
      }));
}

const Bool *Bool::GetAsBool() const {
  return this;
}

std::ostream &Bool::AppendStream(std::ostream &stream) const {
  return val() ? (stream << "#t") : (stream << "#f");
}

bool Bool::EqvImpl(const Expr *other) const {
  return val() == other->GetAsBool()->val();
}

// static
Char *Char::Create(char val) {
  return static_cast<Char *>(
      gc::Gc::Get().Alloc(sizeof(Char), [val](void *addr) {
        return new(addr) Char(val);
      }));
}

const Char *Char::GetAsChar() const {
  return this;
}

std::ostream &Char::AppendStream(std::ostream &stream) const {
  switch (val()) {
    case ' ':
      return stream << "#\\space";
    case '\n':
      return stream << "#\\newline";
    default:
      return stream << "#\\" << val();
  }
}

bool Char::EqvImpl(const Expr *other) const {
  return val() == other->GetAsChar()->val();
}

// static
String *String::Create(const std::string &val, bool readonly) {
  return static_cast<String *>(
      gc::Gc::Get().Alloc(sizeof(String), [val, readonly](void *addr) {
        return new(addr) String(val, readonly);
      }));
}

String::~String() {
}

const String *String::GetAsString() const {
  return this;
}

std::ostream &String::AppendStream(std::ostream &stream) const {
  return stream << "\"" << val() << "\"";
}

String *String::GetAsString() {
  return this;
}

bool String::EqualImpl(const Expr *other) const {
  return val() == other->GetAsString()->val();
}

// static
Symbol *Symbol::Create(const std::string &val) {
  return static_cast<Symbol *>(
      gc::Gc::Get().Alloc(sizeof(Symbol), [val](void *addr) {
        return new(addr) Symbol(val);
      }));
}

Symbol::~Symbol() {
}

const Symbol *Symbol::GetAsSymbol() const {
  return this;
}

std::ostream &Symbol::AppendStream(std::ostream &stream) const {
  return stream << val();
}

bool Symbol::EqvImpl(const Expr *other) const {
  return val() == other->GetAsSymbol()->val();
}

// static
Pair *Pair::Create(Expr *car, Expr *cdr, bool readonly) {
  return static_cast<Pair *>(
      gc::Gc::Get().Alloc(sizeof(Pair), [car, cdr, readonly](void *addr) {
        return new(addr) Pair(car, cdr, readonly);
      }));
}

const Pair *Pair::GetAsPair() const {
  return this;
}

Pair *Pair::GetAsPair() {
  return this;
}

// TODO(bcf): Check if its a list, if so print as (a b c ...)
std::ostream &Pair::AppendStream(std::ostream &stream) const {
  return stream << "(" << *car() << " . " << *cdr() << ")";
}

bool Pair::EqvImpl(const Expr *other) const {
  return car() == other->GetAsPair()->car() &&
    cdr() == other->GetAsPair()->cdr();
}

bool Pair::EqualImpl(const Expr *other) const {
  return car()->Equal(other->GetAsPair()->car()) &&
    cdr()->Equal(other->GetAsPair()->cdr());
}

// static
Vector *Vector::Create(const std::vector<Expr *> &vals, bool readonly) {
  return static_cast<Vector *>(
      gc::Gc::Get().Alloc(sizeof(Vector), [vals, readonly](void *addr) {
        return new(addr) Vector(vals, readonly);
      }));
}

Vector::~Vector() {
}

const Vector *Vector::GetAsVector() const {
  return this;
}

Vector *Vector::GetAsVector() {
  return this;
}

std::ostream &Vector::AppendStream(std::ostream &stream) const {
  stream << "#(";

  for (auto e : vals())
    stream << *e << " ";

  return stream << ")";
}

bool Vector::EqvImpl(const Expr *other) const {
  return vals() == other->GetAsVector()->vals();
}

bool Vector::EqualImpl(const Expr *other) const {
  const auto &v1 = vals();
  const auto &v2 = other->GetAsVector()->vals();
  if (v1.size() != v2.size())
    return false;

  for (auto i1 = v1.begin(), i2 = v2.begin(); i1 != v1.end(); ++i1, ++i2) {
    if (!(*i1)->Equal(*i2))
      return false;
  }

  return true;
}

Var::~Var() {
}

const Var *Var::GetAsVar() const {
  return this;
}

std::ostream &Var::AppendStream(std::ostream &stream) const {
  return stream << name();
}

Apply::Apply(const std::vector<Expr *> &exprs)
  : Evals(Type::APPLY, true) {
  assert(exprs.size() > 0);
  op_ = exprs[0];
  args_.reserve(exprs.size() - 1);
  for (std::size_t i = 1; i < exprs.size(); ++i) {
    args_.push_back(exprs[i]);
  }
}

// static
Apply *Apply::Create(const std::vector<Expr *> &exprs) {
  return static_cast<Apply *>(
      gc::Gc::Get().Alloc(sizeof(Apply), [exprs](void *addr) {
        return new(addr) Apply(exprs);
      }));
}

Apply::~Apply() {
}

const Apply *Apply::GetAsApply() const {
  return this;
}

std::ostream &Apply::AppendStream(std::ostream &stream) const {
  stream << "(" << *op();

  for (auto arg : args())
    stream << *arg << " ";

  return stream << ")";
}

Lambda::Lambda(const std::vector<Var *> &required_args, Var *variable_arg,
    const std::vector<Expr *> &body)
  : Expr(Type::LAMBDA, true), required_args_(required_args),
  variable_arg_(variable_arg), body_(body) {
  // TODO(bcf): assert Error checking on body.
}

// static
Lambda *Lambda::Create(const std::vector<Var *> &required_args,
    Var *variable_arg, const std::vector<Expr *> &body) {
  return static_cast<Lambda *>(
      gc::Gc::Get().Alloc(sizeof(Lambda),
        [required_args, variable_arg, body](void *addr) {
          return new(addr) Lambda(required_args, variable_arg, body);
      }));
}

Lambda::~Lambda() {
}

const Lambda *Lambda::GetAsLambda() const {
  return this;
}

std::ostream &Lambda::AppendStream(std::ostream &stream) const {
  stream << "(lambda ";
  if (required_args().size() == 0 &&
      variable_arg() != nullptr) {
    stream << *variable_arg();
  } else {
    stream << "(";
    for (auto arg : required_args())
      stream << *arg << " ";

    if (variable_arg() != nullptr) {
      stream << ". " << *variable_arg();
    }
    stream << ")";
  }

  for (auto e : body())
    stream << *e;

  return stream << ")";
}

// static
Cond *Cond::Create(Expr *test, Expr *true_expr, Expr *false_expr) {
  return static_cast<Cond *>(
      gc::Gc::Get().Alloc(sizeof(Cond),
        [test, true_expr, false_expr](void *addr) {
          return new(addr) Cond(test, true_expr, false_expr);
      }));
}

const Cond *Cond::GetAsCond() const {
  return this;
}

std::ostream &Cond::AppendStream(std::ostream &stream) const {
  stream << "(if " << *test() << " " << *true_expr();

  if (false_expr() != nullptr)
    stream << " " << *false_expr();

  return stream << ")";
}

// static
Assign *Assign::Create(Var *var, Expr *expr) {
  return static_cast<Assign *>(
      gc::Gc::Get().Alloc(sizeof(Assign), [var, expr](void *addr) {
        return new(addr) Assign(var, expr);
      }));
}

const Assign *Assign::GetAsAssign() const {
  return this;
}

std::ostream &Assign::AppendStream(std::ostream &stream) const {
  return stream << "(set! " << *var() << " " << *expr() << ")";
}

// static
LetSyntax *LetSyntax::Create() {
  return static_cast<LetSyntax *>(
      gc::Gc::Get().Alloc(sizeof(LetSyntax), [](void *addr) {
        return new(addr) LetSyntax();
      }));
}

LetSyntax::~LetSyntax() {
}

const LetSyntax *LetSyntax::GetAsLetSyntax() const {
  return this;
}

std::ostream &LetSyntax::AppendStream(std::ostream &stream) const {
  return stream;
}

}  // namespace expr
