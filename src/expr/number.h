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

class NumReal;
class NumFloat;

class Number : public Expr {
 public:
  // TODO(bcf): Add support for more types
  enum class Type {
    RATIONAL,
    FLOAT,
  };

  ~Number() override = default;

  // Override from Expr
  const Number* AsNumber() const override { return this; }

  virtual const NumReal* AsNumReal() const { return nullptr; }
  virtual const NumFloat* AsNumFloat() const { return nullptr; }

  Type num_type() const { return num_type_; }
  bool exact() const { return exact_; }

 protected:
  Number(Type num_type, bool exact)
      : Expr(Expr::Type::NUMBER), num_type_(num_type), exact_(exact) {}

 private:
  // Override from Expr
  bool EqvImpl(const Expr* other) const override {
    auto as_num = other->AsNumber();
    return num_type() == as_num->num_type() && NumEqv(as_num);
  }

  virtual bool NumEqv(const Number* other) const = 0;

  Type num_type_;
  bool exact_;
};

// TODO(bcf): Expand this to be arbitrary precision rational
class NumReal : public Number {
 public:
  explicit NumReal(int64_t val) : Number(Type::RATIONAL, true), val_(val) {}
  explicit NumReal(const std::string& str, int radix);

  // Override from Expr
  std::ostream& AppendStream(std::ostream& stream) const override;

  // Override from Number
  const NumReal* AsNumReal() const override { return this; }

  // TODO(bcf): Temp function for testing
  int64_t val() const { return val_; }

 private:
  ~NumReal() override = default;

  // Override from Number
  bool NumEqv(const Number* other) const override {
    return val_ == other->AsNumReal()->val_;
  }

  int64_t val_;
};

class NumFloat : public Number {
 public:
  explicit NumFloat(double val) : Number(Type::FLOAT, false), val_(val) {}
  explicit NumFloat(const std::string& str, int radix);
  ~NumFloat() override = default;

  // Override from Expr
  std::ostream& AppendStream(std::ostream& stream) const override;

  // Override from Number
  const NumFloat* AsNumFloat() const override { return this; }

  // TODO(bcf): Temp function for testing
  double val() const { return val_; }

 private:
  // Override from Number
  bool NumEqv(const Number* other) const override {
    return val_ == other->AsNumFloat()->val_;
  }

  double val_;
};

}  // namespace expr

#endif  // EXPR_NUMBER_H_
