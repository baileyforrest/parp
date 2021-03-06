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

#ifndef EVAL_EVAL_H_
#define EVAL_EVAL_H_

#include <vector>
#include <string>

#include "expr/expr.h"
#include "gc/lock.h"

namespace eval {

gc::Lock<expr::Expr> Analyze(expr::Expr* expr);
gc::Lock<expr::Expr> Eval(expr::Expr* expr, expr::Env* env);
std::vector<gc::Lock<expr::Expr>> EvalString(
    const std::string& str,
    expr::Env* env,
    const std::string& filename = "string");
gc::Lock<expr::Env> GetDefaultEnv();

}  // namespace eval

#endif  // EVAL_EVAL_H_
