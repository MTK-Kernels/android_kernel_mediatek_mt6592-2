// Copyright (C) 2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ITERATIVE_SOLVER_BASE_H
#define EIGEN_ITERATIVE_SOLVER_BASE_H

namespace Eigen { 

template< typename Derived>
class IterativeSolverBase : internal::noncopyable
{
public:
  typedef typename internal::traits<Derived>::MatrixType MatrixType;
  typedef typename internal::traits<Derived>::Preconditioner Preconditioner;
  typedef typename MatrixType::Scalar Scalar;
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::RealScalar RealScalar;

public:

  Derived& derived() { return *static_cast<Derived*>(this); }
  const Derived& derived() const { return *static_cast<const Derived*>(this); }

  
  IterativeSolverBase()
    : mp_matrix(0)
  {
    init();
  }

  IterativeSolverBase(const MatrixType& A)
  {
    init();
    compute(A);
  }

  ~IterativeSolverBase() {}
  
  Derived& analyzePattern(const MatrixType& A)
  {
    m_preconditioner.analyzePattern(A);
    m_isInitialized = true;
    m_analysisIsOk = true;
    m_info = Success;
    return derived();
  }
  
  Derived& factorize(const MatrixType& A)
  {
    eigen_assert(m_analysisIsOk && "You must first call analyzePattern()"); 
    mp_matrix = &A;
    m_preconditioner.factorize(A);
    m_factorizationIsOk = true;
    m_info = Success;
    return derived();
  }

  Derived& compute(const MatrixType& A)
  {
    mp_matrix = &A;
    m_preconditioner.compute(A);
    m_isInitialized = true;
    m_analysisIsOk = true;
    m_factorizationIsOk = true;
    m_info = Success;
    return derived();
  }

  
  Index rows() const { return mp_matrix ? mp_matrix->rows() : 0; }
  
  Index cols() const { return mp_matrix ? mp_matrix->cols() : 0; }

  
  RealScalar tolerance() const { return m_tolerance; }
  
  
  Derived& setTolerance(const RealScalar& tolerance)
  {
    m_tolerance = tolerance;
    return derived();
  }

  
  Preconditioner& preconditioner() { return m_preconditioner; }
  
  
  const Preconditioner& preconditioner() const { return m_preconditioner; }

  
  int maxIterations() const
  {
    return (mp_matrix && m_maxIterations<0) ? mp_matrix->cols() : m_maxIterations;
  }
  
  
  Derived& setMaxIterations(int maxIters)
  {
    m_maxIterations = maxIters;
    return derived();
  }

  
  int iterations() const
  {
    eigen_assert(m_isInitialized && "ConjugateGradient is not initialized.");
    return m_iterations;
  }

  
  RealScalar error() const
  {
    eigen_assert(m_isInitialized && "ConjugateGradient is not initialized.");
    return m_error;
  }

  template<typename Rhs> inline const internal::solve_retval<Derived, Rhs>
  solve(const MatrixBase<Rhs>& b) const
  {
    eigen_assert(m_isInitialized && "IterativeSolverBase is not initialized.");
    eigen_assert(rows()==b.rows()
              && "IterativeSolverBase::solve(): invalid number of rows of the right hand side matrix b");
    return internal::solve_retval<Derived, Rhs>(derived(), b.derived());
  }
  
  template<typename Rhs>
  inline const internal::sparse_solve_retval<IterativeSolverBase, Rhs>
  solve(const SparseMatrixBase<Rhs>& b) const
  {
    eigen_assert(m_isInitialized && "IterativeSolverBase is not initialized.");
    eigen_assert(rows()==b.rows()
              && "IterativeSolverBase::solve(): invalid number of rows of the right hand side matrix b");
    return internal::sparse_solve_retval<IterativeSolverBase, Rhs>(*this, b.derived());
  }

  
  ComputationInfo info() const
  {
    eigen_assert(m_isInitialized && "IterativeSolverBase is not initialized.");
    return m_info;
  }
  
  
  template<typename Rhs, typename DestScalar, int DestOptions, typename DestIndex>
  void _solve_sparse(const Rhs& b, SparseMatrix<DestScalar,DestOptions,DestIndex> &dest) const
  {
    eigen_assert(rows()==b.rows());
    
    int rhsCols = b.cols();
    int size = b.rows();
    Eigen::Matrix<DestScalar,Dynamic,1> tb(size);
    Eigen::Matrix<DestScalar,Dynamic,1> tx(size);
    for(int k=0; k<rhsCols; ++k)
    {
      tb = b.col(k);
      tx = derived().solve(tb);
      dest.col(k) = tx.sparseView(0);
    }
  }

protected:
  void init()
  {
    m_isInitialized = false;
    m_analysisIsOk = false;
    m_factorizationIsOk = false;
    m_maxIterations = -1;
    m_tolerance = NumTraits<Scalar>::epsilon();
  }
  const MatrixType* mp_matrix;
  Preconditioner m_preconditioner;

  int m_maxIterations;
  RealScalar m_tolerance;
  
  mutable RealScalar m_error;
  mutable int m_iterations;
  mutable ComputationInfo m_info;
  mutable bool m_isInitialized, m_analysisIsOk, m_factorizationIsOk;
};

namespace internal {
 
template<typename Derived, typename Rhs>
struct sparse_solve_retval<IterativeSolverBase<Derived>, Rhs>
  : sparse_solve_retval_base<IterativeSolverBase<Derived>, Rhs>
{
  typedef IterativeSolverBase<Derived> Dec;
  EIGEN_MAKE_SPARSE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec().derived()._solve_sparse(rhs(),dst);
  }
};

} 

} 

#endif 
