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

#ifndef EXPR_PRIMITIVE_H_
#define EXPR_PRIMITIVE_H_

namespace expr {

class Env;
class Primitive;

namespace primitive {

#define X(name, str) Primitive* name();
#include "expr/primitives.inc"
#undef X

void LoadPrimitives(Env* env);

}  // namespace primitive
}  // namespace expr

#endif  // EXPR_PRIMITIVE_H_
