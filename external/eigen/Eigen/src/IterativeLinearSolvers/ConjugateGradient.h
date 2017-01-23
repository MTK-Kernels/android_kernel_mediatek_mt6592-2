// Copyright (C) 2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_CONJUGATE_GRADIENT_H
#define EIGEN_CONJUGATE_GRADIENT_H

namespace Eigen { 

namespace internal {

template<typename MatrixType, typename Rhs, typename Dest, typename Preconditioner>
EIGEN_DONT_INLINE
void conjugate_gradient(const MatrixType& mat, const Rhs& rhs, Dest& x,
                        const Preconditioner& precond, int& iters,
                        typename Dest::RealScalar& tol_error)
{
  using std::sqrt;
  using std::abs;
  typedef typename Dest::RealScalar RealScalar;
  typedef typename Dest::Scalar Scalar;
  typedef Matrix<Scalar,Dynamic,1> VectorType;
  
  RealScalar tol = tol_error;
  int maxIters = iters;
  
  int n = mat.cols();

  VectorType residual = rhs - mat * x; 

  RealScalar rhsNorm2 = rhs.squaredNorm();
  if(rhsNorm2 == 0) 
  {
    x.setZero();
    iters = 0;
    tol_error = 0;
    return;
  }
  RealScalar threshold = tol*tol*rhsNorm2;
  RealScalar residualNorm2 = residual.squaredNorm();
  if (residualNorm2 < threshold)
  {
    iters = 0;
    tol_error = sqrt(residualNorm2 / rhsNorm2);
    return;
  }
  
  VectorType p(n);
  p = precond.solve(residual);      

  VectorType z(n), tmp(n);
  RealScalar absNew = numext::real(residual.dot(p));  
  int i = 0;
  while(i < maxIters)
  {
    tmp.noalias() = mat * p;              

    Scalar alpha = absNew / p.dot(tmp);   
    x += alpha * p;                       
    residual -= alpha * tmp;              
    
    residualNorm2 = residual.squaredNorm();
    if(residualNorm2 < threshold)
      break;
    
    z = precond.solve(residual);          

    RealScalar absOld = absNew;
    absNew = numext::real(residual.dot(z));     
    RealScalar beta = absNew / absOld;            
    p = z + beta * p;                             
    i++;
  }
  tol_error = sqrt(residualNorm2 / rhsNorm2);
  iters = i;
}

}

template< typename _MatrixType, int _UpLo=Lower,
          typename _Preconditioner = DiagonalPreconditioner<typename _MatrixType::Scalar> >
class ConjugateGradient;

namespace internal {

template< typename _MatrixType, int _UpLo, typename _Preconditioner>
struct traits<ConjugateGradient<_MatrixType,_UpLo,_Preconditioner> >
{
  typedef _MatrixType MatrixType;
  typedef _Preconditioner Preconditioner;
};

}

template< typename _MatrixType, int _UpLo, typename _Preconditioner>
class ConjugateGradient : public IterativeSolverBase<ConjugateGradient<_MatrixType,_UpLo,_Preconditioner> >
{
  typedef IterativeSolverBase<ConjugateGradient> Base;
  using Base::mp_matrix;
  using Base::m_error;
  using Base::m_iterations;
  using Base::m_info;
  using Base::m_isInitialized;
public:
  typedef _MatrixType MatrixType;
  typedef typename MatrixType::Scalar Scalar;
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::RealScalar RealScalar;
  typedef _Preconditioner Preconditioner;

  enum {
    UpLo = _UpLo
  };

public:

  
  ConjugateGradient() : Base() {}

  ConjugateGradient(const MatrixType& A) : Base(A) {}

  ~ConjugateGradient() {}
  
  template<typename Rhs,typename Guess>
  inline const internal::solve_retval_with_guess<ConjugateGradient, Rhs, Guess>
  solveWithGuess(const MatrixBase<Rhs>& b, const Guess& x0) const
  {
    eigen_assert(m_isInitialized && "ConjugateGradient is not initialized.");
    eigen_assert(Base::rows()==b.rows()
              && "ConjugateGradient::solve(): invalid number of rows of the right hand side matrix b");
    return internal::solve_retval_with_guess
            <ConjugateGradient, Rhs, Guess>(*this, b.derived(), x0);
  }

  
  template<typename Rhs,typename Dest>
  void _solveWithGuess(const Rhs& b, Dest& x) const
  {
    m_iterations = Base::maxIterations();
    m_error = Base::m_tolerance;

    for(int j=0; j<b.cols(); ++j)
    {
      m_iterations = Base::maxIterations();
      m_error = Base::m_tolerance;

      typename Dest::ColXpr xj(x,j);
      internal::conjugate_gradient(mp_matrix->template selfadjointView<UpLo>(), b.col(j), xj,
                                   Base::m_preconditioner, m_iterations, m_error);
    }

    m_isInitialized = true;
    m_info = m_error <= Base::m_tolerance ? Success : NoConvergence;
  }
  
  
  template<typename Rhs,typename Dest>
  void _solve(const Rhs& b, Dest& x) const
  {
    x.setOnes();
    _solveWithGuess(b,x);
  }

protected:

};


namespace internal {

template<typename _MatrixType, int _UpLo, typename _Preconditioner, typename Rhs>
struct solve_retval<ConjugateGradient<_MatrixType,_UpLo,_Preconditioner>, Rhs>
  : solve_retval_base<ConjugateGradient<_MatrixType,_UpLo,_Preconditioner>, Rhs>
{
  typedef ConjugateGradient<_MatrixType,_UpLo,_Preconditioner> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec()._solve(rhs(),dst);
  }
};

} 

} 

#endif 
