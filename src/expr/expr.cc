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

#include "util/exceptions.h"

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

bool Expr::Eqv(const Expr* other) const {
  return Eq(other) || (type() == other->type() && EqvImpl(other));
}

bool Expr::Equal(const Expr* other) const {
  return Eq(other) || (type() == other->type() && EqualImpl(other));
}

bool Expr::EqvImpl(const Expr* other) const {
  return Eq(other);
}

bool Expr::EqualImpl(const Expr* other) const {
  return Eqv(other);
}

const EmptyList* Expr::GetAsEmptyList() const {
  return nullptr;
}

const Bool* Expr::GetAsBool() const {
  return nullptr;
}

const Number* Expr::GetAsNumber() const {
  return nullptr;
}

const Char* Expr::GetAsChar() const {
  return nullptr;
}

const String* Expr::GetAsString() const {
  return nullptr;
}

String* Expr::GetAsString() {
  return nullptr;
}

const Symbol* Expr::GetAsSymbol() const {
  return nullptr;
}

const Pair* Expr::GetAsPair() const {
  return nullptr;
}

Pair* Expr::GetAsPair() {
  return nullptr;
}

const Vector* Expr::GetAsVector() const {
  return nullptr;
}

Vector* Expr::GetAsVector() {
  return nullptr;
}

const Var* Expr::GetAsVar() const {
  return nullptr;
}

const Apply* Expr::GetAsApply() const {
  return nullptr;
}

const Lambda* Expr::GetAsLambda() const {
  return nullptr;
}

const Cond* Expr::GetAsCond() const {
  return nullptr;
}

const Assign* Expr::GetAsAssign() const {
  return nullptr;
}

const LetSyntax* Expr::GetAsLetSyntax() const {
  return nullptr;
}

const Env* Expr::GetAsEnv() const {
  return nullptr;
}

Env* Expr::GetAsEnv() {
  return nullptr;
}

const Analyzed* Expr::GetAsAnalyzed() const {
  return nullptr;
}

const Primitive* Expr::GetAsPrimitive() const {
  return nullptr;
}

bool Evals::EqvImpl(const Expr* other) const {
  (void)other;
  assert(false && "The evaluation of this expr should be compared instead");
}

EmptyList* Nil() {
  static EmptyList empty_list;
  return &empty_list;
}

const EmptyList* EmptyList::GetAsEmptyList() const {
  return this;
}

std::ostream& EmptyList::AppendStream(std::ostream& stream) const {
  return stream << "'()";
}

const Bool* Bool::GetAsBool() const {
  return this;
}

std::ostream& Bool::AppendStream(std::ostream& stream) const {
  return val_ ? (stream << "#t") : (stream << "#f");
}

bool Bool::EqvImpl(const Expr* other) const {
  return val_ == other->GetAsBool()->val_;
}

Bool* True() {
  static Bool true_val(true);
  return &true_val;
}

Bool* False() {
  static Bool false_val(false);
  return &false_val;
}

// static
Char* Char::Create(char val) {
  return static_cast<Char*>(
      gc::Gc::Get().Alloc(sizeof(Char), [val](void* addr) {
        return new (addr) Char(val);
      }));  // NOLINT(whitespace/newline)
}

const Char* Char::GetAsChar() const {
  return this;
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

bool Char::EqvImpl(const Expr* other) const {
  return val_ == other->GetAsChar()->val_;
}

// static
String* String::Create(const std::string& val, bool readonly) {
  return static_cast<String*>(
      gc::Gc::Get().Alloc(sizeof(String), [val, readonly](void* addr) {
        return new (addr) String(val, readonly);
      }));
}

String::~String() = default;

const String* String::GetAsString() const {
  return this;
}

std::ostream& String::AppendStream(std::ostream& stream) const {
  return stream << "\"" << val_ << "\"";
}

String* String::GetAsString() {
  return this;
}

bool String::EqualImpl(const Expr* other) const {
  return val_ == other->GetAsString()->val_;
}

// static
Symbol* Symbol::Create(const std::string& val) {
  return static_cast<Symbol*>(
      gc::Gc::Get().Alloc(sizeof(Symbol), [val](void* addr) {
        return new (addr) Symbol(val);
      }));  // NOLINT(whitespace/newline)
}

Symbol::~Symbol() = default;

const Symbol* Symbol::GetAsSymbol() const {
  return this;
}

std::ostream& Symbol::AppendStream(std::ostream& stream) const {
  return stream << val_;
}

bool Symbol::EqvImpl(const Expr* other) const {
  return val_ == other->GetAsSymbol()->val_;
}

// static
Pair* Pair::Create(Expr* car, Expr* cdr, bool readonly) {
  return static_cast<Pair*>(
      gc::Gc::Get().Alloc(sizeof(Pair), [car, cdr, readonly](void* addr) {
        return new (addr) Pair(car, cdr, readonly);
      }));
}

const Pair* Pair::GetAsPair() const {
  return this;
}

Pair* Pair::GetAsPair() {
  return this;
}

// TODO(bcf): Check if its a list, if so print as (a b c ...)
std::ostream& Pair::AppendStream(std::ostream& stream) const {
  return stream << "(" << *car_ << " . " << *cdr_ << ")";
}

bool Pair::EqvImpl(const Expr* other) const {
  return car_ == other->GetAsPair()->car_ && cdr_ == other->GetAsPair()->cdr_;
}

bool Pair::EqualImpl(const Expr* other) const {
  return car_->Equal(other->GetAsPair()->car_) &&
         cdr_->Equal(other->GetAsPair()->cdr_);
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

        auto* pair = cexpr->GetAsPair();
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
Vector* Vector::Create(const std::vector<Expr*>& vals, bool readonly) {
  return static_cast<Vector*>(
      gc::Gc::Get().Alloc(sizeof(Vector), [vals, readonly](void* addr) {
        return new (addr) Vector(vals, readonly);
      }));
}

Vector::~Vector() = default;

const Vector* Vector::GetAsVector() const {
  return this;
}

Vector* Vector::GetAsVector() {
  return this;
}

std::ostream& Vector::AppendStream(std::ostream& stream) const {
  stream << "#(";

  for (auto* e : vals_)
    stream << *e << " ";

  return stream << ")";
}

bool Vector::EqvImpl(const Expr* other) const {
  return vals_ == other->GetAsVector()->vals_;
}

bool Vector::EqualImpl(const Expr* other) const {
  const auto& v1 = vals_;
  const auto& v2 = other->GetAsVector()->vals_;
  if (v1.size() != v2.size())
    return false;

  for (auto i1 = v1.begin(), i2 = v2.begin(); i1 != v1.end(); ++i1, ++i2) {
    if (!(*i1)->Equal(*i2))
      return false;
  }

  return true;
}

// static
Var* Var::Create(const std::string& name) {
  return static_cast<Var*>(gc::Gc::Get().Alloc(sizeof(Var), [name](void* addr) {
    return new (addr) Var(name);
  }));  // NOLINT(whitespace/newline)
}

Var::~Var() = default;

const Var* Var::GetAsVar() const {
  return this;
}

std::ostream& Var::AppendStream(std::ostream& stream) const {
  return stream << name_;
}

Apply::Apply(const std::vector<Expr*>& exprs) : Evals(Type::APPLY, true) {
  assert(exprs.size() > 0);
  op_ = exprs[0];
  args_.reserve(exprs.size() - 1);
  for (std::size_t i = 1; i < exprs.size(); ++i) {
    args_.push_back(exprs[i]);
  }
}

// static
Apply* Apply::Create(const std::vector<Expr*>& exprs) {
  return static_cast<Apply*>(
      gc::Gc::Get().Alloc(sizeof(Apply), [exprs](void* addr) {
        return new (addr) Apply(exprs);
      }));  // NOLINT(whitespace/newline)
}

Apply::~Apply() = default;

const Apply* Apply::GetAsApply() const {
  return this;
}

std::ostream& Apply::AppendStream(std::ostream& stream) const {
  stream << "(" << *op_;

  for (auto* arg : args_)
    stream << *arg << " ";

  return stream << ")";
}

Lambda::Lambda(const std::vector<const Symbol*>& required_args,
               const Symbol* variable_arg,
               Expr* body,
               Env* env)
    : Expr(Type::LAMBDA, true),
      required_args_(required_args),
      variable_arg_(variable_arg),
      body_(body),
      env_(env) {
  // TODO(bcf): assert Error checking on body.
}

// static
Lambda* Lambda::Create(const std::vector<const Symbol*>& required_args,
                       const Symbol* variable_arg,
                       Expr* body,
                       Env* env) {
  return static_cast<Lambda*>(gc::Gc::Get().Alloc(
      sizeof(Lambda), [required_args, variable_arg, body, env](void* addr) {
        return new (addr) Lambda(required_args, variable_arg, body, env);
      }));
}

Lambda::~Lambda() = default;

const Lambda* Lambda::GetAsLambda() const {
  return this;
}

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

  stream << *body_;
  return stream << ")";
}

// static
Cond* Cond::Create(Expr* test, Expr* true_expr, Expr* false_expr) {
  return static_cast<Cond*>(gc::Gc::Get().Alloc(
      sizeof(Cond), [test, true_expr, false_expr](void* addr) {
        return new (addr) Cond(test, true_expr, false_expr);
      }));
}

const Cond* Cond::GetAsCond() const {
  return this;
}

std::ostream& Cond::AppendStream(std::ostream& stream) const {
  stream << "(if " << *test_ << " " << *true_expr_;

  if (false_expr_) {
    stream << " " << *false_expr_;
  }

  return stream << ")";
}

// static
Assign* Assign::Create(Var* var, Expr* expr) {
  return static_cast<Assign*>(
      gc::Gc::Get().Alloc(sizeof(Assign), [var, expr](void* addr) {
        return new (addr) Assign(var, expr);
      }));  // NOLINT(whitespace/newline)
}

const Assign* Assign::GetAsAssign() const {
  return this;
}

std::ostream& Assign::AppendStream(std::ostream& stream) const {
  return stream << "(set! " << *var_ << " " << *expr_ << ")";
}

// static
LetSyntax* LetSyntax::Create() {
  return static_cast<LetSyntax*>(
      gc::Gc::Get().Alloc(sizeof(LetSyntax), [](void* addr) {
        return new (addr) LetSyntax();
      }));  // NOLINT(whitespace/newline)
}

LetSyntax::~LetSyntax() = default;

const LetSyntax* LetSyntax::GetAsLetSyntax() const {
  return this;
}

std::ostream& LetSyntax::AppendStream(std::ostream& stream) const {
  return stream;
}

// static
Env* Env::Create(const std::vector<std::pair<const Symbol*, Expr*>>& vars,
                 Env* enclosing,
                 bool readonly) {
  return static_cast<Env*>(
      gc::Gc::Get().Alloc(sizeof(Env), [vars, enclosing, readonly](void* addr) {
        return new (addr) Env(vars, enclosing, readonly);
      }));
}

Env::~Env() = default;

const Env* Env::GetAsEnv() const {
  return this;
}

Env* Env::GetAsEnv() {
  return this;
}

std::ostream& Env::AppendStream(std::ostream& stream) const {
  stream << "{";
  for (const auto& pair : map_)
    stream << "(" << *pair.first << ", " << *pair.second << ")";

  return stream << "}";
}

void Env::ThrowUnboundException(const Symbol* var) const {
  std::ostringstream os;
  os << *var;
  throw util::RuntimeException("Attempt to reference unbound variable: " +
                               os.str());
}

std::size_t Env::VarHash::operator()(const Symbol* var) const {
  std::hash<std::string> hash;
  return hash(var->val());
}

Expr* Env::Lookup(const Symbol* var) const {
  auto* env = this;
  while (env != nullptr) {
    auto search = env->map_.find(var);
    if (search != env->map_.end())
      return search->second;

    env = env->enclosing_;
  }

  // TODO(bcf): Should we throw here?
  ThrowUnboundException(var);
  return nullptr;
}

void Env::DefineVar(const Symbol* var, Expr* expr) {
  map_[var] = expr;
}

void Env::SetVar(const Symbol* var, Expr* expr) {
  auto* env = this;
  while (env != nullptr) {
    auto search = env->map_.find(var);
    if (search != env->map_.end()) {
      search->second = expr;
      return;
    }

    env = env->enclosing_;
  }

  // TODO(bcf): Should we throw here?
  ThrowUnboundException(var);
}

// static
Analyzed* Analyzed::Create(const std::function<Expr*(Env*)>& func,
                           const std::vector<const Expr*>& refs) {
  return static_cast<Analyzed*>(
      gc::Gc::Get().Alloc(sizeof(Analyzed), [func, refs](void* addr) {
        return new (addr) Analyzed(func, refs);
      }));  // NOLINT(whitespace/newline)
}

Analyzed::~Analyzed() = default;

const Analyzed* Analyzed::GetAsAnalyzed() const {
  return this;
}

// TODO(bcf): possibly hold ref to original expression? (Is memory worth it)
std::ostream& Analyzed::AppendStream(std::ostream& stream) const {
  return stream << "Analyzed expression";
}

}  // namespace expr
