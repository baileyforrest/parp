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

#ifndef GC_LOCK_H_
#define GC_LOCK_H_

#include <utility>

#include "util/macros.h"

namespace gc {

// This class is used to prevent an expression from being garbage collected.
template <typename T>
class Lock {
 public:
  Lock() = default;

  explicit Lock(T* expr) : expr_(expr) { TakeLock(); }

  Lock(Lock&& other) { *this = std::move(other); }
  Lock(const Lock& other) {
    expr_ = other.expr_;
    TakeLock();
  }

  ~Lock() { ReleaseLock(); }

  void reset(T* expr = nullptr) {
    ReleaseLock();
    expr_ = expr;
    TakeLock();
  }

  Lock& operator=(Lock&& other) {
    ReleaseLock();
    this->expr_ = other.expr_;
    other.expr_ = nullptr;
    return *this;
  }

  Lock& operator=(const Lock& other) {
    ReleaseLock();
    this->expr_ = other.expr_;
    TakeLock();
    return *this;
  }

  T* operator->() const { return expr_; }
  T& operator*() const { return *expr_; }
  T* get() const { return expr_; }
  operator bool() const { return expr_ != nullptr; }

 private:
  void TakeLock() {
    if (expr_) {
      expr_->gc_lock_inc();
    }
  }
  void ReleaseLock() {
    if (expr_) {
      expr_->gc_lock_dec();
    }
  }
  T* expr_ = nullptr;
};

template <typename T, typename... Args>
Lock<T> make_locked(Args&&... args) {
  return Lock<T>(new T(std::forward<Args>(args)...));
}

}  // namespace gc

#endif  //  GC_LOCK_H_
