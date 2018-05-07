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

#ifndef UTIL_FLAGS_H_
#define UTIL_FLAGS_H_

#include <vector>
#include <string>

namespace util {

class Flags {
 public:
  static constexpr char kDebugMemory[] = "debug-memory";

  // Test mode ignores unrecognized flags
  static void Init(int argc, char** argv, bool test_mode = false);
  static const std::vector<std::string>& Argv();
  static bool IsSet(const std::string& value);

 private:
  Flags() = default;
};

}  // namespace util

#endif  // UTIL_FLAGS_H_
