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
#ifndef X
#define X(name)
#endif

// 4.1. Primitive expression types
X(Quote, quote)
X(Lambda, lambda)
X(If, if)
X(Set, set!)

// 4.2 Derived expression types
X(Cond, cond)
X(Case, case)
X(And, and)
X(Or, or)
X(Let, let)
X(LetStar, let*)
X(LetRec, letrec)
X(Begin, begin)
X(Do, do)
X(Delay, delay)
X(Quasiquote, quasiquote)

// 4.3 Macros
X(LetSyntax, let-syntax)
X(LetRecSyntax, letrec-syntax)
X(SyntaxRules, syntax-rules)

// 5.2. Definitions
X(Define, define)

// 5.3. Syntax definitions
X(DefineSyntax, define-syntax)
