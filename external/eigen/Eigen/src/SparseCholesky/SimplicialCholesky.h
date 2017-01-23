// Copyright (C) 2008-2012 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SIMPLICIAL_CHOLESKY_H
#define EIGEN_SIMPLICIAL_CHOLESKY_H

namespace Eigen { 

enum SimplicialCholeskyMode {
  SimplicialCholeskyLLT,
  SimplicialCholeskyLDLT
};

template<typename Derived>
class SimplicialCholeskyBase : internal::noncopyable
{
  public:
    typedef typename internal::traits<Derived>::MatrixType MatrixType;
    typedef typename internal::traits<Derived>::OrderingType OrderingType;
    enum { UpLo = internal::traits<Derived>::UpLo };
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::RealScalar RealScalar;
    typedef typename MatrixType::Index Index;
    typedef SparseMatrix<Scalar,ColMajor,Index> CholMatrixType;
    typedef Matrix<Scalar,Dynamic,1> VectorType;

  public:

    
    SimplicialCholeskyBase()
      : m_info(Success), m_isInitialized(false), m_shiftOffset(0), m_shiftScale(1)
    {}

    SimplicialCholeskyBase(const MatrixType& matrix)
      : m_info(Success), m_isInitialized(false), m_shiftOffset(0), m_shiftScale(1)
    {
      derived().compute(matrix);
    }

    ~SimplicialCholeskyBase()
    {
    }

    Derived& derived() { return *static_cast<Derived*>(this); }
    const Derived& derived() const { return *static_cast<const Derived*>(this); }
    
    inline Index cols() const { return m_matrix.cols(); }
    inline Index rows() const { return m_matrix.rows(); }
    
    ComputationInfo info() const
    {
      eigen_assert(m_isInitialized && "Decomposition is not initialized.");
      return m_info;
    }
    
    template<typename Rhs>
    inline const internal::solve_retval<SimplicialCholeskyBase, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "Simplicial LLT or LDLT is not initialized.");
      eigen_assert(rows()==b.rows()
                && "SimplicialCholeskyBase::solve(): invalid number of rows of the right hand side matrix b");
      return internal::solve_retval<SimplicialCholeskyBase, Rhs>(*this, b.derived());
    }
    
    template<typename Rhs>
    inline const internal::sparse_solve_retval<SimplicialCholeskyBase, Rhs>
    solve(const SparseMatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "Simplicial LLT or LDLT is not initialized.");
      eigen_assert(rows()==b.rows()
                && "SimplicialCholesky::solve(): invalid number of rows of the right hand side matrix b");
      return internal::sparse_solve_retval<SimplicialCholeskyBase, Rhs>(*this, b.derived());
    }
    
    const PermutationMatrix<Dynamic,Dynamic,Index>& permutationP() const
    { return m_P; }
    
    const PermutationMatrix<Dynamic,Dynamic,Index>& permutationPinv() const
    { return m_Pinv; }

    Derived& setShift(const RealScalar& offset, const RealScalar& scale = 1)
    {
      m_shiftOffset = offset;
      m_shiftScale = scale;
      return derived();
    }

#ifndef EIGEN_PARSED_BY_DOXYGEN
    
    template<typename Stream>
    void dumpMemory(Stream& s)
    {
      int total = 0;
      s << "  L:        " << ((total+=(m_matrix.cols()+1) * sizeof(int) + m_matrix.nonZeros()*(sizeof(int)+sizeof(Scalar))) >> 20) << "Mb" << "\n";
      s << "  diag:     " << ((total+=m_diag.size() * sizeof(Scalar)) >> 20) << "Mb" << "\n";
      s << "  tree:     " << ((total+=m_parent.size() * sizeof(int)) >> 20) << "Mb" << "\n";
      s << "  nonzeros: " << ((total+=m_nonZerosPerCol.size() * sizeof(int)) >> 20) << "Mb" << "\n";
      s << "  perm:     " << ((total+=m_P.size() * sizeof(int)) >> 20) << "Mb" << "\n";
      s << "  perm^-1:  " << ((total+=m_Pinv.size() * sizeof(int)) >> 20) << "Mb" << "\n";
      s << "  TOTAL:    " << (total>> 20) << "Mb" << "\n";
    }

    
    template<typename Rhs,typename Dest>
    void _solve(const MatrixBase<Rhs> &b, MatrixBase<Dest> &dest) const
    {
      eigen_assert(m_factorizationIsOk && "The decomposition is not in a valid state for solving, you must first call either compute() or symbolic()/numeric()");
      eigen_assert(m_matrix.rows()==b.rows());

      if(m_info!=Success)
        return;

      if(m_P.size()>0)
        dest = m_P * b;
      else
        dest = b;

      if(m_matrix.nonZeros()>0) 
        derived().matrixL().solveInPlace(dest);

      if(m_diag.size()>0)
        dest = m_diag.asDiagonal().inverse() * dest;

      if (m_matrix.nonZeros()>0) 
        derived().matrixU().solveInPlace(dest);

      if(m_P.size()>0)
        dest = m_Pinv * dest;
    }

#endif 

  protected:
    
    
    template<bool DoLDLT>
    void compute(const MatrixType& matrix)
    {
      eigen_assert(matrix.rows()==matrix.cols());
      Index size = matrix.cols();
      CholMatrixType ap(size,size);
      ordering(matrix, ap);
      analyzePattern_preordered(ap, DoLDLT);
      factorize_preordered<DoLDLT>(ap);
    }
    
    template<bool DoLDLT>
    void factorize(const MatrixType& a)
    {
      eigen_assert(a.rows()==a.cols());
      int size = a.cols();
      CholMatrixType ap(size,size);
      ap.template selfadjointView<Upper>() = a.template selfadjointView<UpLo>().twistedBy(m_P);
      factorize_preordered<DoLDLT>(ap);
    }

    template<bool DoLDLT>
    void factorize_preordered(const CholMatrixType& a);

    void analyzePattern(const MatrixType& a, bool doLDLT)
    {
      eigen_assert(a.rows()==a.cols());
      int size = a.cols();
      CholMatrixType ap(size,size);
      ordering(a, ap);
      analyzePattern_preordered(ap,doLDLT);
    }
    void analyzePattern_preordered(const CholMatrixType& a, bool doLDLT);
    
    void ordering(const MatrixType& a, CholMatrixType& ap);

    
    struct keep_diag {
      inline bool operator() (const Index& row, const Index& col, const Scalar&) const
      {
        return row!=col;
      }
    };

    mutable ComputationInfo m_info;
    bool m_isInitialized;
    bool m_factorizationIsOk;
    bool m_analysisIsOk;
    
    CholMatrixType m_matrix;
    VectorType m_diag;                                
    VectorXi m_parent;                                
    VectorXi m_nonZerosPerCol;
    PermutationMatrix<Dynamic,Dynamic,Index> m_P;     
    PermutationMatrix<Dynamic,Dynamic,Index> m_Pinv;  

    RealScalar m_shiftOffset;
    RealScalar m_shiftScale;
};

template<typename _MatrixType, int _UpLo = Lower, typename _Ordering = AMDOrdering<typename _MatrixType::Index> > class SimplicialLLT;
template<typename _MatrixType, int _UpLo = Lower, typename _Ordering = AMDOrdering<typename _MatrixType::Index> > class SimplicialLDLT;
template<typename _MatrixType, int _UpLo = Lower, typename _Ordering = AMDOrdering<typename _MatrixType::Index> > class SimplicialCholesky;

namespace internal {

template<typename _MatrixType, int _UpLo, typename _Ordering> struct traits<SimplicialLLT<_MatrixType,_UpLo,_Ordering> >
{
  typedef _MatrixType MatrixType;
  typedef _Ordering OrderingType;
  enum { UpLo = _UpLo };
  typedef typename MatrixType::Scalar                         Scalar;
  typedef typename MatrixType::Index                          Index;
  typedef SparseMatrix<Scalar, ColMajor, Index>               CholMatrixType;
  typedef SparseTriangularView<CholMatrixType, Eigen::Lower>  MatrixL;
  typedef SparseTriangularView<typename CholMatrixType::AdjointReturnType, Eigen::Upper>   MatrixU;
  static inline MatrixL getL(const MatrixType& m) { return m; }
  static inline MatrixU getU(const MatrixType& m) { return m.adjoint(); }
};

template<typename _MatrixType,int _UpLo, typename _Ordering> struct traits<SimplicialLDLT<_MatrixType,_UpLo,_Ordering> >
{
  typedef _MatrixType MatrixType;
  typedef _Ordering OrderingType;
  enum { UpLo = _UpLo };
  typedef typename MatrixType::Scalar                             Scalar;
  typedef typename MatrixType::Index                              Index;
  typedef SparseMatrix<Scalar, ColMajor, Index>                   CholMatrixType;
  typedef SparseTriangularView<CholMatrixType, Eigen::UnitLower>  MatrixL;
  typedef SparseTriangularView<typename CholMatrixType::AdjointReturnType, Eigen::UnitUpper> MatrixU;
  static inline MatrixL getL(const MatrixType& m) { return m; }
  static inline MatrixU getU(const MatrixType& m) { return m.adjoint(); }
};

template<typename _MatrixType, int _UpLo, typename _Ordering> struct traits<SimplicialCholesky<_MatrixType,_UpLo,_Ordering> >
{
  typedef _MatrixType MatrixType;
  typedef _Ordering OrderingType;
  enum { UpLo = _UpLo };
};

}

template<typename _MatrixType, int _UpLo, typename _Ordering>
    class SimplicialLLT : public SimplicialCholeskyBase<SimplicialLLT<_MatrixType,_UpLo,_Ordering> >
{
public:
    typedef _MatrixType MatrixType;
    enum { UpLo = _UpLo };
    typedef SimplicialCholeskyBase<SimplicialLLT> Base;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::RealScalar RealScalar;
    typedef typename MatrixType::Index Index;
    typedef SparseMatrix<Scalar,ColMajor,Index> CholMatrixType;
    typedef Matrix<Scalar,Dynamic,1> VectorType;
    typedef internal::traits<SimplicialLLT> Traits;
    typedef typename Traits::MatrixL  MatrixL;
    typedef typename Traits::MatrixU  MatrixU;
public:
    
    SimplicialLLT() : Base() {}
    
    SimplicialLLT(const MatrixType& matrix)
        : Base(matrix) {}

    
    inline const MatrixL matrixL() const {
        eigen_assert(Base::m_factorizationIsOk && "Simplicial LLT not factorized");
        return Traits::getL(Base::m_matrix);
    }

    
    inline const MatrixU matrixU() const {
        eigen_assert(Base::m_factorizationIsOk && "Simplicial LLT not factorized");
        return Traits::getU(Base::m_matrix);
    }
    
    
    SimplicialLLT& compute(const MatrixType& matrix)
    {
      Base::template compute<false>(matrix);
      return *this;
    }

    void analyzePattern(const MatrixType& a)
    {
      Base::analyzePattern(a, false);
    }

    void factorize(const MatrixType& a)
    {
      Base::template factorize<false>(a);
    }

    
    Scalar determinant() const
    {
      Scalar detL = Base::m_matrix.diagonal().prod();
      return numext::abs2(detL);
    }
};

template<typename _MatrixType, int _UpLo, typename _Ordering>
    class SimplicialLDLT : public SimplicialCholeskyBase<SimplicialLDLT<_MatrixType,_UpLo,_Ordering> >
{
public:
    typedef _MatrixType MatrixType;
    enum { UpLo = _UpLo };
    typedef SimplicialCholeskyBase<SimplicialLDLT> Base;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::RealScalar RealScalar;
    typedef typename MatrixType::Index Index;
    typedef SparseMatrix<Scalar,ColMajor,Index> CholMatrixType;
    typedef Matrix<Scalar,Dynamic,1> VectorType;
    typedef internal::traits<SimplicialLDLT> Traits;
    typedef typename Traits::MatrixL  MatrixL;
    typedef typename Traits::MatrixU  MatrixU;
public:
    
    SimplicialLDLT() : Base() {}

    
    SimplicialLDLT(const MatrixType& matrix)
        : Base(matrix) {}

    
    inline const VectorType vectorD() const {
        eigen_assert(Base::m_factorizationIsOk && "Simplicial LDLT not factorized");
        return Base::m_diag;
    }
    
    inline const MatrixL matrixL() const {
        eigen_assert(Base::m_factorizationIsOk && "Simplicial LDLT not factorized");
        return Traits::getL(Base::m_matrix);
    }

    
    inline const MatrixU matrixU() const {
        eigen_assert(Base::m_factorizationIsOk && "Simplicial LDLT not factorized");
        return Traits::getU(Base::m_matrix);
    }

    
    SimplicialLDLT& compute(const MatrixType& matrix)
    {
      Base::template compute<true>(matrix);
      return *this;
    }
    
    void analyzePattern(const MatrixType& a)
    {
      Base::analyzePattern(a, true);
    }

    void factorize(const MatrixType& a)
    {
      Base::template factorize<true>(a);
    }

    
    Scalar determinant() const
    {
      return Base::m_diag.prod();
    }
};

template<typename _MatrixType, int _UpLo, typename _Ordering>
    class SimplicialCholesky : public SimplicialCholeskyBase<SimplicialCholesky<_MatrixType,_UpLo,_Ordering> >
{
public:
    typedef _MatrixType MatrixType;
    enum { UpLo = _UpLo };
    typedef SimplicialCholeskyBase<SimplicialCholesky> Base;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::RealScalar RealScalar;
    typedef typename MatrixType::Index Index;
    typedef SparseMatrix<Scalar,ColMajor,Index> CholMatrixType;
    typedef Matrix<Scalar,Dynamic,1> VectorType;
    typedef internal::traits<SimplicialCholesky> Traits;
    typedef internal::traits<SimplicialLDLT<MatrixType,UpLo> > LDLTTraits;
    typedef internal::traits<SimplicialLLT<MatrixType,UpLo>  > LLTTraits;
  public:
    SimplicialCholesky() : Base(), m_LDLT(true) {}

    SimplicialCholesky(const MatrixType& matrix)
      : Base(), m_LDLT(true)
    {
      compute(matrix);
    }

    SimplicialCholesky& setMode(SimplicialCholeskyMode mode)
    {
      switch(mode)
      {
      case SimplicialCholeskyLLT:
        m_LDLT = false;
        break;
      case SimplicialCholeskyLDLT:
        m_LDLT = true;
        break;
      default:
        break;
      }

      return *this;
    }

    inline const VectorType vectorD() const {
        eigen_assert(Base::m_factorizationIsOk && "Simplicial Cholesky not factorized");
        return Base::m_diag;
    }
    inline const CholMatrixType rawMatrix() const {
        eigen_assert(Base::m_factorizationIsOk && "Simplicial Cholesky not factorized");
        return Base::m_matrix;
    }
    
    
    SimplicialCholesky& compute(const MatrixType& matrix)
    {
      if(m_LDLT)
        Base::template compute<true>(matrix);
      else
        Base::template compute<false>(matrix);
      return *this;
    }

    void analyzePattern(const MatrixType& a)
    {
      Base::analyzePattern(a, m_LDLT);
    }

    void factorize(const MatrixType& a)
    {
      if(m_LDLT)
        Base::template factorize<true>(a);
      else
        Base::template factorize<false>(a);
    }

    
    template<typename Rhs,typename Dest>
    void _solve(const MatrixBase<Rhs> &b, MatrixBase<Dest> &dest) const
    {
      eigen_assert(Base::m_factorizationIsOk && "The decomposition is not in a valid state for solving, you must first call either compute() or symbolic()/numeric()");
      eigen_assert(Base::m_matrix.rows()==b.rows());

      if(Base::m_info!=Success)
        return;

      if(Base::m_P.size()>0)
        dest = Base::m_P * b;
      else
        dest = b;

      if(Base::m_matrix.nonZeros()>0) 
      {
        if(m_LDLT)
          LDLTTraits::getL(Base::m_matrix).solveInPlace(dest);
        else
          LLTTraits::getL(Base::m_matrix).solveInPlace(dest);
      }

      if(Base::m_diag.size()>0)
        dest = Base::m_diag.asDiagonal().inverse() * dest;

      if (Base::m_matrix.nonZeros()>0) 
      {
        if(m_LDLT)
          LDLTTraits::getU(Base::m_matrix).solveInPlace(dest);
        else
          LLTTraits::getU(Base::m_matrix).solveInPlace(dest);
      }

      if(Base::m_P.size()>0)
        dest = Base::m_Pinv * dest;
    }
    
    Scalar determinant() const
    {
      if(m_LDLT)
      {
        return Base::m_diag.prod();
      }
      else
      {
        Scalar detL = Diagonal<const CholMatrixType>(Base::m_matrix).prod();
        return numext::abs2(detL);
      }
    }
    
  protected:
    bool m_LDLT;
};

template<typename Derived>
void SimplicialCholeskyBase<Derived>::ordering(const MatrixType& a, CholMatrixType& ap)
{
  eigen_assert(a.rows()==a.cols());
  const Index size = a.rows();
  
  {
    CholMatrixType C;
    C = a.template selfadjointView<UpLo>();
    
    OrderingType ordering;
    ordering(C,m_Pinv);
  }

  if(m_Pinv.size()>0)
    m_P = m_Pinv.inverse();
  else
    m_P.resize(0);

  ap.resize(size,size);
  ap.template selfadjointView<Upper>() = a.template selfadjointView<UpLo>().twistedBy(m_P);
}

namespace internal {
  
template<typename Derived, typename Rhs>
struct solve_retval<SimplicialCholeskyBase<Derived>, Rhs>
  : solve_retval_base<SimplicialCholeskyBase<Derived>, Rhs>
{
  typedef SimplicialCholeskyBase<Derived> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec().derived()._solve(rhs(),dst);
  }
};

template<typename Derived, typename Rhs>
struct sparse_solve_retval<SimplicialCholeskyBase<Derived>, Rhs>
  : sparse_solve_retval_base<SimplicialCholeskyBase<Derived>, Rhs>
{
  typedef SimplicialCholeskyBase<Derived> Dec;
  EIGEN_MAKE_SPARSE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    this->defaultEvalTo(dst);
  }
};

} 

} 

#endif 
