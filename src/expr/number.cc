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

namespace expr {

// static
NumRational *NumRational::Create(int64_t val) {
  return static_cast<NumRational *>(
      gc::Gc::Get().Alloc(sizeof(NumRational), [val](void *addr) {
        return new(addr) NumRational(val);
      }));
}

// static
NumFloat *NumFloat::Create(double val) {
  return static_cast<NumFloat *>(
      gc::Gc::Get().Alloc(sizeof(NumFloat), [val](void *addr) {
        return new(addr) NumFloat(val);
      }));
}

}  // namespace expr
