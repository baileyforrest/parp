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

namespace {

// Right now just run collection every |kCollectionRate| allocations.
constexpr int kCollectionRate = 1000;

}  // namespace

Gc::~Gc() {
  Purge();
}

// static
Gc& Gc::Get() {
  static Gc gc;
  return gc;
}

expr::Symbol* Gc::GetSymbol(const std::string& name) {
  auto it = symbol_name_to_symbol_.find(name);
  if (it != symbol_name_to_symbol_.end()) {
    return it->second;
  }

  auto res = symbol_name_to_symbol_.emplace(name, nullptr);
  assert(res.second);
  auto& inserted_it = res.first;
  inserted_it->second = (new expr::Symbol(&inserted_it->first));
  return inserted_it->second;
}

void* Gc::AllocExpr(std::size_t size) {
  if (debug_mode_) {
    Collect();
  }

  if (alloc_since_last_collection_ >= kCollectionRate) {
    Collect();
  }

  auto* addr = reinterpret_cast<expr::Expr*>(new char[size]);
  exprs_.insert(addr);
  return addr;
}

void Gc::Purge() {
  for (auto* expr : exprs_) {
    DeleteExpr(expr);
  }
  exprs_.clear();
  symbol_name_to_symbol_.clear();
}

void Gc::Collect() {
  alloc_since_last_collection_ = 0;
  for (auto expr : exprs_) {
    if (expr->gc_lock_count_ > 0) {
      expr->GcMark();
    }
  }

  for (auto it = exprs_.begin(); it != exprs_.end();) {
    expr::Expr* expr = *it;
    if (expr->gc_mark_) {
      expr->gc_mark_ = false;
      ++it;
      continue;
    }

    DeleteExpr(expr);
    it = exprs_.erase(it);
  }
}

void Gc::DeleteExpr(expr::Expr* expr) {
  if (auto* as_sym = expr->AsSymbol()) {
    symbol_name_to_symbol_.erase(as_sym->val());
  }

  expr->~Expr();
  delete[] reinterpret_cast<char*>(expr);
}

}  // namespace gc
