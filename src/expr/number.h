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

#ifndef EXPR_NUMBER_H_
#define EXPR_NUMBER_H_

#include <cstdint>
#include <string>

#include "expr/expr.h"

namespace expr {

class Int;
class Float;

// TODO(bcf): Just put Float and Int directly into Expr.
class Number : public Expr {
 public:
  // TODO(bcf): Add support for more types
  enum class Type {
    INT,
    FLOAT,
  };

  virtual const Int* AsInt() const { return nullptr; }
  virtual Int* AsInt() { return nullptr; }
  virtual const Float* AsFloat() const { return nullptr; }
  virtual Float* AsFloat() { return nullptr; }
  // TODO(bcf): Move clone to expr if useful.
  virtual Number* Clone() = 0;

  Type num_type() const { return num_type_; }
  bool exact() const { return num_type_ == Type::INT; }

 protected:
  explicit Number(Type num_type)
      : Expr(Expr::Type::NUMBER), num_type_(num_type) {}

  ~Number() override = default;

 private:
  // Override from Expr
  const Number* AsNumber() const override { return this; }
  Number* AsNumber() override { return this; }
  bool EqvImpl(const Expr* other) const override {
    auto as_num = other->AsNumber();
    return num_type() == as_num->num_type() && NumEqv(as_num);
  }

  virtual bool NumEqv(const Number* other) const = 0;

  Type num_type_;
};

const char* TypeToString(Number::Type type);

inline std::ostream& operator<<(std::ostream& stream,
                                const Number::Type& type) {
  return stream << TypeToString(type);
}

// TODO(bcf): Expand this to be arbitrary precision rational
class Int : public Number {
 public:
  using ValType = int64_t;

  static Int* Parse(const std::string& str, int radix);

  explicit Int(ValType val) : Number(Type::INT), val_(val) {}

  ValType val() const { return val_; }
  void set_val(ValType val) { val_ = val; }

 private:
  // Override from Number
  std::ostream& AppendStream(std::ostream& stream) const override {
    return stream << val_;
  }
  const Int* AsInt() const override { return this; }
  Int* AsInt() override { return this; }
  Number* Clone() override { return new Int(val_); }
  bool NumEqv(const Number* other) const override {
    return val_ == other->AsInt()->val_;
  }

  ~Int() override = default;

  ValType val_;
};

class Float : public Number {
 public:
  using ValType = double;

  static Float* Parse(const std::string& str, int radix);

  explicit Float(ValType val) : Number(Type::FLOAT), val_(val) {}

  ValType val() const { return val_; }
  void set_val(ValType val) { val_ = val; }

 private:
  // Override from Number
  std::ostream& AppendStream(std::ostream& stream) const override {
    return stream << val_;
  }
  const Float* AsFloat() const override { return this; }
  Float* AsFloat() override { return this; }
  Number* Clone() override { return new Float(val_); }
  bool NumEqv(const Number* other) const override {
    return val_ == other->AsFloat()->val_;
  }

  ~Float() override = default;

  ValType val_;
};

inline Int* TryInt(Expr* expr) {
  auto* num = TryNumber(expr);
  auto* ret = num->AsInt();
  if (!ret) {
    std::ostringstream os;
    os << "Expected " << Number::Type::INT << ". Given: " << expr->type();
    throw util::RuntimeException(os.str(), expr);
  }

  return ret;
}

template <template <typename T> class Op>
Number* OpInPlace(Number* target, Number* other) {
  auto* itarget = target->AsInt();
  auto* iother = other->AsInt();

  if (itarget && iother) {
    Op<Int::ValType> op;
    itarget->set_val(op(itarget->val(), iother->val()));
    return itarget;
  }

  Op<Float::ValType> op;
  if (iother) {
    auto* ftarget = target->AsFloat();
    ftarget->set_val(op(ftarget->val(), iother->val()));
    return target;
  }
  auto* ftarget = itarget ? new Float(itarget->val()) : target->AsFloat();
  auto* fother = other->AsFloat();

  ftarget->set_val(op(ftarget->val(), fother->val()));
  return ftarget;
}

template <template <typename T> class Op>
bool OpCmp(Number* a, Number* b) {
  auto* ia = a->AsInt();
  auto* ib = b->AsInt();

  if (ia && ib) {
    Op<Int::ValType> op;
    return op(ia->val(), ib->val());
  }

  Op<Float::ValType> op;
  if (ib) {
    return op(a->AsFloat()->val(), ib->val());
  } else if (ia) {
    return op(ia->val(), b->AsFloat()->val());
  }

  return op(a->AsFloat()->val(), b->AsFloat()->val());
}

}  // namespace expr

#endif  // EXPR_NUMBER_H_
