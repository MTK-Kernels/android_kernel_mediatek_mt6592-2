// Copyright (C) 2009-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2009-2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_RETURNBYVALUE_H
#define EIGEN_RETURNBYVALUE_H

namespace Eigen {


namespace internal {

template<typename Derived>
struct traits<ReturnByValue<Derived> >
  : public traits<typename traits<Derived>::ReturnType>
{
  enum {
    
    
    
    Flags = (traits<typename traits<Derived>::ReturnType>::Flags
             | EvalBeforeNestingBit) & ~DirectAccessBit
  };
};

template<typename Derived,int n,typename PlainObject>
struct nested<ReturnByValue<Derived>, n, PlainObject>
{
  typedef typename traits<Derived>::ReturnType type;
};

} 

template<typename Derived> class ReturnByValue
  : internal::no_assignment_operator, public internal::dense_xpr_base< ReturnByValue<Derived> >::type
{
  public:
    typedef typename internal::traits<Derived>::ReturnType ReturnType;

    typedef typename internal::dense_xpr_base<ReturnByValue>::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(ReturnByValue)

    template<typename Dest>
    inline void evalTo(Dest& dst) const
    { static_cast<const Derived*>(this)->evalTo(dst); }
    inline Index rows() const { return static_cast<const Derived*>(this)->rows(); }
    inline Index cols() const { return static_cast<const Derived*>(this)->cols(); }

#ifndef EIGEN_PARSED_BY_DOXYGEN
#define Unusable YOU_ARE_TRYING_TO_ACCESS_A_SINGLE_COEFFICIENT_IN_A_SPECIAL_EXPRESSION_WHERE_THAT_IS_NOT_ALLOWED_BECAUSE_THAT_WOULD_BE_INEFFICIENT
    class Unusable{
      Unusable(const Unusable&) {}
      Unusable& operator=(const Unusable&) {return *this;}
    };
    const Unusable& coeff(Index) const { return *reinterpret_cast<const Unusable*>(this); }
    const Unusable& coeff(Index,Index) const { return *reinterpret_cast<const Unusable*>(this); }
    Unusable& coeffRef(Index) { return *reinterpret_cast<Unusable*>(this); }
    Unusable& coeffRef(Index,Index) { return *reinterpret_cast<Unusable*>(this); }
#endif
};

template<typename Derived>
template<typename OtherDerived>
Derived& DenseBase<Derived>::operator=(const ReturnByValue<OtherDerived>& other)
{
  other.evalTo(derived());
  return derived();
}

} 

#endif 
