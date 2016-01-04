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

// TODO(bcf): consider restricting copy/move operators, and how that relates to
// garbage collection

// TODO(bcf): Consider making everything immutable and associated performance
// costs

#ifndef EXPR_EXPR_H_
#define EXPR_EXPR_H_

#include <cstddef>
#include <string>
#include <vector>

#include "gc/gc.h"

namespace expr {

class Bool;
class Number;
class Char;
class String;
class Symbol;
class Pair;
class Vector;
class Var;
class Literal;
class Apply;
class Lambda;
class Cond;
class Assign;
class LetSyntax;

class Expr : public gc::Collectable{
 public:
  enum class Type {
    // Expr values
    BOOL,
    NUMBER,
    CHAR,
    STRING,
    SYMBOL,
    PAIR,
    VECTOR,

    VAR,           // Variable
    LITERAL,       // literal
    APPLY,         // procure call/macro use
    LAMBDA,        // (lambda <formals> <body>)
    COND,          // (if <test> <consequent> <alternate>)
    ASSIGN,        // (!set <variable> <expression>)
    LET_SYNTAX,    // (let{rec}-syntax (<syntax spec>*) <body>)
  };

  virtual ~Expr() {}

  Type type() const { return type_; }
  bool IsDatum() const;

  virtual Bool *GetAsBool();
  virtual Number *GetAsNumber();
  virtual Char *GetAsChar();
  virtual String *GetAsString();
  virtual Symbol *GetAsSymbol();
  virtual Pair *GetAsPair();
  virtual Vector *GetAsVector();
  virtual Var *GetAsVar();
  virtual Literal *GetAsLiteral();
  virtual Apply *GetAsApply();
  virtual Lambda *GetAsLambda();
  virtual Cond *GetAsCond();
  virtual Assign *GetAsAssign();
  virtual LetSyntax *GetAsLetSyntax();

 protected:
  explicit Expr(Type type) : type_(type) {}

 private:
  Type type_;
};

class Bool : public Expr {
 public:
  static Bool *Create(bool val);
  ~Bool() override {}

  // Override from Expr
  Bool *GetAsBool() override;

  bool val() const { return val_; }

 private:
  explicit Bool(bool val) : Expr(Type::BOOL), val_(val) {}
  bool val_;
};

class Char : public Expr {
 public:
  static Char *Create(char val);
  ~Char() override {}

  // Override from Expr
  Char *GetAsChar() override;

  char val() const { return val_; }

 private:
  explicit Char(char val) : Expr(Type::CHAR), val_(val) {}
  char val_;
};

class String : public Expr {
 public:
  static String *Create(const std::string &val);
  ~String() override;

  // Override from Expr
  String *GetAsString() override;

  const std::string &val() const { return val_; }

 private:
  explicit String(const std::string &val) : Expr(Type::STRING), val_(val) {}
  std::string val_;
};

class Symbol : public Expr {
 public:
  static Symbol *Create(const std::string &val);
  ~Symbol() override;

  // Override from Expr
  Symbol *GetAsSymbol() override;

  const std::string &val() const { return val_; }

 private:
  explicit Symbol(const std::string &val) : Expr(Type::STRING), val_(val) {}
  std::string val_;
};

class Pair : public Expr {
 public:
  static Pair *Create(Expr *car, Expr *cdr);
  ~Pair() override {}

  // Override from Expr
  Pair *GetAsPair() override;

  const Expr *car() { return car_; }
  const Expr *cdr() { return cdr_; }

 private:
  Pair(Expr *car, Expr *cdr) : Expr(Type::STRING), car_(car), cdr_(cdr) {}
  Expr *car_;
  Expr *cdr_;
};

class Vector : public Expr {
 public:
  static Vector *Create(const std::vector<Expr *> &vals);
  ~Vector() override;

  // Override from Expr
  Vector *GetAsVector() override;

  const std::vector<Expr *> &vals() const { return vals_; }

 private:
  explicit Vector(const std::vector<Expr *> &vals)
    : Expr(Type::STRING), vals_(vals) {}
  std::vector<Expr *> vals_;
};


class Var : public Expr {
 public:
  explicit Var(const std::string &name) : Expr(Type::VAR), name_(name) {}
  ~Var() override;

  // Override from Expr
  Var *GetAsVar() override;

  const std::string &name() const { return name_; }

 private:
  const std::string name_;
};

class Literal : public Expr {
 public:
  explicit Literal(Expr *expr) : Expr(Type::LITERAL), expr_(expr) {}
  ~Literal() override {}

  // Override from Expr
  Literal *GetAsLiteral() override;

  const Expr *expr() const { return expr_; }

 private:
  Expr *expr_;
};

class Apply : public Expr {
 public:
  explicit Apply(const std::vector<Expr *> &exprs);
  ~Apply() override;

  // Override from Expr
  Apply *GetAsApply() override;

  const Expr *op() const { return op_; }
  const std::vector<Expr *> &args() const { return args_; }

 private:
  Expr *op_;
  std::vector<Expr *> args_;
};

class Lambda : public Expr {
 public:
  explicit Lambda(const std::vector<Var *> &required_args, Var *variable_arg,
      const std::vector<Expr *> &body);
  ~Lambda() override;

  // Override from Expr
  Lambda *GetAsLambda() override;

  const std::vector<Var *> required_args() const { return required_args_; }
  const Var *variable_arg() const { return variable_arg_; }
  const std::vector<Expr *> body() const { return body_; }

 private:
  std::vector<Var *> required_args_;
  Var *variable_arg_;
  std::vector<Expr *> body_;
};

class Cond : public Expr {
 public:
  Cond(Expr *test, Expr *true_expr, Expr *false_expr)
    : Expr(Type::COND), test_(test), true_expr_(true_expr),
      false_expr_(false_expr) {}
  ~Cond() override {}

  // Override from Expr
  Cond *GetAsCond() override;

  const Expr *test() const { return test_; }
  const Expr *true_expr() const { return true_expr_; }
  const Expr *false_expr() const { return false_expr_; }

 private:
  Expr *test_;
  Expr *true_expr_;
  Expr *false_expr_;
};

class Assign : public Expr {
 public:
  Assign(Var *var, Expr *expr)
    : Expr(Type::ASSIGN), var_(var), expr_(expr) {}
  ~Assign() override {}

  // Override from Expr
  Assign *GetAsAssign() override;

  const Var *var() const { return var_; }
  const Expr *expr() const { return expr_; }

 private:
  Var *var_;
  Expr *expr_;
};

// TODO(bcf): Complete this
class LetSyntax : public Expr {
 public:
  LetSyntax() : Expr(Type::LET_SYNTAX) {}
  ~LetSyntax() override;

  // Override from Expr
  LetSyntax *GetAsLetSyntax() override;

 private:
};

}  // namespace expr

#endif  // EXPR_EXPR_H_
