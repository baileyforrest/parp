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

// TODO(bcf): Expand this to be arbitrary precision rational
class Int : public Number {
 public:
  explicit Int(int64_t val) : Number(Type::INT), val_(val) {}
  explicit Int(const std::string& str, int radix);

  int64_t val() const { return val_; }
  void set_val(int64_t val) { val_ = val; }

 private:
  // Override from Number
  std::ostream& AppendStream(std::ostream& stream) const override;
  const Int* AsInt() const override { return this; }
  Int* AsInt() override { return this; }
  Number* Clone() override { return new Int(val_); }
  bool NumEqv(const Number* other) const override {
    return val_ == other->AsInt()->val_;
  }

  ~Int() override = default;

  int64_t val_;
};

class Float : public Number {
 public:
  explicit Float(double val) : Number(Type::FLOAT), val_(val) {}
  explicit Float(const std::string& str, int radix);

  double val() const { return val_; }
  void set_val(double val) { val_ = val; }

 private:
  // Override from Number
  std::ostream& AppendStream(std::ostream& stream) const override;
  const Float* AsFloat() const override { return this; }
  Float* AsFloat() override { return this; }
  Number* Clone() override { return new Float(val_); }
  bool NumEqv(const Number* other) const override {
    return val_ == other->AsFloat()->val_;
  }

  ~Float() override = default;

  double val_;
};

template <typename OpInt, typename OpFloat>
Number* OpInPlace(Number* target, Number* other) {
  auto* itarget = target->AsInt();
  auto* iother = other->AsInt();

  if (itarget && iother) {
    OpInt op;
    itarget->set_val(op(itarget->val(), iother->val()));
    return itarget;
  }

  OpFloat op;
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

}  // namespace expr

#endif  // EXPR_NUMBER_H_
