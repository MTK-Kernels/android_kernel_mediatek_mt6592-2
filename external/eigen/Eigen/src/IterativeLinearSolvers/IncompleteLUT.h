// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_INCOMPLETE_LUT_H
#define EIGEN_INCOMPLETE_LUT_H


namespace Eigen { 

namespace internal {
    
 
template <typename VectorV, typename VectorI, typename Index>
Index QuickSplit(VectorV &row, VectorI &ind, Index ncut)
{
  typedef typename VectorV::RealScalar RealScalar;
  using std::swap;
  using std::abs;
  Index mid;
  Index n = row.size(); 
  Index first, last ;
  
  ncut--; 
  first = 0; 
  last = n-1; 
  if (ncut < first || ncut > last ) return 0;
  
  do {
    mid = first; 
    RealScalar abskey = abs(row(mid)); 
    for (Index j = first + 1; j <= last; j++) {
      if ( abs(row(j)) > abskey) {
        ++mid;
        swap(row(mid), row(j));
        swap(ind(mid), ind(j));
      }
    }
    
    swap(row(mid), row(first));
    swap(ind(mid), ind(first));
    
    if (mid > ncut) last = mid - 1;
    else if (mid < ncut ) first = mid + 1; 
  } while (mid != ncut );
  
  return 0;  
}

}

/** \ingroup IterativeLinearSolvers_Module
  * \class IncompleteLUT
  * \brief Incomplete LU factorization with dual-threshold strategy
  *
  * During the numerical factorization, two dropping rules are used :
  *  1) any element whose magnitude is less than some tolerance is dropped.
  *    This tolerance is obtained by multiplying the input tolerance @p droptol 
  *    by the average magnitude of all the original elements in the current row.
  *  2) After the elimination of the row, only the @p fill largest elements in 
  *    the L part and the @p fill largest elements in the U part are kept 
  *    (in addition to the diagonal element ). Note that @p fill is computed from 
  *    the input parameter @p fillfactor which is used the ratio to control the fill_in 
  *    relatively to the initial number of nonzero elements.
  * 
  * The two extreme cases are when @p droptol=0 (to keep all the @p fill*2 largest elements)
  * and when @p fill=n/2 with @p droptol being different to zero. 
  * 
  * References : Yousef Saad, ILUT: A dual threshold incomplete LU factorization, 
  *              Numerical Linear Algebra with Applications, 1(4), pp 387-402, 1994.
  * 
  * NOTE : The following implementation is derived from the ILUT implementation
  * in the SPARSKIT package, Copyright (C) 2005, the Regents of the University of Minnesota 
  *  released under the terms of the GNU LGPL: 
  *    http://www-users.cs.umn.edu/~saad/software/SPARSKIT/README
  * However, Yousef Saad gave us permission to relicense his ILUT code to MPL2.
  * See the Eigen mailing list archive, thread: ILUT, date: July 8, 2012:
  *   http://listengine.tuxfamily.org/lists.tuxfamily.org/eigen/2012/07/msg00064.html
  * alternatively, on GMANE:
  *   http://comments.gmane.org/gmane.comp.lib.eigen/3302
  */
template <typename _Scalar>
class IncompleteLUT : internal::noncopyable
{
    typedef _Scalar Scalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;
    typedef Matrix<Scalar,Dynamic,1> Vector;
    typedef SparseMatrix<Scalar,RowMajor> FactorType;
    typedef SparseMatrix<Scalar,ColMajor> PermutType;
    typedef typename FactorType::Index Index;

  public:
    typedef Matrix<Scalar,Dynamic,Dynamic> MatrixType;
    
    IncompleteLUT()
      : m_droptol(NumTraits<Scalar>::dummy_precision()), m_fillfactor(10),
        m_analysisIsOk(false), m_factorizationIsOk(false), m_isInitialized(false)
    {}
    
    template<typename MatrixType>
    IncompleteLUT(const MatrixType& mat, const RealScalar& droptol=NumTraits<Scalar>::dummy_precision(), int fillfactor = 10)
      : m_droptol(droptol),m_fillfactor(fillfactor),
        m_analysisIsOk(false),m_factorizationIsOk(false),m_isInitialized(false)
    {
      eigen_assert(fillfactor != 0);
      compute(mat); 
    }
    
    Index rows() const { return m_lu.rows(); }
    
    Index cols() const { return m_lu.cols(); }

    ComputationInfo info() const
    {
      eigen_assert(m_isInitialized && "IncompleteLUT is not initialized.");
      return m_info;
    }
    
    template<typename MatrixType>
    void analyzePattern(const MatrixType& amat);
    
    template<typename MatrixType>
    void factorize(const MatrixType& amat);
    
    template<typename MatrixType>
    IncompleteLUT<Scalar>& compute(const MatrixType& amat)
    {
      analyzePattern(amat); 
      factorize(amat);
      m_isInitialized = m_factorizationIsOk;
      return *this;
    }

    void setDroptol(const RealScalar& droptol); 
    void setFillfactor(int fillfactor); 
    
    template<typename Rhs, typename Dest>
    void _solve(const Rhs& b, Dest& x) const
    {
      x = m_Pinv * b;  
      x = m_lu.template triangularView<UnitLower>().solve(x);
      x = m_lu.template triangularView<Upper>().solve(x);
      x = m_P * x; 
    }

    template<typename Rhs> inline const internal::solve_retval<IncompleteLUT, Rhs>
     solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "IncompleteLUT is not initialized.");
      eigen_assert(cols()==b.rows()
                && "IncompleteLUT::solve(): invalid number of rows of the right hand side matrix b");
      return internal::solve_retval<IncompleteLUT, Rhs>(*this, b.derived());
    }

protected:

    
    struct keep_diag {
      inline bool operator() (const Index& row, const Index& col, const Scalar&) const
      {
        return row!=col;
      }
    };

protected:

    FactorType m_lu;
    RealScalar m_droptol;
    int m_fillfactor;
    bool m_analysisIsOk;
    bool m_factorizationIsOk;
    bool m_isInitialized;
    ComputationInfo m_info;
    PermutationMatrix<Dynamic,Dynamic,Index> m_P;     
    PermutationMatrix<Dynamic,Dynamic,Index> m_Pinv;  
};

 
template<typename Scalar>
void IncompleteLUT<Scalar>::setDroptol(const RealScalar& droptol)
{
  this->m_droptol = droptol;   
}

 
template<typename Scalar>
void IncompleteLUT<Scalar>::setFillfactor(int fillfactor)
{
  this->m_fillfactor = fillfactor;   
}

template <typename Scalar>
template<typename _MatrixType>
void IncompleteLUT<Scalar>::analyzePattern(const _MatrixType& amat)
{
  
  SparseMatrix<Scalar,ColMajor, Index> mat1 = amat;
  SparseMatrix<Scalar,ColMajor, Index> mat2 = amat.transpose();
  
  
  
  SparseMatrix<Scalar,ColMajor, Index> AtA = mat2 + mat1;
  AtA.prune(keep_diag());
  internal::minimum_degree_ordering<Scalar, Index>(AtA, m_P);  

  m_Pinv  = m_P.inverse(); 

  m_analysisIsOk = true;
}

template <typename Scalar>
template<typename _MatrixType>
void IncompleteLUT<Scalar>::factorize(const _MatrixType& amat)
{
  using std::sqrt;
  using std::swap;
  using std::abs;

  eigen_assert((amat.rows() == amat.cols()) && "The factorization should be done on a square matrix");
  Index n = amat.cols();  
  m_lu.resize(n,n);
  
  Vector u(n) ;     
  VectorXi ju(n);   
  VectorXi jr(n);   

  
  eigen_assert(m_analysisIsOk && "You must first call analyzePattern()");
  SparseMatrix<Scalar,RowMajor, Index> mat;
  mat = amat.twistedBy(m_Pinv);

  
  jr.fill(-1);
  ju.fill(0);
  u.fill(0);

  
  Index fill_in =   static_cast<Index> (amat.nonZeros()*m_fillfactor)/n+1;
  if (fill_in > n) fill_in = n;

  
  Index nnzL = fill_in/2;
  Index nnzU = nnzL;
  m_lu.reserve(n * (nnzL + nnzU + 1));

  
  for (Index ii = 0; ii < n; ii++)
  {
    

    Index sizeu = 1; 
    Index sizel = 0; 
    ju(ii)    = ii;
    u(ii)     = 0;
    jr(ii)    = ii;
    RealScalar rownorm = 0;

    typename FactorType::InnerIterator j_it(mat, ii); 
    for (; j_it; ++j_it)
    {
      Index k = j_it.index();
      if (k < ii)
      {
        
        ju(sizel) = k;
        u(sizel) = j_it.value();
        jr(k) = sizel;
        ++sizel;
      }
      else if (k == ii)
      {
        u(ii) = j_it.value();
      }
      else
      {
        
        Index jpos = ii + sizeu;
        ju(jpos) = k;
        u(jpos) = j_it.value();
        jr(k) = jpos;
        ++sizeu;
      }
      rownorm += numext::abs2(j_it.value());
    }

    
    if(rownorm==0)
    {
      m_info = NumericalIssue;
      return;
    }
    
    rownorm = sqrt(rownorm);

    
    Index jj = 0;
    Index len = 0;
    while (jj < sizel)
    {
      
      
      Index k;
      Index minrow = ju.segment(jj,sizel-jj).minCoeff(&k); 
      k += jj;
      if (minrow != ju(jj))
      {
        
        Index j = ju(jj);
        swap(ju(jj), ju(k));
        jr(minrow) = jj;   jr(j) = k;
        swap(u(jj), u(k));
      }
      
      jr(minrow) = -1;

      
      typename FactorType::InnerIterator ki_it(m_lu, minrow);
      while (ki_it && ki_it.index() < minrow) ++ki_it;
      eigen_internal_assert(ki_it && ki_it.col()==minrow);
      Scalar fact = u(jj) / ki_it.value();

      
      if(abs(fact) <= m_droptol)
      {
        jj++;
        continue;
      }

      
      ++ki_it;
      for (; ki_it; ++ki_it)
      {
        Scalar prod = fact * ki_it.value();
        Index j       = ki_it.index();
        Index jpos    = jr(j);
        if (jpos == -1) 
        {
          Index newpos;
          if (j >= ii) 
          {
            newpos = ii + sizeu;
            sizeu++;
            eigen_internal_assert(sizeu<=n);
          }
          else 
          {
            newpos = sizel;
            sizel++;
            eigen_internal_assert(sizel<=ii);
          }
          ju(newpos) = j;
          u(newpos) = -prod;
          jr(j) = newpos;
        }
        else
          u(jpos) -= prod;
      }
      
      u(len) = fact;
      ju(len) = minrow;
      ++len;

      jj++;
    } 

    
    for(Index k = 0; k <sizeu; k++) jr(ju(ii+k)) = -1;

    

    
    sizel = len;
    len = (std::min)(sizel, nnzL);
    typename Vector::SegmentReturnType ul(u.segment(0, sizel));
    typename VectorXi::SegmentReturnType jul(ju.segment(0, sizel));
    internal::QuickSplit(ul, jul, len);

    
    m_lu.startVec(ii);
    for(Index k = 0; k < len; k++)
      m_lu.insertBackByOuterInnerUnordered(ii,ju(k)) = u(k);

    
    
    if (u(ii) == Scalar(0))
      u(ii) = sqrt(m_droptol) * rownorm;
    m_lu.insertBackByOuterInnerUnordered(ii, ii) = u(ii);

    
    
    len = 0;
    for(Index k = 1; k < sizeu; k++)
    {
      if(abs(u(ii+k)) > m_droptol * rownorm )
      {
        ++len;
        u(ii + len)  = u(ii + k);
        ju(ii + len) = ju(ii + k);
      }
    }
    sizeu = len + 1; 
    len = (std::min)(sizeu, nnzU);
    typename Vector::SegmentReturnType uu(u.segment(ii+1, sizeu-1));
    typename VectorXi::SegmentReturnType juu(ju.segment(ii+1, sizeu-1));
    internal::QuickSplit(uu, juu, len);

    
    for(Index k = ii + 1; k < ii + len; k++)
      m_lu.insertBackByOuterInnerUnordered(ii,ju(k)) = u(k);
  }

  m_lu.finalize();
  m_lu.makeCompressed();

  m_factorizationIsOk = true;
  m_info = Success;
}

namespace internal {

template<typename _MatrixType, typename Rhs>
struct solve_retval<IncompleteLUT<_MatrixType>, Rhs>
  : solve_retval_base<IncompleteLUT<_MatrixType>, Rhs>
{
  typedef IncompleteLUT<_MatrixType> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec()._solve(rhs(),dst);
  }
};

} 

} 

#endif 
