// Copyright (C) 2011 Jitse Niesen <jitse@maths.leeds.ac.uk>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_MATRIX_SQUARE_ROOT
#define EIGEN_MATRIX_SQUARE_ROOT

namespace Eigen { 

template <typename MatrixType>
class MatrixSquareRootQuasiTriangular
{
  public:

    MatrixSquareRootQuasiTriangular(const MatrixType& A) 
      : m_A(A) 
    {
      eigen_assert(A.rows() == A.cols());
    }
    
    template <typename ResultType> void compute(ResultType &result);    
    
  private:
    typedef typename MatrixType::Index Index;
    typedef typename MatrixType::Scalar Scalar;
    
    void computeDiagonalPartOfSqrt(MatrixType& sqrtT, const MatrixType& T);
    void computeOffDiagonalPartOfSqrt(MatrixType& sqrtT, const MatrixType& T);
    void compute2x2diagonalBlock(MatrixType& sqrtT, const MatrixType& T, typename MatrixType::Index i);
    void compute1x1offDiagonalBlock(MatrixType& sqrtT, const MatrixType& T, 
				  typename MatrixType::Index i, typename MatrixType::Index j);
    void compute1x2offDiagonalBlock(MatrixType& sqrtT, const MatrixType& T, 
				  typename MatrixType::Index i, typename MatrixType::Index j);
    void compute2x1offDiagonalBlock(MatrixType& sqrtT, const MatrixType& T, 
				  typename MatrixType::Index i, typename MatrixType::Index j);
    void compute2x2offDiagonalBlock(MatrixType& sqrtT, const MatrixType& T, 
				  typename MatrixType::Index i, typename MatrixType::Index j);
  
    template <typename SmallMatrixType>
    static void solveAuxiliaryEquation(SmallMatrixType& X, const SmallMatrixType& A, 
				     const SmallMatrixType& B, const SmallMatrixType& C);
  
    const MatrixType& m_A;
};

template <typename MatrixType>
template <typename ResultType> 
void MatrixSquareRootQuasiTriangular<MatrixType>::compute(ResultType &result)
{
  result.resize(m_A.rows(), m_A.cols());
  computeDiagonalPartOfSqrt(result, m_A);
  computeOffDiagonalPartOfSqrt(result, m_A);
}

template <typename MatrixType>
void MatrixSquareRootQuasiTriangular<MatrixType>::computeDiagonalPartOfSqrt(MatrixType& sqrtT, 
									  const MatrixType& T)
{
  using std::sqrt;
  const Index size = m_A.rows();
  for (Index i = 0; i < size; i++) {
    if (i == size - 1 || T.coeff(i+1, i) == 0) {
      eigen_assert(T(i,i) >= 0);
      sqrtT.coeffRef(i,i) = sqrt(T.coeff(i,i));
    }
    else {
      compute2x2diagonalBlock(sqrtT, T, i);
      ++i;
    }
  }
}

template <typename MatrixType>
void MatrixSquareRootQuasiTriangular<MatrixType>::computeOffDiagonalPartOfSqrt(MatrixType& sqrtT, 
									     const MatrixType& T)
{
  const Index size = m_A.rows();
  for (Index j = 1; j < size; j++) {
      if (T.coeff(j, j-1) != 0)  
	continue;
    for (Index i = j-1; i >= 0; i--) {
      if (i > 0 && T.coeff(i, i-1) != 0)  
	continue;
      bool iBlockIs2x2 = (i < size - 1) && (T.coeff(i+1, i) != 0);
      bool jBlockIs2x2 = (j < size - 1) && (T.coeff(j+1, j) != 0);
      if (iBlockIs2x2 && jBlockIs2x2) 
	compute2x2offDiagonalBlock(sqrtT, T, i, j);
      else if (iBlockIs2x2 && !jBlockIs2x2) 
	compute2x1offDiagonalBlock(sqrtT, T, i, j);
      else if (!iBlockIs2x2 && jBlockIs2x2) 
	compute1x2offDiagonalBlock(sqrtT, T, i, j);
      else if (!iBlockIs2x2 && !jBlockIs2x2) 
	compute1x1offDiagonalBlock(sqrtT, T, i, j);
    }
  }
}

template <typename MatrixType>
void MatrixSquareRootQuasiTriangular<MatrixType>
     ::compute2x2diagonalBlock(MatrixType& sqrtT, const MatrixType& T, typename MatrixType::Index i)
{
  
  
  Matrix<Scalar,2,2> block = T.template block<2,2>(i,i);
  EigenSolver<Matrix<Scalar,2,2> > es(block);
  sqrtT.template block<2,2>(i,i)
    = (es.eigenvectors() * es.eigenvalues().cwiseSqrt().asDiagonal() * es.eigenvectors().inverse()).real();
}

template <typename MatrixType>
void MatrixSquareRootQuasiTriangular<MatrixType>
     ::compute1x1offDiagonalBlock(MatrixType& sqrtT, const MatrixType& T, 
				  typename MatrixType::Index i, typename MatrixType::Index j)
{
  Scalar tmp = (sqrtT.row(i).segment(i+1,j-i-1) * sqrtT.col(j).segment(i+1,j-i-1)).value();
  sqrtT.coeffRef(i,j) = (T.coeff(i,j) - tmp) / (sqrtT.coeff(i,i) + sqrtT.coeff(j,j));
}

template <typename MatrixType>
void MatrixSquareRootQuasiTriangular<MatrixType>
     ::compute1x2offDiagonalBlock(MatrixType& sqrtT, const MatrixType& T, 
				  typename MatrixType::Index i, typename MatrixType::Index j)
{
  Matrix<Scalar,1,2> rhs = T.template block<1,2>(i,j);
  if (j-i > 1)
    rhs -= sqrtT.block(i, i+1, 1, j-i-1) * sqrtT.block(i+1, j, j-i-1, 2);
  Matrix<Scalar,2,2> A = sqrtT.coeff(i,i) * Matrix<Scalar,2,2>::Identity();
  A += sqrtT.template block<2,2>(j,j).transpose();
  sqrtT.template block<1,2>(i,j).transpose() = A.fullPivLu().solve(rhs.transpose());
}

template <typename MatrixType>
void MatrixSquareRootQuasiTriangular<MatrixType>
     ::compute2x1offDiagonalBlock(MatrixType& sqrtT, const MatrixType& T, 
				  typename MatrixType::Index i, typename MatrixType::Index j)
{
  Matrix<Scalar,2,1> rhs = T.template block<2,1>(i,j);
  if (j-i > 2)
    rhs -= sqrtT.block(i, i+2, 2, j-i-2) * sqrtT.block(i+2, j, j-i-2, 1);
  Matrix<Scalar,2,2> A = sqrtT.coeff(j,j) * Matrix<Scalar,2,2>::Identity();
  A += sqrtT.template block<2,2>(i,i);
  sqrtT.template block<2,1>(i,j) = A.fullPivLu().solve(rhs);
}

template <typename MatrixType>
void MatrixSquareRootQuasiTriangular<MatrixType>
     ::compute2x2offDiagonalBlock(MatrixType& sqrtT, const MatrixType& T, 
				  typename MatrixType::Index i, typename MatrixType::Index j)
{
  Matrix<Scalar,2,2> A = sqrtT.template block<2,2>(i,i);
  Matrix<Scalar,2,2> B = sqrtT.template block<2,2>(j,j);
  Matrix<Scalar,2,2> C = T.template block<2,2>(i,j);
  if (j-i > 2)
    C -= sqrtT.block(i, i+2, 2, j-i-2) * sqrtT.block(i+2, j, j-i-2, 2);
  Matrix<Scalar,2,2> X;
  solveAuxiliaryEquation(X, A, B, C);
  sqrtT.template block<2,2>(i,j) = X;
}

template <typename MatrixType>
template <typename SmallMatrixType>
void MatrixSquareRootQuasiTriangular<MatrixType>
     ::solveAuxiliaryEquation(SmallMatrixType& X, const SmallMatrixType& A,
			      const SmallMatrixType& B, const SmallMatrixType& C)
{
  EIGEN_STATIC_ASSERT((internal::is_same<SmallMatrixType, Matrix<Scalar,2,2> >::value),
		      EIGEN_INTERNAL_ERROR_PLEASE_FILE_A_BUG_REPORT);

  Matrix<Scalar,4,4> coeffMatrix = Matrix<Scalar,4,4>::Zero();
  coeffMatrix.coeffRef(0,0) = A.coeff(0,0) + B.coeff(0,0);
  coeffMatrix.coeffRef(1,1) = A.coeff(0,0) + B.coeff(1,1);
  coeffMatrix.coeffRef(2,2) = A.coeff(1,1) + B.coeff(0,0);
  coeffMatrix.coeffRef(3,3) = A.coeff(1,1) + B.coeff(1,1);
  coeffMatrix.coeffRef(0,1) = B.coeff(1,0);
  coeffMatrix.coeffRef(0,2) = A.coeff(0,1);
  coeffMatrix.coeffRef(1,0) = B.coeff(0,1);
  coeffMatrix.coeffRef(1,3) = A.coeff(0,1);
  coeffMatrix.coeffRef(2,0) = A.coeff(1,0);
  coeffMatrix.coeffRef(2,3) = B.coeff(1,0);
  coeffMatrix.coeffRef(3,1) = A.coeff(1,0);
  coeffMatrix.coeffRef(3,2) = B.coeff(0,1);
  
  Matrix<Scalar,4,1> rhs;
  rhs.coeffRef(0) = C.coeff(0,0);
  rhs.coeffRef(1) = C.coeff(0,1);
  rhs.coeffRef(2) = C.coeff(1,0);
  rhs.coeffRef(3) = C.coeff(1,1);
  
  Matrix<Scalar,4,1> result;
  result = coeffMatrix.fullPivLu().solve(rhs);

  X.coeffRef(0,0) = result.coeff(0);
  X.coeffRef(0,1) = result.coeff(1);
  X.coeffRef(1,0) = result.coeff(2);
  X.coeffRef(1,1) = result.coeff(3);
}


template <typename MatrixType>
class MatrixSquareRootTriangular
{
  public:
    MatrixSquareRootTriangular(const MatrixType& A) 
      : m_A(A) 
    {
      eigen_assert(A.rows() == A.cols());
    }

    template <typename ResultType> void compute(ResultType &result);    

 private:
    const MatrixType& m_A;
};

template <typename MatrixType>
template <typename ResultType> 
void MatrixSquareRootTriangular<MatrixType>::compute(ResultType &result)
{
  using std::sqrt;

  
  
  result.resize(m_A.rows(), m_A.cols());
  typedef typename MatrixType::Index Index;
  for (Index i = 0; i < m_A.rows(); i++) {
    result.coeffRef(i,i) = sqrt(m_A.coeff(i,i));
  }
  for (Index j = 1; j < m_A.cols(); j++) {
    for (Index i = j-1; i >= 0; i--) {
      typedef typename MatrixType::Scalar Scalar;
      
      Scalar tmp = (result.row(i).segment(i+1,j-i-1) * result.col(j).segment(i+1,j-i-1)).value();
      
      result.coeffRef(i,j) = (m_A.coeff(i,j) - tmp) / (result.coeff(i,i) + result.coeff(j,j));
    }
  }
}


template <typename MatrixType, int IsComplex = NumTraits<typename internal::traits<MatrixType>::Scalar>::IsComplex>
class MatrixSquareRoot
{
  public:

    MatrixSquareRoot(const MatrixType& A); 
    
    template <typename ResultType> void compute(ResultType &result);    
};



template <typename MatrixType>
class MatrixSquareRoot<MatrixType, 0>
{
  public:

    MatrixSquareRoot(const MatrixType& A) 
      : m_A(A) 
    {  
      eigen_assert(A.rows() == A.cols());
    }
  
    template <typename ResultType> void compute(ResultType &result)
    {
      
      const RealSchur<MatrixType> schurOfA(m_A);  
      const MatrixType& T = schurOfA.matrixT();
      const MatrixType& U = schurOfA.matrixU();
    
      
      MatrixType sqrtT = MatrixType::Zero(m_A.rows(), m_A.cols());
      MatrixSquareRootQuasiTriangular<MatrixType>(T).compute(sqrtT);
    
      
      result = U * sqrtT * U.adjoint();
    }
    
  private:
    const MatrixType& m_A;
};



template <typename MatrixType>
class MatrixSquareRoot<MatrixType, 1>
{
  public:

    MatrixSquareRoot(const MatrixType& A) 
      : m_A(A) 
    {  
      eigen_assert(A.rows() == A.cols());
    }
  
    template <typename ResultType> void compute(ResultType &result)
    {
      
      const ComplexSchur<MatrixType> schurOfA(m_A);  
      const MatrixType& T = schurOfA.matrixT();
      const MatrixType& U = schurOfA.matrixU();
    
      
      MatrixType sqrtT;
      MatrixSquareRootTriangular<MatrixType>(T).compute(sqrtT);
    
      
      result = U * (sqrtT.template triangularView<Upper>() * U.adjoint());
    }
    
  private:
    const MatrixType& m_A;
};


template<typename Derived> class MatrixSquareRootReturnValue
: public ReturnByValue<MatrixSquareRootReturnValue<Derived> >
{
    typedef typename Derived::Index Index;
  public:
    MatrixSquareRootReturnValue(const Derived& src) : m_src(src) { }

    template <typename ResultType>
    inline void evalTo(ResultType& result) const
    {
      const typename Derived::PlainObject srcEvaluated = m_src.eval();
      MatrixSquareRoot<typename Derived::PlainObject> me(srcEvaluated);
      me.compute(result);
    }

    Index rows() const { return m_src.rows(); }
    Index cols() const { return m_src.cols(); }

  protected:
    const Derived& m_src;
  private:
    MatrixSquareRootReturnValue& operator=(const MatrixSquareRootReturnValue&);
};

namespace internal {
template<typename Derived>
struct traits<MatrixSquareRootReturnValue<Derived> >
{
  typedef typename Derived::PlainObject ReturnType;
};
}

template <typename Derived>
const MatrixSquareRootReturnValue<Derived> MatrixBase<Derived>::sqrt() const
{
  eigen_assert(rows() == cols());
  return MatrixSquareRootReturnValue<Derived>(derived());
}

} 

#endif 
