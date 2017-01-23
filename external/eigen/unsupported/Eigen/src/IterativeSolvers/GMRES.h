// Copyright (C) 2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2012, 2014 Kolja Brix <brix@igpm.rwth-aaachen.de>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_GMRES_H
#define EIGEN_GMRES_H

namespace Eigen { 

namespace internal {

template<typename MatrixType, typename Rhs, typename Dest, typename Preconditioner>
bool gmres(const MatrixType & mat, const Rhs & rhs, Dest & x, const Preconditioner & precond,
		int &iters, const int &restart, typename Dest::RealScalar & tol_error) {

	using std::sqrt;
	using std::abs;

	typedef typename Dest::RealScalar RealScalar;
	typedef typename Dest::Scalar Scalar;
	typedef Matrix < Scalar, Dynamic, 1 > VectorType;
	typedef Matrix < Scalar, Dynamic, Dynamic > FMatrixType;

	RealScalar tol = tol_error;
	const int maxIters = iters;
	iters = 0;

	const int m = mat.rows();

	VectorType p0 = rhs - mat*x;
	VectorType r0 = precond.solve(p0);
 
	
	if(abs(r0.norm()) < tol) {
		return true; 
	}

	VectorType w = VectorType::Zero(restart + 1);

	FMatrixType H = FMatrixType::Zero(m, restart + 1); 
	VectorType tau = VectorType::Zero(restart + 1);
	std::vector < JacobiRotation < Scalar > > G(restart);

	
	VectorType e(m-1);
	RealScalar beta;
	r0.makeHouseholder(e, tau.coeffRef(0), beta);
	w(0)=(Scalar) beta;
	H.bottomLeftCorner(m - 1, 1) = e;

	for (int k = 1; k <= restart; ++k) {

		++iters;

		VectorType v = VectorType::Unit(m, k - 1), workspace(m);

		
		for (int i = k - 1; i >= 0; --i) {
			v.tail(m - i).applyHouseholderOnTheLeft(H.col(i).tail(m - i - 1), tau.coeffRef(i), workspace.data());
		}

		
		VectorType t=mat*v;
		v=precond.solve(t);

		
		for (int i = 0; i < k; ++i) {
			v.tail(m - i).applyHouseholderOnTheLeft(H.col(i).tail(m - i - 1), tau.coeffRef(i), workspace.data());
		}

		if (v.tail(m - k).norm() != 0.0) {

			if (k <= restart) {

				
                                  VectorType e(m - k - 1);
				RealScalar beta;
				v.tail(m - k).makeHouseholder(e, tau.coeffRef(k), beta);
				H.col(k).tail(m - k - 1) = e;

				
				v.tail(m - k).applyHouseholderOnTheLeft(H.col(k).tail(m - k - 1), tau.coeffRef(k), workspace.data());

			}
                }

                if (k > 1) {
                        for (int i = 0; i < k - 1; ++i) {
                                
                                v.applyOnTheLeft(i, i + 1, G[i].adjoint());
                        }
                }

                if (k<m && v(k) != (Scalar) 0) {
                        
                        G[k - 1].makeGivens(v(k - 1), v(k));

                        
                        v.applyOnTheLeft(k - 1, k, G[k - 1].adjoint());
                        w.applyOnTheLeft(k - 1, k, G[k - 1].adjoint());

                }

                
                H.col(k - 1).head(k) = v.head(k);

                bool stop=(k==m || abs(w(k)) < tol || iters == maxIters);

                if (stop || k == restart) {

                        
                        VectorType y = w.head(k);
                        H.topLeftCorner(k, k).template triangularView < Eigen::Upper > ().solveInPlace(y);

                        
                        VectorType x_new = y(k - 1) * VectorType::Unit(m, k - 1);

                        
                        x_new.tail(m - k + 1).applyHouseholderOnTheLeft(H.col(k - 1).tail(m - k), tau.coeffRef(k - 1), workspace.data());

                        for (int i = k - 2; i >= 0; --i) {
                                x_new += y(i) * VectorType::Unit(m, i);
                                
                                x_new.tail(m - i).applyHouseholderOnTheLeft(H.col(i).tail(m - i - 1), tau.coeffRef(i), workspace.data());
                        }

                        x += x_new;

                        if (stop) {
                                return true;
                        } else {
                                k=0;

                                
                                VectorType p0=mat*x;
                                VectorType p1=precond.solve(p0);
                                r0 = rhs - p1;
                                w = VectorType::Zero(restart + 1);
                                H = FMatrixType::Zero(m, restart + 1);
                                tau = VectorType::Zero(restart + 1);

                                
                                RealScalar beta;
                                r0.makeHouseholder(e, tau.coeffRef(0), beta);
                                w(0)=(Scalar) beta;
                                H.bottomLeftCorner(m - 1, 1) = e;

                        }

                }



	}
	
	return false;

}

}

template< typename _MatrixType,
          typename _Preconditioner = DiagonalPreconditioner<typename _MatrixType::Scalar> >
class GMRES;

namespace internal {

template< typename _MatrixType, typename _Preconditioner>
struct traits<GMRES<_MatrixType,_Preconditioner> >
{
  typedef _MatrixType MatrixType;
  typedef _Preconditioner Preconditioner;
};

}

template< typename _MatrixType, typename _Preconditioner>
class GMRES : public IterativeSolverBase<GMRES<_MatrixType,_Preconditioner> >
{
  typedef IterativeSolverBase<GMRES> Base;
  using Base::mp_matrix;
  using Base::m_error;
  using Base::m_iterations;
  using Base::m_info;
  using Base::m_isInitialized;
 
private:
  int m_restart;
  
public:
  typedef _MatrixType MatrixType;
  typedef typename MatrixType::Scalar Scalar;
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::RealScalar RealScalar;
  typedef _Preconditioner Preconditioner;

public:

  
  GMRES() : Base(), m_restart(30) {}

  GMRES(const MatrixType& A) : Base(A), m_restart(30) {}

  ~GMRES() {}
  
  int get_restart() { return m_restart; }
  
  void set_restart(const int restart) { m_restart=restart; }
  
  template<typename Rhs,typename Guess>
  inline const internal::solve_retval_with_guess<GMRES, Rhs, Guess>
  solveWithGuess(const MatrixBase<Rhs>& b, const Guess& x0) const
  {
    eigen_assert(m_isInitialized && "GMRES is not initialized.");
    eigen_assert(Base::rows()==b.rows()
              && "GMRES::solve(): invalid number of rows of the right hand side matrix b");
    return internal::solve_retval_with_guess
            <GMRES, Rhs, Guess>(*this, b.derived(), x0);
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
      if(!internal::gmres(*mp_matrix, b.col(j), xj, Base::m_preconditioner, m_iterations, m_restart, m_error))
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
    if(x.squaredNorm() == 0) return; 
    _solveWithGuess(b,x);
  }

protected:

};


namespace internal {

  template<typename _MatrixType, typename _Preconditioner, typename Rhs>
struct solve_retval<GMRES<_MatrixType, _Preconditioner>, Rhs>
  : solve_retval_base<GMRES<_MatrixType, _Preconditioner>, Rhs>
{
  typedef GMRES<_MatrixType, _Preconditioner> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec()._solve(rhs(),dst);
  }
};

} 

} 

#endif 
