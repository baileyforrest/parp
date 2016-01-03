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

namespace expr {

class Var;
class Quote;
class Bool;
class Number;
class Char;
class String;
class Apply;
class Lambda;
class Cond;
class Assign;
class LetSyntax;
class Empty;
class Moved;

class Expr {
 public:
  enum class Type {
    VAR,           // Variable
    LIT_QUOTE,     // (quote e)
    LIT_BOOL,      // #t, #f
    LIT_NUM,       // number
    LIT_CHAR,      // character
    LIT_STRING,    // string
    LIT_SYMBOL,    // symbol
    APPLY,         // procure call/macro use
    LAMBDA,        // (lambda <formals> <body>)
    COND,          // (if <test> <consequent> <alternate>)
    ASSIGN,        // (!set <variable> <expression>)
    LET_SYNTAX,    // (let{rec}-syntax (<syntax spec>*) <body>)

    // For garbage collector
    EMPTY,
    MOVED,
  };

  virtual ~Expr() {}

  Type type() const { return type_; }

  virtual Var *GetAsVar();
  virtual Quote *GetAsQuote();
  virtual Bool *GetAsBool();
  virtual Number *GetAsNumber();
  virtual Char *GetAsChar();
  virtual String *GetAsString();
  virtual Apply *GetAsApply();
  virtual Lambda *GetAsLambda();
  virtual Cond *GetAsCond();
  virtual Assign *GetAsAssign();
  virtual LetSyntax *GetAsLetSyntax();

  // Garbage collection methods
  bool marked() const { return marked_; }
  virtual size_t size() const = 0;
  virtual void Mark();
  virtual Empty *GetAsEmpty();
  virtual Moved *GetAsMoved();

 protected:
  explicit Expr(Type type) : type_(type) {}

 private:
  Type type_;
  bool marked_;  // Marked for garbage collection
};

class Var : public Expr {
 public:
  explicit Var(const std::string &name) : Expr(Type::VAR), name_(name) {}
  ~Var() override;

  // Override from Expr
  Var *GetAsVar() override;
  std::size_t size() const override;

  const std::string &name() const { return name_; }

 private:
  const std::string name_;
};

class Quote : public Expr {
 public:
  explicit Quote(Expr *expr) : Expr(Type::LIT_QUOTE), expr_(expr) {}
  ~Quote() override {};

  // Override from Expr
  Quote *GetAsQuote() override;
  std::size_t size() const override;

  const Expr *expr() const { return expr_; }

 private:
  Expr *expr_;
};

class Bool : public Expr {
 public:
  explicit Bool(bool val) : Expr(Type::LIT_BOOL), val_(val) {}
  ~Bool() override {};

  // Override from Expr
  Bool *GetAsBool() override;
  std::size_t size() const override;

  bool val() const { return val_; }

 private:
  bool val_;
};

class Char : public Expr {
 public:
  explicit Char(char val) : Expr(Type::LIT_CHAR), val_(val) {}
  ~Char() override {};

  // Override from Expr
  Char *GetAsChar() override;
  std::size_t size() const override;

  char val() const { return val_; }

 private:
  char val_;
};

class String : public Expr {
 public:
  explicit String(const std::string &val) : Expr(Type::LIT_STRING), val_(val) {}
  ~String() override;

  // Override from Expr
  String *GetAsString() override;
  std::size_t size() const override;

  const std::string &val() const { return val_; }

 private:
  std::string val_;
};

class Apply : public Expr {
 public:
  explicit Apply(const std::vector<Expr *> &exprs);
  ~Apply() override;

  // Override from Expr
  Apply *GetAsApply() override;
  std::size_t size() const override;

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
  std::size_t size() const override;

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
  ~Cond() override {};

  // Override from Expr
  Cond *GetAsCond() override;
  std::size_t size() const override;

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
    : Expr(Type::LIT_STRING), var_(var), expr_(expr) {}
  ~Assign() override {};

  // Override from Expr
  Assign *GetAsAssign() override;
  std::size_t size() const override;

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
  std::size_t size() const override;

 private:
};

/**
 * Record of empty space on the heap
 */
class Empty : public Expr {
 public:
  explicit Empty(std::size_t size) : Expr(Type::EMPTY), size_(size) {}
  ~Empty() override {};

  // Override from Expr
  Empty *GetAsEmpty() override;
  std::size_t size() const override;

 private:
  std::size_t size_;
};

// TODO(bcf): Must make sure this is the smallest allocation size
/**
 * Marker to point to new location of a moved expression
 */
class Moved : public Expr {
 public:
  explicit Moved(Expr *new_loc) : Expr(Type::MOVED), new_loc_(new_loc) {}
  ~Moved() override {};

  // Override from Expr
  Moved *GetAsMoved() override;
  std::size_t size() const override;

  const Expr *new_loc() const { return new_loc_; }

 private:
  Expr *new_loc_;
};

}  // namespace expr

#endif  // EXPR_EXPR_H_
