// Copyright (C) 2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_BICGSTAB_H
#define EIGEN_BICGSTAB_H

namespace Eigen { 

namespace internal {

template<typename MatrixType, typename Rhs, typename Dest, typename Preconditioner>
bool bicgstab(const MatrixType& mat, const Rhs& rhs, Dest& x,
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
  VectorType r  = rhs - mat * x;
  VectorType r0 = r;
  
  RealScalar r0_sqnorm = r0.squaredNorm();
  RealScalar rhs_sqnorm = rhs.squaredNorm();
  if(rhs_sqnorm == 0)
  {
    x.setZero();
    return true;
  }
  Scalar rho    = 1;
  Scalar alpha  = 1;
  Scalar w      = 1;
  
  VectorType v = VectorType::Zero(n), p = VectorType::Zero(n);
  VectorType y(n),  z(n);
  VectorType kt(n), ks(n);

  VectorType s(n), t(n);

  RealScalar tol2 = tol*tol;
  RealScalar eps2 = NumTraits<Scalar>::epsilon()*NumTraits<Scalar>::epsilon();
  int i = 0;
  int restarts = 0;

  while ( r.squaredNorm()/rhs_sqnorm > tol2 && i<maxIters )
  {
    Scalar rho_old = rho;

    rho = r0.dot(r);
    if (abs(rho) < eps2*r0_sqnorm)
    {
      
      
      r0 = r;
      rho = r0_sqnorm = r.squaredNorm();
      if(restarts++ == 0)
        i = 0;
    }
    Scalar beta = (rho/rho_old) * (alpha / w);
    p = r + beta * (p - w * v);
    
    y = precond.solve(p);
    
    v.noalias() = mat * y;

    alpha = rho / r0.dot(v);
    s = r - alpha * v;

    z = precond.solve(s);
    t.noalias() = mat * z;

    RealScalar tmp = t.squaredNorm();
    if(tmp>RealScalar(0))
      w = t.dot(s) / tmp;
    else
      w = Scalar(0);
    x += alpha * y + w * z;
    r = s - w * t;
    ++i;
  }
  tol_error = sqrt(r.squaredNorm()/rhs_sqnorm);
  iters = i;
  return true; 
}

}

template< typename _MatrixType,
          typename _Preconditioner = DiagonalPreconditioner<typename _MatrixType::Scalar> >
class BiCGSTAB;

namespace internal {

template< typename _MatrixType, typename _Preconditioner>
struct traits<BiCGSTAB<_MatrixType,_Preconditioner> >
{
  typedef _MatrixType MatrixType;
  typedef _Preconditioner Preconditioner;
};

}

template< typename _MatrixType, typename _Preconditioner>
class BiCGSTAB : public IterativeSolverBase<BiCGSTAB<_MatrixType,_Preconditioner> >
{
  typedef IterativeSolverBase<BiCGSTAB> Base;
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

public:

  
  BiCGSTAB() : Base() {}

  BiCGSTAB(const MatrixType& A) : Base(A) {}

  ~BiCGSTAB() {}
  
  template<typename Rhs,typename Guess>
  inline const internal::solve_retval_with_guess<BiCGSTAB, Rhs, Guess>
  solveWithGuess(const MatrixBase<Rhs>& b, const Guess& x0) const
  {
    eigen_assert(m_isInitialized && "BiCGSTAB is not initialized.");
    eigen_assert(Base::rows()==b.rows()
              && "BiCGSTAB::solve(): invalid number of rows of the right hand side matrix b");
    return internal::solve_retval_with_guess
            <BiCGSTAB, Rhs, Guess>(*this, b.derived(), x0);
  }
  
  
  template<typename Rhs,typename Dest>
  void _solveWithGuess(const Rhs& b, Dest& x) const
  {    
    bool failed = false;
    for(int j=0; j<b.cols(); ++j)
    {
      m_iterations = Base::maxIterations();
      m_error = Base::m_tolerance;
      
      typename Dest::ColXpr xj(x,j);
      if(!internal::bicgstab(*mp_matrix, b.col(j), xj, Base::m_preconditioner, m_iterations, m_error))
        failed = true;
    }
    m_info = failed ? NumericalIssue
           : m_error <= Base::m_tolerance ? Success
           : NoConvergence;
    m_isInitialized = true;
  }

  
  template<typename Rhs,typename Dest>
  void _solve(const Rhs& b, Dest& x) const
  {
  x = b;
    _solveWithGuess(b,x);
  }

protected:

};


namespace internal {

  template<typename _MatrixType, typename _Preconditioner, typename Rhs>
struct solve_retval<BiCGSTAB<_MatrixType, _Preconditioner>, Rhs>
  : solve_retval_base<BiCGSTAB<_MatrixType, _Preconditioner>, Rhs>
{
  typedef BiCGSTAB<_MatrixType, _Preconditioner> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec()._solve(rhs(),dst);
  }
};

} 

} 

#endif 
