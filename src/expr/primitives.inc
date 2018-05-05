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
X(Arrow, =>) // Not defined in the standard, used for cond
X(Else, else) // Not defined in the standard, used for cond
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

// 6.1. Equivalence predicates
X(IsEqv, eqv?)
X(IsEq, eq?)
X(IsEqual, equal?)

// 6.2. Numbers
X(IsNumber, number?)
X(IsComplex, complex?)
X(IsReal, real?)
X(IsRational, rational?)
X(IsInteger, integer?)

X(IsExact, exact?)
X(IsInexact, inexact?)

X(OpEq, =)
X(OpLt, <)
X(OpGt, >)
X(OpLe, <=)
X(OpGe, >=)

X(IsZero, zero?)
X(IsPositive, positive?)
X(IsNegative, negative?)
X(IsOdd, odd?)
X(IsEven, even?)

X(Max, min)
X(Min, max)

X(Plus, +)
X(Star, *)

X(Minus, -)
X(Slash, /)

X(Abs, abs)

X(Quotient, quotient)
X(Remainder, remainder)
X(Modulo, modulo)

X(Gcd, gcd)
X(Lcm, lcm)

X(Numerator, numerator)
X(Denominator, denominator)

X(Floor, floor)
X(Ceiling, ceiling)
X(Truncate, truncate)
X(Round, round)

X(Rationalize, rationalize)

X(Exp, exp)
X(Log, log)
X(Sin, sin)
X(Cos, cos)
X(Tan, tan)
X(Asin, asin)
X(ACos, acos)
X(ATan, atan)

X(Sqrt, sqrt)

X(Expt, expt)

X(MakeRectangular, make-rectangular)
X(MakePolar, make-polar)
X(RealPart, real-part)
X(ImagPart, imag-part)
X(Magnitude, magnitude)
X(Angle, angle)

X(ExactToInexact, exact->inexact)
X(InexactToExact, inexact->exact)

X(NumberToString, number->string)
X(StringToNumber, string->number)

// 6.3.1. Booleans
X(Not, not)
X(IsBoolean, boolean?)

// 6.3.2. Pairs and lists
X(IsPair, pair?)
X(Cons, cons)
X(Car, car)
X(Cdr, cdr)
X(SetCar, set-car!)
X(SetCdr, set-cdr!)

X(IsNull, null?)
X(IsList, list?)
X(List, list?)
X(Length, length)
X(Append, append)
X(Reverse, reverse)
X(ListTail, list-tail)
X(ListRef, list-ref)
X(Memq, memq)
X(Memv, memv)
X(Member, member)
X(Assq, assq)
X(Assv, assv)
X(Assoc, assoc)

// 6.3.3. Symbols
X(SymbolPrim, symbol)
X(SymbolToString, symbol-to-string)
X(StringToSymbol, string-to-symbol)

// 6.3.4. Characters
X(IsChar, char?)
X(IsCharEq, char=?)
X(IsCharLt, char<?)
X(IsCharGt, char>?)
X(IsCharLe, char<=?)
X(IsCharGe, char>=?)
X(IsCharCiEq, char-ci=?)
X(IsCharCiLt, char-ci<?)
X(IsCharCiGt, char-ci>?)
X(IsCharCiLe, char-ci<=?)
X(IsCharCiGe, char-ci>=?)
X(IsCharAlphabetic, char-alphabetic?)
X(IsCharNumeric, char-numeric?)
X(IsCharWhitespace, char-whitespace?)
X(IsCharUpperCase, char-uppercase?)
X(IsCharLowerCase, char-lowercase?)
X(CharToInteger, char-to-integer)
X(IntegerToChar, integer-to-char)
X(CharUpCase, char-upcase)
X(CharDownCase, char-downcase)

// 6.3.5. Strings
X(IsString, string?)
X(MakeString, make-string)
X(String, string)
X(StringLength, string-length)
X(StringRef, string-ref)
X(StringSet, string-set)
X(IsStringEq, string=?)
X(IsStringEqCi, string-ci=?)
X(IsStringLt, string<?)
X(IsStringGt, string>?)
X(IsStringLe, string<=?)
X(IsStringGe, string>=?)
X(IsStringLtCi, string-ci<)
X(IsStringGtCi, string-ci>)
X(IsStringLeCi, string-ci<=)
X(IsStringGeCi, string-ci>=)
X(Substring, substring)
X(StringAppend, string-append)
X(StringToList, string-to-list)
X(ListToString, list-to-string)
X(StringCopy, string-copy)
X(StringFill, string-fill)

// 6.3.6. Vectors
X(IsVector, vector?)
X(MakeVector, make-vector)
X(Vector, vector)
X(VectorLength, vector-length)
X(VectorRef, vector-ref)
X(VectorSet, vector-set!)
X(VectorToList, vector-to-list)
X(ListToVector, list-to-vector)
X(VectorFill, vector-fill)

// 6.4. Control features
X(IsProcedure, procedure?)
X(Map, map)
X(ForEach, for-each)
X(Force, force)
X(CallWithCurrentContinuation, call-with-current-continuation)
X(Values, values)
X(CallWithValues, call-with-values)
X(DynamicWind, dynamic-wind)

// 6.5. Eval
X(EvalPrim, eval)
X(SchemeReportEnvironment, scheme-report-environment)
X(NullEnvironment, null-environment)
X(InteractionEnvironment, interaction-environment)

// 6.6. Input and output
// 6.6.1. Ports
X(CallWithInputFile, call-with-input-file)
X(CallWithOutputFile, call-withoutput-file)
X(IsInputPort, input-port?)
X(IsOutputPort, output-port?)
X(CurrentInputPort, current-input-port)
X(CurrentOutputPort, current-output-port)
X(WithInputFromFile, with-input-from-file)
X(WithOutputToFile, with-output-to-file)
X(OpenInputFile, open-input-file)
X(OpenOutputFile, open-output-file)
X(CloseInputPort, close-input-port)
X(CloseOutputPort, close-output-port)

// 6.6.2. Input
X(Read, read)
X(ReadChar, read-char)
X(PeekChar, peek-char)
X(IsEofObject, eof-object?)
X(IsCharReady, char-ready?)

// 6.6.3. Output
X(Write, write)
X(Display, display)
X(Newline, newline)
X(WriteChar, write-char)
X(Load, load)
X(TranscriptOn, transcript-on)
X(TranscriptOff, transcript-off)
