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
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
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
class Env;
class Var;
class Apply;
class Lambda;
class Cond;
class Assign;
class LetSyntax;
class Analyzed;

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

    // TODO(bcf): remove if unused
    VAR,           // Variable
    APPLY,         // procure call/macro use
    LAMBDA,        // (lambda <formals> <body>)
    COND,          // (if <test> <consequent> <alternate>)
    ASSIGN,        // (!set <variable> <expression>)
    LET_SYNTAX,    // (let{rec}-syntax (<syntax spec>*) <body>)

    ENV,           // Environment
    ANALYZED,      // Analyzed expression
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
  virtual const Env *GetAsEnv() const;
  virtual Env *GetAsEnv();
  virtual const Analyzed *GetAsAnalyzed() const;

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

inline std::ostream& operator<<(std::ostream& stream, const Expr *expr) {
  return stream << *expr;
}

inline bool operator==(const Expr &lhs, const Expr &rhs) {
  return lhs.Equal(&rhs);
}

// Class for expressions which don't evaluate to itself
// TODO(bcf): Remove if unnecessary
class Evals : public Expr {
 protected:
  Evals(Type type, bool readonly) : Expr(type, readonly) {}

 private:
  bool EqvImpl(const Expr *other) const override;
};

// TODO(bcf): Optimize for singletons
class EmptyList : public Expr {
 public:
  ~EmptyList() override {}

  // Override from Expr
  const EmptyList *GetAsEmptyList() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

 private:
  EmptyList() : Expr(Type::EMPTY_LIST, true) {}
  friend EmptyList *Nil();
};

EmptyList *Nil();

// TODO(bcf): Optimize for singletons
class Bool : public Expr {
 public:
  ~Bool() override {}

  // Override from Expr
  const Bool *GetAsBool() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  bool val() const { return val_; }

 private:
  explicit Bool(bool val) : Expr(Type::BOOL, true), val_(val) {}
  friend Bool *True();
  friend Bool *False();

  // Override from Expr
  bool EqvImpl(const Expr *other) const override;

  bool val_;
};

Bool *True();
Bool *False();

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

  Expr *car() const { return car_; }
  Expr *cdr() const { return cdr_; }
  void set_car(Expr *expr) { car_ = expr; }
  void set_cdr(Expr *expr) { cdr_ = expr; }

  expr::Expr *Cr(const std::string &str) const;

 private:
  Pair(Expr *car, Expr *cdr, bool readonly)
    : Expr(Type::PAIR, readonly), car_(car), cdr_(cdr) {}

  // Override from Expr
  bool EqvImpl(const Expr *other) const override;
  bool EqualImpl(const Expr *other) const override;

  Expr *car_;
  Expr *cdr_;
};

template<typename T>
Expr *ListFromIt(T it, T e) {
  Expr *res = Nil();
  for (; it != e; ++it) {
    res = Cons(*it, res);
  }

  return res;
}

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
  static Lambda *Create(const std::vector<const Symbol *> &required_args,
      const Symbol *variable_arg, const Expr *body, Env *env);
  ~Lambda() override;

  // Override from Expr
  const Lambda *GetAsLambda() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  const std::vector<const Symbol *> required_args() const {
    return required_args_;
  }
  Symbol *variable_arg() const { return variable_arg_; }
  Expr *body() const { return body_; }
  Env *env() const { return env_; }

 private:
  explicit Lambda(const std::vector<const Symbol *> &required_args,
      const Symbol *variable_arg, const std::vector<Expr *> &body);

  std::vector<const Symbol *> required_args_;
  Symbol *variable_arg_;
  Expr *body_;
  Env *env_;;
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

class Env : public Expr {
 public:
  static Env *Create(const std::vector<std::pair<const Symbol *, Expr *> > &vars,
      Env *enclosing, bool readonly = false);
  ~Env() override;

  // Override from Expr
  const Env *GetAsEnv() const override;
  Env *GetAsEnv() override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  Expr *Lookup(const Symbol *var) const;
  const Env *enclosing() const { return enclosing_; }
  void DefineVar(const Symbol *var, Expr *expr);
  void SetVar(const Symbol *var, Expr *expr);

 private:
  void ThrowUnboundException(const Symbol *var) const;
  explicit Env(const std::vector<std::pair<const Symbol *, Expr *> > &vars,
      Env *enclosing, bool readonly)
    : Expr(Type::ENV, readonly),
      enclosing_(enclosing),
      map_(vars.begin(), vars.end()) {}

  struct VarHash {
    std::size_t operator()(const Symbol *var) const;
  };

  struct VarEqual {
    bool operator()(const Symbol *lhs, const Symbol *rhs) const {
      return lhs->Eqv(rhs);
    }
  };

  Env *enclosing_;
  std::unordered_map<const Symbol *, Expr *, VarHash, VarEqual> map_;
};

using Evaluation = std::function<Expr *(Env *)>;

class Analyzed : public Evals {
 public:
  static Analyzed *Create(const Evaluation &func,
      const std::vector<Expr *> &refs);
  ~Analyzed() override;

  // Override from Expr
  const Analyzed *GetAsAnalyzed() const override;
  std::ostream &AppendStream(std::ostream &stream) const override;

  const Evaluation &func() const { return func_; }

 private:
  Analyzed(const Evaluation &func, const std::vector<Expr *> &refs)
    : Evals(Type::ANALYZED, true), func_(func), refs_(refs) {}
  Evaluation func_;
  std::vector<Expr *> refs_;
};

}  // namespace expr

#endif  // EXPR_EXPR_H_
