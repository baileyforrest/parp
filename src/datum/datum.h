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

#ifndef DATUM_DATUM_H_
#define DATUM_DATUM_H_

#include <string>
#include <vector>

namespace datum {

class Bool;
class Number;
class Char;
class String;
class Symbol;
class Pair;
class Vector;

class Datum {
 public:
  enum class Type {
    BOOL,
    NUMBER,
    CHAR,
    STRING,
    SYMBOL,
    PAIR,
    VECTOR,
  };

  virtual ~Datum() {}

  Type type() const { return type_; }

  virtual Bool *GetAsBool();
  virtual Number *GetAsNumber();
  virtual Char *GetAsChar();
  virtual String *GetAsString();
  virtual Symbol *GetAsSymbol();
  virtual Pair *GetAsPair();
  virtual Vector *GetAsVector();

 protected:
  explicit Datum(Type type) : type_(type) {}

 private:
  Type type_;
};

class Bool : public Datum {
 public:
  explicit Bool(bool val) : Datum(Type::BOOL), val_(val) {}
  ~Bool() override {}

  // Override from Datum
  Bool *GetAsBool() override;

  bool val() const { return val_; }

 private:
  bool val_;
};

class Char : public Datum {
 public:
  explicit Char(char val) : Datum(Type::CHAR), val_(val) {}
  ~Char() override {}

  // Override from Datum
  Char *GetAsChar() override;

  char val() const { return val_; }

 private:
  char val_;
};

class String : public Datum {
 public:
  explicit String(const std::string &val) : Datum(Type::STRING), val_(val) {}
  ~String() override;

  // Override from Datum
  String *GetAsString() override;

  const std::string &val() const { return val_; }

 private:
  std::string val_;
};

class Symbol : public Datum {
 public:
  explicit Symbol(const std::string &val) : Datum(Type::STRING), val_(val) {}
  ~Symbol() override;

  // Override from Datum
  Symbol *GetAsSymbol() override;

  const std::string &val() const { return val_; }

 private:
  std::string val_;
};

class Pair : public Datum {
 public:
  explicit Pair(Datum *car, Datum *cdr)
    : Datum(Type::STRING), car_(car), cdr_(cdr) {}
  ~Pair() override {}

  // Override from Datum
  Pair *GetAsPair() override;

  const Datum *car() { return car_; }
  const Datum *cdr() { return cdr_; }

 private:
  Datum *car_;
  Datum *cdr_;
};

class Vector : public Datum {
 public:
  explicit Vector(const std::vector<Datum *> &vals)
    : Datum(Type::STRING), vals_(vals) {}
  ~Vector() override;

  // Override from Datum
  Vector *GetAsVector() override;

  const std::vector<Datum *> &vals() const { return vals_; }

 private:
  std::vector<Datum *> vals_;
};

} // namespace datum

#endif  // DATUM_DATUM_H_
