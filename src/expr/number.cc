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

#include "expr/number.h"

#include <cassert>
#include <stdexcept>
#include <string>

namespace expr {

namespace {

/**
 *Wraps stoll throwing an exception if the whole string isn't consumed.
 */
int64_t stoi64_whole(const std::string& str, int radix) {
  std::size_t pos;
  int64_t i = std::stoll(str, &pos, radix);
  if (pos != str.size())
    throw std::invalid_argument("Invalid trailing characters: " +
                                str.substr(pos));

  return i;
}

}  // namespace

const Number* Number::GetAsNumber() const {
  return this;
}

const NumReal* Number::GetAsNumReal() const {
  assert(false);
  return nullptr;
}

const NumFloat* Number::GetAsNumFloat() const {
  assert(false);
  return nullptr;
}

const NumReal* NumReal::GetAsNumReal() const {
  return this;
}

bool Number::EqvImpl(const Expr* other) const {
  auto as_num = other->GetAsNumber();
  return num_type() == as_num->num_type() && NumEqv(as_num);
}

// static
NumReal* NumReal::Create(int64_t val) {
  return static_cast<NumReal*>(
      gc::Gc::Get().Alloc(sizeof(NumReal), [val](void* addr) {
        return new (addr) NumReal(val);
      }));  // NOLINT(whitespace/newline)
}

// static
NumReal* NumReal::Create(const std::string& str, int radix) {
  return NumReal::Create(static_cast<int64_t>(stoi64_whole(str, radix)));
}

std::ostream& NumReal::AppendStream(std::ostream& stream) const {
  return stream << val();
}

bool NumReal::NumEqv(const Number* other) const {
  return val() == other->GetAsNumReal()->val();
}

const NumFloat* NumFloat::GetAsNumFloat() const {
  return this;
}

// static
NumFloat* NumFloat::Create(double val) {
  return static_cast<NumFloat*>(
      gc::Gc::Get().Alloc(sizeof(NumFloat), [val](void* addr) {
        return new (addr) NumFloat(val);
      }));  // NOLINT(whitespace/newline)
}

// static
NumFloat* NumFloat::Create(const std::string& str, int radix) {
  double d;
  if (radix == 10) {
    std::size_t pos;
    d = stod(str, &pos);
    if (pos != str.size())
      throw std::invalid_argument("Invalid trailing characters: " +
                                  str.substr(pos));
  } else {
    d = stoi64_whole(str, radix);
  }

  return NumFloat::Create(d);
}

std::ostream& NumFloat::AppendStream(std::ostream& stream) const {
  return stream << val();
}

bool NumFloat::NumEqv(const Number* other) const {
  return val() == other->GetAsNumFloat()->val();
}

}  // namespace expr
