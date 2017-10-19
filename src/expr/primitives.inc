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

// Y is used to denote functions which aren't implemented.
#ifndef Y
#define Y(name)
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
Y(Do)
Y(Delay)
Y(Quasiquote)

// 4.3 Macros
Y(LetSyntax)
Y(LetRecSyntax)
Y(SyntaxRules)

// 5.2. Definitions
X(Define, define)

// 5.3. Syntax definitions
Y(DefineSyntax)

// 6.1. Equivalence predicates
X(IsEqv, eqv?)
X(IsEq, eq?)
X(IsEqual, equal?)

// 6.2. Numbers
Y(IsNumber)
Y(IsComplex)
Y(IsReal)
Y(IsRational)
Y(IsInteger)

Y(IsExact)
Y(IsInexact)

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

Y(Max)
Y(Min)

X(Plus, +)
X(Star, *)

X(Minus, -)
X(Slash, /)

Y(Abs)

Y(Quotient)
Y(Remainder)
Y(Modulo)

Y(Gcd)
Y(Lcm)

Y(Numerator)
Y(Denominator)

Y(Floor)
Y(Ceiling)
Y(Truncate)
Y(Round)

Y(Rationalize)

Y(Exp)
Y(Log)
Y(Sin)
Y(Cos)
Y(Tan)
Y(Asin)
Y(ACos)
Y(ATan)

Y(Sqrt)

Y(Expt)

Y(MakeRectangular)
Y(MakePolar)
Y(RealPart)
Y(ImagPart)
Y(Magnitude)
Y(Angle)

Y(ExactToInexact)
Y(InexactToExact)

Y(NumberToString)
Y(StringToNumber)

// 6.3.1. Booleans
Y(Not)
Y(IsBoolean)

// 6.3.2. Pairs and lists
Y(IsPair)
Y(Cons)
Y(Car)
Y(Cdr)
Y(SetCar)
Y(SetCdr)

Y(IsNull)
Y(IsList)
Y(List)
Y(Length)
Y(Append)
Y(Reverse)
Y(ListTail)
Y(ListRef)
Y(Memq)
Y(Memv)
Y(Member)
Y(Assq)
Y(Assv)
Y(Assoc)

// 6.3.3. Symbols
Y(Symbol)
Y(SymbolToString)
Y(StringToSymbol)

// 6.3.4. Characters
Y(IsChar)
Y(IsCharEq)
Y(IsCharLt)
Y(IsCharGt)
Y(IsCharLe)
Y(IsCharGe)
Y(IsCharCiEq)
Y(IsCharCiLt)
Y(IsCharCiGt)
Y(IsCharCiLe)
Y(IsCharCiGe)
Y(CharAlphabetic)
Y(CharNumeric)
Y(CharWhitespace)
Y(CharUpperCase)
Y(CharLowerCase)
Y(CharToInteger)
Y(IntegerToChar)
Y(CharUpCase)
Y(CharDownCase)

// 6.3.5. Strings
Y(IsString)
Y(MakeString)
Y(String)
Y(StringLength)
Y(StringRef)
Y(StringSet)
Y(StringEq)
Y(StringEqCi)
Y(StringLt)
Y(StringGt)
Y(StringLe)
Y(StringGe)
Y(StringLtCi)
Y(StringGtCi)
Y(StringLeCi)
Y(StringGeCi)
Y(Substring)
Y(StringAppend)
Y(StringToList)
Y(ListToString)
Y(StringCopy)
Y(StringFill)

// 6.3.6. Vectors
Y(IsVector)
Y(MakeVector)
Y(Vector)
Y(VectorLength)
Y(VectorRef)
Y(VectorSet)
Y(VectorToList)
Y(ListToVector)
Y(VectorFill)

// 6.4. Control features
Y(IsProcedure)
Y(Map)
Y(ForEach)
Y(Force)
Y(CallWithCurrentContinuation)
Y(Values)
Y(CallWithValues)
Y(DynamicWind)

// 6.5. Eval
Y(Eval)
Y(SchemeReportEnvironment)
Y(NullEnvironment)
Y(InteractionEnvironment)

// 6.6. Input and output
// 6.6.1. Ports
Y(CallWithInputFile)
Y(CallWithOutputFile)
Y(IsInputPort)
Y(IsOutputPort)
Y(CurrentInputPort)
Y(CurrentOutputPort)
Y(WithInputFromFile)
Y(WithOutputToFile)
Y(OpenInputFile)
Y(OpenOutputFile)
Y(CloseInputPort)
Y(CloseOutputPort)

// 6.6.2. Input
Y(Read)
Y(ReadChar)
Y(PeekChar)
Y(IsEofObject)
Y(IsCharReady)

// 6.6.3. Output
Y(Write)
Y(Display)
Y(Newline)
Y(WriteChar)
Y(Load)
Y(TranscriptOn)
Y(TranscriptOff)
