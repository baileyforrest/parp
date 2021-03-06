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

#include <sstream>

#include "expr/expr.h"
#include "util/exceptions.h"
#include "util/mark.h"
#include "util/util.h"

namespace util {

SyntaxException::SyntaxException(const std::string& msg,
                                 const util::Mark* mark) {
  std::ostringstream ss;
  if (mark) {
    ss << *mark << ": ";
  }
  ss << msg;
  full_msg_ = ss.str();
}

RuntimeException::RuntimeException(const std::string& msg,
                                   const expr::Expr* expr)
    : expr_(expr), full_msg_(msg) {
  std::ostringstream os;
  os << msg;
  if (expr) {
    os << " " << *expr;
  }
  full_msg_ = os.str();
}

}  // namespace util
