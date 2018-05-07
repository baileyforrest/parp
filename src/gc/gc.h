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

#ifndef GC_GC_H_
#define GC_GC_H_

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "util/macros.h"

namespace expr {
class Expr;
class Symbol;
}  // namespace expr

namespace gc {

// TODO(bcf): Implement garbage collection for real
class Gc {
 public:
  static Gc& Get();

  expr::Symbol* GetSymbol(const std::string& name);
  void* AllocExpr(std::size_t size);
  void Purge();
  void Collect();
  size_t NumObjects() { return exprs_.size(); }

  // If true, will collect on every single allocation.
  void set_debug_mode(bool debug_mode) { debug_mode_ = debug_mode; }

 private:
  Gc() = default;
  ~Gc();

  void DeleteExpr(expr::Expr* expr);

  bool debug_mode_ = false;

  // Number of allocations since last collection.
  size_t alloc_since_last_collection_ = 0;

  std::unordered_map<std::string, expr::Symbol*> symbol_name_to_symbol_;
  std::unordered_set<expr::Expr*> exprs_;

  DISALLOW_MOVE_COPY_AND_ASSIGN(Gc);
};

}  // namespace gc

#endif  // GC_GC_H_
