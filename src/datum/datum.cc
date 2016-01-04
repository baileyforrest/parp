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

#include "datum/datum.h"

#include <cassert>

namespace datum {

Bool *Datum::GetAsBool() {
  assert(false);
  return nullptr;
}

Number *Datum::GetAsNumber() {
  assert(false);
  return nullptr;
}

Char *Datum::GetAsChar() {
  assert(false);
  return nullptr;
}

String *Datum::GetAsString() {
  assert(false);
  return nullptr;
}

Symbol *Datum::GetAsSymbol() {
  assert(false);
  return nullptr;
}

Pair *Datum::GetAsPair() {
  assert(false);
  return nullptr;
}

Vector *Datum::GetAsVector() {
  assert(false);
  return nullptr;
}

Bool *Bool::GetAsBool() {
  return this;
}

Char *Char::GetAsChar() {
  return this;
}

String::~String() {
}

String *String::GetAsString() {
  return this;
}

Symbol::~Symbol() {
}

Symbol *Symbol::GetAsSymbol() {
  return this;
}

Pair *Pair::GetAsPair() {
  return this;
}

Vector::~Vector() {
}

Vector *Vector::GetAsVector() {
  return this;
}

}  // namespace datum
