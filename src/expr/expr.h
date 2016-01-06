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
#include <iostream>
#include <string>
#include <vector>

#include "gc/gc.h"

namespace expr {

class EmptyList;
class Bool;
class Number;
class Char;
class String;
class Symbol;
class Pair;
class Vector;
class Var;
class Apply;
class Lambda;
class Cond;
class Assign;
class LetSyntax;

class Expr : public gc::Collectable{
 public:
  enum class Type : char {
    // Expr values
    EMPTY_LIST,
    BOOL,
    NUMBER,
    CHAR,
    STRING,
    SYMBOL,
    PAIR,
    VECTOR,

    VAR,           // Variable
    APPLY,         // procure call/macro use
    LAMBDA,        // (lambda <formals> <body>)
    COND,          // (if <test> <consequent> <alternate>)
    ASSIGN,        // (!set <variable> <expression>)
    LET_SYNTAX,    // (let{rec}-syntax (<syntax spec>*) <body>)
  };

  virtual ~Expr() {}

  Type type() const { return type_; }
  bool readonly() const { return readonly_; }
  bool IsDatum() const;

  bool Eq(const Expr *other) const { return other == this; }
  bool Eqv(const Expr *other) const;
  bool Equal(const Expr *other) const;

  virtual std::ostream &AppendStream(
      std::ostream &stream) const = 0;  // NOLINT(runtime/references)

  virtual const EmptyList *GetAsEmptyList() const;
  virtual const Bool *GetAsBool() const;
  virtual const Number *GetAsNumber() const;
  virtual const Char *GetAsChar() const;
  virtual const String *GetAsString() const;
  virtual String *GetAsString();
  virtual const Symbol *GetAsSymbol() const;
  virtual const Pair *GetAsPair() const;
  virtual Pair *GetAsPair();
  virtual const Vector *GetAsVector() const;
  virtual Vector *GetAsVector();
  virtual const Var *GetAsVar() const;
  virtual const Apply *GetAsApply() const;
  virtual const Lambda *GetAsLambda() const;
  virtual const Cond *GetAsCond() const;
  virtual const Assign *GetAsAssign() const;
  virtual const LetSyntax *GetAsLetSyntax() const;

 protected:
  Expr(Type type, bool readonly) : type_(type), readonly_(readonly) {}

 private:
  virtual bool EqvImpl(const Expr *other) const;
  virtual bool EqualImpl(const Expr *other) const;

  Type type_;
  bool readonly_;
};

inline std::ostream& operator<<(std::ostream& stream, const Expr &expr) {
  return expr.AppendStream(stream);
}

inline bool operator==(const Expr &lhs, const Expr &rhs) {
  return lhs.Equal(&rhs);
}

// Class for expressions which don't evaluate to itself
class Evals : public Expr {
 protected:
  Evals(Type type, bool readonly) : Expr(type, readonly) {}

 private:
  bool EqvImpl(const Expr *other) const override;
};

class EmptyList : public Expr {
 public:
  static EmptyList *Create();
  ~EmptyList() override {}

  // Override from Expr
  const EmptyList *GetAsEmptyList() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

 private:
  EmptyList() : Expr(Type::EMPTY_LIST, true) {}
};

// Alias for EmptyList::Create
inline EmptyList *Nil() { return EmptyList::Create(); }

class Bool : public Expr {
 public:
  static Bool *Create(bool val);
  ~Bool() override {}

  // Override from Expr
  const Bool *GetAsBool() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  bool val() const { return val_; }

 private:
  explicit Bool(bool val) : Expr(Type::BOOL, true), val_(val) {}

  // Override from Expr
  bool EqvImpl(const Expr *other) const override;

  bool val_;
};

class Char : public Expr {
 public:
  static Char *Create(char val);
  ~Char() override {}

  // Override from Expr
  const Char *GetAsChar() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  char val() const { return val_; }

 private:
  explicit Char(char val) : Expr(Type::CHAR, true), val_(val) {}

  // Override from Expr
  bool EqvImpl(const Expr *other) const override;

  char val_;
};

class String : public Expr {
 public:
  static String *Create(const std::string &val, bool readonly = true);
  ~String() override;

  // Override from Expr
  const String *GetAsString() const override;
  String *GetAsString() override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  const std::string &val() const { return val_; }

 private:
  String(const std::string &val, bool readonly)
    : Expr(Type::STRING, readonly), val_(val) {}

  // Override from Expr
  bool EqualImpl(const Expr *other) const override;

  std::string val_;
};

class Symbol : public Expr {
 public:
  static Symbol *Create(const std::string &val);
  ~Symbol() override;

  // Override from Expr
  const Symbol *GetAsSymbol() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  const std::string &val() const { return val_; }

 private:
  explicit Symbol(const std::string &val)
    : Expr(Type::SYMBOL, true), val_(val) {}

  // Override from Expr
  bool EqvImpl(const Expr *other) const override;

  std::string val_;
};

class Pair : public Expr {
 public:
  static Pair *Create(Expr *car, Expr *cdr, bool readonly = true);
  ~Pair() override {}

  // Override from Expr
  const Pair *GetAsPair() const override;
  Pair *GetAsPair() override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  const Expr *car() const { return car_; }
  const Expr *cdr() const { return cdr_; }
  void set_car(Expr *expr) { car_ = expr; }
  void set_cdr(Expr *expr) { cdr_ = expr; }

 private:
  Pair(Expr *car, Expr *cdr, bool readonly)
    : Expr(Type::PAIR, readonly), car_(car), cdr_(cdr) {}

  // Override from Expr
  bool EqvImpl(const Expr *other) const override;
  bool EqualImpl(const Expr *other) const override;

  Expr *car_;
  Expr *cdr_;
};

// Alias for Pair::Create
inline Pair *Cons(Expr *e1, Expr *e2) { return Pair::Create(e1, e2); }

class Vector : public Expr {
 public:
  static Vector *Create(const std::vector<Expr *> &vals, bool readonly = true);
  ~Vector() override;

  // Override from Expr
  const Vector *GetAsVector() const override;
  Vector *GetAsVector() override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  const std::vector<Expr *> &vals() const { return vals_; }

 private:
  Vector(const std::vector<Expr *> &vals, bool readonly)
    : Expr(Type::VECTOR, readonly), vals_(vals) {}

  // Override from Expr
  bool EqvImpl(const Expr *other) const override;
  bool EqualImpl(const Expr *other) const override;

  std::vector<Expr *> vals_;
};


class Var : public Evals {
 public:
  Var *Create(const std::string &name);
  ~Var() override;

  // Override from Expr
  const Var *GetAsVar() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  const std::string &name() const { return name_; }

 private:
  explicit Var(const std::string &name) : Evals(Type::VAR, true), name_(name) {}

  const std::string name_;
};

class Apply : public Evals {
 public:
  Apply *Create(const std::vector<Expr *> &exprs);
  ~Apply() override;

  // Override from Expr
  const Apply *GetAsApply() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  const Expr *op() const { return op_; }
  const std::vector<Expr *> &args() const { return args_; }

 private:
  explicit Apply(const std::vector<Expr *> &exprs);

  Expr *op_;
  std::vector<Expr *> args_;
};

class Lambda : public Expr {
 public:
  Lambda *Create(const std::vector<Var *> &required_args, Var *variable_arg,
      const std::vector<Expr *> &body);
  ~Lambda() override;

  // Override from Expr
  const Lambda *GetAsLambda() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  const std::vector<Var *> required_args() const { return required_args_; }
  const Var *variable_arg() const { return variable_arg_; }
  const std::vector<Expr *> body() const { return body_; }

 private:
  explicit Lambda(const std::vector<Var *> &required_args, Var *variable_arg,
      const std::vector<Expr *> &body);

  std::vector<Var *> required_args_;
  Var *variable_arg_;
  std::vector<Expr *> body_;
};

class Cond : public Evals {
 public:
  Cond *Create(Expr *test, Expr *true_expr, Expr *false_expr);
  ~Cond() override {}

  // Override from Expr
  const Cond *GetAsCond() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  const Expr *test() const { return test_; }
  const Expr *true_expr() const { return true_expr_; }
  const Expr *false_expr() const { return false_expr_; }

 private:
  Cond(Expr *test, Expr *true_expr, Expr *false_expr)
    : Evals(Type::COND, true), test_(test), true_expr_(true_expr),
      false_expr_(false_expr) {}

  Expr *test_;
  Expr *true_expr_;
  Expr *false_expr_;
};

class Assign : public Evals {
 public:
  Assign *Create(Var *var, Expr *expr);
  ~Assign() override {}

  // Override from Expr
  const Assign *GetAsAssign() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  const Var *var() const { return var_; }
  const Expr *expr() const { return expr_; }

 private:
  Assign(Var *var, Expr *expr)
    : Evals(Type::ASSIGN, true), var_(var), expr_(expr) {}

  Var *var_;
  Expr *expr_;
};

// TODO(bcf): Complete this
class LetSyntax : public Expr {
 public:
  LetSyntax *Create();
  ~LetSyntax() override;

  // Override from Expr
  const LetSyntax *GetAsLetSyntax() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

 private:
  LetSyntax() : Expr(Type::LET_SYNTAX, true) {}
};

}  // namespace expr

#endif  // EXPR_EXPR_H_
