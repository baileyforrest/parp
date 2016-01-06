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

#ifndef PARSE_PARSE_H_
#define PARSE_PARSE_H_

#include <string>
#include <vector>

#include "util/text_stream.h"
#include "expr/expr.h"

namespace parse {

using ExprVec = std::vector<expr::Expr *>;

// Implementation of read procedure. Parses |stream| into datum
ExprVec Read(util::TextStream &stream);  // NOLINT(runtime/references)
ExprVec Read(const std::string &str, const std::string &filename = "string");

}  // namespace parse

#endif  //  PARSE_PARSE_H_
