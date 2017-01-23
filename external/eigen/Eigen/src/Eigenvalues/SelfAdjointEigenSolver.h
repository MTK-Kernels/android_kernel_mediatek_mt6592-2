// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2010 Jitse Niesen <jitse@maths.leeds.ac.uk>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SELFADJOINTEIGENSOLVER_H
#define EIGEN_SELFADJOINTEIGENSOLVER_H

#include "./Tridiagonalization.h"

namespace Eigen { 

template<typename _MatrixType>
class GeneralizedSelfAdjointEigenSolver;

namespace internal {
template<typename SolverType,int Size,bool IsComplex> struct direct_selfadjoint_eigenvalues;
}

template<typename _MatrixType> class SelfAdjointEigenSolver
{
  public:

    typedef _MatrixType MatrixType;
    enum {
      Size = MatrixType::RowsAtCompileTime,
      ColsAtCompileTime = MatrixType::ColsAtCompileTime,
      Options = MatrixType::Options,
      MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime
    };
    
    
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;

    typedef typename NumTraits<Scalar>::Real RealScalar;
    
    friend struct internal::direct_selfadjoint_eigenvalues<SelfAdjointEigenSolver,Size,NumTraits<Scalar>::IsComplex>;

    typedef typename internal::plain_col_type<MatrixType, RealScalar>::type RealVectorType;
    typedef Tridiagonalization<MatrixType> TridiagonalizationType;

    SelfAdjointEigenSolver()
        : m_eivec(),
          m_eivalues(),
          m_subdiag(),
          m_isInitialized(false)
    { }

    SelfAdjointEigenSolver(Index size)
        : m_eivec(size, size),
          m_eivalues(size),
          m_subdiag(size > 1 ? size - 1 : 1),
          m_isInitialized(false)
    {}

    SelfAdjointEigenSolver(const MatrixType& matrix, int options = ComputeEigenvectors)
      : m_eivec(matrix.rows(), matrix.cols()),
        m_eivalues(matrix.cols()),
        m_subdiag(matrix.rows() > 1 ? matrix.rows() - 1 : 1),
        m_isInitialized(false)
    {
      compute(matrix, options);
    }

    SelfAdjointEigenSolver& compute(const MatrixType& matrix, int options = ComputeEigenvectors);
    
    SelfAdjointEigenSolver& computeDirect(const MatrixType& matrix, int options = ComputeEigenvectors);

    const MatrixType& eigenvectors() const
    {
      eigen_assert(m_isInitialized && "SelfAdjointEigenSolver is not initialized.");
      eigen_assert(m_eigenvectorsOk && "The eigenvectors have not been computed together with the eigenvalues.");
      return m_eivec;
    }

    const RealVectorType& eigenvalues() const
    {
      eigen_assert(m_isInitialized && "SelfAdjointEigenSolver is not initialized.");
      return m_eivalues;
    }

    MatrixType operatorSqrt() const
    {
      eigen_assert(m_isInitialized && "SelfAdjointEigenSolver is not initialized.");
      eigen_assert(m_eigenvectorsOk && "The eigenvectors have not been computed together with the eigenvalues.");
      return m_eivec * m_eivalues.cwiseSqrt().asDiagonal() * m_eivec.adjoint();
    }

    MatrixType operatorInverseSqrt() const
    {
      eigen_assert(m_isInitialized && "SelfAdjointEigenSolver is not initialized.");
      eigen_assert(m_eigenvectorsOk && "The eigenvectors have not been computed together with the eigenvalues.");
      return m_eivec * m_eivalues.cwiseInverse().cwiseSqrt().asDiagonal() * m_eivec.adjoint();
    }

    ComputationInfo info() const
    {
      eigen_assert(m_isInitialized && "SelfAdjointEigenSolver is not initialized.");
      return m_info;
    }

    static const int m_maxIterations = 30;

    #ifdef EIGEN2_SUPPORT
    SelfAdjointEigenSolver(const MatrixType& matrix, bool computeEigenvectors)
      : m_eivec(matrix.rows(), matrix.cols()),
        m_eivalues(matrix.cols()),
        m_subdiag(matrix.rows() > 1 ? matrix.rows() - 1 : 1),
        m_isInitialized(false)
    {
      compute(matrix, computeEigenvectors);
    }
    
    SelfAdjointEigenSolver(const MatrixType& matA, const MatrixType& matB, bool computeEigenvectors = true)
        : m_eivec(matA.cols(), matA.cols()),
          m_eivalues(matA.cols()),
          m_subdiag(matA.cols() > 1 ? matA.cols() - 1 : 1),
          m_isInitialized(false)
    {
      static_cast<GeneralizedSelfAdjointEigenSolver<MatrixType>*>(this)->compute(matA, matB, computeEigenvectors ? ComputeEigenvectors : EigenvaluesOnly);
    }
    
    void compute(const MatrixType& matrix, bool computeEigenvectors)
    {
      compute(matrix, computeEigenvectors ? ComputeEigenvectors : EigenvaluesOnly);
    }

    void compute(const MatrixType& matA, const MatrixType& matB, bool computeEigenvectors = true)
    {
      compute(matA, matB, computeEigenvectors ? ComputeEigenvectors : EigenvaluesOnly);
    }
    #endif 

  protected:
    MatrixType m_eivec;
    RealVectorType m_eivalues;
    typename TridiagonalizationType::SubDiagonalType m_subdiag;
    ComputationInfo m_info;
    bool m_isInitialized;
    bool m_eigenvectorsOk;
};

namespace internal {
template<int StorageOrder,typename RealScalar, typename Scalar, typename Index>
static void tridiagonal_qr_step(RealScalar* diag, RealScalar* subdiag, Index start, Index end, Scalar* matrixQ, Index n);
}

template<typename MatrixType>
SelfAdjointEigenSolver<MatrixType>& SelfAdjointEigenSolver<MatrixType>
::compute(const MatrixType& matrix, int options)
{
  using std::abs;
  eigen_assert(matrix.cols() == matrix.rows());
  eigen_assert((options&~(EigVecMask|GenEigMask))==0
          && (options&EigVecMask)!=EigVecMask
          && "invalid option parameter");
  bool computeEigenvectors = (options&ComputeEigenvectors)==ComputeEigenvectors;
  Index n = matrix.cols();
  m_eivalues.resize(n,1);

  if(n==1)
  {
    m_eivalues.coeffRef(0,0) = numext::real(matrix.coeff(0,0));
    if(computeEigenvectors)
      m_eivec.setOnes(n,n);
    m_info = Success;
    m_isInitialized = true;
    m_eigenvectorsOk = computeEigenvectors;
    return *this;
  }

  
  RealVectorType& diag = m_eivalues;
  MatrixType& mat = m_eivec;

  
  mat = matrix.template triangularView<Lower>();
  RealScalar scale = mat.cwiseAbs().maxCoeff();
  if(scale==RealScalar(0)) scale = RealScalar(1);
  mat.template triangularView<Lower>() /= scale;
  m_subdiag.resize(n-1);
  internal::tridiagonalization_inplace(mat, diag, m_subdiag, computeEigenvectors);
  
  Index end = n-1;
  Index start = 0;
  Index iter = 0; 

  while (end>0)
  {
    for (Index i = start; i<end; ++i)
      if (internal::isMuchSmallerThan(abs(m_subdiag[i]),(abs(diag[i])+abs(diag[i+1]))))
        m_subdiag[i] = 0;

    
    while (end>0 && m_subdiag[end-1]==0)
    {
      end--;
    }
    if (end<=0)
      break;

    
    iter++;
    if(iter > m_maxIterations * n) break;

    start = end - 1;
    while (start>0 && m_subdiag[start-1]!=0)
      start--;

    internal::tridiagonal_qr_step<MatrixType::Flags&RowMajorBit ? RowMajor : ColMajor>(diag.data(), m_subdiag.data(), start, end, computeEigenvectors ? m_eivec.data() : (Scalar*)0, n);
  }

  if (iter <= m_maxIterations * n)
    m_info = Success;
  else
    m_info = NoConvergence;

  
  
  
  if (m_info == Success)
  {
    for (Index i = 0; i < n-1; ++i)
    {
      Index k;
      m_eivalues.segment(i,n-i).minCoeff(&k);
      if (k > 0)
      {
        std::swap(m_eivalues[i], m_eivalues[k+i]);
        if(computeEigenvectors)
          m_eivec.col(i).swap(m_eivec.col(k+i));
      }
    }
  }
  
  
  m_eivalues *= scale;

  m_isInitialized = true;
  m_eigenvectorsOk = computeEigenvectors;
  return *this;
}


namespace internal {
  
template<typename SolverType,int Size,bool IsComplex> struct direct_selfadjoint_eigenvalues
{
  static inline void run(SolverType& eig, const typename SolverType::MatrixType& A, int options)
  { eig.compute(A,options); }
};

template<typename SolverType> struct direct_selfadjoint_eigenvalues<SolverType,3,false>
{
  typedef typename SolverType::MatrixType MatrixType;
  typedef typename SolverType::RealVectorType VectorType;
  typedef typename SolverType::Scalar Scalar;
  
  static inline void computeRoots(const MatrixType& m, VectorType& roots)
  {
    using std::sqrt;
    using std::atan2;
    using std::cos;
    using std::sin;
    const Scalar s_inv3 = Scalar(1.0)/Scalar(3.0);
    const Scalar s_sqrt3 = sqrt(Scalar(3.0));

    
    
    
    Scalar c0 = m(0,0)*m(1,1)*m(2,2) + Scalar(2)*m(1,0)*m(2,0)*m(2,1) - m(0,0)*m(2,1)*m(2,1) - m(1,1)*m(2,0)*m(2,0) - m(2,2)*m(1,0)*m(1,0);
    Scalar c1 = m(0,0)*m(1,1) - m(1,0)*m(1,0) + m(0,0)*m(2,2) - m(2,0)*m(2,0) + m(1,1)*m(2,2) - m(2,1)*m(2,1);
    Scalar c2 = m(0,0) + m(1,1) + m(2,2);

    
    
    Scalar c2_over_3 = c2*s_inv3;
    Scalar a_over_3 = (c1 - c2*c2_over_3)*s_inv3;
    if (a_over_3 > Scalar(0))
      a_over_3 = Scalar(0);

    Scalar half_b = Scalar(0.5)*(c0 + c2_over_3*(Scalar(2)*c2_over_3*c2_over_3 - c1));

    Scalar q = half_b*half_b + a_over_3*a_over_3*a_over_3;
    if (q > Scalar(0))
      q = Scalar(0);

    
    Scalar rho = sqrt(-a_over_3);
    Scalar theta = atan2(sqrt(-q),half_b)*s_inv3;
    Scalar cos_theta = cos(theta);
    Scalar sin_theta = sin(theta);
    roots(0) = c2_over_3 + Scalar(2)*rho*cos_theta;
    roots(1) = c2_over_3 - rho*(cos_theta + s_sqrt3*sin_theta);
    roots(2) = c2_over_3 - rho*(cos_theta - s_sqrt3*sin_theta);

    
    if (roots(0) >= roots(1))
      std::swap(roots(0),roots(1));
    if (roots(1) >= roots(2))
    {
      std::swap(roots(1),roots(2));
      if (roots(0) >= roots(1))
        std::swap(roots(0),roots(1));
    }
  }
  
  static inline void run(SolverType& solver, const MatrixType& mat, int options)
  {
    using std::sqrt;
    eigen_assert(mat.cols() == 3 && mat.cols() == mat.rows());
    eigen_assert((options&~(EigVecMask|GenEigMask))==0
            && (options&EigVecMask)!=EigVecMask
            && "invalid option parameter");
    bool computeEigenvectors = (options&ComputeEigenvectors)==ComputeEigenvectors;
    
    MatrixType& eivecs = solver.m_eivec;
    VectorType& eivals = solver.m_eivalues;
  
    
    Scalar scale = mat.cwiseAbs().maxCoeff();
    MatrixType scaledMat = mat / scale;

    
    computeRoots(scaledMat,eivals);

    
    if(computeEigenvectors)
    {
      Scalar safeNorm2 = Eigen::NumTraits<Scalar>::epsilon();
      if((eivals(2)-eivals(0))<=Eigen::NumTraits<Scalar>::epsilon())
      {
        eivecs.setIdentity();
      }
      else
      {
        scaledMat = scaledMat.template selfadjointView<Lower>();
        MatrixType tmp;
        tmp = scaledMat;

        Scalar d0 = eivals(2) - eivals(1);
        Scalar d1 = eivals(1) - eivals(0);
        int k =  d0 > d1 ? 2 : 0;
        d0 = d0 > d1 ? d0 : d1;

        tmp.diagonal().array () -= eivals(k);
        VectorType cross;
        Scalar n;
        n = (cross = tmp.row(0).cross(tmp.row(1))).squaredNorm();

        if(n>safeNorm2)
        {
          eivecs.col(k) = cross / sqrt(n);
        }
        else
        {
          n = (cross = tmp.row(0).cross(tmp.row(2))).squaredNorm();

          if(n>safeNorm2)
          {
            eivecs.col(k) = cross / sqrt(n);
          }
          else
          {
            n = (cross = tmp.row(1).cross(tmp.row(2))).squaredNorm();

            if(n>safeNorm2)
            {
              eivecs.col(k) = cross / sqrt(n);
            }
            else
            {
              
              
              
              eivals *= scale;

              solver.m_info = NumericalIssue;
              solver.m_isInitialized = true;
              solver.m_eigenvectorsOk = computeEigenvectors;
              return;
            }
          }
        }

        tmp = scaledMat;
        tmp.diagonal().array() -= eivals(1);

        if(d0<=Eigen::NumTraits<Scalar>::epsilon())
        {
          eivecs.col(1) = eivecs.col(k).unitOrthogonal();
        }
        else
        {
          n = (cross = eivecs.col(k).cross(tmp.row(0))).squaredNorm();
          if(n>safeNorm2)
          {
            eivecs.col(1) = cross / sqrt(n);
          }
          else
          {
            n = (cross = eivecs.col(k).cross(tmp.row(1))).squaredNorm();
            if(n>safeNorm2)
              eivecs.col(1) = cross / sqrt(n);
            else
            {
              n = (cross = eivecs.col(k).cross(tmp.row(2))).squaredNorm();
              if(n>safeNorm2)
                eivecs.col(1) = cross / sqrt(n);
              else
              {
                
                
                eivecs.col(1) = eivecs.col(k).unitOrthogonal();
              }
            }
          }

          
          
          Scalar d = eivecs.col(1).dot(eivecs.col(k));
          eivecs.col(1) = (eivecs.col(1) - d * eivecs.col(k)).normalized();
        }

        eivecs.col(k==2 ? 0 : 2) = eivecs.col(k).cross(eivecs.col(1)).normalized();
      }
    }
    
    eivals *= scale;
    
    solver.m_info = Success;
    solver.m_isInitialized = true;
    solver.m_eigenvectorsOk = computeEigenvectors;
  }
};

template<typename SolverType> struct direct_selfadjoint_eigenvalues<SolverType,2,false>
{
  typedef typename SolverType::MatrixType MatrixType;
  typedef typename SolverType::RealVectorType VectorType;
  typedef typename SolverType::Scalar Scalar;
  
  static inline void computeRoots(const MatrixType& m, VectorType& roots)
  {
    using std::sqrt;
    const Scalar t0 = Scalar(0.5) * sqrt( numext::abs2(m(0,0)-m(1,1)) + Scalar(4)*m(1,0)*m(1,0));
    const Scalar t1 = Scalar(0.5) * (m(0,0) + m(1,1));
    roots(0) = t1 - t0;
    roots(1) = t1 + t0;
  }
  
  static inline void run(SolverType& solver, const MatrixType& mat, int options)
  {
    using std::sqrt;
    eigen_assert(mat.cols() == 2 && mat.cols() == mat.rows());
    eigen_assert((options&~(EigVecMask|GenEigMask))==0
            && (options&EigVecMask)!=EigVecMask
            && "invalid option parameter");
    bool computeEigenvectors = (options&ComputeEigenvectors)==ComputeEigenvectors;
    
    MatrixType& eivecs = solver.m_eivec;
    VectorType& eivals = solver.m_eivalues;
  
    
    Scalar scale = mat.cwiseAbs().maxCoeff();
    scale = (std::max)(scale,Scalar(1));
    MatrixType scaledMat = mat / scale;
    
    
    computeRoots(scaledMat,eivals);
    
    
    if(computeEigenvectors)
    {
      scaledMat.diagonal().array () -= eivals(1);
      Scalar a2 = numext::abs2(scaledMat(0,0));
      Scalar c2 = numext::abs2(scaledMat(1,1));
      Scalar b2 = numext::abs2(scaledMat(1,0));
      if(a2>c2)
      {
        eivecs.col(1) << -scaledMat(1,0), scaledMat(0,0);
        eivecs.col(1) /= sqrt(a2+b2);
      }
      else
      {
        eivecs.col(1) << -scaledMat(1,1), scaledMat(1,0);
        eivecs.col(1) /= sqrt(c2+b2);
      }

      eivecs.col(0) << eivecs.col(1).unitOrthogonal();
    }
    
    
    eivals *= scale;
    
    solver.m_info = Success;
    solver.m_isInitialized = true;
    solver.m_eigenvectorsOk = computeEigenvectors;
  }
};

}

template<typename MatrixType>
SelfAdjointEigenSolver<MatrixType>& SelfAdjointEigenSolver<MatrixType>
::computeDirect(const MatrixType& matrix, int options)
{
  internal::direct_selfadjoint_eigenvalues<SelfAdjointEigenSolver,Size,NumTraits<Scalar>::IsComplex>::run(*this,matrix,options);
  return *this;
}

namespace internal {
template<int StorageOrder,typename RealScalar, typename Scalar, typename Index>
static void tridiagonal_qr_step(RealScalar* diag, RealScalar* subdiag, Index start, Index end, Scalar* matrixQ, Index n)
{
  using std::abs;
  RealScalar td = (diag[end-1] - diag[end])*RealScalar(0.5);
  RealScalar e = subdiag[end-1];
  
  
  
  RealScalar mu = diag[end];
  if(td==0)
    mu -= abs(e);
  else
  {
    RealScalar e2 = numext::abs2(subdiag[end-1]);
    RealScalar h = numext::hypot(td,e);
    if(e2==0)  mu -= (e / (td + (td>0 ? 1 : -1))) * (e / h);
    else       mu -= e2 / (td + (td>0 ? h : -h));
  }
  
  RealScalar x = diag[start] - mu;
  RealScalar z = subdiag[start];
  for (Index k = start; k < end; ++k)
  {
    JacobiRotation<RealScalar> rot;
    rot.makeGivens(x, z);

    
    RealScalar sdk = rot.s() * diag[k] + rot.c() * subdiag[k];
    RealScalar dkp1 = rot.s() * subdiag[k] + rot.c() * diag[k+1];

    diag[k] = rot.c() * (rot.c() * diag[k] - rot.s() * subdiag[k]) - rot.s() * (rot.c() * subdiag[k] - rot.s() * diag[k+1]);
    diag[k+1] = rot.s() * sdk + rot.c() * dkp1;
    subdiag[k] = rot.c() * sdk - rot.s() * dkp1;
    

    if (k > start)
      subdiag[k - 1] = rot.c() * subdiag[k-1] - rot.s() * z;

    x = subdiag[k];

    if (k < end - 1)
    {
      z = -rot.s() * subdiag[k+1];
      subdiag[k + 1] = rot.c() * subdiag[k+1];
    }
    
    
    if (matrixQ)
    {
      
      Map<Matrix<Scalar,Dynamic,Dynamic,StorageOrder> > q(matrixQ,n,n);
      q.applyOnTheRight(k,k+1,rot);
    }
  }
}

} 

} 

#endif 
