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

#include "repl.h"  // NOLINT(build/include)

#include <readline/readline.h>
#include <readline/history.h>

#include <cstdlib>
#include <iostream>

#include "eval/eval.h"

namespace repl {

namespace {

static constexpr char kPrompt[] = "> ";

}  // namespace

void Start() {
  // Configure readline to auto-complete paths when the tab key is hit.
  rl_bind_key('\t', rl_complete);

  auto env = eval::GetDefaultEnv();

  while (true) {
    char* input = readline(kPrompt);
    if (!input) {
      break;
    }
    add_history(input);
    auto exprs = eval::EvalString(input, env.get(), "repl");
    if (!exprs.empty()) {
      std::cout << *exprs.back() << "\n";
    }

    free(input);
  }
}

}  // namespace repl
