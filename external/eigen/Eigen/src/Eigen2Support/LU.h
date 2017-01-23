// Copyright (C) 2011 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN2_LU_H
#define EIGEN2_LU_H

namespace Eigen { 

template<typename MatrixType>
class LU : public FullPivLU<MatrixType>
{
  public:

    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef Matrix<int, 1, MatrixType::ColsAtCompileTime, MatrixType::Options, 1, MatrixType::MaxColsAtCompileTime> IntRowVectorType;
    typedef Matrix<int, MatrixType::RowsAtCompileTime, 1, MatrixType::Options, MatrixType::MaxRowsAtCompileTime, 1> IntColVectorType;
    typedef Matrix<Scalar, 1, MatrixType::ColsAtCompileTime, MatrixType::Options, 1, MatrixType::MaxColsAtCompileTime> RowVectorType;
    typedef Matrix<Scalar, MatrixType::RowsAtCompileTime, 1, MatrixType::Options, MatrixType::MaxRowsAtCompileTime, 1> ColVectorType;

    typedef Matrix<typename MatrixType::Scalar,
                  MatrixType::ColsAtCompileTime, 
                                                 
                  Dynamic,                       
                  MatrixType::Options,
                  MatrixType::MaxColsAtCompileTime, 
                  MatrixType::MaxColsAtCompileTime 
                                                   
    > KernelResultType;

    typedef Matrix<typename MatrixType::Scalar,
                   MatrixType::RowsAtCompileTime, 
                                                  
                   Dynamic,                       
                   MatrixType::Options,
                   MatrixType::MaxRowsAtCompileTime, 
                   MatrixType::MaxColsAtCompileTime  
    > ImageResultType;

    typedef FullPivLU<MatrixType> Base;

    template<typename T>
    explicit LU(const T& t) : Base(t), m_originalMatrix(t) {}

    template<typename OtherDerived, typename ResultType>
    bool solve(const MatrixBase<OtherDerived>& b, ResultType *result) const
    {
      *result = static_cast<const Base*>(this)->solve(b);
      return true;
    }

    template<typename ResultType>
    inline void computeInverse(ResultType *result) const
    {
      solve(MatrixType::Identity(this->rows(), this->cols()), result);
    }
    
    template<typename KernelMatrixType>
    void computeKernel(KernelMatrixType *result) const
    {
      *result = static_cast<const Base*>(this)->kernel();
    }
    
    template<typename ImageMatrixType>
    void computeImage(ImageMatrixType *result) const
    {
      *result = static_cast<const Base*>(this)->image(m_originalMatrix);
    }
    
    const ImageResultType image() const
    {
      return static_cast<const Base*>(this)->image(m_originalMatrix);
    }
    
    const MatrixType& m_originalMatrix;
};

#if EIGEN2_SUPPORT_STAGE < STAGE20_RESOLVE_API_CONFLICTS
template<typename Derived>
inline const LU<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::lu() const
{
  return LU<PlainObject>(eval());
}
#endif

#ifdef EIGEN2_SUPPORT
template<typename Derived>
inline const LU<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::eigen2_lu() const
{
  return LU<PlainObject>(eval());
}
#endif

} 

#endif 
