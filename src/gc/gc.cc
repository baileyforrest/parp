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

#include "gc/gc.h"

#include <cstdlib>

namespace gc {

Gc::~Gc() {
  Purge();
}

// static
Gc &Gc::Get() {
  static Gc gc;
  return gc;
}

void *Gc::Alloc(std::size_t size,
    std::function<Collectable *(void *)> creator) {
  void *addr = std::malloc(size);
  allocs_.emplace_back(creator(addr), addr);

  return addr;
}

void Gc::Purge() {
  for (const auto &pair : allocs_) {
    pair.first->~Collectable();
    std::free(pair.second);
  }
  allocs_.clear();
}

}  // namespace gc
