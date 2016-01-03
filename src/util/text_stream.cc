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

#include "util/text_stream.h"

#include <cassert>
#include <iostream>

namespace util {

TextStream::TextStream(std::istream *istream, const std::string *file_name)
  : istream_(istream),
    istream_except_mask_(istream_->exceptions()) {
  // Enable all the exceptions on istream
  istream_->exceptions(std::istream::badbit | std::istream::failbit);
  mark_ = { file_name, 1, 1 };
}

TextStream::~TextStream() {
  // Restore istream except mask
  istream_->exceptions(istream_except_mask_);
}

int TextStream::Get() {
  assert(!Eof());
  int c;
  try {
    c = istream_->get();
  } catch (std::ios_base::failure &fail) {
    std::cerr << "I/O error reading " << mark_ << std::endl;
    throw;
  }
  if (c == '\n') {
    ++mark_.line;
    mark_.col = 1;
  } else {
    ++mark_.col;
  }

  return c;
}

int TextStream::Peek() const {
  assert(!Eof());
  return istream_->peek();
}

bool TextStream::Eof() const {
  // Next character may be EOF before eof bit is set
  return istream_->eof() ||
      istream_->peek() == std::istream::traits_type::eof();
}

}  // namespace util
