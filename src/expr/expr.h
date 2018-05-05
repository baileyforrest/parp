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

#ifndef EXPR_EXPR_H_
#define EXPR_EXPR_H_

#include <cassert>
#include <cstddef>
#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gc/gc.h"
#include "util/macros.h"
#include "util/exceptions.h"

namespace expr {

class EmptyList;
class Bool;
class Number;
class Char;
class String;
class Symbol;
class Pair;
class Vector;
class Env;
class Evals;

// TODO(bcf): Define interface to get all references.
class Expr {
 public:
  enum class Type : char {
    // Base types
    EMPTY_LIST,
    BOOL,
    NUMBER,
    CHAR,
    STRING,
    SYMBOL,
    PAIR,
    VECTOR,

    // Special types
    ENV,    // Environment
    EVALS,  // Type which is not self evaluating.
  };

  Type type() const { return type_; }

  bool Eq(const Expr* other) const { return other == this; }
  bool Eqv(const Expr* other) const {
    return Eq(other) || (type_ == other->type_ && EqvImpl(other));
  }
  bool Equal(const Expr* other) const {
    return Eq(other) || (type_ == other->type_ && EqualImpl(other));
  }

  virtual std::ostream& AppendStream(
      std::ostream& stream) const = 0;  // NOLINT(runtime/references)

  virtual const EmptyList* AsEmptyList() const { return nullptr; }
  virtual EmptyList* AsEmptyList() { return nullptr; }
  virtual const Bool* AsBool() const { return nullptr; }
  virtual Bool* AsBool() { return nullptr; }
  virtual const Number* AsNumber() const { return nullptr; }
  virtual Number* AsNumber() { return nullptr; }
  virtual const Char* AsChar() const { return nullptr; }
  virtual Char* AsChar() { return nullptr; }
  virtual const String* AsString() const { return nullptr; }
  virtual String* AsString() { return nullptr; }
  virtual const Symbol* AsSymbol() const { return nullptr; }
  virtual Symbol* AsSymbol() { return nullptr; }
  virtual const Pair* AsPair() const { return nullptr; }
  virtual Pair* AsPair() { return nullptr; }
  virtual const Vector* AsVector() const { return nullptr; }
  virtual Vector* AsVector() { return nullptr; }
  virtual const Env* AsEnv() const { return nullptr; }
  virtual Env* AsEnv() { return nullptr; }
  virtual const Evals* AsEvals() const { return nullptr; }
  virtual Evals* AsEvals() { return nullptr; }

  static void* operator new(std::size_t size) {
    return gc::Gc::Get().AllocExpr(size);
  }

  // Do nothing. Garbage collector will take care of it.
  static void operator delete(void* /* ptr */) {}

 protected:
  explicit Expr(Type type) : type_(type) {}
  virtual ~Expr() = default;

 private:
  friend class gc::Gc;

  virtual bool EqvImpl(const Expr* other) const { return Eq(other); }
  virtual bool EqualImpl(const Expr* other) const { return Eqv(other); }

  // We do now allow allocating arrays.
  static void operator delete[](void* ptr) = delete;
  static void* operator new[](std::size_t size) = delete;

  Type type_;

  DISALLOW_MOVE_COPY_AND_ASSIGN(Expr);
};

const char* TypeToString(Expr::Type type);

inline std::ostream& operator<<(std::ostream& stream, const Expr::Type& type) {
  return stream << TypeToString(type);
}

inline std::ostream& operator<<(std::ostream& stream, const Expr& expr) {
  return expr.AppendStream(stream);
}

inline bool operator==(const Expr& lhs, const Expr& rhs) {
  return lhs.Equal(&rhs);
}

class EmptyList : public Expr {
 public:
  // Expr implementation:
  const EmptyList* AsEmptyList() const override { return this; }
  EmptyList* AsEmptyList() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override {
    return stream << "'()";
  }

 private:
  friend EmptyList* Nil();

  EmptyList() : Expr(Type::EMPTY_LIST) {}
  ~EmptyList() override = default;
};

class Bool : public Expr {
 public:
  bool val() const { return val_; }

  // Expr implementation:
  const Bool* AsBool() const override { return this; }
  Bool* AsBool() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

 private:
  friend Bool* True();
  friend Bool* False();

  explicit Bool(bool val) : Expr(Type::BOOL), val_(val) {}
  ~Bool() override = default;

  const bool val_;
};

class Char : public Expr {
 public:
  using ValType = char;

  explicit Char(ValType val) : Expr(Type::CHAR), val_(val) {}

  // Expr implementation:
  const Char* AsChar() const override { return this; }
  Char* AsChar() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;
  bool EqvImpl(const Expr* other) const override {
    return val_ == other->AsChar()->val_;
  }

  ValType val() const { return val_; }

 private:
  ~Char() override = default;

  const ValType val_;
};

// TODO(bcf): Optimize for readonly strings.
class String : public Expr {
 public:
  explicit String(std::string val, bool read_only = false)
      : Expr(Type::STRING), val_(std::move(val)), read_only_(read_only) {
    val_.shrink_to_fit();
  }

  // Expr implementation:
  const String* AsString() const override { return this; }
  String* AsString() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override {
    return stream << "\"" << val_ << "\"";
  }
  bool EqualImpl(const Expr* other) const override {
    return val_ == other->AsString()->val_;
  }

  void set_val_idx(size_t idx, char c) {
    assert(!read_only_);
    assert(idx < val_.size());
    val_[c] = c;
  }
  const std::string& val() const { return val_; }
  bool read_only() const { return read_only_; }

 private:
  ~String() override = default;

  std::string val_;
  const bool read_only_;
};

class Symbol : public Expr {
 public:
  static Symbol* New(const std::string& val) {
    return gc::Gc::Get().GetSymbol(val);
  }

  ~Symbol() override = default;

  // Expr implementation:
  const Symbol* AsSymbol() const override { return this; }
  Symbol* AsSymbol() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override {
    return stream << val();
  }
  bool EqvImpl(const Expr* other) const override {
    return val_ == other->AsSymbol()->val_;
  }

  const std::string& val() const { return *val_; }

  // Just use default new/delete for symbols
  static void* operator new(std::size_t size) { return new char[size]; }
  static void operator delete(void* ptr) { delete[] static_cast<char*>(ptr); }

 private:
  friend expr::Symbol* gc::Gc::GetSymbol(const std::string& name);

  explicit Symbol(const std::string* val) : Expr(Type::SYMBOL), val_(val) {}

  const std::string* const val_ = nullptr;
};

class Pair : public Expr {
 public:
  Pair(Expr* car, Expr* cdr) : Expr(Type::PAIR), car_(car), cdr_(cdr) {
    assert(car);
    assert(cdr);
  }

  // Expr implementation:
  const Pair* AsPair() const override { return this; }
  Pair* AsPair() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override {
    return stream << "(" << *car_ << " . " << *cdr_ << ")";
  }
  bool EqvImpl(const Expr* other) const override {
    return car_ == other->AsPair()->car_ && cdr_ == other->AsPair()->cdr_;
  }
  bool EqualImpl(const Expr* other) const override {
    return car_->Equal(other->AsPair()->car_) &&
           cdr_->Equal(other->AsPair()->cdr_);
  }

  expr::Expr* Cr(const std::string& str) const;

  Expr* car() const { return car_; }
  Expr* cdr() const { return cdr_; }
  void set_car(Expr* expr) { car_ = expr; }
  void set_cdr(Expr* expr) { cdr_ = expr; }

 private:
  ~Pair() override = default;

  Expr* car_;
  Expr* cdr_;
};

class Vector : public Expr {
 public:
  explicit Vector(std::vector<Expr*> vals)
      : Expr(Type::VECTOR), vals_(std::move(vals)) {
    vals_.shrink_to_fit();
  }

  // Expr implementation:
  const Vector* AsVector() const override { return this; }
  Vector* AsVector() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;
  bool EqvImpl(const Expr* other) const override {
    return vals_ == other->AsVector()->vals_;
  }
  bool EqualImpl(const Expr* other) const override;

  std::vector<Expr*>& vals() { return vals_; }
  const std::vector<Expr*>& vals() const { return vals_; }

 private:
  ~Vector() override = default;

  std::vector<Expr*> vals_;
};

class Env : public Expr {
 public:
  explicit Env(Env* enclosing = nullptr)
      : Expr(Type::ENV), enclosing_(enclosing) {}

  // Expr implementation:
  const Env* AsEnv() const override { return this; }
  Env* AsEnv() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

  Expr* Lookup(Symbol* var) const;
  const Env* enclosing() const { return enclosing_; }
  void DefineVar(Symbol* var, Expr* expr) { map_[var] = expr; }
  void SetVar(Symbol* var, Expr* expr);

 private:
  ~Env() override = default;

  struct VarHash {
    std::size_t operator()(Symbol* var) const {
      return std::hash<std::string>()(var->val());
    }
  };

  struct VarEqual {
    bool operator()(Symbol* lhs, Symbol* rhs) const { return lhs->Eqv(rhs); }
  };

  Env* enclosing_;
  std::unordered_map<Symbol*, Expr*, VarHash, VarEqual> map_;
};

class Evals : public Expr {
 public:
  virtual Expr* DoEval(Env* env, Expr** args, size_t num_args) const = 0;

  // Expr implementation:
  const Evals* AsEvals() const override { return this; }
  Evals* AsEvals() override { return this; }

 protected:
  Evals() : Expr(Type::EVALS) {}
  virtual ~Evals() = default;
};

// Special constants
EmptyList* Nil();
Bool* True();
Bool* False();

// Helpers

std::vector<Expr*> ExprVecFromList(Expr* expr);

template <typename T>
Expr* ListFromIt(T it, T e) {
  if (it == e) {
    return Nil();
  }

  auto* val = *it;
  ++it;
  return Cons(val, ListFromIt(it, e));
}

// Alias for new Pair
inline Pair* Cons(Expr* e1, Expr* e2) {
  return new Pair(e1, e2);
}

#define TRY_AS_IMPL(op, etype)                                               \
  {                                                                          \
    auto* ret = expr->op();                                                  \
    if (!ret) {                                                              \
      std::ostringstream os;                                                 \
      os << "Expected " << Expr::Type::etype << ". Given: " << expr->type(); \
      throw util::RuntimeException(os.str(), expr);                          \
    }                                                                        \
    return ret;                                                              \
  }

// TODO(bcf): Replace similar pattern with these.
// clang-format off
inline EmptyList* TryEmptyList(Expr* expr) TRY_AS_IMPL(AsEmptyList, EMPTY_LIST)
inline Bool* TryBool(Expr* expr) TRY_AS_IMPL(AsBool, BOOL)
inline Number* TryNumber(Expr* expr) TRY_AS_IMPL(AsNumber, NUMBER)
inline Char* TryChar(Expr* expr) TRY_AS_IMPL(AsChar, CHAR)
inline String* TryString(Expr* expr) TRY_AS_IMPL(AsString, STRING)
inline Symbol* TrySymbol(Expr* expr) TRY_AS_IMPL(AsSymbol, SYMBOL)
inline Pair* TryPair(Expr* expr) TRY_AS_IMPL(AsPair, PAIR)
inline Vector* TryVector(Expr* expr) TRY_AS_IMPL(AsVector, VECTOR)
inline Env* TryEnv(Expr* expr) TRY_AS_IMPL(AsEnv, ENV)
inline Evals* TryEvals(Expr* expr) TRY_AS_IMPL(AsEvals, EVALS)
// clang-format on

#undef TRY_AS_IMPL

}  // namespace expr

#endif  // EXPR_EXPR_H_
