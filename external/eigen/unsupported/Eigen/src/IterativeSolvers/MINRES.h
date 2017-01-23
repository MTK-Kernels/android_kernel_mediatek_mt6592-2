// Copyright (C) 2012 Giacomo Po <gpo@ucla.edu>
// Copyright (C) 2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed


#ifndef EIGEN_MINRES_H_
#define EIGEN_MINRES_H_


namespace Eigen {
    
    namespace internal {
        
        template<typename MatrixType, typename Rhs, typename Dest, typename Preconditioner>
        EIGEN_DONT_INLINE
        void minres(const MatrixType& mat, const Rhs& rhs, Dest& x,
                    const Preconditioner& precond, int& iters,
                    typename Dest::RealScalar& tol_error)
        {
            using std::sqrt;
            typedef typename Dest::RealScalar RealScalar;
            typedef typename Dest::Scalar Scalar;
            typedef Matrix<Scalar,Dynamic,1> VectorType;

            
            const int maxIters(iters);  
            const int N(mat.cols());    
            const RealScalar rhsNorm2(rhs.squaredNorm());
            const RealScalar threshold2(tol_error*tol_error*rhsNorm2); 
            
            
            VectorType v( VectorType::Zero(N) ); 
            VectorType v_new(rhs-mat*x); 
            RealScalar residualNorm2(v_new.squaredNorm());
            VectorType w_new(precond.solve(v_new)); 
            RealScalar beta_new2(v_new.dot(w_new));
            eigen_assert(beta_new2 >= 0 && "PRECONDITIONER IS NOT POSITIVE DEFINITE");
            RealScalar beta_new(sqrt(beta_new2));
            const RealScalar beta_one(beta_new);
            v_new /= beta_new;
            w_new /= beta_new;
            
            RealScalar c(1.0); 
            RealScalar c_old(1.0);
            RealScalar s(0.0); 
            RealScalar s_old(0.0); 
            VectorType p_old(VectorType::Zero(N)); 
            VectorType p(p_old); 
            RealScalar eta(1.0);
                        
            iters = 0; 
            while ( iters < maxIters ){
                
                
                const RealScalar beta(beta_new);
                const VectorType v_old(v); 
                v = v_new; 
                const VectorType w(w_new); 
                v_new.noalias() = mat*w - beta*v_old; 
                const RealScalar alpha = v_new.dot(w);
                v_new -= alpha*v; 
                w_new = precond.solve(v_new); 
                beta_new2 = v_new.dot(w_new); 
                eigen_assert(beta_new2 >= 0 && "PRECONDITIONER IS NOT POSITIVE DEFINITE");
                beta_new = sqrt(beta_new2); 
                v_new /= beta_new; 
                w_new /= beta_new; 
                
                
                const RealScalar r2 =s*alpha+c*c_old*beta; 
                const RealScalar r3 =s_old*beta; 
                const RealScalar r1_hat=c*alpha-c_old*s*beta;
                const RealScalar r1 =sqrt( std::pow(r1_hat,2) + std::pow(beta_new,2) );
                c_old = c; 
                s_old = s; 
                c=r1_hat/r1; 
                s=beta_new/r1; 
                
                
                const VectorType p_oold(p_old); 
                p_old = p;
                p.noalias()=(w-r2*p_old-r3*p_oold) /r1; 
                x += beta_one*c*eta*p;
                residualNorm2 *= s*s;
                
                if ( residualNorm2 < threshold2){
                    break;
                }
                
                eta=-s*eta; 
                iters++; 
            }
            tol_error = std::sqrt(residualNorm2 / rhsNorm2); 
        }
        
    }
    
    template< typename _MatrixType, int _UpLo=Lower,
    typename _Preconditioner = IdentityPreconditioner>
    class MINRES;
    
    namespace internal {
        
        template< typename _MatrixType, int _UpLo, typename _Preconditioner>
        struct traits<MINRES<_MatrixType,_UpLo,_Preconditioner> >
        {
            typedef _MatrixType MatrixType;
            typedef _Preconditioner Preconditioner;
        };
        
    }
    
    template< typename _MatrixType, int _UpLo, typename _Preconditioner>
    class MINRES : public IterativeSolverBase<MINRES<_MatrixType,_UpLo,_Preconditioner> >
    {
        
        typedef IterativeSolverBase<MINRES> Base;
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
        
        enum {UpLo = _UpLo};
        
    public:
        
        
        MINRES() : Base() {}
        
        MINRES(const MatrixType& A) : Base(A) {}
        
        
        ~MINRES(){}
		
        template<typename Rhs,typename Guess>
        inline const internal::solve_retval_with_guess<MINRES, Rhs, Guess>
        solveWithGuess(const MatrixBase<Rhs>& b, const Guess& x0) const
        {
            eigen_assert(m_isInitialized && "MINRES is not initialized.");
            eigen_assert(Base::rows()==b.rows()
                         && "MINRES::solve(): invalid number of rows of the right hand side matrix b");
            return internal::solve_retval_with_guess
            <MINRES, Rhs, Guess>(*this, b.derived(), x0);
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
                internal::minres(mp_matrix->template selfadjointView<UpLo>(), b.col(j), xj,
                                 Base::m_preconditioner, m_iterations, m_error);
            }
            
            m_isInitialized = true;
            m_info = m_error <= Base::m_tolerance ? Success : NoConvergence;
        }
        
        
        template<typename Rhs,typename Dest>
        void _solve(const Rhs& b, Dest& x) const
        {
            x.setZero();
            _solveWithGuess(b,x);
        }
        
    protected:
        
    };
    
    namespace internal {
        
        template<typename _MatrixType, int _UpLo, typename _Preconditioner, typename Rhs>
        struct solve_retval<MINRES<_MatrixType,_UpLo,_Preconditioner>, Rhs>
        : solve_retval_base<MINRES<_MatrixType,_UpLo,_Preconditioner>, Rhs>
        {
            typedef MINRES<_MatrixType,_UpLo,_Preconditioner> Dec;
            EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)
            
            template<typename Dest> void evalTo(Dest& dst) const
            {
                dec()._solve(rhs(),dst);
            }
        };
        
    } 
    
} 

#endif 

