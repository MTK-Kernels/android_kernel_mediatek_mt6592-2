// Copyright (C) 2008-2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2009 Keir Mierle <mierle@gmail.com>
// Copyright (C) 2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2011 Timothy E. Holy <tim.holy@gmail.com >
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_LDLT_H
#define EIGEN_LDLT_H

namespace Eigen { 

namespace internal {
  template<typename MatrixType, int UpLo> struct LDLT_Traits;

  
  enum SignMatrix { PositiveSemiDef, NegativeSemiDef, ZeroSign, Indefinite };
}

template<typename _MatrixType, int _UpLo> class LDLT
{
  public:
    typedef _MatrixType MatrixType;
    enum {
      RowsAtCompileTime = MatrixType::RowsAtCompileTime,
      ColsAtCompileTime = MatrixType::ColsAtCompileTime,
      Options = MatrixType::Options & ~RowMajorBit, 
      MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
      MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime,
      UpLo = _UpLo
    };
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef typename MatrixType::Index Index;
    typedef Matrix<Scalar, RowsAtCompileTime, 1, Options, MaxRowsAtCompileTime, 1> TmpMatrixType;

    typedef Transpositions<RowsAtCompileTime, MaxRowsAtCompileTime> TranspositionType;
    typedef PermutationMatrix<RowsAtCompileTime, MaxRowsAtCompileTime> PermutationType;

    typedef internal::LDLT_Traits<MatrixType,UpLo> Traits;

    LDLT() 
      : m_matrix(), 
        m_transpositions(), 
        m_sign(internal::ZeroSign),
        m_isInitialized(false) 
    {}

    LDLT(Index size)
      : m_matrix(size, size),
        m_transpositions(size),
        m_temporary(size),
        m_sign(internal::ZeroSign),
        m_isInitialized(false)
    {}

    LDLT(const MatrixType& matrix)
      : m_matrix(matrix.rows(), matrix.cols()),
        m_transpositions(matrix.rows()),
        m_temporary(matrix.rows()),
        m_sign(internal::ZeroSign),
        m_isInitialized(false)
    {
      compute(matrix);
    }

    void setZero()
    {
      m_isInitialized = false;
    }

    
    inline typename Traits::MatrixU matrixU() const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return Traits::getU(m_matrix);
    }

    
    inline typename Traits::MatrixL matrixL() const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return Traits::getL(m_matrix);
    }

    inline const TranspositionType& transpositionsP() const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return m_transpositions;
    }

    
    inline Diagonal<const MatrixType> vectorD() const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return m_matrix.diagonal();
    }

    
    inline bool isPositive() const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return m_sign == internal::PositiveSemiDef || m_sign == internal::ZeroSign;
    }
    
    #ifdef EIGEN2_SUPPORT
    inline bool isPositiveDefinite() const
    {
      return isPositive();
    }
    #endif

    
    inline bool isNegative(void) const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return m_sign == internal::NegativeSemiDef || m_sign == internal::ZeroSign;
    }

    template<typename Rhs>
    inline const internal::solve_retval<LDLT, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      eigen_assert(m_matrix.rows()==b.rows()
                && "LDLT::solve(): invalid number of rows of the right hand side matrix b");
      return internal::solve_retval<LDLT, Rhs>(*this, b.derived());
    }

    #ifdef EIGEN2_SUPPORT
    template<typename OtherDerived, typename ResultType>
    bool solve(const MatrixBase<OtherDerived>& b, ResultType *result) const
    {
      *result = this->solve(b);
      return true;
    }
    #endif

    template<typename Derived>
    bool solveInPlace(MatrixBase<Derived> &bAndX) const;

    LDLT& compute(const MatrixType& matrix);

    template <typename Derived>
    LDLT& rankUpdate(const MatrixBase<Derived>& w, const RealScalar& alpha=1);

    inline const MatrixType& matrixLDLT() const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return m_matrix;
    }

    MatrixType reconstructedMatrix() const;

    inline Index rows() const { return m_matrix.rows(); }
    inline Index cols() const { return m_matrix.cols(); }

    ComputationInfo info() const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return Success;
    }

  protected:

    MatrixType m_matrix;
    TranspositionType m_transpositions;
    TmpMatrixType m_temporary;
    internal::SignMatrix m_sign;
    bool m_isInitialized;
};

namespace internal {

template<int UpLo> struct ldlt_inplace;

template<> struct ldlt_inplace<Lower>
{
  template<typename MatrixType, typename TranspositionType, typename Workspace>
  static bool unblocked(MatrixType& mat, TranspositionType& transpositions, Workspace& temp, SignMatrix& sign)
  {
    using std::abs;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::RealScalar RealScalar;
    typedef typename MatrixType::Index Index;
    eigen_assert(mat.rows()==mat.cols());
    const Index size = mat.rows();

    if (size <= 1)
    {
      transpositions.setIdentity();
      if (numext::real(mat.coeff(0,0)) > 0) sign = PositiveSemiDef;
      else if (numext::real(mat.coeff(0,0)) < 0) sign = NegativeSemiDef;
      else sign = ZeroSign;
      return true;
    }

    for (Index k = 0; k < size; ++k)
    {
      
      Index index_of_biggest_in_corner;
      mat.diagonal().tail(size-k).cwiseAbs().maxCoeff(&index_of_biggest_in_corner);
      index_of_biggest_in_corner += k;

      transpositions.coeffRef(k) = index_of_biggest_in_corner;
      if(k != index_of_biggest_in_corner)
      {
        
        
        Index s = size-index_of_biggest_in_corner-1; 
        mat.row(k).head(k).swap(mat.row(index_of_biggest_in_corner).head(k));
        mat.col(k).tail(s).swap(mat.col(index_of_biggest_in_corner).tail(s));
        std::swap(mat.coeffRef(k,k),mat.coeffRef(index_of_biggest_in_corner,index_of_biggest_in_corner));
        for(int i=k+1;i<index_of_biggest_in_corner;++i)
        {
          Scalar tmp = mat.coeffRef(i,k);
          mat.coeffRef(i,k) = numext::conj(mat.coeffRef(index_of_biggest_in_corner,i));
          mat.coeffRef(index_of_biggest_in_corner,i) = numext::conj(tmp);
        }
        if(NumTraits<Scalar>::IsComplex)
          mat.coeffRef(index_of_biggest_in_corner,k) = numext::conj(mat.coeff(index_of_biggest_in_corner,k));
      }

      
      
      
      
      Index rs = size - k - 1;
      Block<MatrixType,Dynamic,1> A21(mat,k+1,k,rs,1);
      Block<MatrixType,1,Dynamic> A10(mat,k,0,1,k);
      Block<MatrixType,Dynamic,Dynamic> A20(mat,k+1,0,rs,k);

      if(k>0)
      {
        temp.head(k) = mat.diagonal().real().head(k).asDiagonal() * A10.adjoint();
        mat.coeffRef(k,k) -= (A10 * temp.head(k)).value();
        if(rs>0)
          A21.noalias() -= A20 * temp.head(k);
      }
      
      
      
      
      
      RealScalar realAkk = numext::real(mat.coeffRef(k,k));
      if((rs>0) && (abs(realAkk) > RealScalar(0)))
        A21 /= realAkk;

      if (sign == PositiveSemiDef) {
        if (realAkk < 0) sign = Indefinite;
      } else if (sign == NegativeSemiDef) {
        if (realAkk > 0) sign = Indefinite;
      } else if (sign == ZeroSign) {
        if (realAkk > 0) sign = PositiveSemiDef;
        else if (realAkk < 0) sign = NegativeSemiDef;
      }
    }

    return true;
  }

  
  
  
  
  
  
  
  template<typename MatrixType, typename WDerived>
  static bool updateInPlace(MatrixType& mat, MatrixBase<WDerived>& w, const typename MatrixType::RealScalar& sigma=1)
  {
    using numext::isfinite;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::RealScalar RealScalar;
    typedef typename MatrixType::Index Index;

    const Index size = mat.rows();
    eigen_assert(mat.cols() == size && w.size()==size);

    RealScalar alpha = 1;

    
    for (Index j = 0; j < size; j++)
    {
      
      if (!(isfinite)(alpha))
        break;

      
      RealScalar dj = numext::real(mat.coeff(j,j));
      Scalar wj = w.coeff(j);
      RealScalar swj2 = sigma*numext::abs2(wj);
      RealScalar gamma = dj*alpha + swj2;

      mat.coeffRef(j,j) += swj2/alpha;
      alpha += swj2/dj;


      
      Index rs = size-j-1;
      w.tail(rs) -= wj * mat.col(j).tail(rs);
      if(gamma != 0)
        mat.col(j).tail(rs) += (sigma*numext::conj(wj)/gamma)*w.tail(rs);
    }
    return true;
  }

  template<typename MatrixType, typename TranspositionType, typename Workspace, typename WType>
  static bool update(MatrixType& mat, const TranspositionType& transpositions, Workspace& tmp, const WType& w, const typename MatrixType::RealScalar& sigma=1)
  {
    
    tmp = transpositions * w;

    return ldlt_inplace<Lower>::updateInPlace(mat,tmp,sigma);
  }
};

template<> struct ldlt_inplace<Upper>
{
  template<typename MatrixType, typename TranspositionType, typename Workspace>
  static EIGEN_STRONG_INLINE bool unblocked(MatrixType& mat, TranspositionType& transpositions, Workspace& temp, SignMatrix& sign)
  {
    Transpose<MatrixType> matt(mat);
    return ldlt_inplace<Lower>::unblocked(matt, transpositions, temp, sign);
  }

  template<typename MatrixType, typename TranspositionType, typename Workspace, typename WType>
  static EIGEN_STRONG_INLINE bool update(MatrixType& mat, TranspositionType& transpositions, Workspace& tmp, WType& w, const typename MatrixType::RealScalar& sigma=1)
  {
    Transpose<MatrixType> matt(mat);
    return ldlt_inplace<Lower>::update(matt, transpositions, tmp, w.conjugate(), sigma);
  }
};

template<typename MatrixType> struct LDLT_Traits<MatrixType,Lower>
{
  typedef const TriangularView<const MatrixType, UnitLower> MatrixL;
  typedef const TriangularView<const typename MatrixType::AdjointReturnType, UnitUpper> MatrixU;
  static inline MatrixL getL(const MatrixType& m) { return m; }
  static inline MatrixU getU(const MatrixType& m) { return m.adjoint(); }
};

template<typename MatrixType> struct LDLT_Traits<MatrixType,Upper>
{
  typedef const TriangularView<const typename MatrixType::AdjointReturnType, UnitLower> MatrixL;
  typedef const TriangularView<const MatrixType, UnitUpper> MatrixU;
  static inline MatrixL getL(const MatrixType& m) { return m.adjoint(); }
  static inline MatrixU getU(const MatrixType& m) { return m; }
};

} 

template<typename MatrixType, int _UpLo>
LDLT<MatrixType,_UpLo>& LDLT<MatrixType,_UpLo>::compute(const MatrixType& a)
{
  eigen_assert(a.rows()==a.cols());
  const Index size = a.rows();

  m_matrix = a;

  m_transpositions.resize(size);
  m_isInitialized = false;
  m_temporary.resize(size);
  m_sign = internal::ZeroSign;

  internal::ldlt_inplace<UpLo>::unblocked(m_matrix, m_transpositions, m_temporary, m_sign);

  m_isInitialized = true;
  return *this;
}

template<typename MatrixType, int _UpLo>
template<typename Derived>
LDLT<MatrixType,_UpLo>& LDLT<MatrixType,_UpLo>::rankUpdate(const MatrixBase<Derived>& w, const typename NumTraits<typename MatrixType::Scalar>::Real& sigma)
{
  const Index size = w.rows();
  if (m_isInitialized)
  {
    eigen_assert(m_matrix.rows()==size);
  }
  else
  {    
    m_matrix.resize(size,size);
    m_matrix.setZero();
    m_transpositions.resize(size);
    for (Index i = 0; i < size; i++)
      m_transpositions.coeffRef(i) = i;
    m_temporary.resize(size);
    m_sign = sigma>=0 ? internal::PositiveSemiDef : internal::NegativeSemiDef;
    m_isInitialized = true;
  }

  internal::ldlt_inplace<UpLo>::update(m_matrix, m_transpositions, m_temporary, w, sigma);

  return *this;
}

namespace internal {
template<typename _MatrixType, int _UpLo, typename Rhs>
struct solve_retval<LDLT<_MatrixType,_UpLo>, Rhs>
  : solve_retval_base<LDLT<_MatrixType,_UpLo>, Rhs>
{
  typedef LDLT<_MatrixType,_UpLo> LDLTType;
  EIGEN_MAKE_SOLVE_HELPERS(LDLTType,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    eigen_assert(rhs().rows() == dec().matrixLDLT().rows());
    
    dst = dec().transpositionsP() * rhs();

    
    dec().matrixL().solveInPlace(dst);

    
    
    using std::abs;
    using std::max;
    typedef typename LDLTType::MatrixType MatrixType;
    typedef typename LDLTType::RealScalar RealScalar;
    const typename Diagonal<const MatrixType>::RealReturnType vectorD(dec().vectorD());
    
    
    
    
    
    
    RealScalar tolerance = RealScalar(1) / NumTraits<RealScalar>::highest();
    
    for (Index i = 0; i < vectorD.size(); ++i) {
      if(abs(vectorD(i)) > tolerance)
        dst.row(i) /= vectorD(i);
      else
        dst.row(i).setZero();
    }

    
    dec().matrixU().solveInPlace(dst);

    
    dst = dec().transpositionsP().transpose() * dst;
  }
};
}

template<typename MatrixType,int _UpLo>
template<typename Derived>
bool LDLT<MatrixType,_UpLo>::solveInPlace(MatrixBase<Derived> &bAndX) const
{
  eigen_assert(m_isInitialized && "LDLT is not initialized.");
  eigen_assert(m_matrix.rows() == bAndX.rows());

  bAndX = this->solve(bAndX);

  return true;
}

template<typename MatrixType, int _UpLo>
MatrixType LDLT<MatrixType,_UpLo>::reconstructedMatrix() const
{
  eigen_assert(m_isInitialized && "LDLT is not initialized.");
  const Index size = m_matrix.rows();
  MatrixType res(size,size);

  
  res.setIdentity();
  res = transpositionsP() * res;
  
  res = matrixU() * res;
  
  res = vectorD().real().asDiagonal() * res;
  
  res = matrixL() * res;
  
  res = transpositionsP().transpose() * res;

  return res;
}

template<typename MatrixType, unsigned int UpLo>
inline const LDLT<typename SelfAdjointView<MatrixType, UpLo>::PlainObject, UpLo>
SelfAdjointView<MatrixType, UpLo>::ldlt() const
{
  return LDLT<PlainObject,UpLo>(m_matrix);
}

template<typename Derived>
inline const LDLT<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::ldlt() const
{
  return LDLT<PlainObject>(derived());
}

} 

#endif 
