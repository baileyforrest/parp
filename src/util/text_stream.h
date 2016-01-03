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
/**
 * Encapsulation of character stream which keeps track of position in file
 */

#ifndef UTIL_TEXT_STREAM_H_
#define UTIL_TEXT_STREAM_H_

#include <string>
#include <istream>

#include "util/mark.h"
#include "util/macros.h"

namespace util {

class TextStream {
public:
  explicit TextStream(std::istream &istream, const std::string *file_name);
  ~TextStream();

  int Get();
  int Peek() const;
  bool Eof() const { return istream_.eof(); }

  const Mark &mark() const { return mark_; }

private:
  DISALLOW_MOVE_COPY_AND_ASSIGN(TextStream);

  std::istream &istream_;
  std::ios_base::iostate istream_except_mask_;
  Mark mark_;
};

}  // namespace util

#endif  //  UTIL_TEXT_STREAM_H_
