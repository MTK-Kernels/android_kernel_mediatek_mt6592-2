// Copyright (C) 2012 Desire Nuentsa <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SUITESPARSEQRSUPPORT_H
#define EIGEN_SUITESPARSEQRSUPPORT_H

namespace Eigen {
  
  template<typename MatrixType> class SPQR; 
  template<typename SPQRType> struct SPQRMatrixQReturnType; 
  template<typename SPQRType> struct SPQRMatrixQTransposeReturnType; 
  template <typename SPQRType, typename Derived> struct SPQR_QProduct;
  namespace internal {
    template <typename SPQRType> struct traits<SPQRMatrixQReturnType<SPQRType> >
    {
      typedef typename SPQRType::MatrixType ReturnType;
    };
    template <typename SPQRType> struct traits<SPQRMatrixQTransposeReturnType<SPQRType> >
    {
      typedef typename SPQRType::MatrixType ReturnType;
    };
    template <typename SPQRType, typename Derived> struct traits<SPQR_QProduct<SPQRType, Derived> >
    {
      typedef typename Derived::PlainObject ReturnType;
    };
  } 
  
template<typename _MatrixType>
class SPQR
{
  public:
    typedef typename _MatrixType::Scalar Scalar;
    typedef typename _MatrixType::RealScalar RealScalar;
    typedef UF_long Index ; 
    typedef SparseMatrix<Scalar, ColMajor, Index> MatrixType;
    typedef PermutationMatrix<Dynamic, Dynamic> PermutationType;
  public:
    SPQR() 
      : m_isInitialized(false),
      m_ordering(SPQR_ORDERING_DEFAULT),
      m_allow_tol(SPQR_DEFAULT_TOL),
      m_tolerance (NumTraits<Scalar>::epsilon())
    { 
      cholmod_l_start(&m_cc);
    }
    
    SPQR(const _MatrixType& matrix) 
    : m_isInitialized(false),
      m_ordering(SPQR_ORDERING_DEFAULT),
      m_allow_tol(SPQR_DEFAULT_TOL),
      m_tolerance (NumTraits<Scalar>::epsilon())
    {
      cholmod_l_start(&m_cc);
      compute(matrix);
    }
    
    ~SPQR()
    {
      SPQR_free();
      cholmod_l_finish(&m_cc);
    }
    void SPQR_free()
    {
      cholmod_l_free_sparse(&m_H, &m_cc);
      cholmod_l_free_sparse(&m_cR, &m_cc);
      cholmod_l_free_dense(&m_HTau, &m_cc);
      std::free(m_E);
      std::free(m_HPinv);
    }

    void compute(const _MatrixType& matrix)
    {
      if(m_isInitialized) SPQR_free();

      MatrixType mat(matrix);
      cholmod_sparse A; 
      A = viewAsCholmod(mat);
      Index col = matrix.cols();
      m_rank = SuiteSparseQR<Scalar>(m_ordering, m_tolerance, col, &A, 
                             &m_cR, &m_E, &m_H, &m_HPinv, &m_HTau, &m_cc);

      if (!m_cR)
      {
        m_info = NumericalIssue; 
        m_isInitialized = false;
        return;
      }
      m_info = Success;
      m_isInitialized = true;
      m_isRUpToDate = false;
    }
    inline Index rows() const {return m_H->nrow; }
    
    inline Index cols() const { return m_cR->ncol; }
   
    template<typename Rhs>
    inline const internal::solve_retval<SPQR, Rhs> solve(const MatrixBase<Rhs>& B) const 
    {
      eigen_assert(m_isInitialized && " The QR factorization should be computed first, call compute()");
      eigen_assert(this->rows()==B.rows()
                    && "SPQR::solve(): invalid number of rows of the right hand side matrix B");
          return internal::solve_retval<SPQR, Rhs>(*this, B.derived());
    }
    
    template<typename Rhs, typename Dest>
    void _solve(const MatrixBase<Rhs> &b, MatrixBase<Dest> &dest) const
    {
      eigen_assert(m_isInitialized && " The QR factorization should be computed first, call compute()");
      eigen_assert(b.cols()==1 && "This method is for vectors only");
      
      
      typename Dest::PlainObject y;
      y = matrixQ().transpose() * b;
        
      Index rk = this->rank();
      y.topRows(rk) = this->matrixR().topLeftCorner(rk, rk).template triangularView<Upper>().solve(y.topRows(rk));
      y.bottomRows(cols()-rk).setZero();
      
      dest.topRows(cols()) = colsPermutation() * y.topRows(cols());
      
      m_info = Success;
    }
    
    const MatrixType matrixR() const
    {
      eigen_assert(m_isInitialized && " The QR factorization should be computed first, call compute()");
      if(!m_isRUpToDate) {
        m_R = viewAsEigen<Scalar,ColMajor, typename MatrixType::Index>(*m_cR);
        m_isRUpToDate = true;
      }
      return m_R;
    }
    
    SPQRMatrixQReturnType<SPQR> matrixQ() const
    {
      return SPQRMatrixQReturnType<SPQR>(*this);
    }
    
    PermutationType colsPermutation() const
    { 
      eigen_assert(m_isInitialized && "Decomposition is not initialized.");
      Index n = m_cR->ncol;
      PermutationType colsPerm(n);
      for(Index j = 0; j <n; j++) colsPerm.indices()(j) = m_E[j];
      return colsPerm; 
      
    }
    Index rank() const
    {
      eigen_assert(m_isInitialized && "Decomposition is not initialized.");
      return m_cc.SPQR_istat[4];
    }
    
    void setSPQROrdering(int ord) { m_ordering = ord;}
    
    void setPivotThreshold(const RealScalar& tol) { m_tolerance = tol; }
    
    
    cholmod_common *cholmodCommon() const { return &m_cc; }
    
    
    ComputationInfo info() const
    {
      eigen_assert(m_isInitialized && "Decomposition is not initialized.");
      return m_info;
    }
  protected:
    bool m_isInitialized;
    bool m_analysisIsOk;
    bool m_factorizationIsOk;
    mutable bool m_isRUpToDate;
    mutable ComputationInfo m_info;
    int m_ordering; 
    int m_allow_tol; 
    RealScalar m_tolerance; 
    mutable cholmod_sparse *m_cR; 
    mutable MatrixType m_R; 
    mutable Index *m_E; 
    mutable cholmod_sparse *m_H;  
    mutable Index *m_HPinv; 
    mutable cholmod_dense *m_HTau; 
    mutable Index m_rank; 
    mutable cholmod_common m_cc; 
    template<typename ,typename > friend struct SPQR_QProduct;
};

template <typename SPQRType, typename Derived>
struct SPQR_QProduct : ReturnByValue<SPQR_QProduct<SPQRType,Derived> >
{
  typedef typename SPQRType::Scalar Scalar;
  typedef typename SPQRType::Index Index;
  
  SPQR_QProduct(const SPQRType& spqr, const Derived& other, bool transpose) : m_spqr(spqr),m_other(other),m_transpose(transpose) {}
  
  inline Index rows() const { return m_transpose ? m_spqr.rows() : m_spqr.cols(); }
  inline Index cols() const { return m_other.cols(); }
  
  template<typename ResType>
  void evalTo(ResType& res) const
  {
    cholmod_dense y_cd;
    cholmod_dense *x_cd; 
    int method = m_transpose ? SPQR_QTX : SPQR_QX; 
    cholmod_common *cc = m_spqr.cholmodCommon();
    y_cd = viewAsCholmod(m_other.const_cast_derived());
    x_cd = SuiteSparseQR_qmult<Scalar>(method, m_spqr.m_H, m_spqr.m_HTau, m_spqr.m_HPinv, &y_cd, cc);
    res = Matrix<Scalar,ResType::RowsAtCompileTime,ResType::ColsAtCompileTime>::Map(reinterpret_cast<Scalar*>(x_cd->x), x_cd->nrow, x_cd->ncol);
    cholmod_l_free_dense(&x_cd, cc);
  }
  const SPQRType& m_spqr; 
  const Derived& m_other; 
  bool m_transpose; 
  
};
template<typename SPQRType>
struct SPQRMatrixQReturnType{
  
  SPQRMatrixQReturnType(const SPQRType& spqr) : m_spqr(spqr) {}
  template<typename Derived>
  SPQR_QProduct<SPQRType, Derived> operator*(const MatrixBase<Derived>& other)
  {
    return SPQR_QProduct<SPQRType,Derived>(m_spqr,other.derived(),false);
  }
  SPQRMatrixQTransposeReturnType<SPQRType> adjoint() const
  {
    return SPQRMatrixQTransposeReturnType<SPQRType>(m_spqr);
  }
  
  SPQRMatrixQTransposeReturnType<SPQRType> transpose() const
  {
    return SPQRMatrixQTransposeReturnType<SPQRType>(m_spqr);
  }
  const SPQRType& m_spqr;
};

template<typename SPQRType>
struct SPQRMatrixQTransposeReturnType{
  SPQRMatrixQTransposeReturnType(const SPQRType& spqr) : m_spqr(spqr) {}
  template<typename Derived>
  SPQR_QProduct<SPQRType,Derived> operator*(const MatrixBase<Derived>& other)
  {
    return SPQR_QProduct<SPQRType,Derived>(m_spqr,other.derived(), true);
  }
  const SPQRType& m_spqr;
};

namespace internal {
  
template<typename _MatrixType, typename Rhs>
struct solve_retval<SPQR<_MatrixType>, Rhs>
  : solve_retval_base<SPQR<_MatrixType>, Rhs>
{
  typedef SPQR<_MatrixType> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec()._solve(rhs(),dst);
  }
};

} 

}
#endif
