// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_HOUSEHOLDER_SEQUENCE_H
#define EIGEN_HOUSEHOLDER_SEQUENCE_H

namespace Eigen { 


namespace internal {

template<typename VectorsType, typename CoeffsType, int Side>
struct traits<HouseholderSequence<VectorsType,CoeffsType,Side> >
{
  typedef typename VectorsType::Scalar Scalar;
  typedef typename VectorsType::Index Index;
  typedef typename VectorsType::StorageKind StorageKind;
  enum {
    RowsAtCompileTime = Side==OnTheLeft ? traits<VectorsType>::RowsAtCompileTime
                                        : traits<VectorsType>::ColsAtCompileTime,
    ColsAtCompileTime = RowsAtCompileTime,
    MaxRowsAtCompileTime = Side==OnTheLeft ? traits<VectorsType>::MaxRowsAtCompileTime
                                           : traits<VectorsType>::MaxColsAtCompileTime,
    MaxColsAtCompileTime = MaxRowsAtCompileTime,
    Flags = 0
  };
};

template<typename VectorsType, typename CoeffsType, int Side>
struct hseq_side_dependent_impl
{
  typedef Block<const VectorsType, Dynamic, 1> EssentialVectorType;
  typedef HouseholderSequence<VectorsType, CoeffsType, OnTheLeft> HouseholderSequenceType;
  typedef typename VectorsType::Index Index;
  static inline const EssentialVectorType essentialVector(const HouseholderSequenceType& h, Index k)
  {
    Index start = k+1+h.m_shift;
    return Block<const VectorsType,Dynamic,1>(h.m_vectors, start, k, h.rows()-start, 1);
  }
};

template<typename VectorsType, typename CoeffsType>
struct hseq_side_dependent_impl<VectorsType, CoeffsType, OnTheRight>
{
  typedef Transpose<Block<const VectorsType, 1, Dynamic> > EssentialVectorType;
  typedef HouseholderSequence<VectorsType, CoeffsType, OnTheRight> HouseholderSequenceType;
  typedef typename VectorsType::Index Index;
  static inline const EssentialVectorType essentialVector(const HouseholderSequenceType& h, Index k)
  {
    Index start = k+1+h.m_shift;
    return Block<const VectorsType,1,Dynamic>(h.m_vectors, k, start, 1, h.rows()-start).transpose();
  }
};

template<typename OtherScalarType, typename MatrixType> struct matrix_type_times_scalar_type
{
  typedef typename scalar_product_traits<OtherScalarType, typename MatrixType::Scalar>::ReturnType
    ResultScalar;
  typedef Matrix<ResultScalar, MatrixType::RowsAtCompileTime, MatrixType::ColsAtCompileTime,
                 0, MatrixType::MaxRowsAtCompileTime, MatrixType::MaxColsAtCompileTime> Type;
};

} 

template<typename VectorsType, typename CoeffsType, int Side> class HouseholderSequence
  : public EigenBase<HouseholderSequence<VectorsType,CoeffsType,Side> >
{
    typedef typename internal::hseq_side_dependent_impl<VectorsType,CoeffsType,Side>::EssentialVectorType EssentialVectorType;
  
  public:
    enum {
      RowsAtCompileTime = internal::traits<HouseholderSequence>::RowsAtCompileTime,
      ColsAtCompileTime = internal::traits<HouseholderSequence>::ColsAtCompileTime,
      MaxRowsAtCompileTime = internal::traits<HouseholderSequence>::MaxRowsAtCompileTime,
      MaxColsAtCompileTime = internal::traits<HouseholderSequence>::MaxColsAtCompileTime
    };
    typedef typename internal::traits<HouseholderSequence>::Scalar Scalar;
    typedef typename VectorsType::Index Index;

    typedef HouseholderSequence<
      typename internal::conditional<NumTraits<Scalar>::IsComplex,
        typename internal::remove_all<typename VectorsType::ConjugateReturnType>::type,
        VectorsType>::type,
      typename internal::conditional<NumTraits<Scalar>::IsComplex,
        typename internal::remove_all<typename CoeffsType::ConjugateReturnType>::type,
        CoeffsType>::type,
      Side
    > ConjugateReturnType;

    HouseholderSequence(const VectorsType& v, const CoeffsType& h)
      : m_vectors(v), m_coeffs(h), m_trans(false), m_length(v.diagonalSize()),
        m_shift(0)
    {
    }

    
    HouseholderSequence(const HouseholderSequence& other)
      : m_vectors(other.m_vectors),
        m_coeffs(other.m_coeffs),
        m_trans(other.m_trans),
        m_length(other.m_length),
        m_shift(other.m_shift)
    {
    }

    Index rows() const { return Side==OnTheLeft ? m_vectors.rows() : m_vectors.cols(); }

    Index cols() const { return rows(); }

    const EssentialVectorType essentialVector(Index k) const
    {
      eigen_assert(k >= 0 && k < m_length);
      return internal::hseq_side_dependent_impl<VectorsType,CoeffsType,Side>::essentialVector(*this, k);
    }

    
    HouseholderSequence transpose() const
    {
      return HouseholderSequence(*this).setTrans(!m_trans);
    }

    
    ConjugateReturnType conjugate() const
    {
      return ConjugateReturnType(m_vectors.conjugate(), m_coeffs.conjugate())
             .setTrans(m_trans)
             .setLength(m_length)
             .setShift(m_shift);
    }

    
    ConjugateReturnType adjoint() const
    {
      return conjugate().setTrans(!m_trans);
    }

    
    ConjugateReturnType inverse() const { return adjoint(); }

    
    template<typename DestType> inline void evalTo(DestType& dst) const
    {
      Matrix<Scalar, DestType::RowsAtCompileTime, 1,
             AutoAlign|ColMajor, DestType::MaxRowsAtCompileTime, 1> workspace(rows());
      evalTo(dst, workspace);
    }

    
    template<typename Dest, typename Workspace>
    void evalTo(Dest& dst, Workspace& workspace) const
    {
      workspace.resize(rows());
      Index vecs = m_length;
      if(    internal::is_same<typename internal::remove_all<VectorsType>::type,Dest>::value
          && internal::extract_data(dst) == internal::extract_data(m_vectors))
      {
        
        dst.diagonal().setOnes();
        dst.template triangularView<StrictlyUpper>().setZero();
        for(Index k = vecs-1; k >= 0; --k)
        {
          Index cornerSize = rows() - k - m_shift;
          if(m_trans)
            dst.bottomRightCorner(cornerSize, cornerSize)
               .applyHouseholderOnTheRight(essentialVector(k), m_coeffs.coeff(k), workspace.data());
          else
            dst.bottomRightCorner(cornerSize, cornerSize)
               .applyHouseholderOnTheLeft(essentialVector(k), m_coeffs.coeff(k), workspace.data());

          
          dst.col(k).tail(rows()-k-1).setZero();
        }
        
        for(Index k = 0; k<cols()-vecs ; ++k)
          dst.col(k).tail(rows()-k-1).setZero();
      }
      else
      {
        dst.setIdentity(rows(), rows());
        for(Index k = vecs-1; k >= 0; --k)
        {
          Index cornerSize = rows() - k - m_shift;
          if(m_trans)
            dst.bottomRightCorner(cornerSize, cornerSize)
               .applyHouseholderOnTheRight(essentialVector(k), m_coeffs.coeff(k), &workspace.coeffRef(0));
          else
            dst.bottomRightCorner(cornerSize, cornerSize)
               .applyHouseholderOnTheLeft(essentialVector(k), m_coeffs.coeff(k), &workspace.coeffRef(0));
        }
      }
    }

    
    template<typename Dest> inline void applyThisOnTheRight(Dest& dst) const
    {
      Matrix<Scalar,1,Dest::RowsAtCompileTime,RowMajor,1,Dest::MaxRowsAtCompileTime> workspace(dst.rows());
      applyThisOnTheRight(dst, workspace);
    }

    
    template<typename Dest, typename Workspace>
    inline void applyThisOnTheRight(Dest& dst, Workspace& workspace) const
    {
      workspace.resize(dst.rows());
      for(Index k = 0; k < m_length; ++k)
      {
        Index actual_k = m_trans ? m_length-k-1 : k;
        dst.rightCols(rows()-m_shift-actual_k)
           .applyHouseholderOnTheRight(essentialVector(actual_k), m_coeffs.coeff(actual_k), workspace.data());
      }
    }

    
    template<typename Dest> inline void applyThisOnTheLeft(Dest& dst) const
    {
      Matrix<Scalar,1,Dest::ColsAtCompileTime,RowMajor,1,Dest::MaxColsAtCompileTime> workspace(dst.cols());
      applyThisOnTheLeft(dst, workspace);
    }

    
    template<typename Dest, typename Workspace>
    inline void applyThisOnTheLeft(Dest& dst, Workspace& workspace) const
    {
      workspace.resize(dst.cols());
      for(Index k = 0; k < m_length; ++k)
      {
        Index actual_k = m_trans ? k : m_length-k-1;
        dst.bottomRows(rows()-m_shift-actual_k)
           .applyHouseholderOnTheLeft(essentialVector(actual_k), m_coeffs.coeff(actual_k), workspace.data());
      }
    }

    template<typename OtherDerived>
    typename internal::matrix_type_times_scalar_type<Scalar, OtherDerived>::Type operator*(const MatrixBase<OtherDerived>& other) const
    {
      typename internal::matrix_type_times_scalar_type<Scalar, OtherDerived>::Type
        res(other.template cast<typename internal::matrix_type_times_scalar_type<Scalar,OtherDerived>::ResultScalar>());
      applyThisOnTheLeft(res);
      return res;
    }

    template<typename _VectorsType, typename _CoeffsType, int _Side> friend struct internal::hseq_side_dependent_impl;

    HouseholderSequence& setLength(Index length)
    {
      m_length = length;
      return *this;
    }

    HouseholderSequence& setShift(Index shift)
    {
      m_shift = shift;
      return *this;
    }

    Index length() const { return m_length; }  
    Index shift() const { return m_shift; }    

    
    template <typename VectorsType2, typename CoeffsType2, int Side2> friend class HouseholderSequence;

  protected:

    HouseholderSequence& setTrans(bool trans)
    {
      m_trans = trans;
      return *this;
    }

    bool trans() const { return m_trans; }     

    typename VectorsType::Nested m_vectors;
    typename CoeffsType::Nested m_coeffs;
    bool m_trans;
    Index m_length;
    Index m_shift;
};

template<typename OtherDerived, typename VectorsType, typename CoeffsType, int Side>
typename internal::matrix_type_times_scalar_type<typename VectorsType::Scalar,OtherDerived>::Type operator*(const MatrixBase<OtherDerived>& other, const HouseholderSequence<VectorsType,CoeffsType,Side>& h)
{
  typename internal::matrix_type_times_scalar_type<typename VectorsType::Scalar,OtherDerived>::Type
    res(other.template cast<typename internal::matrix_type_times_scalar_type<typename VectorsType::Scalar,OtherDerived>::ResultScalar>());
  h.applyThisOnTheRight(res);
  return res;
}

template<typename VectorsType, typename CoeffsType>
HouseholderSequence<VectorsType,CoeffsType> householderSequence(const VectorsType& v, const CoeffsType& h)
{
  return HouseholderSequence<VectorsType,CoeffsType,OnTheLeft>(v, h);
}

template<typename VectorsType, typename CoeffsType>
HouseholderSequence<VectorsType,CoeffsType,OnTheRight> rightHouseholderSequence(const VectorsType& v, const CoeffsType& h)
{
  return HouseholderSequence<VectorsType,CoeffsType,OnTheRight>(v, h);
}

} 

#endif 
