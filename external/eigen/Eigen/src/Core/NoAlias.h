// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_NOALIAS_H
#define EIGEN_NOALIAS_H

namespace Eigen {

template<typename ExpressionType, template <typename> class StorageBase>
class NoAlias
{
    typedef typename ExpressionType::Scalar Scalar;
  public:
    NoAlias(ExpressionType& expression) : m_expression(expression) {}

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE ExpressionType& operator=(const StorageBase<OtherDerived>& other)
    { return internal::assign_selector<ExpressionType,OtherDerived,false>::run(m_expression,other.derived()); }

    
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE ExpressionType& operator+=(const StorageBase<OtherDerived>& other)
    {
      typedef SelfCwiseBinaryOp<internal::scalar_sum_op<Scalar>, ExpressionType, OtherDerived> SelfAdder;
      SelfAdder tmp(m_expression);
      typedef typename internal::nested<OtherDerived>::type OtherDerivedNested;
      typedef typename internal::remove_all<OtherDerivedNested>::type _OtherDerivedNested;
      internal::assign_selector<SelfAdder,_OtherDerivedNested,false>::run(tmp,OtherDerivedNested(other.derived()));
      return m_expression;
    }

    
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE ExpressionType& operator-=(const StorageBase<OtherDerived>& other)
    {
      typedef SelfCwiseBinaryOp<internal::scalar_difference_op<Scalar>, ExpressionType, OtherDerived> SelfAdder;
      SelfAdder tmp(m_expression);
      typedef typename internal::nested<OtherDerived>::type OtherDerivedNested;
      typedef typename internal::remove_all<OtherDerivedNested>::type _OtherDerivedNested;
      internal::assign_selector<SelfAdder,_OtherDerivedNested,false>::run(tmp,OtherDerivedNested(other.derived()));
      return m_expression;
    }

#ifndef EIGEN_PARSED_BY_DOXYGEN
    template<typename ProductDerived, typename Lhs, typename Rhs>
    EIGEN_STRONG_INLINE ExpressionType& operator+=(const ProductBase<ProductDerived, Lhs,Rhs>& other)
    { other.derived().addTo(m_expression); return m_expression; }

    template<typename ProductDerived, typename Lhs, typename Rhs>
    EIGEN_STRONG_INLINE ExpressionType& operator-=(const ProductBase<ProductDerived, Lhs,Rhs>& other)
    { other.derived().subTo(m_expression); return m_expression; }

    template<typename Lhs, typename Rhs, int NestingFlags>
    EIGEN_STRONG_INLINE ExpressionType& operator+=(const CoeffBasedProduct<Lhs,Rhs,NestingFlags>& other)
    { return m_expression.derived() += CoeffBasedProduct<Lhs,Rhs,NestByRefBit>(other.lhs(), other.rhs()); }

    template<typename Lhs, typename Rhs, int NestingFlags>
    EIGEN_STRONG_INLINE ExpressionType& operator-=(const CoeffBasedProduct<Lhs,Rhs,NestingFlags>& other)
    { return m_expression.derived() -= CoeffBasedProduct<Lhs,Rhs,NestByRefBit>(other.lhs(), other.rhs()); }
    
    template<typename OtherDerived>
    ExpressionType& operator=(const ReturnByValue<OtherDerived>& func)
    { return m_expression = func; }
#endif

    ExpressionType& expression() const
    {
      return m_expression;
    }

  protected:
    ExpressionType& m_expression;
};

template<typename Derived>
NoAlias<Derived,MatrixBase> MatrixBase<Derived>::noalias()
{
  return derived();
}

} 

#endif 
