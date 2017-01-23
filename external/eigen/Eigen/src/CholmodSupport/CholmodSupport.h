// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_CHOLMODSUPPORT_H
#define EIGEN_CHOLMODSUPPORT_H

namespace Eigen { 

namespace internal {

template<typename Scalar, typename CholmodType>
void cholmod_configure_matrix(CholmodType& mat)
{
  if (internal::is_same<Scalar,float>::value)
  {
    mat.xtype = CHOLMOD_REAL;
    mat.dtype = CHOLMOD_SINGLE;
  }
  else if (internal::is_same<Scalar,double>::value)
  {
    mat.xtype = CHOLMOD_REAL;
    mat.dtype = CHOLMOD_DOUBLE;
  }
  else if (internal::is_same<Scalar,std::complex<float> >::value)
  {
    mat.xtype = CHOLMOD_COMPLEX;
    mat.dtype = CHOLMOD_SINGLE;
  }
  else if (internal::is_same<Scalar,std::complex<double> >::value)
  {
    mat.xtype = CHOLMOD_COMPLEX;
    mat.dtype = CHOLMOD_DOUBLE;
  }
  else
  {
    eigen_assert(false && "Scalar type not supported by CHOLMOD");
  }
}

} 

template<typename _Scalar, int _Options, typename _Index>
cholmod_sparse viewAsCholmod(SparseMatrix<_Scalar,_Options,_Index>& mat)
{
  cholmod_sparse res;
  res.nzmax   = mat.nonZeros();
  res.nrow    = mat.rows();;
  res.ncol    = mat.cols();
  res.p       = mat.outerIndexPtr();
  res.i       = mat.innerIndexPtr();
  res.x       = mat.valuePtr();
  res.z       = 0;
  res.sorted  = 1;
  if(mat.isCompressed())
  {
    res.packed  = 1;
    res.nz = 0;
  }
  else
  {
    res.packed  = 0;
    res.nz = mat.innerNonZeroPtr();
  }

  res.dtype   = 0;
  res.stype   = -1;
  
  if (internal::is_same<_Index,int>::value)
  {
    res.itype = CHOLMOD_INT;
  }
  else if (internal::is_same<_Index,UF_long>::value)
  {
    res.itype = CHOLMOD_LONG;
  }
  else
  {
    eigen_assert(false && "Index type not supported yet");
  }

  
  internal::cholmod_configure_matrix<_Scalar>(res);
  
  res.stype = 0;
  
  return res;
}

template<typename _Scalar, int _Options, typename _Index>
const cholmod_sparse viewAsCholmod(const SparseMatrix<_Scalar,_Options,_Index>& mat)
{
  cholmod_sparse res = viewAsCholmod(mat.const_cast_derived());
  return res;
}

template<typename _Scalar, int _Options, typename _Index, unsigned int UpLo>
cholmod_sparse viewAsCholmod(const SparseSelfAdjointView<SparseMatrix<_Scalar,_Options,_Index>, UpLo>& mat)
{
  cholmod_sparse res = viewAsCholmod(mat.matrix().const_cast_derived());
  
  if(UpLo==Upper) res.stype =  1;
  if(UpLo==Lower) res.stype = -1;

  return res;
}

template<typename Derived>
cholmod_dense viewAsCholmod(MatrixBase<Derived>& mat)
{
  EIGEN_STATIC_ASSERT((internal::traits<Derived>::Flags&RowMajorBit)==0,THIS_METHOD_IS_ONLY_FOR_COLUMN_MAJOR_MATRICES);
  typedef typename Derived::Scalar Scalar;

  cholmod_dense res;
  res.nrow   = mat.rows();
  res.ncol   = mat.cols();
  res.nzmax  = res.nrow * res.ncol;
  res.d      = Derived::IsVectorAtCompileTime ? mat.derived().size() : mat.derived().outerStride();
  res.x      = (void*)(mat.derived().data());
  res.z      = 0;

  internal::cholmod_configure_matrix<Scalar>(res);

  return res;
}

template<typename Scalar, int Flags, typename Index>
MappedSparseMatrix<Scalar,Flags,Index> viewAsEigen(cholmod_sparse& cm)
{
  return MappedSparseMatrix<Scalar,Flags,Index>
         (cm.nrow, cm.ncol, static_cast<Index*>(cm.p)[cm.ncol],
          static_cast<Index*>(cm.p), static_cast<Index*>(cm.i),static_cast<Scalar*>(cm.x) );
}

enum CholmodMode {
  CholmodAuto, CholmodSimplicialLLt, CholmodSupernodalLLt, CholmodLDLt
};


template<typename _MatrixType, int _UpLo, typename Derived>
class CholmodBase : internal::noncopyable
{
  public:
    typedef _MatrixType MatrixType;
    enum { UpLo = _UpLo };
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::RealScalar RealScalar;
    typedef MatrixType CholMatrixType;
    typedef typename MatrixType::Index Index;

  public:

    CholmodBase()
      : m_cholmodFactor(0), m_info(Success), m_isInitialized(false)
    {
      m_shiftOffset[0] = m_shiftOffset[1] = RealScalar(0.0);
      cholmod_start(&m_cholmod);
    }

    CholmodBase(const MatrixType& matrix)
      : m_cholmodFactor(0), m_info(Success), m_isInitialized(false)
    {
      m_shiftOffset[0] = m_shiftOffset[1] = RealScalar(0.0);
      cholmod_start(&m_cholmod);
      compute(matrix);
    }

    ~CholmodBase()
    {
      if(m_cholmodFactor)
        cholmod_free_factor(&m_cholmodFactor, &m_cholmod);
      cholmod_finish(&m_cholmod);
    }
    
    inline Index cols() const { return m_cholmodFactor->n; }
    inline Index rows() const { return m_cholmodFactor->n; }
    
    Derived& derived() { return *static_cast<Derived*>(this); }
    const Derived& derived() const { return *static_cast<const Derived*>(this); }
    
    ComputationInfo info() const
    {
      eigen_assert(m_isInitialized && "Decomposition is not initialized.");
      return m_info;
    }

    
    Derived& compute(const MatrixType& matrix)
    {
      analyzePattern(matrix);
      factorize(matrix);
      return derived();
    }
    
    template<typename Rhs>
    inline const internal::solve_retval<CholmodBase, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "LLT is not initialized.");
      eigen_assert(rows()==b.rows()
                && "CholmodDecomposition::solve(): invalid number of rows of the right hand side matrix b");
      return internal::solve_retval<CholmodBase, Rhs>(*this, b.derived());
    }
    
    template<typename Rhs>
    inline const internal::sparse_solve_retval<CholmodBase, Rhs>
    solve(const SparseMatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "LLT is not initialized.");
      eigen_assert(rows()==b.rows()
                && "CholmodDecomposition::solve(): invalid number of rows of the right hand side matrix b");
      return internal::sparse_solve_retval<CholmodBase, Rhs>(*this, b.derived());
    }
    
    void analyzePattern(const MatrixType& matrix)
    {
      if(m_cholmodFactor)
      {
        cholmod_free_factor(&m_cholmodFactor, &m_cholmod);
        m_cholmodFactor = 0;
      }
      cholmod_sparse A = viewAsCholmod(matrix.template selfadjointView<UpLo>());
      m_cholmodFactor = cholmod_analyze(&A, &m_cholmod);
      
      this->m_isInitialized = true;
      this->m_info = Success;
      m_analysisIsOk = true;
      m_factorizationIsOk = false;
    }
    
    void factorize(const MatrixType& matrix)
    {
      eigen_assert(m_analysisIsOk && "You must first call analyzePattern()");
      cholmod_sparse A = viewAsCholmod(matrix.template selfadjointView<UpLo>());
      cholmod_factorize_p(&A, m_shiftOffset, 0, 0, m_cholmodFactor, &m_cholmod);
      
      
      this->m_info = (m_cholmodFactor->minor == m_cholmodFactor->n ? Success : NumericalIssue);
      m_factorizationIsOk = true;
    }
    
    cholmod_common& cholmod() { return m_cholmod; }
    
    #ifndef EIGEN_PARSED_BY_DOXYGEN
    
    template<typename Rhs,typename Dest>
    void _solve(const MatrixBase<Rhs> &b, MatrixBase<Dest> &dest) const
    {
      eigen_assert(m_factorizationIsOk && "The decomposition is not in a valid state for solving, you must first call either compute() or symbolic()/numeric()");
      const Index size = m_cholmodFactor->n;
      EIGEN_UNUSED_VARIABLE(size);
      eigen_assert(size==b.rows());

      
      Rhs& b_ref(b.const_cast_derived());
      cholmod_dense b_cd = viewAsCholmod(b_ref);
      cholmod_dense* x_cd = cholmod_solve(CHOLMOD_A, m_cholmodFactor, &b_cd, &m_cholmod);
      if(!x_cd)
      {
        this->m_info = NumericalIssue;
      }
      
      dest = Matrix<Scalar,Dest::RowsAtCompileTime,Dest::ColsAtCompileTime>::Map(reinterpret_cast<Scalar*>(x_cd->x),b.rows(),b.cols());
      cholmod_free_dense(&x_cd, &m_cholmod);
    }
    
    
    template<typename RhsScalar, int RhsOptions, typename RhsIndex, typename DestScalar, int DestOptions, typename DestIndex>
    void _solve(const SparseMatrix<RhsScalar,RhsOptions,RhsIndex> &b, SparseMatrix<DestScalar,DestOptions,DestIndex> &dest) const
    {
      eigen_assert(m_factorizationIsOk && "The decomposition is not in a valid state for solving, you must first call either compute() or symbolic()/numeric()");
      const Index size = m_cholmodFactor->n;
      EIGEN_UNUSED_VARIABLE(size);
      eigen_assert(size==b.rows());

      
      cholmod_sparse b_cs = viewAsCholmod(b);
      cholmod_sparse* x_cs = cholmod_spsolve(CHOLMOD_A, m_cholmodFactor, &b_cs, &m_cholmod);
      if(!x_cs)
      {
        this->m_info = NumericalIssue;
      }
      
      dest = viewAsEigen<DestScalar,DestOptions,DestIndex>(*x_cs);
      cholmod_free_sparse(&x_cs, &m_cholmod);
    }
    #endif 
    
    
    Derived& setShift(const RealScalar& offset)
    {
      m_shiftOffset[0] = offset;
      return derived();
    }
    
    template<typename Stream>
    void dumpMemory(Stream& )
    {}
    
  protected:
    mutable cholmod_common m_cholmod;
    cholmod_factor* m_cholmodFactor;
    RealScalar m_shiftOffset[2];
    mutable ComputationInfo m_info;
    bool m_isInitialized;
    int m_factorizationIsOk;
    int m_analysisIsOk;
};

template<typename _MatrixType, int _UpLo = Lower>
class CholmodSimplicialLLT : public CholmodBase<_MatrixType, _UpLo, CholmodSimplicialLLT<_MatrixType, _UpLo> >
{
    typedef CholmodBase<_MatrixType, _UpLo, CholmodSimplicialLLT> Base;
    using Base::m_cholmod;
    
  public:
    
    typedef _MatrixType MatrixType;
    
    CholmodSimplicialLLT() : Base() { init(); }

    CholmodSimplicialLLT(const MatrixType& matrix) : Base()
    {
      init();
      compute(matrix);
    }

    ~CholmodSimplicialLLT() {}
  protected:
    void init()
    {
      m_cholmod.final_asis = 0;
      m_cholmod.supernodal = CHOLMOD_SIMPLICIAL;
      m_cholmod.final_ll = 1;
    }
};


template<typename _MatrixType, int _UpLo = Lower>
class CholmodSimplicialLDLT : public CholmodBase<_MatrixType, _UpLo, CholmodSimplicialLDLT<_MatrixType, _UpLo> >
{
    typedef CholmodBase<_MatrixType, _UpLo, CholmodSimplicialLDLT> Base;
    using Base::m_cholmod;
    
  public:
    
    typedef _MatrixType MatrixType;
    
    CholmodSimplicialLDLT() : Base() { init(); }

    CholmodSimplicialLDLT(const MatrixType& matrix) : Base()
    {
      init();
      compute(matrix);
    }

    ~CholmodSimplicialLDLT() {}
  protected:
    void init()
    {
      m_cholmod.final_asis = 1;
      m_cholmod.supernodal = CHOLMOD_SIMPLICIAL;
    }
};

template<typename _MatrixType, int _UpLo = Lower>
class CholmodSupernodalLLT : public CholmodBase<_MatrixType, _UpLo, CholmodSupernodalLLT<_MatrixType, _UpLo> >
{
    typedef CholmodBase<_MatrixType, _UpLo, CholmodSupernodalLLT> Base;
    using Base::m_cholmod;
    
  public:
    
    typedef _MatrixType MatrixType;
    
    CholmodSupernodalLLT() : Base() { init(); }

    CholmodSupernodalLLT(const MatrixType& matrix) : Base()
    {
      init();
      compute(matrix);
    }

    ~CholmodSupernodalLLT() {}
  protected:
    void init()
    {
      m_cholmod.final_asis = 1;
      m_cholmod.supernodal = CHOLMOD_SUPERNODAL;
    }
};

template<typename _MatrixType, int _UpLo = Lower>
class CholmodDecomposition : public CholmodBase<_MatrixType, _UpLo, CholmodDecomposition<_MatrixType, _UpLo> >
{
    typedef CholmodBase<_MatrixType, _UpLo, CholmodDecomposition> Base;
    using Base::m_cholmod;
    
  public:
    
    typedef _MatrixType MatrixType;
    
    CholmodDecomposition() : Base() { init(); }

    CholmodDecomposition(const MatrixType& matrix) : Base()
    {
      init();
      compute(matrix);
    }

    ~CholmodDecomposition() {}
    
    void setMode(CholmodMode mode)
    {
      switch(mode)
      {
        case CholmodAuto:
          m_cholmod.final_asis = 1;
          m_cholmod.supernodal = CHOLMOD_AUTO;
          break;
        case CholmodSimplicialLLt:
          m_cholmod.final_asis = 0;
          m_cholmod.supernodal = CHOLMOD_SIMPLICIAL;
          m_cholmod.final_ll = 1;
          break;
        case CholmodSupernodalLLt:
          m_cholmod.final_asis = 1;
          m_cholmod.supernodal = CHOLMOD_SUPERNODAL;
          break;
        case CholmodLDLt:
          m_cholmod.final_asis = 1;
          m_cholmod.supernodal = CHOLMOD_SIMPLICIAL;
          break;
        default:
          break;
      }
    }
  protected:
    void init()
    {
      m_cholmod.final_asis = 1;
      m_cholmod.supernodal = CHOLMOD_AUTO;
    }
};

namespace internal {
  
template<typename _MatrixType, int _UpLo, typename Derived, typename Rhs>
struct solve_retval<CholmodBase<_MatrixType,_UpLo,Derived>, Rhs>
  : solve_retval_base<CholmodBase<_MatrixType,_UpLo,Derived>, Rhs>
{
  typedef CholmodBase<_MatrixType,_UpLo,Derived> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec()._solve(rhs(),dst);
  }
};

template<typename _MatrixType, int _UpLo, typename Derived, typename Rhs>
struct sparse_solve_retval<CholmodBase<_MatrixType,_UpLo,Derived>, Rhs>
  : sparse_solve_retval_base<CholmodBase<_MatrixType,_UpLo,Derived>, Rhs>
{
  typedef CholmodBase<_MatrixType,_UpLo,Derived> Dec;
  EIGEN_MAKE_SPARSE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec()._solve(rhs(),dst);
  }
};

} 

} 

#endif 
