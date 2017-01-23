// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_DGMRES_H
#define EIGEN_DGMRES_H

#include <Eigen/Eigenvalues>

namespace Eigen { 
  
template< typename _MatrixType,
          typename _Preconditioner = DiagonalPreconditioner<typename _MatrixType::Scalar> >
class DGMRES;

namespace internal {

template< typename _MatrixType, typename _Preconditioner>
struct traits<DGMRES<_MatrixType,_Preconditioner> >
{
  typedef _MatrixType MatrixType;
  typedef _Preconditioner Preconditioner;
};

template <typename VectorType, typename IndexType>
void sortWithPermutation (VectorType& vec, IndexType& perm, typename IndexType::Scalar& ncut)
{
  eigen_assert(vec.size() == perm.size());
  typedef typename IndexType::Scalar Index; 
  typedef typename VectorType::Scalar Scalar; 
  bool flag; 
  for (Index k  = 0; k < ncut; k++)
  {
    flag = false;
    for (Index j = 0; j < vec.size()-1; j++)
    {
      if ( vec(perm(j)) < vec(perm(j+1)) )
      {
        std::swap(perm(j),perm(j+1)); 
        flag = true;
      }
      if (!flag) break; 
    }
  }
}

}
template< typename _MatrixType, typename _Preconditioner>
class DGMRES : public IterativeSolverBase<DGMRES<_MatrixType,_Preconditioner> >
{
    typedef IterativeSolverBase<DGMRES> Base;
    using Base::mp_matrix;
    using Base::m_error;
    using Base::m_iterations;
    using Base::m_info;
    using Base::m_isInitialized;
    using Base::m_tolerance; 
  public:
    typedef _MatrixType MatrixType;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;
    typedef typename MatrixType::RealScalar RealScalar;
    typedef _Preconditioner Preconditioner;
    typedef Matrix<Scalar,Dynamic,Dynamic> DenseMatrix; 
    typedef Matrix<RealScalar,Dynamic,Dynamic> DenseRealMatrix; 
    typedef Matrix<Scalar,Dynamic,1> DenseVector;
    typedef Matrix<RealScalar,Dynamic,1> DenseRealVector; 
    typedef Matrix<std::complex<RealScalar>, Dynamic, 1> ComplexVector;
 
    
  
  DGMRES() : Base(),m_restart(30),m_neig(0),m_r(0),m_maxNeig(5),m_isDeflAllocated(false),m_isDeflInitialized(false) {}

  DGMRES(const MatrixType& A) : Base(A),m_restart(30),m_neig(0),m_r(0),m_maxNeig(5),m_isDeflAllocated(false),m_isDeflInitialized(false)
  {}

  ~DGMRES() {}
  
  template<typename Rhs,typename Guess>
  inline const internal::solve_retval_with_guess<DGMRES, Rhs, Guess>
  solveWithGuess(const MatrixBase<Rhs>& b, const Guess& x0) const
  {
    eigen_assert(m_isInitialized && "DGMRES is not initialized.");
    eigen_assert(Base::rows()==b.rows()
              && "DGMRES::solve(): invalid number of rows of the right hand side matrix b");
    return internal::solve_retval_with_guess
            <DGMRES, Rhs, Guess>(*this, b.derived(), x0);
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
      dgmres(*mp_matrix, b.col(j), xj, Base::m_preconditioner);
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
  int restart() { return m_restart; }
  
  void set_restart(const int restart) { m_restart=restart; }
  
  void setEigenv(const int neig) 
  {
    m_neig = neig;
    if (neig+1 > m_maxNeig) m_maxNeig = neig+1; 
  }
  
 
  int deflSize() {return m_r; }
  
  void setMaxEigenv(const int maxNeig) { m_maxNeig = maxNeig; }
  
  protected:
    
    template<typename Rhs, typename Dest>
    void dgmres(const MatrixType& mat,const Rhs& rhs, Dest& x, const Preconditioner& precond) const;
    
    template<typename Dest>
    int dgmresCycle(const MatrixType& mat, const Preconditioner& precond, Dest& x, DenseVector& r0, RealScalar& beta, const RealScalar& normRhs, int& nbIts) const; 
    
    int dgmresComputeDeflationData(const MatrixType& mat, const Preconditioner& precond, const Index& it, Index& neig) const;
    
    template<typename RhsType, typename DestType>
    int dgmresApplyDeflation(const RhsType& In, DestType& Out) const; 
    ComplexVector schurValues(const ComplexSchur<DenseMatrix>& schurofH) const;
    ComplexVector schurValues(const RealSchur<DenseMatrix>& schurofH) const;
    
    void dgmresInitDeflation(Index& rows) const; 
    mutable DenseMatrix m_V; 
    mutable DenseMatrix m_H; 
    mutable DenseMatrix m_Hes; 
    mutable Index m_restart; 
    mutable DenseMatrix m_U; 
    mutable DenseMatrix m_MU; 
    mutable DenseMatrix m_T; 
    mutable PartialPivLU<DenseMatrix> m_luT; 
    mutable int m_neig; 
    mutable int m_r; 
    mutable int m_maxNeig; 
    mutable RealScalar m_lambdaN; 
    mutable bool m_isDeflAllocated;
    mutable bool m_isDeflInitialized;
    
    
    mutable RealScalar m_smv; 
    mutable bool m_force; 
    
}; 
template< typename _MatrixType, typename _Preconditioner>
template<typename Rhs, typename Dest>
void DGMRES<_MatrixType, _Preconditioner>::dgmres(const MatrixType& mat,const Rhs& rhs, Dest& x,
              const Preconditioner& precond) const
{
  
  int n = mat.rows(); 
  DenseVector r0(n); 
  int nbIts = 0; 
  m_H.resize(m_restart+1, m_restart);
  m_Hes.resize(m_restart, m_restart);
  m_V.resize(n,m_restart+1);
  
  x = precond.solve(x);
  r0 = rhs - mat * x; 
  RealScalar beta = r0.norm(); 
  RealScalar normRhs = rhs.norm();
  m_error = beta/normRhs; 
  if(m_error < m_tolerance)
    m_info = Success; 
  else
    m_info = NoConvergence;
  
  
  while (nbIts < m_iterations && m_info == NoConvergence)
  {
    dgmresCycle(mat, precond, x, r0, beta, normRhs, nbIts); 
    
    
    if (nbIts < m_iterations && m_info == NoConvergence)
      r0 = rhs - mat * x; 
  }
} 

template< typename _MatrixType, typename _Preconditioner>
template<typename Dest>
int DGMRES<_MatrixType, _Preconditioner>::dgmresCycle(const MatrixType& mat, const Preconditioner& precond, Dest& x, DenseVector& r0, RealScalar& beta, const RealScalar& normRhs, int& nbIts) const
{
  
  DenseVector g(m_restart+1); 
  g.setZero();  
  g(0) = Scalar(beta); 
  m_V.col(0) = r0/beta; 
  m_info = NoConvergence; 
  std::vector<JacobiRotation<Scalar> >gr(m_restart); 
  int it = 0; 
  int n = mat.rows();
  DenseVector tv1(n), tv2(n);  
  while (m_info == NoConvergence && it < m_restart && nbIts < m_iterations)
  {    
    
    if (m_isDeflInitialized )
    {
      dgmresApplyDeflation(m_V.col(it), tv1); 
      tv2 = precond.solve(tv1); 
    }
    else
    {
      tv2 = precond.solve(m_V.col(it)); 
    }
    tv1 = mat * tv2; 
   
    
    Scalar coef; 
    for (int i = 0; i <= it; ++i)
    { 
      coef = tv1.dot(m_V.col(i));
      tv1 = tv1 - coef * m_V.col(i); 
      m_H(i,it) = coef; 
      m_Hes(i,it) = coef; 
    }
    
    coef = tv1.norm(); 
    m_V.col(it+1) = tv1/coef;
    m_H(it+1, it) = coef;
    
    
    
    
    for (int i = 1; i <= it; ++i) 
    {
      m_H.col(it).applyOnTheLeft(i-1,i,gr[i-1].adjoint());
    }
    
    gr[it].makeGivens(m_H(it, it), m_H(it+1,it)); 
    
    m_H.col(it).applyOnTheLeft(it,it+1,gr[it].adjoint());
    g.applyOnTheLeft(it,it+1, gr[it].adjoint()); 
    
    beta = std::abs(g(it+1));
    m_error = beta/normRhs; 
    std::cerr << nbIts << " Relative Residual Norm " << m_error << std::endl;
    it++; nbIts++; 
    
    if (m_error < m_tolerance)
    {
      
      m_info = Success;
      break;
    }
  }
  
  
  
  DenseVector nrs(m_restart); 
  nrs = m_H.topLeftCorner(it,it).template triangularView<Upper>().solve(g.head(it)); 
  
  
  if (m_isDeflInitialized)
  {
    tv1 = m_V.leftCols(it) * nrs; 
    dgmresApplyDeflation(tv1, tv2); 
    x = x + precond.solve(tv2);
  }
  else
    x = x + precond.solve(m_V.leftCols(it) * nrs); 
  
  
  if(nbIts < m_iterations && m_info == NoConvergence && m_neig > 0 && (m_r+m_neig) < m_maxNeig)
    dgmresComputeDeflationData(mat, precond, it, m_neig); 
  return 0; 
  
}


template< typename _MatrixType, typename _Preconditioner>
void DGMRES<_MatrixType, _Preconditioner>::dgmresInitDeflation(Index& rows) const
{
  m_U.resize(rows, m_maxNeig);
  m_MU.resize(rows, m_maxNeig); 
  m_T.resize(m_maxNeig, m_maxNeig);
  m_lambdaN = 0.0; 
  m_isDeflAllocated = true; 
}

template< typename _MatrixType, typename _Preconditioner>
inline typename DGMRES<_MatrixType, _Preconditioner>::ComplexVector DGMRES<_MatrixType, _Preconditioner>::schurValues(const ComplexSchur<DenseMatrix>& schurofH) const
{
  return schurofH.matrixT().diagonal();
}

template< typename _MatrixType, typename _Preconditioner>
inline typename DGMRES<_MatrixType, _Preconditioner>::ComplexVector DGMRES<_MatrixType, _Preconditioner>::schurValues(const RealSchur<DenseMatrix>& schurofH) const
{
  typedef typename MatrixType::Index Index;
  const DenseMatrix& T = schurofH.matrixT();
  Index it = T.rows();
  ComplexVector eig(it);
  Index j = 0;
  while (j < it-1)
  {
    if (T(j+1,j) ==Scalar(0))
    {
      eig(j) = std::complex<RealScalar>(T(j,j),RealScalar(0)); 
      j++; 
    }
    else
    {
      eig(j) = std::complex<RealScalar>(T(j,j),T(j+1,j)); 
      eig(j+1) = std::complex<RealScalar>(T(j,j+1),T(j+1,j+1));
      j++;
    }
  }
  if (j < it-1) eig(j) = std::complex<RealScalar>(T(j,j),RealScalar(0));
  return eig;
}

template< typename _MatrixType, typename _Preconditioner>
int DGMRES<_MatrixType, _Preconditioner>::dgmresComputeDeflationData(const MatrixType& mat, const Preconditioner& precond, const Index& it, Index& neig) const
{
  
  typename internal::conditional<NumTraits<Scalar>::IsComplex, ComplexSchur<DenseMatrix>, RealSchur<DenseMatrix> >::type schurofH; 
  bool computeU = true;
  DenseMatrix matrixQ(it,it); 
  matrixQ.setIdentity();
  schurofH.computeFromHessenberg(m_Hes.topLeftCorner(it,it), matrixQ, computeU); 
  
  ComplexVector eig(it);
  Matrix<Index,Dynamic,1>perm(it); 
  eig = this->schurValues(schurofH);
  
  
  DenseRealVector modulEig(it); 
  for (int j=0; j<it; ++j) modulEig(j) = std::abs(eig(j)); 
  perm.setLinSpaced(it,0,it-1);
  internal::sortWithPermutation(modulEig, perm, neig);
  
  if (!m_lambdaN)
  {
    m_lambdaN = (std::max)(modulEig.maxCoeff(), m_lambdaN);
  }
  
  int nbrEig = 0; 
  while (nbrEig < neig)
  {
    if(eig(perm(it-nbrEig-1)).imag() == RealScalar(0)) nbrEig++; 
    else nbrEig += 2; 
  }
  
  DenseMatrix Sr(it, nbrEig); 
  Sr.setZero();
  for (int j = 0; j < nbrEig; j++)
  {
    Sr.col(j) = schurofH.matrixU().col(perm(it-j-1));
  }
  
  
  DenseMatrix X; 
  X = m_V.leftCols(it) * Sr;
  if (m_r)
  {
   
   for (int j = 0; j < nbrEig; j++)
     for (int k =0; k < m_r; k++)
      X.col(j) = X.col(j) - (m_U.col(k).dot(X.col(j)))*m_U.col(k); 
  }
  
  
  Index m = m_V.rows();
  if (!m_isDeflAllocated) 
    dgmresInitDeflation(m); 
  DenseMatrix MX(m, nbrEig);
  DenseVector tv1(m);
  for (int j = 0; j < nbrEig; j++)
  {
    tv1 = mat * X.col(j);
    MX.col(j) = precond.solve(tv1);
  }
  
  
  m_T.block(m_r, m_r, nbrEig, nbrEig) = X.transpose() * MX; 
  if(m_r)
  {
    m_T.block(0, m_r, m_r, nbrEig) = m_U.leftCols(m_r).transpose() * MX; 
    m_T.block(m_r, 0, nbrEig, m_r) = X.transpose() * m_MU.leftCols(m_r);
  }
  
  
  for (int j = 0; j < nbrEig; j++) m_U.col(m_r+j) = X.col(j);
  for (int j = 0; j < nbrEig; j++) m_MU.col(m_r+j) = MX.col(j);
  
  m_r += nbrEig; 
  
  
  m_luT.compute(m_T.topLeftCorner(m_r, m_r));
  
  
  m_isDeflInitialized = true;
  return 0; 
}
template<typename _MatrixType, typename _Preconditioner>
template<typename RhsType, typename DestType>
int DGMRES<_MatrixType, _Preconditioner>::dgmresApplyDeflation(const RhsType &x, DestType &y) const
{
  DenseVector x1 = m_U.leftCols(m_r).transpose() * x; 
  y = x + m_U.leftCols(m_r) * ( m_lambdaN * m_luT.solve(x1) - x1);
  return 0; 
}

namespace internal {

  template<typename _MatrixType, typename _Preconditioner, typename Rhs>
struct solve_retval<DGMRES<_MatrixType, _Preconditioner>, Rhs>
  : solve_retval_base<DGMRES<_MatrixType, _Preconditioner>, Rhs>
{
  typedef DGMRES<_MatrixType, _Preconditioner> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec()._solve(rhs(),dst);
  }
};
} 

} 
#endif 
