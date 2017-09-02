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

#include <cstdlib>

#include "expr/expr.h"
#include "gc/gc.h"

namespace gc {

Gc::~Gc() {
  Purge();
}

// static
Gc& Gc::Get() {
  static Gc gc;
  return gc;
}

void* Gc::AllocExpr(std::size_t size) {
  auto* addr = reinterpret_cast<expr::Expr*>(std::malloc(size));
  exprs_.push_back(addr);
  return addr;
}

void Gc::Purge() {
  for (auto* expr : exprs_) {
    expr->~Expr();
    std::free(expr);
  }
  exprs_.clear();
}

}  // namespace gc
