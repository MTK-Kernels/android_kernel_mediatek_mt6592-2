// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Copyright (C) 2012 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SPARSELU_SUPERNODAL_MATRIX_H
#define EIGEN_SPARSELU_SUPERNODAL_MATRIX_H

namespace Eigen {
namespace internal {

template <typename _Scalar, typename _Index>
class MappedSuperNodalMatrix
{
  public:
    typedef _Scalar Scalar; 
    typedef _Index Index;
    typedef Matrix<Index,Dynamic,1> IndexVector; 
    typedef Matrix<Scalar,Dynamic,1> ScalarVector;
  public:
    MappedSuperNodalMatrix()
    {
      
    }
    MappedSuperNodalMatrix(Index m, Index n,  ScalarVector& nzval, IndexVector& nzval_colptr, IndexVector& rowind, 
             IndexVector& rowind_colptr, IndexVector& col_to_sup, IndexVector& sup_to_col )
    {
      setInfos(m, n, nzval, nzval_colptr, rowind, rowind_colptr, col_to_sup, sup_to_col);
    }
    
    ~MappedSuperNodalMatrix()
    {
      
    }
    void setInfos(Index m, Index n, ScalarVector& nzval, IndexVector& nzval_colptr, IndexVector& rowind, 
             IndexVector& rowind_colptr, IndexVector& col_to_sup, IndexVector& sup_to_col )
    {
      m_row = m;
      m_col = n; 
      m_nzval = nzval.data(); 
      m_nzval_colptr = nzval_colptr.data(); 
      m_rowind = rowind.data(); 
      m_rowind_colptr = rowind_colptr.data(); 
      m_nsuper = col_to_sup(n); 
      m_col_to_sup = col_to_sup.data(); 
      m_sup_to_col = sup_to_col.data(); 
    }
    
    Index rows() { return m_row; }
    
    Index cols() { return m_col; }
    
    Scalar* valuePtr() {  return m_nzval; }
    
    const Scalar* valuePtr() const 
    {
      return m_nzval; 
    }
    Index* colIndexPtr()
    {
      return m_nzval_colptr; 
    }
    
    const Index* colIndexPtr() const
    {
      return m_nzval_colptr; 
    }
    
    Index* rowIndex()  { return m_rowind; }
    
    const Index* rowIndex() const
    {
      return m_rowind; 
    }
    
    Index* rowIndexPtr() { return m_rowind_colptr; }
    
    const Index* rowIndexPtr() const 
    {
      return m_rowind_colptr; 
    }
    
    Index* colToSup()  { return m_col_to_sup; }
    
    const Index* colToSup() const
    {
      return m_col_to_sup;       
    }
    Index* supToCol() { return m_sup_to_col; }
    
    const Index* supToCol() const 
    {
      return m_sup_to_col;
    }
    
    Index nsuper() const 
    {
      return m_nsuper; 
    }
    
    class InnerIterator; 
    template<typename Dest>
    void solveInPlace( MatrixBase<Dest>&X) const;
    
      
      
    
  protected:
    Index m_row; 
    Index m_col; 
    Index m_nsuper; 
    Scalar* m_nzval; 
    Index* m_nzval_colptr; 
    Index* m_rowind; 
    Index* m_rowind_colptr; 
    Index* m_col_to_sup; 
    Index* m_sup_to_col; 
    
  private :
};

template<typename Scalar, typename Index>
class MappedSuperNodalMatrix<Scalar,Index>::InnerIterator
{
  public:
     InnerIterator(const MappedSuperNodalMatrix& mat, Index outer)
      : m_matrix(mat),
        m_outer(outer), 
        m_supno(mat.colToSup()[outer]),
        m_idval(mat.colIndexPtr()[outer]),
        m_startidval(m_idval),
        m_endidval(mat.colIndexPtr()[outer+1]),
        m_idrow(mat.rowIndexPtr()[mat.supToCol()[mat.colToSup()[outer]]]),
        m_endidrow(mat.rowIndexPtr()[mat.supToCol()[mat.colToSup()[outer]]+1])
    {}
    inline InnerIterator& operator++()
    { 
      m_idval++; 
      m_idrow++;
      return *this;
    }
    inline Scalar value() const { return m_matrix.valuePtr()[m_idval]; }
    
    inline Scalar& valueRef() { return const_cast<Scalar&>(m_matrix.valuePtr()[m_idval]); }
    
    inline Index index() const { return m_matrix.rowIndex()[m_idrow]; }
    inline Index row() const { return index(); }
    inline Index col() const { return m_outer; }
    
    inline Index supIndex() const { return m_supno; }
    
    inline operator bool() const 
    { 
      return ( (m_idval < m_endidval) && (m_idval >= m_startidval)
                && (m_idrow < m_endidrow) );
    }
    
  protected:
    const MappedSuperNodalMatrix& m_matrix; 
    const Index m_outer;                    
    const Index m_supno;                    
    Index m_idval;                          
    const Index m_startidval;               
    const Index m_endidval;                 
    Index m_idrow;                          
    Index m_endidrow;                       
};

template<typename Scalar, typename Index>
template<typename Dest>
void MappedSuperNodalMatrix<Scalar,Index>::solveInPlace( MatrixBase<Dest>&X) const
{
    Index n = X.rows(); 
    Index nrhs = X.cols(); 
    const Scalar * Lval = valuePtr();                 
    Matrix<Scalar,Dynamic,Dynamic> work(n, nrhs);     
    work.setZero();
    for (Index k = 0; k <= nsuper(); k ++)
    {
      Index fsupc = supToCol()[k];                    
      Index istart = rowIndexPtr()[fsupc];            
      Index nsupr = rowIndexPtr()[fsupc+1] - istart;  
      Index nsupc = supToCol()[k+1] - fsupc;          
      Index nrow = nsupr - nsupc;                     
      Index irow;                                     
      
      if (nsupc == 1 )
      {
        for (Index j = 0; j < nrhs; j++)
        {
          InnerIterator it(*this, fsupc);
          ++it; 
          for (; it; ++it)
          {
            irow = it.row();
            X(irow, j) -= X(fsupc, j) * it.value();
          }
        }
      }
      else
      {
        
        Index luptr = colIndexPtr()[fsupc]; 
        Index lda = colIndexPtr()[fsupc+1] - luptr;
        
        
        Map<const Matrix<Scalar,Dynamic,Dynamic>, 0, OuterStride<> > A( &(Lval[luptr]), nsupc, nsupc, OuterStride<>(lda) ); 
        Map< Matrix<Scalar,Dynamic,Dynamic>, 0, OuterStride<> > U (&(X(fsupc,0)), nsupc, nrhs, OuterStride<>(n) ); 
        U = A.template triangularView<UnitLower>().solve(U); 
        
        
        new (&A) Map<const Matrix<Scalar,Dynamic,Dynamic>, 0, OuterStride<> > ( &(Lval[luptr+nsupc]), nrow, nsupc, OuterStride<>(lda) ); 
        work.block(0, 0, nrow, nrhs) = A * U; 
        
        
        for (Index j = 0; j < nrhs; j++)
        {
          Index iptr = istart + nsupc; 
          for (Index i = 0; i < nrow; i++)
          {
            irow = rowIndex()[iptr]; 
            X(irow, j) -= work(i, j); 
            work(i, j) = Scalar(0); 
            iptr++;
          }
        }
      }
    } 
}

} 

} 

#endif 
