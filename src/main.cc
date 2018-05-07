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

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>

#include "repl.h"  // NOLINT(build/include)
#include "expr/expr.h"
#include "eval/eval.h"
#include "parse/parse.h"
#include "util/flags.h"
#include "util/text_stream.h"

int main(int argc, char** argv) {
  util::Flags::Init(argc, argv);
  if (argc <= 1) {
    repl::Start();
    return EXIT_SUCCESS;
  }

  auto env = eval::GetDefaultEnv();

  for (int i = 1; i < argc; ++i) {
    std::ifstream ifs(argv[i]);
    if (!ifs) {
      std::cerr << "Failed to read " << argv[i] << ": " << strerror(errno);
      return EXIT_FAILURE;
    }

    try {
      util::TextStream ts(&ifs, argv[i]);
      for (const auto& expr : parse::Read(ts)) {
        eval::Eval(expr.get(), env.get());
      }
    } catch (std::exception& e) {
      std::cerr << e.what() << "\n";
    }
  }

  return 0;
}
