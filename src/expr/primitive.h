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
namespace primitive {

// 4.1. Primitive expression types
Primitive* Quote();
Primitive* Lambda();
Primitive* If();
Primitive* Set();

// 4.2 Derived expression types
// TODO(bcf): Implement as lisp expressions?
Primitive* Cond();
Primitive* Case();
Primitive* And();
Primitive* Or();
Primitive* Let();
Primitive* LetStar();
Primitive* LetRec();
Primitive* LetRec();
Primitive* Begin();
Primitive* Do();
Primitive* Delay();
Primitive* Quasiquote();

// 4.3 Macros
Primitive* LetSyntax();
Primitive* LetRecSyntax();
Primitive* SyntaxRules();

// 5.2. Definitions
Primitive* Define();

// 5.3. Syntax definitions
Primitive* DefineSyntax();

// 6.1. Equivalence predicates
Primitive* IsEqv();
Primitive* IsEq();
Primitive* IsEqual();

// 6.2. Numbers
Primitive* IsNumber();
Primitive* IsComplex();
Primitive* IsReal();
Primitive* IsRational();
Primitive* IsInteger();

Primitive* IsExact();
Primitive* IsInexact();

Primitive* OpEq();
Primitive* OpLt();
Primitive* OpGt();
Primitive* OpLe();
Primitive* OpGe();

Primitive* IsZero();
Primitive* IsPositive();
Primitive* IsNegative();
Primitive* IsOdd();
Primitive* IsEven();

Primitive* Max();
Primitive* Min();

Primitive* Plus();
Primitive* Star();

Primitive* Minus();
Primitive* Slash();

Primitive* Abs();

Primitive* Quotient();
Primitive* Remainder();
Primitive* Modulo();

Primitive* Gcd();
Primitive* Lcm();

Primitive* Numerator();
Primitive* Denominator();

Primitive* Floor();
Primitive* Ceiling();
Primitive* Truncate();
Primitive* Round();

Primitive* Rationalize();

Primitive* Exp();
Primitive* Log();
Primitive* Sin();
Primitive* Cos();
Primitive* Tan();
Primitive* Asin();
Primitive* ACos();
Primitive* ATan();

Primitive* Sqrt();

Primitive* Expt();

Primitive* MakeRectangular();
Primitive* MakePolar();
Primitive* RealPart();
Primitive* ImagPart();
Primitive* Magnitude();
Primitive* Angle();

Primitive* ExactToInexact();
Primitive* InexactToExact();

Primitive* NumberToString();
Primitive* StringToNumber();

// 6.3.1. Booleans
Primitive* Not();
Primitive* IsBoolean();

// 6.3.2. Pairs and lists
Primitive* IsPair();
Primitive* Cons();
Primitive* Car();
Primitive* Cdr();
Primitive* SetCar();
Primitive* SetCdr();

// TODO(bcf): (cdddar) upto 4 deep, there are 28 total
Primitive* IsNull();
Primitive* IsList();
Primitive* List();
Primitive* Length();
Primitive* Append();
Primitive* Reverse();
Primitive* ListTail();
Primitive* ListRef();
Primitive* Memq();
Primitive* Memv();
Primitive* Member();
Primitive* Assq();
Primitive* Assv();
Primitive* Assoc();

// 6.3.3. Symbols
Primitive* Symbol();
Primitive* SymbolToString();
Primitive* StringToSymbol();

// 6.3.4. Characters
Primitive* IsChar();
Primitive* IsCharEq();
Primitive* IsCharLt();
Primitive* IsCharGt();
Primitive* IsCharLe();
Primitive* IsCharGe();
Primitive* IsCharCiEq();
Primitive* IsCharCiLt();
Primitive* IsCharCiGt();
Primitive* IsCharCiLe();
Primitive* IsCharCiGe();
Primitive* CharAlphabetic();
Primitive* CharNumeric();
Primitive* CharWhitespace();
Primitive* CharUpperCase();
Primitive* CharLowerCase();
Primitive* CharToInteger();
Primitive* IntegerToChar();
Primitive* CharUpCase();
Primitive* CharDownCase();

// 6.3.5. Strings
Primitive* IsString();
Primitive* MakeString();
Primitive* String();
Primitive* StringLength();
Primitive* StringRef();
Primitive* StringSet();
Primitive* StringEq();
Primitive* StringEqCi();
Primitive* StringLt();
Primitive* StringGt();
Primitive* StringLe();
Primitive* StringGe();
Primitive* StringLtCi();
Primitive* StringGtCi();
Primitive* StringLeCi();
Primitive* StringGeCi();
Primitive* Substring();
Primitive* StringAppend();
Primitive* StringToList();
Primitive* ListToString();
Primitive* StringCopy();
Primitive* StringFill();

// 6.3.6. Vectors
Primitive* IsVector();
Primitive* MakeVector();
Primitive* Vector();
Primitive* VectorLength();
Primitive* VectorRef();
Primitive* VectorSet();
Primitive* VectorToList();
Primitive* ListToVector();
Primitive* VectorFill();

// 6.4. Control features
Primitive* IsProcedure();
Primitive* Map();
Primitive* ForEach();
Primitive* Force();
Primitive* CallWithCurrentContinuation();
Primitive* Values();
Primitive* CallWithValues();
Primitive* DynamicWind();

// 6.5. Eval
Primitive* Eval();
Primitive* SchemeReportEnvironment();
Primitive* NullEnvironment();
Primitive* InteractionEnvironment();

// 6.6. Input and output
// 6.6.1. Ports
Primitive* CallWithInputFile();
Primitive* CallWithOutputFile();
Primitive* IsInputPort();
Primitive* IsOutputPort();
Primitive* CurrentInputPort();
Primitive* CurrentOutputPort();
Primitive* WithInputFromFile();
Primitive* WithOutputToFile();
Primitive* OpenInputFile();
Primitive* OpenOutputFile();
Primitive* CloseInputPort();
Primitive* CloseOutputPort();

// 6.6.2. Input
Primitive* Read();
Primitive* ReadChar();
Primitive* PeekChar();
Primitive* IsEofObject();
Primitive* IsCharReady();

// 6.6.3. Output
Primitive* Write();
Primitive* Display();
Primitive* Newline();
Primitive* WriteChar();
Primitive* Load();
Primitive* TranscriptOn();
Primitive* TranscriptOff();

}  // namespace primitive
}  // namespace expr

#endif  // EXPR_PRIMITIVE_H_
