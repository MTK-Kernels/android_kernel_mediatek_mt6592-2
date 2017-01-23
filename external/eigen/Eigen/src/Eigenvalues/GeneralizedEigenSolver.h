// Copyright (C) 2012 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2010,2012 Jitse Niesen <jitse@maths.leeds.ac.uk>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_GENERALIZEDEIGENSOLVER_H
#define EIGEN_GENERALIZEDEIGENSOLVER_H

#include "./RealQZ.h"

namespace Eigen { 

template<typename _MatrixType> class GeneralizedEigenSolver
{
  public:

    
    typedef _MatrixType MatrixType;

    enum {
      RowsAtCompileTime = MatrixType::RowsAtCompileTime,
      ColsAtCompileTime = MatrixType::ColsAtCompileTime,
      Options = MatrixType::Options,
      MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
      MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime
    };

    
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;
    typedef typename MatrixType::Index Index;

    typedef std::complex<RealScalar> ComplexScalar;

    typedef Matrix<Scalar, ColsAtCompileTime, 1, Options & ~RowMajor, MaxColsAtCompileTime, 1> VectorType;

    typedef Matrix<ComplexScalar, ColsAtCompileTime, 1, Options & ~RowMajor, MaxColsAtCompileTime, 1> ComplexVectorType;

    typedef CwiseBinaryOp<internal::scalar_quotient_op<ComplexScalar,Scalar>,ComplexVectorType,VectorType> EigenvalueType;

    typedef Matrix<ComplexScalar, RowsAtCompileTime, ColsAtCompileTime, Options, MaxRowsAtCompileTime, MaxColsAtCompileTime> EigenvectorsType;

    GeneralizedEigenSolver() : m_eivec(), m_alphas(), m_betas(), m_isInitialized(false), m_realQZ(), m_matS(), m_tmp() {}

    GeneralizedEigenSolver(Index size)
      : m_eivec(size, size),
        m_alphas(size),
        m_betas(size),
        m_isInitialized(false),
        m_eigenvectorsOk(false),
        m_realQZ(size),
        m_matS(size, size),
        m_tmp(size)
    {}

    GeneralizedEigenSolver(const MatrixType& A, const MatrixType& B, bool computeEigenvectors = true)
      : m_eivec(A.rows(), A.cols()),
        m_alphas(A.cols()),
        m_betas(A.cols()),
        m_isInitialized(false),
        m_eigenvectorsOk(false),
        m_realQZ(A.cols()),
        m_matS(A.rows(), A.cols()),
        m_tmp(A.cols())
    {
      compute(A, B, computeEigenvectors);
    }


    EigenvalueType eigenvalues() const
    {
      eigen_assert(m_isInitialized && "GeneralizedEigenSolver is not initialized.");
      return EigenvalueType(m_alphas,m_betas);
    }

    ComplexVectorType alphas() const
    {
      eigen_assert(m_isInitialized && "GeneralizedEigenSolver is not initialized.");
      return m_alphas;
    }

    VectorType betas() const
    {
      eigen_assert(m_isInitialized && "GeneralizedEigenSolver is not initialized.");
      return m_betas;
    }

    GeneralizedEigenSolver& compute(const MatrixType& A, const MatrixType& B, bool computeEigenvectors = true);

    ComputationInfo info() const
    {
      eigen_assert(m_isInitialized && "EigenSolver is not initialized.");
      return m_realQZ.info();
    }

    GeneralizedEigenSolver& setMaxIterations(Index maxIters)
    {
      m_realQZ.setMaxIterations(maxIters);
      return *this;
    }

  protected:
    MatrixType m_eivec;
    ComplexVectorType m_alphas;
    VectorType m_betas;
    bool m_isInitialized;
    bool m_eigenvectorsOk;
    RealQZ<MatrixType> m_realQZ;
    MatrixType m_matS;

    typedef Matrix<Scalar, ColsAtCompileTime, 1, Options & ~RowMajor, MaxColsAtCompileTime, 1> ColumnVectorType;
    ColumnVectorType m_tmp;
};


template<typename MatrixType>
GeneralizedEigenSolver<MatrixType>&
GeneralizedEigenSolver<MatrixType>::compute(const MatrixType& A, const MatrixType& B, bool computeEigenvectors)
{
  using std::sqrt;
  using std::abs;
  eigen_assert(A.cols() == A.rows() && B.cols() == A.rows() && B.cols() == B.rows());

  
  
  m_realQZ.compute(A, B, computeEigenvectors);

  if (m_realQZ.info() == Success)
  {
    m_matS = m_realQZ.matrixS();
    if (computeEigenvectors)
      m_eivec = m_realQZ.matrixZ().transpose();
  
    
    m_alphas.resize(A.cols());
    m_betas.resize(A.cols());
    Index i = 0;
    while (i < A.cols())
    {
      if (i == A.cols() - 1 || m_matS.coeff(i+1, i) == Scalar(0))
      {
        m_alphas.coeffRef(i) = m_matS.coeff(i, i);
        m_betas.coeffRef(i)  = m_realQZ.matrixT().coeff(i,i);
        ++i;
      }
      else
      {
        Scalar p = Scalar(0.5) * (m_matS.coeff(i, i) - m_matS.coeff(i+1, i+1));
        Scalar z = sqrt(abs(p * p + m_matS.coeff(i+1, i) * m_matS.coeff(i, i+1)));
        m_alphas.coeffRef(i)   = ComplexScalar(m_matS.coeff(i+1, i+1) + p, z);
        m_alphas.coeffRef(i+1) = ComplexScalar(m_matS.coeff(i+1, i+1) + p, -z);

        m_betas.coeffRef(i)   = m_realQZ.matrixT().coeff(i,i);
        m_betas.coeffRef(i+1) = m_realQZ.matrixT().coeff(i,i);
        i += 2;
      }
    }
  }

  m_isInitialized = true;
  m_eigenvectorsOk = false;

  return *this;
}

} 

#endif 
