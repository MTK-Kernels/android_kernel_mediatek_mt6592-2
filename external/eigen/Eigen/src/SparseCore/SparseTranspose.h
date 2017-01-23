// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SPARSETRANSPOSE_H
#define EIGEN_SPARSETRANSPOSE_H

namespace Eigen { 

template<typename MatrixType> class TransposeImpl<MatrixType,Sparse>
  : public SparseMatrixBase<Transpose<MatrixType> >
{
    typedef typename internal::remove_all<typename MatrixType::Nested>::type _MatrixTypeNested;
  public:

    EIGEN_SPARSE_PUBLIC_INTERFACE(Transpose<MatrixType> )

    class InnerIterator;
    class ReverseInnerIterator;

    inline Index nonZeros() const { return derived().nestedExpression().nonZeros(); }
};

template<typename MatrixType> class TransposeImpl<MatrixType,Sparse>::InnerIterator
  : public _MatrixTypeNested::InnerIterator
{
    typedef typename _MatrixTypeNested::InnerIterator Base;
    typedef typename TransposeImpl::Index Index;
  public:

    EIGEN_STRONG_INLINE InnerIterator(const TransposeImpl& trans, typename TransposeImpl<MatrixType,Sparse>::Index outer)
      : Base(trans.derived().nestedExpression(), outer)
    {}
    typename TransposeImpl<MatrixType,Sparse>::Index row() const { return Base::col(); }
    typename TransposeImpl<MatrixType,Sparse>::Index col() const { return Base::row(); }
};

template<typename MatrixType> class TransposeImpl<MatrixType,Sparse>::ReverseInnerIterator
  : public _MatrixTypeNested::ReverseInnerIterator
{
    typedef typename _MatrixTypeNested::ReverseInnerIterator Base;
    typedef typename TransposeImpl::Index Index;
  public:

    EIGEN_STRONG_INLINE ReverseInnerIterator(const TransposeImpl& xpr, typename TransposeImpl<MatrixType,Sparse>::Index outer)
      : Base(xpr.derived().nestedExpression(), outer)
    {}
    typename TransposeImpl<MatrixType,Sparse>::Index row() const { return Base::col(); }
    typename TransposeImpl<MatrixType,Sparse>::Index col() const { return Base::row(); }
};

} 

#endif 
