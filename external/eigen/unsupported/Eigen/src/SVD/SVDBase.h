// Copyright (C) 2009-2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2013 Gauthier Brun <brun.gauthier@gmail.com>
// Copyright (C) 2013 Nicolas Carre <nicolas.carre@ensimag.fr>
// Copyright (C) 2013 Jean Ceccato <jean.ceccato@ensimag.fr>
// Copyright (C) 2013 Pierre Zoppitelli <pierre.zoppitelli@ensimag.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SVD_H
#define EIGEN_SVD_H

namespace Eigen {
template<typename _MatrixType> 
class SVDBase
{

public:
  typedef _MatrixType MatrixType;
  typedef typename MatrixType::Scalar Scalar;
  typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
  typedef typename MatrixType::Index Index;
  enum {
    RowsAtCompileTime = MatrixType::RowsAtCompileTime,
    ColsAtCompileTime = MatrixType::ColsAtCompileTime,
    DiagSizeAtCompileTime = EIGEN_SIZE_MIN_PREFER_DYNAMIC(RowsAtCompileTime,ColsAtCompileTime),
    MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime,
    MaxDiagSizeAtCompileTime = EIGEN_SIZE_MIN_PREFER_FIXED(MaxRowsAtCompileTime,MaxColsAtCompileTime),
    MatrixOptions = MatrixType::Options
  };

  typedef Matrix<Scalar, RowsAtCompileTime, RowsAtCompileTime,
		 MatrixOptions, MaxRowsAtCompileTime, MaxRowsAtCompileTime>
  MatrixUType;
  typedef Matrix<Scalar, ColsAtCompileTime, ColsAtCompileTime,
		 MatrixOptions, MaxColsAtCompileTime, MaxColsAtCompileTime>
  MatrixVType;
  typedef typename internal::plain_diag_type<MatrixType, RealScalar>::type SingularValuesType;
  typedef typename internal::plain_row_type<MatrixType>::type RowType;
  typedef typename internal::plain_col_type<MatrixType>::type ColType;
  typedef Matrix<Scalar, DiagSizeAtCompileTime, DiagSizeAtCompileTime,
		 MatrixOptions, MaxDiagSizeAtCompileTime, MaxDiagSizeAtCompileTime>
  WorkMatrixType;
	



  SVDBase& compute(const MatrixType& matrix, unsigned int computationOptions);

  
  SVDBase& compute(const MatrixType& matrix);

  const MatrixUType& matrixU() const
  {
    eigen_assert(m_isInitialized && "SVD is not initialized.");
    eigen_assert(computeU() && "This SVD decomposition didn't compute U. Did you ask for it?");
    return m_matrixU;
  }

  const MatrixVType& matrixV() const
  {
    eigen_assert(m_isInitialized && "SVD is not initialized.");
    eigen_assert(computeV() && "This SVD decomposition didn't compute V. Did you ask for it?");
    return m_matrixV;
  }

  const SingularValuesType& singularValues() const
  {
    eigen_assert(m_isInitialized && "SVD is not initialized.");
    return m_singularValues;
  }

  

  
  Index nonzeroSingularValues() const
  {
    eigen_assert(m_isInitialized && "SVD is not initialized.");
    return m_nonzeroSingularValues;
  }


  
  inline bool computeU() const { return m_computeFullU || m_computeThinU; }
  
  inline bool computeV() const { return m_computeFullV || m_computeThinV; }


  inline Index rows() const { return m_rows; }
  inline Index cols() const { return m_cols; }


protected:
  
  bool allocate(Index rows, Index cols, unsigned int computationOptions) ;

  MatrixUType m_matrixU;
  MatrixVType m_matrixV;
  SingularValuesType m_singularValues;
  bool m_isInitialized, m_isAllocated;
  bool m_computeFullU, m_computeThinU;
  bool m_computeFullV, m_computeThinV;
  unsigned int m_computationOptions;
  Index m_nonzeroSingularValues, m_rows, m_cols, m_diagSize;


  SVDBase()
    : m_isInitialized(false),
      m_isAllocated(false),
      m_computationOptions(0),
      m_rows(-1), m_cols(-1)
  {}


};


template<typename MatrixType>
bool SVDBase<MatrixType>::allocate(Index rows, Index cols, unsigned int computationOptions)
{
  eigen_assert(rows >= 0 && cols >= 0);

  if (m_isAllocated &&
      rows == m_rows &&
      cols == m_cols &&
      computationOptions == m_computationOptions)
  {
    return true;
  }

  m_rows = rows;
  m_cols = cols;
  m_isInitialized = false;
  m_isAllocated = true;
  m_computationOptions = computationOptions;
  m_computeFullU = (computationOptions & ComputeFullU) != 0;
  m_computeThinU = (computationOptions & ComputeThinU) != 0;
  m_computeFullV = (computationOptions & ComputeFullV) != 0;
  m_computeThinV = (computationOptions & ComputeThinV) != 0;
  eigen_assert(!(m_computeFullU && m_computeThinU) && "SVDBase: you can't ask for both full and thin U");
  eigen_assert(!(m_computeFullV && m_computeThinV) && "SVDBase: you can't ask for both full and thin V");
  eigen_assert(EIGEN_IMPLIES(m_computeThinU || m_computeThinV, MatrixType::ColsAtCompileTime==Dynamic) &&
	       "SVDBase: thin U and V are only available when your matrix has a dynamic number of columns.");

  m_diagSize = (std::min)(m_rows, m_cols);
  m_singularValues.resize(m_diagSize);
  if(RowsAtCompileTime==Dynamic)
    m_matrixU.resize(m_rows, m_computeFullU ? m_rows
		     : m_computeThinU ? m_diagSize
		     : 0);
  if(ColsAtCompileTime==Dynamic)
    m_matrixV.resize(m_cols, m_computeFullV ? m_cols
		     : m_computeThinV ? m_diagSize
		     : 0);

  return false;
}

}

#endif 
