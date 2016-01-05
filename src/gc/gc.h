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
#include <functional>
#include <utility>
#include <vector>

#include "util/macros.h"

namespace gc {

class Collectable {
 public:
  virtual ~Collectable() {}

  // TODO(bcf): define iterator for pointers to other collectables
};

// TODO(bcf): Implement garbage collection for real
class Gc {
 public:
  static Gc &Get();
  void *Alloc(std::size_t size,
      std::function<Collectable *(void *)> creator);
  void Purge();

 private:
  Gc() = default;
  ~Gc();

  DISALLOW_MOVE_COPY_AND_ASSIGN(Gc);
  std::vector<std::pair<Collectable *, void *>> allocs_;
};

}  // namespace gc

#endif  // GC_GC_H_
