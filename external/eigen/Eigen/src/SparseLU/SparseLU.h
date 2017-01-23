// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Copyright (C) 2012 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed


#ifndef EIGEN_SPARSE_LU_H
#define EIGEN_SPARSE_LU_H

namespace Eigen {

template <typename _MatrixType, typename _OrderingType = COLAMDOrdering<typename _MatrixType::Index> > class SparseLU;
template <typename MappedSparseMatrixType> struct SparseLUMatrixLReturnType;
template <typename MatrixLType, typename MatrixUType> struct SparseLUMatrixUReturnType;

template <typename _MatrixType, typename _OrderingType>
class SparseLU : public internal::SparseLUImpl<typename _MatrixType::Scalar, typename _MatrixType::Index>
{
  public:
    typedef _MatrixType MatrixType; 
    typedef _OrderingType OrderingType;
    typedef typename MatrixType::Scalar Scalar; 
    typedef typename MatrixType::RealScalar RealScalar; 
    typedef typename MatrixType::Index Index; 
    typedef SparseMatrix<Scalar,ColMajor,Index> NCMatrix;
    typedef internal::MappedSuperNodalMatrix<Scalar, Index> SCMatrix; 
    typedef Matrix<Scalar,Dynamic,1> ScalarVector;
    typedef Matrix<Index,Dynamic,1> IndexVector;
    typedef PermutationMatrix<Dynamic, Dynamic, Index> PermutationType;
    typedef internal::SparseLUImpl<Scalar, Index> Base;
    
  public:
    SparseLU():m_isInitialized(true),m_lastError(""),m_Ustore(0,0,0,0,0,0),m_symmetricmode(false),m_diagpivotthresh(1.0),m_detPermR(1)
    {
      initperfvalues(); 
    }
    SparseLU(const MatrixType& matrix):m_isInitialized(true),m_lastError(""),m_Ustore(0,0,0,0,0,0),m_symmetricmode(false),m_diagpivotthresh(1.0),m_detPermR(1)
    {
      initperfvalues(); 
      compute(matrix);
    }
    
    ~SparseLU()
    {
      
    }
    
    void analyzePattern (const MatrixType& matrix);
    void factorize (const MatrixType& matrix);
    void simplicialfactorize(const MatrixType& matrix);
    
    void compute (const MatrixType& matrix)
    {
      
      analyzePattern(matrix); 
      
      factorize(matrix);
    } 
    
    inline Index rows() const { return m_mat.rows(); }
    inline Index cols() const { return m_mat.cols(); }
    
    void isSymmetric(bool sym)
    {
      m_symmetricmode = sym;
    }
    
    SparseLUMatrixLReturnType<SCMatrix> matrixL() const
    {
      return SparseLUMatrixLReturnType<SCMatrix>(m_Lstore);
    }
    SparseLUMatrixUReturnType<SCMatrix,MappedSparseMatrix<Scalar,ColMajor,Index> > matrixU() const
    {
      return SparseLUMatrixUReturnType<SCMatrix, MappedSparseMatrix<Scalar,ColMajor,Index> >(m_Lstore, m_Ustore);
    }

    inline const PermutationType& rowsPermutation() const
    {
      return m_perm_r;
    }
    inline const PermutationType& colsPermutation() const
    {
      return m_perm_c;
    }
    
    void setPivotThreshold(const RealScalar& thresh)
    {
      m_diagpivotthresh = thresh; 
    }

    template<typename Rhs>
    inline const internal::solve_retval<SparseLU, Rhs> solve(const MatrixBase<Rhs>& B) const 
    {
      eigen_assert(m_factorizationIsOk && "SparseLU is not initialized."); 
      eigen_assert(rows()==B.rows()
                    && "SparseLU::solve(): invalid number of rows of the right hand side matrix B");
          return internal::solve_retval<SparseLU, Rhs>(*this, B.derived());
    }

    template<typename Rhs>
    inline const internal::sparse_solve_retval<SparseLU, Rhs> solve(const SparseMatrixBase<Rhs>& B) const 
    {
      eigen_assert(m_factorizationIsOk && "SparseLU is not initialized."); 
      eigen_assert(rows()==B.rows()
                    && "SparseLU::solve(): invalid number of rows of the right hand side matrix B");
          return internal::sparse_solve_retval<SparseLU, Rhs>(*this, B.derived());
    }
    
    ComputationInfo info() const
    {
      eigen_assert(m_isInitialized && "Decomposition is not initialized.");
      return m_info;
    }
    
    std::string lastErrorMessage() const
    {
      return m_lastError; 
    }

    template<typename Rhs, typename Dest>
    bool _solve(const MatrixBase<Rhs> &B, MatrixBase<Dest> &X_base) const
    {
      Dest& X(X_base.derived());
      eigen_assert(m_factorizationIsOk && "The matrix should be factorized first");
      EIGEN_STATIC_ASSERT((Dest::Flags&RowMajorBit)==0,
                        THIS_METHOD_IS_ONLY_FOR_COLUMN_MAJOR_MATRICES);
      
      
      // on return, X is overwritten by the computed solution
      X.resize(B.rows(),B.cols());

      
      for(Index j = 0; j < B.cols(); ++j)
        X.col(j) = rowsPermutation() * B.const_cast_derived().col(j);
      
      
      this->matrixL().solveInPlace(X);
      this->matrixU().solveInPlace(X);
      
      
      for (Index j = 0; j < B.cols(); ++j)
        X.col(j) = colsPermutation().inverse() * X.col(j);
      
      return true; 
    }
    
     Scalar absDeterminant()
    {
      eigen_assert(m_factorizationIsOk && "The matrix should be factorized first.");
      
      Scalar det = Scalar(1.);
      
      
      for (Index j = 0; j < this->cols(); ++j)
      {
        for (typename SCMatrix::InnerIterator it(m_Lstore, j); it; ++it)
        {
          if(it.index() == j)
          {
            det *= (std::abs)(it.value());
            break;
          }
        }
       }
       return det;
     }

     Scalar logAbsDeterminant() const
     {
       eigen_assert(m_factorizationIsOk && "The matrix should be factorized first.");
       Scalar det = Scalar(0.);
       for (Index j = 0; j < this->cols(); ++j)
       {
         for (typename SCMatrix::InnerIterator it(m_Lstore, j); it; ++it)
         {
           if(it.row() < j) continue;
           if(it.row() == j)
           {
             det += (std::log)((std::abs)(it.value()));
             break;
           }
         }
       }
       return det;
     }

     Scalar signDeterminant()
     {
       eigen_assert(m_factorizationIsOk && "The matrix should be factorized first.");
       return Scalar(m_detPermR);
     }

  protected:
    
    void initperfvalues()
    {
      m_perfv.panel_size = 1;
      m_perfv.relax = 1; 
      m_perfv.maxsuper = 128; 
      m_perfv.rowblk = 16; 
      m_perfv.colblk = 8; 
      m_perfv.fillfactor = 20;  
    }
      
    
    mutable ComputationInfo m_info;
    bool m_isInitialized;
    bool m_factorizationIsOk;
    bool m_analysisIsOk;
    std::string m_lastError;
    NCMatrix m_mat; 
    SCMatrix m_Lstore; 
    MappedSparseMatrix<Scalar,ColMajor,Index> m_Ustore; 
    PermutationType m_perm_c; 
    PermutationType m_perm_r ; 
    IndexVector m_etree; 
    
    typename Base::GlobalLU_t m_glu; 
                               
    
    bool m_symmetricmode;
    
    internal::perfvalues<Index> m_perfv; 
    RealScalar m_diagpivotthresh; 
    Index m_nnzL, m_nnzU; 
    Index m_detPermR; 
  private:
    
    SparseLU (const SparseLU& );
  
}; 



template <typename MatrixType, typename OrderingType>
void SparseLU<MatrixType, OrderingType>::analyzePattern(const MatrixType& mat)
{
  
  
  
  OrderingType ord; 
  ord(mat,m_perm_c);
  
  
  
  m_mat = mat;
  if (m_perm_c.size()) {
    m_mat.uncompress(); 
    
    const Index * outerIndexPtr;
    if (mat.isCompressed()) outerIndexPtr = mat.outerIndexPtr();
    else
    {
      Index *outerIndexPtr_t = new Index[mat.cols()+1];
      for(Index i = 0; i <= mat.cols(); i++) outerIndexPtr_t[i] = m_mat.outerIndexPtr()[i];
      outerIndexPtr = outerIndexPtr_t;
    }
    for (Index i = 0; i < mat.cols(); i++)
    {
      m_mat.outerIndexPtr()[m_perm_c.indices()(i)] = outerIndexPtr[i];
      m_mat.innerNonZeroPtr()[m_perm_c.indices()(i)] = outerIndexPtr[i+1] - outerIndexPtr[i];
    }
    if(!mat.isCompressed()) delete[] outerIndexPtr;
  }
  
  IndexVector firstRowElt;
  internal::coletree(m_mat, m_etree,firstRowElt); 
     
  
  if (!m_symmetricmode) {
    IndexVector post, iwork; 
    
    internal::treePostorder(m_mat.cols(), m_etree, post); 
      
   
    
    Index m = m_mat.cols(); 
    iwork.resize(m+1);
    for (Index i = 0; i < m; ++i) iwork(post(i)) = post(m_etree(i));
    m_etree = iwork;
    
    
    PermutationType post_perm(m); 
    for (Index i = 0; i < m; i++) 
      post_perm.indices()(i) = post(i); 
        
    
    if(m_perm_c.size()) {
      m_perm_c = post_perm * m_perm_c;
    }
    
  } 
  
  m_analysisIsOk = true; 
}



template <typename MatrixType, typename OrderingType>
void SparseLU<MatrixType, OrderingType>::factorize(const MatrixType& matrix)
{
  using internal::emptyIdxLU;
  eigen_assert(m_analysisIsOk && "analyzePattern() should be called first"); 
  eigen_assert((matrix.rows() == matrix.cols()) && "Only for squared matrices");
  
  typedef typename IndexVector::Scalar Index; 
  
  
  
  
  m_mat = matrix;
  if (m_perm_c.size()) 
  {
    m_mat.uncompress(); 
    
    const Index * outerIndexPtr;
    if (matrix.isCompressed()) outerIndexPtr = matrix.outerIndexPtr();
    else
    {
      Index* outerIndexPtr_t = new Index[matrix.cols()+1];
      for(Index i = 0; i <= matrix.cols(); i++) outerIndexPtr_t[i] = m_mat.outerIndexPtr()[i];
      outerIndexPtr = outerIndexPtr_t;
    }
    for (Index i = 0; i < matrix.cols(); i++)
    {
      m_mat.outerIndexPtr()[m_perm_c.indices()(i)] = outerIndexPtr[i];
      m_mat.innerNonZeroPtr()[m_perm_c.indices()(i)] = outerIndexPtr[i+1] - outerIndexPtr[i];
    }
    if(!matrix.isCompressed()) delete[] outerIndexPtr;
  } 
  else 
  { 
    m_perm_c.resize(matrix.cols());
    for(Index i = 0; i < matrix.cols(); ++i) m_perm_c.indices()(i) = i;
  }
  
  Index m = m_mat.rows();
  Index n = m_mat.cols();
  Index nnz = m_mat.nonZeros();
  Index maxpanel = m_perfv.panel_size * m;
  
  Index lwork = 0;
  Index info = Base::memInit(m, n, nnz, lwork, m_perfv.fillfactor, m_perfv.panel_size, m_glu); 
  if (info) 
  {
    m_lastError = "UNABLE TO ALLOCATE WORKING MEMORY\n\n" ;
    m_factorizationIsOk = false;
    return ; 
  }
  
  
  IndexVector segrep(m); segrep.setZero();
  IndexVector parent(m); parent.setZero();
  IndexVector xplore(m); xplore.setZero();
  IndexVector repfnz(maxpanel);
  IndexVector panel_lsub(maxpanel);
  IndexVector xprune(n); xprune.setZero();
  IndexVector marker(m*internal::LUNoMarker); marker.setZero();
  
  repfnz.setConstant(-1); 
  panel_lsub.setConstant(-1);
  
  
  ScalarVector dense; 
  dense.setZero(maxpanel);
  ScalarVector tempv; 
  tempv.setZero(internal::LUnumTempV(m, m_perfv.panel_size, m_perfv.maxsuper, m) );
  
  
  PermutationType iperm_c(m_perm_c.inverse()); 
  
  
  IndexVector relax_end(n);
  if ( m_symmetricmode == true ) 
    Base::heap_relax_snode(n, m_etree, m_perfv.relax, marker, relax_end);
  else
    Base::relax_snode(n, m_etree, m_perfv.relax, marker, relax_end);
  
  
  m_perm_r.resize(m); 
  m_perm_r.indices().setConstant(-1);
  marker.setConstant(-1);
  m_detPermR = 1; 
  
  m_glu.supno(0) = emptyIdxLU; m_glu.xsup.setConstant(0);
  m_glu.xsup(0) = m_glu.xlsub(0) = m_glu.xusub(0) = m_glu.xlusup(0) = Index(0);
  
  
  
  
  Index jcol; 
  IndexVector panel_histo(n);
  Index pivrow; 
  Index nseg1; 
  Index nseg; 
  Index irep; 
  Index i, k, jj; 
  for (jcol = 0; jcol < n; )
  {
    
    Index panel_size = m_perfv.panel_size; 
    for (k = jcol + 1; k < (std::min)(jcol+panel_size, n); k++)
    {
      if (relax_end(k) != emptyIdxLU) 
      {
        panel_size = k - jcol; 
        break; 
      }
    }
    if (k == n) 
      panel_size = n - jcol; 
      
    
    Base::panel_dfs(m, panel_size, jcol, m_mat, m_perm_r.indices(), nseg1, dense, panel_lsub, segrep, repfnz, xprune, marker, parent, xplore, m_glu); 
    
    
    Base::panel_bmod(m, panel_size, jcol, nseg1, dense, tempv, segrep, repfnz, m_glu); 
    
    
    for ( jj = jcol; jj< jcol + panel_size; jj++) 
    {
      k = (jj - jcol) * m; 
      
      nseg = nseg1; 
      
      VectorBlock<IndexVector> panel_lsubk(panel_lsub, k, m);
      VectorBlock<IndexVector> repfnz_k(repfnz, k, m); 
      info = Base::column_dfs(m, jj, m_perm_r.indices(), m_perfv.maxsuper, nseg, panel_lsubk, segrep, repfnz_k, xprune, marker, parent, xplore, m_glu); 
      if ( info ) 
      {
        m_lastError =  "UNABLE TO EXPAND MEMORY IN COLUMN_DFS() ";
        m_info = NumericalIssue; 
        m_factorizationIsOk = false; 
        return; 
      }
      
      VectorBlock<ScalarVector> dense_k(dense, k, m); 
      VectorBlock<IndexVector> segrep_k(segrep, nseg1, m-nseg1); 
      info = Base::column_bmod(jj, (nseg - nseg1), dense_k, tempv, segrep_k, repfnz_k, jcol, m_glu); 
      if ( info ) 
      {
        m_lastError = "UNABLE TO EXPAND MEMORY IN COLUMN_BMOD() ";
        m_info = NumericalIssue; 
        m_factorizationIsOk = false; 
        return; 
      }
      
      
      info = Base::copy_to_ucol(jj, nseg, segrep, repfnz_k ,m_perm_r.indices(), dense_k, m_glu); 
      if ( info ) 
      {
        m_lastError = "UNABLE TO EXPAND MEMORY IN COPY_TO_UCOL() ";
        m_info = NumericalIssue; 
        m_factorizationIsOk = false; 
        return; 
      }
      
      
      info = Base::pivotL(jj, m_diagpivotthresh, m_perm_r.indices(), iperm_c.indices(), pivrow, m_glu);
      if ( info ) 
      {
        m_lastError = "THE MATRIX IS STRUCTURALLY SINGULAR ... ZERO COLUMN AT ";
        std::ostringstream returnInfo;
        returnInfo << info; 
        m_lastError += returnInfo.str();
        m_info = NumericalIssue; 
        m_factorizationIsOk = false; 
        return; 
      }
      
      
      if (pivrow != jj) m_detPermR *= -1;

      
      Base::pruneL(jj, m_perm_r.indices(), pivrow, nseg, segrep, repfnz_k, xprune, m_glu); 
      
      
      for (i = 0; i < nseg; i++)
      {
        irep = segrep(i); 
        repfnz_k(irep) = emptyIdxLU; 
      }
    } 
    jcol += panel_size;  
  } 
  
  
  Base::countnz(n, m_nnzL, m_nnzU, m_glu); 
  
  Base::fixupL(n, m_perm_r.indices(), m_glu); 
  
  
  m_Lstore.setInfos(m, n, m_glu.lusup, m_glu.xlusup, m_glu.lsub, m_glu.xlsub, m_glu.supno, m_glu.xsup); 
  
  new (&m_Ustore) MappedSparseMatrix<Scalar, ColMajor, Index> ( m, n, m_nnzU, m_glu.xusub.data(), m_glu.usub.data(), m_glu.ucol.data() ); 
  
  m_info = Success;
  m_factorizationIsOk = true;
}

template<typename MappedSupernodalType>
struct SparseLUMatrixLReturnType : internal::no_assignment_operator
{
  typedef typename MappedSupernodalType::Index Index;
  typedef typename MappedSupernodalType::Scalar Scalar;
  SparseLUMatrixLReturnType(const MappedSupernodalType& mapL) : m_mapL(mapL)
  { }
  Index rows() { return m_mapL.rows(); }
  Index cols() { return m_mapL.cols(); }
  template<typename Dest>
  void solveInPlace( MatrixBase<Dest> &X) const
  {
    m_mapL.solveInPlace(X);
  }
  const MappedSupernodalType& m_mapL;
};

template<typename MatrixLType, typename MatrixUType>
struct SparseLUMatrixUReturnType : internal::no_assignment_operator
{
  typedef typename MatrixLType::Index Index;
  typedef typename MatrixLType::Scalar Scalar;
  SparseLUMatrixUReturnType(const MatrixLType& mapL, const MatrixUType& mapU)
  : m_mapL(mapL),m_mapU(mapU)
  { }
  Index rows() { return m_mapL.rows(); }
  Index cols() { return m_mapL.cols(); }

  template<typename Dest>   void solveInPlace(MatrixBase<Dest> &X) const
  {
    Index nrhs = X.cols();
    Index n = X.rows();
    
    for (Index k = m_mapL.nsuper(); k >= 0; k--)
    {
      Index fsupc = m_mapL.supToCol()[k];
      Index lda = m_mapL.colIndexPtr()[fsupc+1] - m_mapL.colIndexPtr()[fsupc]; 
      Index nsupc = m_mapL.supToCol()[k+1] - fsupc;
      Index luptr = m_mapL.colIndexPtr()[fsupc];

      if (nsupc == 1)
      {
        for (Index j = 0; j < nrhs; j++)
        {
          X(fsupc, j) /= m_mapL.valuePtr()[luptr];
        }
      }
      else
      {
        Map<const Matrix<Scalar,Dynamic,Dynamic>, 0, OuterStride<> > A( &(m_mapL.valuePtr()[luptr]), nsupc, nsupc, OuterStride<>(lda) );
        Map< Matrix<Scalar,Dynamic,Dynamic>, 0, OuterStride<> > U (&(X(fsupc,0)), nsupc, nrhs, OuterStride<>(n) );
        U = A.template triangularView<Upper>().solve(U);
      }

      for (Index j = 0; j < nrhs; ++j)
      {
        for (Index jcol = fsupc; jcol < fsupc + nsupc; jcol++)
        {
          typename MatrixUType::InnerIterator it(m_mapU, jcol);
          for ( ; it; ++it)
          {
            Index irow = it.index();
            X(irow, j) -= X(jcol, j) * it.value();
          }
        }
      }
    } 
  }
  const MatrixLType& m_mapL;
  const MatrixUType& m_mapU;
};

namespace internal {
  
template<typename _MatrixType, typename Derived, typename Rhs>
struct solve_retval<SparseLU<_MatrixType,Derived>, Rhs>
  : solve_retval_base<SparseLU<_MatrixType,Derived>, Rhs>
{
  typedef SparseLU<_MatrixType,Derived> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec()._solve(rhs(),dst);
  }
};

template<typename _MatrixType, typename Derived, typename Rhs>
struct sparse_solve_retval<SparseLU<_MatrixType,Derived>, Rhs>
  : sparse_solve_retval_base<SparseLU<_MatrixType,Derived>, Rhs>
{
  typedef SparseLU<_MatrixType,Derived> Dec;
  EIGEN_MAKE_SPARSE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    this->defaultEvalTo(dst);
  }
};
} 

} 

#endif
