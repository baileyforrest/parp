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

#ifndef EXPR_EXPR_H_
#define EXPR_EXPR_H_

#include <cassert>
#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gc/gc.h"
#include "util/macros.h"

namespace expr {

class EmptyList;
class Bool;
class Number;
class Char;
class String;
class Symbol;
class Pair;
class Vector;
class Lambda;
class Env;
class Analyzed;
class Primitive;

class Expr : public gc::Collectable {
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

    // Analyzed types
    LAMBDA,     // (lambda <formals> <body>)
    ENV,        // Environment
    ANALYZED,   // Analyzed expression
    PRIMITIVE,  // Primitive expression
  };

  virtual ~Expr() = default;

  Type type() const { return type_; }

  bool Eq(const Expr* other) const { return other == this; }
  bool Eqv(const Expr* other) const {
    return Eq(other) || (type_ == other->type_ && EqvImpl(other));
  }
  bool Equal(const Expr* other) const {
    return Eq(other) || (type_ == other->type_ && EqualImpl(other));
  }

  virtual std::ostream& AppendStream(
      std::ostream& stream) const = 0;  // NOLINT(runtime/references)

  // TODO(bcf): Rename as AsX.
  virtual const EmptyList* GetAsEmptyList() const { return nullptr; }
  virtual EmptyList* GetAsEmptyList() { return nullptr; }
  virtual const Bool* GetAsBool() const { return nullptr; }
  virtual Bool* GetAsBool() { return nullptr; }
  virtual const Number* GetAsNumber() const { return nullptr; }
  virtual Number* GetAsNumber() { return nullptr; }
  virtual const Char* GetAsChar() const { return nullptr; }
  virtual Char* GetAsChar() { return nullptr; }
  virtual const String* GetAsString() const { return nullptr; }
  virtual String* GetAsString() { return nullptr; }
  virtual const Symbol* GetAsSymbol() const { return nullptr; }
  virtual Symbol* GetAsSymbol() { return nullptr; }
  virtual const Pair* GetAsPair() const { return nullptr; }
  virtual Pair* GetAsPair() { return nullptr; }
  virtual const Vector* GetAsVector() const { return nullptr; }
  virtual Vector* GetAsVector() { return nullptr; }
  virtual const Lambda* GetAsLambda() const { return nullptr; }
  virtual Lambda* GetAsLambda() { return nullptr; }
  virtual const Env* GetAsEnv() const { return nullptr; }
  virtual Env* GetAsEnv() { return nullptr; }
  virtual const Analyzed* GetAsAnalyzed() const { return nullptr; }
  virtual Analyzed* GetAsAnalyzed() { return nullptr; }
  virtual const Primitive* GetAsPrimitive() const { return nullptr; }
  virtual Primitive* GetAsPrimitive() { return nullptr; }

 protected:
  explicit Expr(Type type) : type_(type) {}

 private:
  virtual bool EqvImpl(const Expr* other) const { return Eq(other); }
  virtual bool EqualImpl(const Expr* other) const { return Eqv(other); }

  Type type_;

  DISALLOW_MOVE_COPY_AND_ASSIGN(Expr);
};

const char* TypeToString(Expr::Type type);
inline std::ostream& operator<<(std::ostream& stream, const Expr::Type& type) {
  return stream << TypeToString(type);
}

inline std::ostream& operator<<(std::ostream& stream, const Expr& expr) {
  return expr.AppendStream(stream);
}

inline bool operator==(const Expr& lhs, const Expr& rhs) {
  return lhs.Equal(&rhs);
}

class EmptyList : public Expr {
 public:
  ~EmptyList() override = default;

  // Expr implementation:
  const EmptyList* GetAsEmptyList() const override { return this; }
  EmptyList* GetAsEmptyList() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

 private:
  EmptyList() : Expr(Type::EMPTY_LIST) {}
  friend EmptyList* Nil();
};

class Bool : public Expr {
 public:
  ~Bool() override = default;

  // Expr implementation:
  const Bool* GetAsBool() const override { return this; }
  Bool* GetAsBool() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

  bool val() const { return val_; }

 private:
  explicit Bool(bool val) : Expr(Type::BOOL), val_(val) {}
  friend Bool* True();
  friend Bool* False();

  bool val_;
};

class Char : public Expr {
 public:
  static Char* Create(char val);
  ~Char() override = default;

  // Expr implementation:
  const Char* GetAsChar() const override { return this; }
  Char* GetAsChar() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

  char val() const { return val_; }

 private:
  explicit Char(char val) : Expr(Type::CHAR), val_(val) {}

  // Expr implementation:
  bool EqvImpl(const Expr* other) const override {
    return val_ == other->GetAsChar()->val_;
  }

  char val_;
};

// TODO(bcf): Optimize for readonly strings.
class String : public Expr {
 public:
  static String* Create(std::string val, bool read_only = false);
  ~String() override;

  // Expr implementation:
  const String* GetAsString() const override { return this; }
  String* GetAsString() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

  const std::string& val() const { return val_; }

 private:
  explicit String(std::string val, bool read_only = false);

  // Expr implementation:
  bool EqualImpl(const Expr* other) const override;

  std::string val_;
  bool read_only_;
};

// TODO(bcf): Optimize for read only.
class Symbol : public Expr {
 public:
  static Symbol* Create(std::string val);
  ~Symbol() override;

  // Expr implementation:
  const Symbol* GetAsSymbol() const override { return this; }
  Symbol* GetAsSymbol() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

  const std::string& val() const { return val_; }

 private:
  explicit Symbol(std::string val);

  // Expr implementation:
  bool EqvImpl(const Expr* other) const override;

  std::string val_;
};

class Pair : public Expr {
 public:
  static Pair* Create(Expr* car, Expr* cdr);
  ~Pair() override = default;

  // Expr implementation:
  const Pair* GetAsPair() const override { return this; }
  Pair* GetAsPair() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

  Expr* car() const { return car_; }
  Expr* cdr() const { return cdr_; }
  void set_car(Expr* expr) { car_ = expr; }
  void set_cdr(Expr* expr) { cdr_ = expr; }

  expr::Expr* Cr(const std::string& str) const;

 private:
  Pair(Expr* car, Expr* cdr) : Expr(Type::PAIR), car_(car), cdr_(cdr) {}

  // Expr implementation:
  bool EqvImpl(const Expr* other) const override;
  bool EqualImpl(const Expr* other) const override;

  Expr* car_;
  Expr* cdr_;
};

class Vector : public Expr {
 public:
  static Vector* Create(std::vector<Expr*> vals);
  ~Vector() override;

  // Expr implementation:
  const Vector* GetAsVector() const override { return this; }
  Vector* GetAsVector() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

  const std::vector<Expr*>& vals() const { return vals_; }

 private:
  explicit Vector(std::vector<Expr*> vals);

  // Expr implementation:
  bool EqvImpl(const Expr* other) const override;
  bool EqualImpl(const Expr* other) const override;

  std::vector<Expr*> vals_;
};

class Lambda : public Expr {
 public:
  static Lambda* Create(std::vector<Symbol*> required_args,
                        Symbol* variable_arg,
                        std::vector<Expr*> body,
                        Env* env);
  ~Lambda() override;

  // Expr implementation:
  const Lambda* GetAsLambda() const override { return this; }
  Lambda* GetAsLambda() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

  const std::vector<Symbol*>& required_args() const { return required_args_; }
  Symbol* variable_arg() const { return variable_arg_; }
  const std::vector<Expr*>& body() const { return body_; }
  Env* env() const { return env_; }

 private:
  explicit Lambda(std::vector<Symbol*> required_args,
                  Symbol* variable_arg,
                  std::vector<Expr*> body,
                  Env* env);

  std::vector<Symbol*> required_args_;
  Symbol* variable_arg_;
  std::vector<Expr*> body_;
  Env* env_;
};

class Env : public Expr {
 public:
  static Env* Create(Env* enclosing);
  ~Env() override;

  // Expr implementation:
  const Env* GetAsEnv() const override { return this; }
  Env* GetAsEnv() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

  Expr* Lookup(Symbol* var) const;
  const Env* enclosing() const { return enclosing_; }
  void DefineVar(Symbol* var, Expr* expr);
  void SetVar(Symbol* var, Expr* expr);

 private:
  explicit Env(Env* enclosing);

  struct VarHash {
    std::size_t operator()(Symbol* var) const;
  };

  struct VarEqual {
    bool operator()(Symbol* lhs, Symbol* rhs) const { return lhs->Eqv(rhs); }
  };

  Env* enclosing_;
  std::unordered_map<Symbol*, Expr*, VarHash, VarEqual> map_;
};

class Analyzed : public Expr {
 public:
  using Evaluation = std::function<Expr*(Env*)>;

  static Analyzed* Create(Expr* orig_expr,
                          Evaluation func,
                          std::vector<Expr*> refs);
  ~Analyzed() override;

  // Expr implementation:
  const Analyzed* GetAsAnalyzed() const override { return this; }
  Analyzed* GetAsAnalyzed() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override;

  const Evaluation& func() const { return func_; }

 private:
  Analyzed(Expr* orig_expr, Evaluation func, std::vector<Expr*> refs);

  Expr* orig_expr_;
  Evaluation func_;
  std::vector<Expr*> refs_;
};

class Primitive : public Expr {
 public:
  virtual Expr* Eval(Env* env, Expr** exprs, size_t size) const = 0;

  // Expr implementation:
  const Primitive* GetAsPrimitive() const override { return this; }
  Primitive* GetAsPrimitive() override { return this; }
  std::ostream& AppendStream(std::ostream& stream) const override = 0;

 protected:
  Primitive() : Expr(Type::PRIMITIVE) {}
  virtual ~Primitive() = default;
};

// Special constants
EmptyList* Nil();
Bool* True();
Bool* False();

// Helpers

std::vector<Expr*> ExprVecFromList(Expr* expr);

template <typename T>
Expr* ListFromIt(T it, T e) {
  if (it == e) {
    return Nil();
  }

  auto* val = *it;
  ++it;
  return Cons(val, ListFromIt(it, e));
}

// Alias for Pair::Create
inline Pair* Cons(Expr* e1, Expr* e2) {
  return Pair::Create(e1, e2);
}

}  // namespace expr

#endif  // EXPR_EXPR_H_
