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

#include "expr/expr.h"

#include <cstdint>

namespace expr {

class Number : public Expr {
public:
  // TODO: Add support for more types
  enum class Type {
    RATIONAL,
    FLOAT,
  };

  virtual Type type() const = 0;
  virtual bool Exact() const = 0;
};

// TODO: Expand this to be arbitrary precision rational
class Rational : public Number {
public:
  Type type() const override { return Type::RATIONAL; }
  bool Exact() const override { return true; }

  // TODO: Temp function for testing
  int64_t val() { return val_; }

private:
  int64_t val_;
};

class Float : public Number {
public:
  Type type() const override { return Type::FLOAT; }
  bool Exact() const override { return false; }

  // TODO: Temp function for testing
  double val() { return val_; }

private:
  double val_;
};

}  // namespace expr

#endif  // EXPR_NUMBER_H_
