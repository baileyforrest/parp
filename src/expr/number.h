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

#include "expr/expr.h"

namespace expr {

class Number : public Expr {
 public:
  // TODO(bcf): Add support for more types
  enum class Type {
    RATIONAL,
    FLOAT,
  };

  ~Number() override {};

  // Override from Expr
  Number *GetAsNumber() override { return this; }

  Type num_type() const { return num_type_; }

  bool exact() const { return exact_; }

 protected:
  Number(Type num_type, bool exact)
    : Expr(Expr::Type::LIT_NUM), num_type_(num_type), exact_(exact) {}

 private:
  Type num_type_;
  bool exact_;
};

// TODO(bcf): Expand this to be arbitrary precision rational
class Rational : public Number {
 public:
  explicit Rational(int64_t val) : Number(Type::RATIONAL, true), val_(val) {}
  ~Rational() override {}

  // Override from Expr
  std::size_t size() const override { return sizeof(*this); }

  // TODO(bcf): Temp function for testing
  int64_t val() { return val_; }

 private:
  int64_t val_;
};

class Float : public Number {
 public:
  explicit Float(double val) : Number(Type::FLOAT, false), val_(val) {}
  ~Float() override {}

  // Override from Expr
  std::size_t size() const override { return sizeof(*this); }

  // TODO(bcf): Temp function for testing
  double val() { return val_; }

 private:
  double val_;
};

}  // namespace expr

#endif  // EXPR_NUMBER_H_
