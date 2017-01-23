// Copyright (C) 2009-2011 Jitse Niesen <jitse@maths.leeds.ac.uk>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_MATRIX_FUNCTION
#define EIGEN_MATRIX_FUNCTION

#include "StemFunction.h"
#include "MatrixFunctionAtomic.h"


namespace Eigen { 

template <typename MatrixType, 
	  typename AtomicType,  
          int IsComplex = NumTraits<typename internal::traits<MatrixType>::Scalar>::IsComplex>
class MatrixFunction
{  
  public:

    MatrixFunction(const MatrixType& A, AtomicType& atomic);

    template <typename ResultType> 
    void compute(ResultType &result);    
};


template <typename MatrixType, typename AtomicType>
class MatrixFunction<MatrixType, AtomicType, 0>
{  
  private:

    typedef internal::traits<MatrixType> Traits;
    typedef typename Traits::Scalar Scalar;
    static const int Rows = Traits::RowsAtCompileTime;
    static const int Cols = Traits::ColsAtCompileTime;
    static const int Options = MatrixType::Options;
    static const int MaxRows = Traits::MaxRowsAtCompileTime;
    static const int MaxCols = Traits::MaxColsAtCompileTime;

    typedef std::complex<Scalar> ComplexScalar;
    typedef Matrix<ComplexScalar, Rows, Cols, Options, MaxRows, MaxCols> ComplexMatrix;

  public:

    MatrixFunction(const MatrixType& A, AtomicType& atomic) : m_A(A), m_atomic(atomic) { }

    template <typename ResultType>
    void compute(ResultType& result) 
    {
      ComplexMatrix CA = m_A.template cast<ComplexScalar>();
      ComplexMatrix Cresult;
      MatrixFunction<ComplexMatrix, AtomicType> mf(CA, m_atomic);
      mf.compute(Cresult);
      result = Cresult.real();
    }

  private:
    typename internal::nested<MatrixType>::type m_A; 
    AtomicType& m_atomic; 

    MatrixFunction& operator=(const MatrixFunction&);
};

      
template <typename MatrixType, typename AtomicType>
class MatrixFunction<MatrixType, AtomicType, 1>
{
  private:

    typedef internal::traits<MatrixType> Traits;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;
    static const int RowsAtCompileTime = Traits::RowsAtCompileTime;
    static const int ColsAtCompileTime = Traits::ColsAtCompileTime;
    static const int Options = MatrixType::Options;
    typedef typename NumTraits<Scalar>::Real RealScalar;
    typedef Matrix<Scalar, Traits::RowsAtCompileTime, 1> VectorType;
    typedef Matrix<Index, Traits::RowsAtCompileTime, 1> IntVectorType;
    typedef Matrix<Index, Dynamic, 1> DynamicIntVectorType;
    typedef std::list<Scalar> Cluster;
    typedef std::list<Cluster> ListOfClusters;
    typedef Matrix<Scalar, Dynamic, Dynamic, Options, RowsAtCompileTime, ColsAtCompileTime> DynMatrixType;

  public:

    MatrixFunction(const MatrixType& A, AtomicType& atomic);
    template <typename ResultType> void compute(ResultType& result);

  private:

    void computeSchurDecomposition();
    void partitionEigenvalues();
    typename ListOfClusters::iterator findCluster(Scalar key);
    void computeClusterSize();
    void computeBlockStart();
    void constructPermutation();
    void permuteSchur();
    void swapEntriesInSchur(Index index);
    void computeBlockAtomic();
    Block<MatrixType> block(MatrixType& A, Index i, Index j);
    void computeOffDiagonal();
    DynMatrixType solveTriangularSylvester(const DynMatrixType& A, const DynMatrixType& B, const DynMatrixType& C);

    typename internal::nested<MatrixType>::type m_A; 
    AtomicType& m_atomic; 
    MatrixType m_T; 
    MatrixType m_U; 
    MatrixType m_fT; 
    ListOfClusters m_clusters; 
    DynamicIntVectorType m_eivalToCluster; 
    DynamicIntVectorType m_clusterSize; 
    DynamicIntVectorType m_blockStart; 
    IntVectorType m_permutation; 

    static const RealScalar separation() { return static_cast<RealScalar>(0.1); }

    MatrixFunction& operator=(const MatrixFunction&);
};

template <typename MatrixType, typename AtomicType>
MatrixFunction<MatrixType,AtomicType,1>::MatrixFunction(const MatrixType& A, AtomicType& atomic)
  : m_A(A), m_atomic(atomic)
{
  
}

template <typename MatrixType, typename AtomicType>
template <typename ResultType>
void MatrixFunction<MatrixType,AtomicType,1>::compute(ResultType& result) 
{
  computeSchurDecomposition();
  partitionEigenvalues();
  computeClusterSize();
  computeBlockStart();
  constructPermutation();
  permuteSchur();
  computeBlockAtomic();
  computeOffDiagonal();
  result = m_U * (m_fT.template triangularView<Upper>() * m_U.adjoint());
}

template <typename MatrixType, typename AtomicType>
void MatrixFunction<MatrixType,AtomicType,1>::computeSchurDecomposition()
{
  const ComplexSchur<MatrixType> schurOfA(m_A);  
  m_T = schurOfA.matrixT();
  m_U = schurOfA.matrixU();
}

template <typename MatrixType, typename AtomicType>
void MatrixFunction<MatrixType,AtomicType,1>::partitionEigenvalues()
{
  using std::abs;
  const Index rows = m_T.rows();
  VectorType diag = m_T.diagonal(); 

  for (Index i=0; i<rows; ++i) {
    
    typename ListOfClusters::iterator qi = findCluster(diag(i));
    if (qi == m_clusters.end()) {
      Cluster l;
      l.push_back(diag(i));
      m_clusters.push_back(l);
      qi = m_clusters.end();
      --qi;
    }

    
    for (Index j=i+1; j<rows; ++j) {
      if (abs(diag(j) - diag(i)) <= separation() && std::find(qi->begin(), qi->end(), diag(j)) == qi->end()) {
        typename ListOfClusters::iterator qj = findCluster(diag(j));
        if (qj == m_clusters.end()) {
          qi->push_back(diag(j));
        } else {
          qi->insert(qi->end(), qj->begin(), qj->end());
          m_clusters.erase(qj);
        }
      }
    }
  }
}

template <typename MatrixType, typename AtomicType>
typename MatrixFunction<MatrixType,AtomicType,1>::ListOfClusters::iterator MatrixFunction<MatrixType,AtomicType,1>::findCluster(Scalar key)
{
  typename Cluster::iterator j;
  for (typename ListOfClusters::iterator i = m_clusters.begin(); i != m_clusters.end(); ++i) {
    j = std::find(i->begin(), i->end(), key);
    if (j != i->end())
      return i;
  }
  return m_clusters.end();
}

template <typename MatrixType, typename AtomicType>
void MatrixFunction<MatrixType,AtomicType,1>::computeClusterSize()
{
  const Index rows = m_T.rows();
  VectorType diag = m_T.diagonal(); 
  const Index numClusters = static_cast<Index>(m_clusters.size());

  m_clusterSize.setZero(numClusters);
  m_eivalToCluster.resize(rows);
  Index clusterIndex = 0;
  for (typename ListOfClusters::const_iterator cluster = m_clusters.begin(); cluster != m_clusters.end(); ++cluster) {
    for (Index i = 0; i < diag.rows(); ++i) {
      if (std::find(cluster->begin(), cluster->end(), diag(i)) != cluster->end()) {
        ++m_clusterSize[clusterIndex];
        m_eivalToCluster[i] = clusterIndex;
      }
    }
    ++clusterIndex;
  }
}

template <typename MatrixType, typename AtomicType>
void MatrixFunction<MatrixType,AtomicType,1>::computeBlockStart()
{
  m_blockStart.resize(m_clusterSize.rows());
  m_blockStart(0) = 0;
  for (Index i = 1; i < m_clusterSize.rows(); i++) {
    m_blockStart(i) = m_blockStart(i-1) + m_clusterSize(i-1);
  }
}

template <typename MatrixType, typename AtomicType>
void MatrixFunction<MatrixType,AtomicType,1>::constructPermutation()
{
  DynamicIntVectorType indexNextEntry = m_blockStart;
  m_permutation.resize(m_T.rows());
  for (Index i = 0; i < m_T.rows(); i++) {
    Index cluster = m_eivalToCluster[i];
    m_permutation[i] = indexNextEntry[cluster];
    ++indexNextEntry[cluster];
  }
}  

template <typename MatrixType, typename AtomicType>
void MatrixFunction<MatrixType,AtomicType,1>::permuteSchur()
{
  IntVectorType p = m_permutation;
  for (Index i = 0; i < p.rows() - 1; i++) {
    Index j;
    for (j = i; j < p.rows(); j++) {
      if (p(j) == i) break;
    }
    eigen_assert(p(j) == i);
    for (Index k = j-1; k >= i; k--) {
      swapEntriesInSchur(k);
      std::swap(p.coeffRef(k), p.coeffRef(k+1));
    }
  }
}

template <typename MatrixType, typename AtomicType>
void MatrixFunction<MatrixType,AtomicType,1>::swapEntriesInSchur(Index index)
{
  JacobiRotation<Scalar> rotation;
  rotation.makeGivens(m_T(index, index+1), m_T(index+1, index+1) - m_T(index, index));
  m_T.applyOnTheLeft(index, index+1, rotation.adjoint());
  m_T.applyOnTheRight(index, index+1, rotation);
  m_U.applyOnTheRight(index, index+1, rotation);
}  

template <typename MatrixType, typename AtomicType>
void MatrixFunction<MatrixType,AtomicType,1>::computeBlockAtomic()
{ 
  m_fT.resize(m_T.rows(), m_T.cols());
  m_fT.setZero();
  for (Index i = 0; i < m_clusterSize.rows(); ++i) {
    block(m_fT, i, i) = m_atomic.compute(block(m_T, i, i));
  }
}

template <typename MatrixType, typename AtomicType>
Block<MatrixType> MatrixFunction<MatrixType,AtomicType,1>::block(MatrixType& A, Index i, Index j)
{
  return A.block(m_blockStart(i), m_blockStart(j), m_clusterSize(i), m_clusterSize(j));
}

template <typename MatrixType, typename AtomicType>
void MatrixFunction<MatrixType,AtomicType,1>::computeOffDiagonal()
{ 
  for (Index diagIndex = 1; diagIndex < m_clusterSize.rows(); diagIndex++) {
    for (Index blockIndex = 0; blockIndex < m_clusterSize.rows() - diagIndex; blockIndex++) {
      
      DynMatrixType A = block(m_T, blockIndex, blockIndex);
      DynMatrixType B = -block(m_T, blockIndex+diagIndex, blockIndex+diagIndex);
      DynMatrixType C = block(m_fT, blockIndex, blockIndex) * block(m_T, blockIndex, blockIndex+diagIndex);
      C -= block(m_T, blockIndex, blockIndex+diagIndex) * block(m_fT, blockIndex+diagIndex, blockIndex+diagIndex);
      for (Index k = blockIndex + 1; k < blockIndex + diagIndex; k++) {
	C += block(m_fT, blockIndex, k) * block(m_T, k, blockIndex+diagIndex);
	C -= block(m_T, blockIndex, k) * block(m_fT, k, blockIndex+diagIndex);
      }
      block(m_fT, blockIndex, blockIndex+diagIndex) = solveTriangularSylvester(A, B, C);
    }
  }
}

template <typename MatrixType, typename AtomicType>
typename MatrixFunction<MatrixType,AtomicType,1>::DynMatrixType MatrixFunction<MatrixType,AtomicType,1>::solveTriangularSylvester(
  const DynMatrixType& A, 
  const DynMatrixType& B, 
  const DynMatrixType& C)
{
  eigen_assert(A.rows() == A.cols());
  eigen_assert(A.isUpperTriangular());
  eigen_assert(B.rows() == B.cols());
  eigen_assert(B.isUpperTriangular());
  eigen_assert(C.rows() == A.rows());
  eigen_assert(C.cols() == B.rows());

  Index m = A.rows();
  Index n = B.rows();
  DynMatrixType X(m, n);

  for (Index i = m - 1; i >= 0; --i) {
    for (Index j = 0; j < n; ++j) {

      
      Scalar AX;
      if (i == m - 1) {
	AX = 0; 
      } else {
	Matrix<Scalar,1,1> AXmatrix = A.row(i).tail(m-1-i) * X.col(j).tail(m-1-i);
	AX = AXmatrix(0,0);
      }

      
      Scalar XB;
      if (j == 0) {
	XB = 0; 
      } else {
	Matrix<Scalar,1,1> XBmatrix = X.row(i).head(j) * B.col(j).head(j);
	XB = XBmatrix(0,0);
      }

      X(i,j) = (C(i,j) - AX - XB) / (A(i,i) + B(j,j));
    }
  }
  return X;
}

template<typename Derived> class MatrixFunctionReturnValue
: public ReturnByValue<MatrixFunctionReturnValue<Derived> >
{
  public:

    typedef typename Derived::Scalar Scalar;
    typedef typename Derived::Index Index;
    typedef typename internal::stem_function<Scalar>::type StemFunction;

    MatrixFunctionReturnValue(const Derived& A, StemFunction f) : m_A(A), m_f(f) { }

    template <typename ResultType>
    inline void evalTo(ResultType& result) const
    {
      typedef typename Derived::PlainObject PlainObject;
      typedef internal::traits<PlainObject> Traits;
      static const int RowsAtCompileTime = Traits::RowsAtCompileTime;
      static const int ColsAtCompileTime = Traits::ColsAtCompileTime;
      static const int Options = PlainObject::Options;
      typedef std::complex<typename NumTraits<Scalar>::Real> ComplexScalar;
      typedef Matrix<ComplexScalar, Dynamic, Dynamic, Options, RowsAtCompileTime, ColsAtCompileTime> DynMatrixType;
      typedef MatrixFunctionAtomic<DynMatrixType> AtomicType;
      AtomicType atomic(m_f);

      const PlainObject Aevaluated = m_A.eval();
      MatrixFunction<PlainObject, AtomicType> mf(Aevaluated, atomic);
      mf.compute(result);
    }

    Index rows() const { return m_A.rows(); }
    Index cols() const { return m_A.cols(); }

  private:
    typename internal::nested<Derived>::type m_A;
    StemFunction *m_f;

    MatrixFunctionReturnValue& operator=(const MatrixFunctionReturnValue&);
};

namespace internal {
template<typename Derived>
struct traits<MatrixFunctionReturnValue<Derived> >
{
  typedef typename Derived::PlainObject ReturnType;
};
}




template <typename Derived>
const MatrixFunctionReturnValue<Derived> MatrixBase<Derived>::matrixFunction(typename internal::stem_function<typename internal::traits<Derived>::Scalar>::type f) const
{
  eigen_assert(rows() == cols());
  return MatrixFunctionReturnValue<Derived>(derived(), f);
}

template <typename Derived>
const MatrixFunctionReturnValue<Derived> MatrixBase<Derived>::sin() const
{
  eigen_assert(rows() == cols());
  typedef typename internal::stem_function<Scalar>::ComplexScalar ComplexScalar;
  return MatrixFunctionReturnValue<Derived>(derived(), StdStemFunctions<ComplexScalar>::sin);
}

template <typename Derived>
const MatrixFunctionReturnValue<Derived> MatrixBase<Derived>::cos() const
{
  eigen_assert(rows() == cols());
  typedef typename internal::stem_function<Scalar>::ComplexScalar ComplexScalar;
  return MatrixFunctionReturnValue<Derived>(derived(), StdStemFunctions<ComplexScalar>::cos);
}

template <typename Derived>
const MatrixFunctionReturnValue<Derived> MatrixBase<Derived>::sinh() const
{
  eigen_assert(rows() == cols());
  typedef typename internal::stem_function<Scalar>::ComplexScalar ComplexScalar;
  return MatrixFunctionReturnValue<Derived>(derived(), StdStemFunctions<ComplexScalar>::sinh);
}

template <typename Derived>
const MatrixFunctionReturnValue<Derived> MatrixBase<Derived>::cosh() const
{
  eigen_assert(rows() == cols());
  typedef typename internal::stem_function<Scalar>::ComplexScalar ComplexScalar;
  return MatrixFunctionReturnValue<Derived>(derived(), StdStemFunctions<ComplexScalar>::cosh);
}

} 

#endif 
