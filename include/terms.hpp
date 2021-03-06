// Copyright 2021 Pierre Talbot

#ifndef TERMS_HPP
#define TERMS_HPP

#include "arithmetic.hpp"
#include "ptr_utility.hpp"
#include "vector.hpp"

namespace lala {

template <class AD>
class Term {
public:
  using A = AD;
  using U = typename A::Universe;
  CUDA virtual ~Term() {}
  CUDA virtual void tell(A&, const U&, BInc&) const = 0;
  CUDA virtual U project(const A&) const = 0;
  CUDA virtual BInc is_top(const A&) const = 0;
  CUDA virtual void print(const A&) const = 0;
};

// `DynTerm` wraps a term and inherits from Term.
// A vtable will be created.
template<class BaseTerm>
class DynTerm: public Term<typename BaseTerm::A> {
  BaseTerm t;
public:
  using A = typename BaseTerm::A;
  using U = typename A::Universe;

  CUDA DynTerm(BaseTerm&& t): t(std::move(t)) {}
  CUDA DynTerm(DynTerm<BaseTerm>&& other): DynTerm(std::move(other.t)) {}

  CUDA void tell(A& a, const U& u, BInc& has_changed) const override {
    t.tell(a, u, has_changed);
  }

  CUDA U project(const A& a) const override {
    return t.project(a);
  }

  CUDA BInc is_top(const A& a) const override {
    return t.is_top(a);
  }

  CUDA void print(const A& a) const override {
    t.print(a);
  }

  CUDA ~DynTerm() {}
};

template <class AD>
class Constant {
public:
  using A = AD;
  using U = typename A::Universe;

private:
  U k;

public:
  CUDA Constant(U&& k) : k(k) {}
  CUDA Constant(Constant<A>&& other): k(std::move(other.k)) {}
  CUDA void tell(A&, const U&, BInc&) const {}
  CUDA U project(const A&) const { return k; }
  CUDA BInc is_top(const A&) const { return BInc::bot(); }
  CUDA void print(const A&) const { ::battery::print(k); }
};

template<class T>
struct is_constant_term {
  static constexpr bool value = false;
};

template <class A>
struct is_constant_term<Constant<A>> {
  static constexpr bool value = true;
};

template<class T>
inline constexpr bool is_constant_term_v = is_constant_term<T>::value;

template <class AD>
class Variable {
public:
  using A = AD;
  using U = typename A::Universe;

private:
  AVar avar;

public:
  CUDA Variable() {}
  CUDA Variable(AVar avar) : avar(avar) {}
  Variable(Variable<A>&& other) = default;
  Variable(const Variable<A>& other) = default;
  Variable<A>& operator=(Variable<A>&&) = default;
  Variable<A>& operator=(const Variable<A>&) = default;

  CUDA void tell(A& a, const U& u, BInc& has_changed) const {
    a.tell(avar, u, has_changed);
  }

  CUDA U project(const A& a) const {
    return a.project(avar);
  }

  CUDA BInc is_top(const A& a) const { return project(a).is_top(); }
  CUDA void print(const A& a) const { a.environment().to_lvar(avar).print(); }
};

template<class Universe, Approx appx = EXACT>
struct NegOp {
  using U = Universe;

  CUDA static U op(const U& a) {
    return neg<appx>(a);
  }

  CUDA static char symbol() { return '-'; }
};

template <class UnaryOp, class TermX>
class Unary {
public:
  using TermX_ = typename remove_ptr<TermX>::type;
  using A = typename TermX_::A;
  using U = typename A::Universe;
  using this_type = Unary<UnaryOp, TermX>;
private:
  TermX x_term;

  CUDA INLINE const TermX_& x() const {
    return deref(x_term);
  }

public:
  CUDA Unary(TermX&& x_term): x_term(std::move(x_term)) {}
  CUDA Unary(this_type&& other): Unary(std::move(other.x_term)) {}

  CUDA void tell(A& a, const U& u, BInc& has_changed) const {
    if(x().is_top(a).guard()) { return; }
    x().tell(a, UnaryOp::op(u), has_changed);
  }

  CUDA U project(const A& a) const {
    return UnaryOp::op(x().project(a));
  }

  CUDA BInc is_top(const A& a) const {
    return x().is_top(a);
  }

  CUDA void print(const A& a) const {
    printf("%c", UnaryOp::symbol());
    x().print(a);
  }
};

template <class TermX, Approx appx = EXACT>
using Neg = Unary<
  NegOp<typename remove_ptr<TermX>::type::U, appx>,
  TermX>;

template<class Universe, Approx appx = EXACT>
struct GroupAdd {
  using U = Universe;
  CUDA static U op(const U& a, const U& b) {
    return add<appx>(a, b);
  }

  CUDA static U rev_op(const U& a, const U& b) {
    return rev_add<appx>(a, b);
  }

  CUDA static U inv1(const U& a, const U& b) {
    return sub<appx>(a, b);
  }

  CUDA static U inv2(const U& a, const U& b) {
    return inv1(a,b);
  }

  CUDA static char symbol() { return '+'; }
};

template<class Universe, Approx appx = EXACT>
struct GroupSub {
  using U = Universe;
  CUDA static U op(const U& a, const U& b) {
    return sub<appx>(a, b);
  }

  CUDA static U inv1(const U& a, const U& b) {
    return add<appx>(a, b);
  }

  CUDA static U inv2(const U& a, const U& b) {
    return neg<appx>(sub<appx>(a, b));
  }

  CUDA static char symbol() { return '-'; }
};

template<class Universe, Approx appx = OVER>
struct GroupMul {
  using U = Universe;
  CUDA static U op(const U& a, const U& b) {
    return mul<appx>(a, b);
  }

  CUDA static U rev_op(const U& a, const U& b) {
    return div<appx>(a, b); // probably not ideal? Could think more about that.
  }

  CUDA static U inv1(const U& a, const U& b) {
    return div<appx>(a, b);
  }

  CUDA static U inv2(const U& a, const U& b) {
    return inv1(a, b);
  }

  CUDA static char symbol() { return '*'; }
};

template<class Universe, Approx appx = OVER>
struct GroupDiv {
  using U = Universe;
  CUDA static U op(const U& a, const U& b) {
    return div<appx>(a, b);
  }

  CUDA static U inv1(const U& a, const U& b) {
    return mul<appx>(a, b);
  }

  CUDA static U inv2(const U& a, const U& b) {
    return div<appx>(b, a);
  }

  CUDA static char symbol() { return '/'; }
};

template <class Group, class TermX, class TermY>
class Binary {
public:
  using TermX_ = typename remove_ptr<TermX>::type;
  using TermY_ = typename remove_ptr<TermY>::type;

  using A = typename TermX_::A;
  using U = typename Group::U;
  using G = Group;
  using this_type = Binary<Group, TermX, TermY>;

private:
  TermX x_term;
  TermY y_term;

  CUDA INLINE const TermX_& x() const {
    return deref(x_term);
  }

  CUDA INLINE const TermY_& y() const {
    return deref(y_term);
  }

public:
  CUDA Binary(TermX&& x_term, TermY&& y_term): x_term(std::move(x_term)), y_term(std::move(y_term)) {}
  CUDA Binary(this_type&& other): Binary(std::move(other.x_term), std::move(other.y_term)) {}

  /** Enforce `x <op> y >= u` where >= is the lattice order of the underlying abstract universe.
      For instance, over the interval abstract universe, `x + y >= [2..5]` will ensure that `x + y` is eventually at least `2` and at most `5`. */
  CUDA void tell(A& a, const U& u, BInc& has_changed) const {
    auto xt = x().project(a);
    auto yt = y().project(a);
    if(lor(xt.is_top(), yt.is_top()).guard()) { return; }
    if constexpr(!is_constant_term_v<TermX>) {
      x().tell(a, G::inv1(u, yt), has_changed);   // x <- u <inv> y
    }
    if constexpr(!is_constant_term_v<TermY>) {
      y().tell(a, G::inv2(u, x().project(a)), has_changed);   // y <- u <inv> x
    }
  }

  CUDA U project(const A& a) const {
    return G::op(x().project(a), y().project(a));
  }

  CUDA BInc is_top(const A& a) const {
    return lor(x().is_top(a), y().is_top(a));
  }

  CUDA void print(const A& a) const {
    x().print(a);
    printf(" %c ", G::symbol());
    y().print(a);
  }
};

template <class TermX, class TermY, Approx appx = EXACT>
using Add = Binary<
  GroupAdd<typename remove_ptr<TermX>::type::U, appx>,
  TermX,
  TermY>;

template <class TermX, class TermY, Approx appx = EXACT>
using Sub = Binary<
  GroupSub<typename remove_ptr<TermX>::type::U, appx>,
  TermX,
  TermY>;

template <class TermX, class TermY, Approx appx = OVER>
using Mul = Binary<
  GroupMul<typename remove_ptr<TermX>::type::U, appx>,
  TermX,
  TermY>;

template <class TermX, class TermY, Approx appx = OVER>
using Div = Binary<
  GroupDiv<typename remove_ptr<TermX>::type::U, appx>,
  TermX,
  TermY>;

// Nary is only valid for commutative group (e.g., addition and multiplication).
template <class T, class Combinator, class Allocator>
class Nary {
  battery::vector<T, Allocator> terms;

  using T_ = typename remove_ptr<T>::type;
  CUDA INLINE const T_& t(size_t i) const {
    return deref(terms[i]);
  }

public:
  using this_type = Nary<T, Combinator, Allocator>;
  using A = typename Combinator::A;
  using U = typename Combinator::U;
  using G = typename Combinator::G;

  CUDA Nary(battery::vector<T, Allocator>&& terms): terms(std::move(terms)) {}
  CUDA Nary(this_type&& other): Nary(std::move(other.terms)) {}

  CUDA U project(const A& a) const {
    U accu = t(0).project(a);
    for(int i = 1; i < terms.size(); ++i) {
      accu = G::op(accu, t(i).project(a));
    }
    return accu;
  }

  CUDA void tell(A& a, const U& u, BInc& has_changed) const {
    U all = project(a);
    for(int i = 0; i < terms.size(); ++i) {
      t(i).tell(a, G::inv1(u, G::rev_op(all, t(i).project(a))), has_changed);
    }
  }

  CUDA BInc is_top(const A& a) const {
    for(int i = 0; i < terms.size(); ++i) {
      if(t(i).is_top(a).guard()) {
        return BInc::top();
      }
    }
    return BInc::bot();
  }

  CUDA void print(const A& a) const {
    t(0).print(a);
    for(int i = 1; i < terms.size(); ++i) {
      printf(" %c ", G::symbol());
      t(i).print(a);
    }
  }
};

template<class T, class Allocator, Approx appx = EXACT>
using NaryAdd = Nary<T, Add<T, T, appx>, Allocator>;

template<class T, class Allocator, Approx appx = OVER>
using NaryMul = Nary<T, Mul<T, T, appx>, Allocator>;

} // namespace lala

#endif
