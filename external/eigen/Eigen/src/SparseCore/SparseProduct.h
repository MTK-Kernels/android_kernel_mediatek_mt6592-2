// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SPARSEPRODUCT_H
#define EIGEN_SPARSEPRODUCT_H

namespace Eigen { 

template<typename Lhs, typename Rhs>
struct SparseSparseProductReturnType
{
  typedef typename internal::traits<Lhs>::Scalar Scalar;
  typedef typename internal::traits<Lhs>::Index Index;
  enum {
    LhsRowMajor = internal::traits<Lhs>::Flags & RowMajorBit,
    RhsRowMajor = internal::traits<Rhs>::Flags & RowMajorBit,
    TransposeRhs = (!LhsRowMajor) && RhsRowMajor,
    TransposeLhs = LhsRowMajor && (!RhsRowMajor)
  };

  typedef typename internal::conditional<TransposeLhs,
    SparseMatrix<Scalar,0,Index>,
    typename internal::nested<Lhs,Rhs::RowsAtCompileTime>::type>::type LhsNested;

  typedef typename internal::conditional<TransposeRhs,
    SparseMatrix<Scalar,0,Index>,
    typename internal::nested<Rhs,Lhs::RowsAtCompileTime>::type>::type RhsNested;

  typedef SparseSparseProduct<LhsNested, RhsNested> Type;
};

namespace internal {
template<typename LhsNested, typename RhsNested>
struct traits<SparseSparseProduct<LhsNested, RhsNested> >
{
  typedef MatrixXpr XprKind;
  
  typedef typename remove_all<LhsNested>::type _LhsNested;
  typedef typename remove_all<RhsNested>::type _RhsNested;
  typedef typename _LhsNested::Scalar Scalar;
  typedef typename promote_index_type<typename traits<_LhsNested>::Index,
                                         typename traits<_RhsNested>::Index>::type Index;

  enum {
    LhsCoeffReadCost = _LhsNested::CoeffReadCost,
    RhsCoeffReadCost = _RhsNested::CoeffReadCost,
    LhsFlags = _LhsNested::Flags,
    RhsFlags = _RhsNested::Flags,

    RowsAtCompileTime    = _LhsNested::RowsAtCompileTime,
    ColsAtCompileTime    = _RhsNested::ColsAtCompileTime,
    MaxRowsAtCompileTime = _LhsNested::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = _RhsNested::MaxColsAtCompileTime,

    InnerSize = EIGEN_SIZE_MIN_PREFER_FIXED(_LhsNested::ColsAtCompileTime, _RhsNested::RowsAtCompileTime),

    EvalToRowMajor = (RhsFlags & LhsFlags & RowMajorBit),

    RemovedBits = ~(EvalToRowMajor ? 0 : RowMajorBit),

    Flags = (int(LhsFlags | RhsFlags) & HereditaryBits & RemovedBits)
          | EvalBeforeAssigningBit
          | EvalBeforeNestingBit,

    CoeffReadCost = Dynamic
  };

  typedef Sparse StorageKind;
};

} 

template<typename LhsNested, typename RhsNested>
class SparseSparseProduct : internal::no_assignment_operator,
  public SparseMatrixBase<SparseSparseProduct<LhsNested, RhsNested> >
{
  public:

    typedef SparseMatrixBase<SparseSparseProduct> Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(SparseSparseProduct)

  private:

    typedef typename internal::traits<SparseSparseProduct>::_LhsNested _LhsNested;
    typedef typename internal::traits<SparseSparseProduct>::_RhsNested _RhsNested;

  public:

    template<typename Lhs, typename Rhs>
    EIGEN_STRONG_INLINE SparseSparseProduct(const Lhs& lhs, const Rhs& rhs)
      : m_lhs(lhs), m_rhs(rhs), m_tolerance(0), m_conservative(true)
    {
      init();
    }

    template<typename Lhs, typename Rhs>
    EIGEN_STRONG_INLINE SparseSparseProduct(const Lhs& lhs, const Rhs& rhs, const RealScalar& tolerance)
      : m_lhs(lhs), m_rhs(rhs), m_tolerance(tolerance), m_conservative(false)
    {
      init();
    }

    SparseSparseProduct pruned(const Scalar& reference = 0, const RealScalar& epsilon = NumTraits<RealScalar>::dummy_precision()) const
    {
      using std::abs;
      return SparseSparseProduct(m_lhs,m_rhs,abs(reference)*epsilon);
    }

    template<typename Dest>
    void evalTo(Dest& result) const
    {
      if(m_conservative)
        internal::conservative_sparse_sparse_product_selector<_LhsNested, _RhsNested, Dest>::run(lhs(),rhs(),result);
      else
        internal::sparse_sparse_product_with_pruning_selector<_LhsNested, _RhsNested, Dest>::run(lhs(),rhs(),result,m_tolerance);
    }

    EIGEN_STRONG_INLINE Index rows() const { return m_lhs.rows(); }
    EIGEN_STRONG_INLINE Index cols() const { return m_rhs.cols(); }

    EIGEN_STRONG_INLINE const _LhsNested& lhs() const { return m_lhs; }
    EIGEN_STRONG_INLINE const _RhsNested& rhs() const { return m_rhs; }

  protected:
    void init()
    {
      eigen_assert(m_lhs.cols() == m_rhs.rows());

      enum {
        ProductIsValid = _LhsNested::ColsAtCompileTime==Dynamic
                      || _RhsNested::RowsAtCompileTime==Dynamic
                      || int(_LhsNested::ColsAtCompileTime)==int(_RhsNested::RowsAtCompileTime),
        AreVectors = _LhsNested::IsVectorAtCompileTime && _RhsNested::IsVectorAtCompileTime,
        SameSizes = EIGEN_PREDICATE_SAME_MATRIX_SIZE(_LhsNested,_RhsNested)
      };
      
      
      
      EIGEN_STATIC_ASSERT(ProductIsValid || !(AreVectors && SameSizes),
        INVALID_VECTOR_VECTOR_PRODUCT__IF_YOU_WANTED_A_DOT_OR_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTIONS)
      EIGEN_STATIC_ASSERT(ProductIsValid || !(SameSizes && !AreVectors),
        INVALID_MATRIX_PRODUCT__IF_YOU_WANTED_A_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTION)
      EIGEN_STATIC_ASSERT(ProductIsValid || SameSizes, INVALID_MATRIX_PRODUCT)
    }

    LhsNested m_lhs;
    RhsNested m_rhs;
    RealScalar m_tolerance;
    bool m_conservative;
};

template<typename Derived>
template<typename Lhs, typename Rhs>
inline Derived& SparseMatrixBase<Derived>::operator=(const SparseSparseProduct<Lhs,Rhs>& product)
{
  product.evalTo(derived());
  return derived();
}

template<typename Derived>
template<typename OtherDerived>
inline const typename SparseSparseProductReturnType<Derived,OtherDerived>::Type
SparseMatrixBase<Derived>::operator*(const SparseMatrixBase<OtherDerived> &other) const
{
  return typename SparseSparseProductReturnType<Derived,OtherDerived>::Type(derived(), other.derived());
}

} 

#endif 
