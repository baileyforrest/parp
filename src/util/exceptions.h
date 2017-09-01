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

#ifndef UTIL_EXCEPTIONS_H_
#define UTIL_EXCEPTIONS_H_

#include <exception>
#include <string>

namespace util {

struct Mark;

class SyntaxException : public std::exception {
 public:
  SyntaxException(const std::string& msg, const util::Mark& mark);
  const char* what() const throw() override { return full_msg_.c_str(); }

 private:
  std::string full_msg_;
};

// TODO(bcf): Add marks for this
class RuntimeException : public std::exception {
 public:
  explicit RuntimeException(const std::string& msg) : full_msg_(msg) {}

  const char* what() const throw() override { return full_msg_.c_str(); }

 private:
  std::string full_msg_;
};

}  // namespace util

#endif  // UTIL_EXCEPTIONS_H_
