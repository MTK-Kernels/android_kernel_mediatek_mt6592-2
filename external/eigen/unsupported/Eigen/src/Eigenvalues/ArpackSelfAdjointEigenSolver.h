// Copyright (C) 2012 David Harmon <dharmon@gmail.com>
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
// modify it under the terms of the GNU General Public License as
// the License, or (at your option) any later version.
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_ARPACKGENERALIZEDSELFADJOINTEIGENSOLVER_H
#define EIGEN_ARPACKGENERALIZEDSELFADJOINTEIGENSOLVER_H

#include <Eigen/Dense>

namespace Eigen { 

namespace internal {
  template<typename Scalar, typename RealScalar> struct arpack_wrapper;
  template<typename MatrixSolver, typename MatrixType, typename Scalar, bool BisSPD> struct OP;
}



template<typename MatrixType, typename MatrixSolver=SimplicialLLT<MatrixType>, bool BisSPD=false>
class ArpackGeneralizedSelfAdjointEigenSolver
{
public:
  

  
  typedef typename MatrixType::Scalar Scalar;
  typedef typename MatrixType::Index Index;

  typedef typename NumTraits<Scalar>::Real RealScalar;

  typedef typename internal::plain_col_type<MatrixType, RealScalar>::type RealVectorType;

  ArpackGeneralizedSelfAdjointEigenSolver()
   : m_eivec(),
     m_eivalues(),
     m_isInitialized(false),
     m_eigenvectorsOk(false),
     m_nbrConverged(0),
     m_nbrIterations(0)
  { }

  ArpackGeneralizedSelfAdjointEigenSolver(const MatrixType& A, const MatrixType& B,
                                          Index nbrEigenvalues, std::string eigs_sigma="LM",
                               int options=ComputeEigenvectors, RealScalar tol=0.0)
    : m_eivec(),
      m_eivalues(),
      m_isInitialized(false),
      m_eigenvectorsOk(false),
      m_nbrConverged(0),
      m_nbrIterations(0)
  {
    compute(A, B, nbrEigenvalues, eigs_sigma, options, tol);
  }


  ArpackGeneralizedSelfAdjointEigenSolver(const MatrixType& A,
                                          Index nbrEigenvalues, std::string eigs_sigma="LM",
                               int options=ComputeEigenvectors, RealScalar tol=0.0)
    : m_eivec(),
      m_eivalues(),
      m_isInitialized(false),
      m_eigenvectorsOk(false),
      m_nbrConverged(0),
      m_nbrIterations(0)
  {
    compute(A, nbrEigenvalues, eigs_sigma, options, tol);
  }


  ArpackGeneralizedSelfAdjointEigenSolver& compute(const MatrixType& A, const MatrixType& B,
                                                   Index nbrEigenvalues, std::string eigs_sigma="LM",
                                        int options=ComputeEigenvectors, RealScalar tol=0.0);
  
  ArpackGeneralizedSelfAdjointEigenSolver& compute(const MatrixType& A,
                                                   Index nbrEigenvalues, std::string eigs_sigma="LM",
                                        int options=ComputeEigenvectors, RealScalar tol=0.0);


  const Matrix<Scalar, Dynamic, Dynamic>& eigenvectors() const
  {
    eigen_assert(m_isInitialized && "ArpackGeneralizedSelfAdjointEigenSolver is not initialized.");
    eigen_assert(m_eigenvectorsOk && "The eigenvectors have not been computed together with the eigenvalues.");
    return m_eivec;
  }

  const Matrix<Scalar, Dynamic, 1>& eigenvalues() const
  {
    eigen_assert(m_isInitialized && "ArpackGeneralizedSelfAdjointEigenSolver is not initialized.");
    return m_eivalues;
  }

  Matrix<Scalar, Dynamic, Dynamic> operatorSqrt() const
  {
    eigen_assert(m_isInitialized && "SelfAdjointEigenSolver is not initialized.");
    eigen_assert(m_eigenvectorsOk && "The eigenvectors have not been computed together with the eigenvalues.");
    return m_eivec * m_eivalues.cwiseSqrt().asDiagonal() * m_eivec.adjoint();
  }

  Matrix<Scalar, Dynamic, Dynamic> operatorInverseSqrt() const
  {
    eigen_assert(m_isInitialized && "SelfAdjointEigenSolver is not initialized.");
    eigen_assert(m_eigenvectorsOk && "The eigenvectors have not been computed together with the eigenvalues.");
    return m_eivec * m_eivalues.cwiseInverse().cwiseSqrt().asDiagonal() * m_eivec.adjoint();
  }

  ComputationInfo info() const
  {
    eigen_assert(m_isInitialized && "ArpackGeneralizedSelfAdjointEigenSolver is not initialized.");
    return m_info;
  }

  size_t getNbrConvergedEigenValues() const
  { return m_nbrConverged; }

  size_t getNbrIterations() const
  { return m_nbrIterations; }

protected:
  Matrix<Scalar, Dynamic, Dynamic> m_eivec;
  Matrix<Scalar, Dynamic, 1> m_eivalues;
  ComputationInfo m_info;
  bool m_isInitialized;
  bool m_eigenvectorsOk;

  size_t m_nbrConverged;
  size_t m_nbrIterations;
};





template<typename MatrixType, typename MatrixSolver, bool BisSPD>
ArpackGeneralizedSelfAdjointEigenSolver<MatrixType, MatrixSolver, BisSPD>&
    ArpackGeneralizedSelfAdjointEigenSolver<MatrixType, MatrixSolver, BisSPD>
::compute(const MatrixType& A, Index nbrEigenvalues,
          std::string eigs_sigma, int options, RealScalar tol)
{
    MatrixType B(0,0);
    compute(A, B, nbrEigenvalues, eigs_sigma, options, tol);
    
    return *this;
}


template<typename MatrixType, typename MatrixSolver, bool BisSPD>
ArpackGeneralizedSelfAdjointEigenSolver<MatrixType, MatrixSolver, BisSPD>&
    ArpackGeneralizedSelfAdjointEigenSolver<MatrixType, MatrixSolver, BisSPD>
::compute(const MatrixType& A, const MatrixType& B, Index nbrEigenvalues,
          std::string eigs_sigma, int options, RealScalar tol)
{
  eigen_assert(A.cols() == A.rows());
  eigen_assert(B.cols() == B.rows());
  eigen_assert(B.rows() == 0 || A.cols() == B.rows());
  eigen_assert((options &~ (EigVecMask | GenEigMask)) == 0
            && (options & EigVecMask) != EigVecMask
            && "invalid option parameter");

  bool isBempty = (B.rows() == 0) || (B.cols() == 0);

  
  
  
  
  int ido = 0;

  int n = (int)A.cols();

  
  
  char whch[3] = "LM";
    
  
  
  RealScalar sigma = 0.0;

  if (eigs_sigma.length() >= 2 && isalpha(eigs_sigma[0]) && isalpha(eigs_sigma[1]))
  {
      eigs_sigma[0] = toupper(eigs_sigma[0]);
      eigs_sigma[1] = toupper(eigs_sigma[1]);

      
      
      
      
      if (eigs_sigma.substr(0,2) != "SM")
      {
          whch[0] = eigs_sigma[0];
          whch[1] = eigs_sigma[1];
      }
  }
  else
  {
      eigen_assert(false && "Specifying clustered eigenvalues is not yet supported!");

      
      
      
      sigma = atof(eigs_sigma.c_str());

      
      
  }

  
  
  char bmat[2] = "I";
  if (eigs_sigma.substr(0,2) == "SM" || !(isalpha(eigs_sigma[0]) && isalpha(eigs_sigma[1])) || (!isBempty && !BisSPD))
      bmat[0] = 'G';

  
  
  int mode = (bmat[0] == 'G') + 1;
  if (eigs_sigma.substr(0,2) == "SM" || !(isalpha(eigs_sigma[0]) && isalpha(eigs_sigma[1])))
  {
      
      
      
      mode = 3;
  }

  
  
  int nev = (int)nbrEigenvalues;

  
  
  Scalar *resid = new Scalar[n];

  
  
  
  
  int ncv = std::min(std::max(2*nev, 20), n);

  
  
  Scalar *v = new Scalar[n*ncv];
  int ldv = n;

  
  
  Scalar *workd = new Scalar[3*n];
  int lworkl = ncv*ncv+8*ncv; 
  Scalar *workl = new Scalar[lworkl];

  int *iparam= new int[11];
  iparam[0] = 1; 
  iparam[2] = std::max(300, (int)std::ceil(2*n/std::max(ncv,1)));
  iparam[6] = mode; 

  
  
  int *ipntr = new int[11]; 

  
  
  
  
  int info = 0;

  Scalar scale = 1.0;
  
  
  
  
  
  
  
  
  

  MatrixSolver OP;
  if (mode == 1 || mode == 2)
  {
      if (!isBempty)
          OP.compute(B);
  }
  else if (mode == 3)
  {
      if (sigma == 0.0)
      {
          OP.compute(A);
      }
      else
      {
          
          
          if (isBempty)
          {
            MatrixType AminusSigmaB(A);
            for (Index i=0; i<A.rows(); ++i)
                AminusSigmaB.coeffRef(i,i) -= sigma;
            
            OP.compute(AminusSigmaB);
          }
          else
          {
              MatrixType AminusSigmaB = A - sigma * B;
              OP.compute(AminusSigmaB);
          }
      }
  }
 
  if (!(mode == 1 && isBempty) && !(mode == 2 && isBempty) && OP.info() != Success)
      std::cout << "Error factoring matrix" << std::endl;

  do
  {
    internal::arpack_wrapper<Scalar, RealScalar>::saupd(&ido, bmat, &n, whch, &nev, &tol, resid, 
                                                        &ncv, v, &ldv, iparam, ipntr, workd, workl,
                                                        &lworkl, &info);

    if (ido == -1 || ido == 1)
    {
      Scalar *in  = workd + ipntr[0] - 1;
      Scalar *out = workd + ipntr[1] - 1;

      if (ido == 1 && mode != 2)
      {
          Scalar *out2 = workd + ipntr[2] - 1;
          if (isBempty || mode == 1)
            Matrix<Scalar, Dynamic, 1>::Map(out2, n) = Matrix<Scalar, Dynamic, 1>::Map(in, n);
          else
            Matrix<Scalar, Dynamic, 1>::Map(out2, n) = B * Matrix<Scalar, Dynamic, 1>::Map(in, n);
          
          in = workd + ipntr[2] - 1;
      }

      if (mode == 1)
      {
        if (isBempty)
        {
          
          
          Matrix<Scalar, Dynamic, 1>::Map(out, n) = A * Matrix<Scalar, Dynamic, 1>::Map(in, n);
        }
        else
        {
          
          
          internal::OP<MatrixSolver, MatrixType, Scalar, BisSPD>::applyOP(OP, A, n, in, out);
        }
      }
      else if (mode == 2)
      {
        if (ido == 1)
          Matrix<Scalar, Dynamic, 1>::Map(in, n)  = A * Matrix<Scalar, Dynamic, 1>::Map(in, n);
        
        
        
        Matrix<Scalar, Dynamic, 1>::Map(out, n) = OP.solve(Matrix<Scalar, Dynamic, 1>::Map(in, n));
      }
      else if (mode == 3)
      {
        
        
        
        if (ido == 1 || isBempty)
          Matrix<Scalar, Dynamic, 1>::Map(out, n) = OP.solve(Matrix<Scalar, Dynamic, 1>::Map(in, n));
        else
          Matrix<Scalar, Dynamic, 1>::Map(out, n) = OP.solve(B * Matrix<Scalar, Dynamic, 1>::Map(in, n));
      }
    }
    else if (ido == 2)
    {
      Scalar *in  = workd + ipntr[0] - 1;
      Scalar *out = workd + ipntr[1] - 1;

      if (isBempty || mode == 1)
        Matrix<Scalar, Dynamic, 1>::Map(out, n) = Matrix<Scalar, Dynamic, 1>::Map(in, n);
      else
        Matrix<Scalar, Dynamic, 1>::Map(out, n) = B * Matrix<Scalar, Dynamic, 1>::Map(in, n);
    }
  } while (ido != 99);

  if (info == 1)
    m_info = NoConvergence;
  else if (info == 3)
    m_info = NumericalIssue;
  else if (info < 0)
    m_info = InvalidInput;
  else if (info != 0)
    eigen_assert(false && "Unknown ARPACK return value!");
  else
  {
    
    
    int rvec = (options & ComputeEigenvectors) == ComputeEigenvectors;

    
    
    char howmny[2] = "A"; 

    
    
    int *select = new int[ncv];

    
    
    m_eivalues.resize(nev, 1);

    internal::arpack_wrapper<Scalar, RealScalar>::seupd(&rvec, howmny, select, m_eivalues.data(), v, &ldv,
                                                        &sigma, bmat, &n, whch, &nev, &tol, resid, &ncv,
                                                        v, &ldv, iparam, ipntr, workd, workl, &lworkl, &info);

    if (info == -14)
      m_info = NoConvergence;
    else if (info != 0)
      m_info = InvalidInput;
    else
    {
      if (rvec)
      {
        m_eivec.resize(A.rows(), nev);
        for (int i=0; i<nev; i++)
          for (int j=0; j<n; j++)
            m_eivec(j,i) = v[i*n+j] / scale;
      
        if (mode == 1 && !isBempty && BisSPD)
          internal::OP<MatrixSolver, MatrixType, Scalar, BisSPD>::project(OP, n, nev, m_eivec.data());

        m_eigenvectorsOk = true;
      }

      m_nbrIterations = iparam[2];
      m_nbrConverged  = iparam[4];

      m_info = Success;
    }

    delete select;
  }

  delete v;
  delete iparam;
  delete ipntr;
  delete workd;
  delete workl;
  delete resid;

  m_isInitialized = true;

  return *this;
}


extern "C" void ssaupd_(int *ido, char *bmat, int *n, char *which,
    int *nev, float *tol, float *resid, int *ncv,
    float *v, int *ldv, int *iparam, int *ipntr,
    float *workd, float *workl, int *lworkl,
    int *info);

extern "C" void sseupd_(int *rvec, char *All, int *select, float *d,
    float *z, int *ldz, float *sigma, 
    char *bmat, int *n, char *which, int *nev,
    float *tol, float *resid, int *ncv, float *v,
    int *ldv, int *iparam, int *ipntr, float *workd,
    float *workl, int *lworkl, int *ierr);

extern "C" void dsaupd_(int *ido, char *bmat, int *n, char *which,
    int *nev, double *tol, double *resid, int *ncv,
    double *v, int *ldv, int *iparam, int *ipntr,
    double *workd, double *workl, int *lworkl,
    int *info);

extern "C" void dseupd_(int *rvec, char *All, int *select, double *d,
    double *z, int *ldz, double *sigma, 
    char *bmat, int *n, char *which, int *nev,
    double *tol, double *resid, int *ncv, double *v,
    int *ldv, int *iparam, int *ipntr, double *workd,
    double *workl, int *lworkl, int *ierr);


namespace internal {

template<typename Scalar, typename RealScalar> struct arpack_wrapper
{
  static inline void saupd(int *ido, char *bmat, int *n, char *which,
      int *nev, RealScalar *tol, Scalar *resid, int *ncv,
      Scalar *v, int *ldv, int *iparam, int *ipntr,
      Scalar *workd, Scalar *workl, int *lworkl, int *info)
  { 
    EIGEN_STATIC_ASSERT(!NumTraits<Scalar>::IsComplex, NUMERIC_TYPE_MUST_BE_REAL)
  }

  static inline void seupd(int *rvec, char *All, int *select, Scalar *d,
      Scalar *z, int *ldz, RealScalar *sigma,
      char *bmat, int *n, char *which, int *nev,
      RealScalar *tol, Scalar *resid, int *ncv, Scalar *v,
      int *ldv, int *iparam, int *ipntr, Scalar *workd,
      Scalar *workl, int *lworkl, int *ierr)
  {
    EIGEN_STATIC_ASSERT(!NumTraits<Scalar>::IsComplex, NUMERIC_TYPE_MUST_BE_REAL)
  }
};

template <> struct arpack_wrapper<float, float>
{
  static inline void saupd(int *ido, char *bmat, int *n, char *which,
      int *nev, float *tol, float *resid, int *ncv,
      float *v, int *ldv, int *iparam, int *ipntr,
      float *workd, float *workl, int *lworkl, int *info)
  {
    ssaupd_(ido, bmat, n, which, nev, tol, resid, ncv, v, ldv, iparam, ipntr, workd, workl, lworkl, info);
  }

  static inline void seupd(int *rvec, char *All, int *select, float *d,
      float *z, int *ldz, float *sigma,
      char *bmat, int *n, char *which, int *nev,
      float *tol, float *resid, int *ncv, float *v,
      int *ldv, int *iparam, int *ipntr, float *workd,
      float *workl, int *lworkl, int *ierr)
  {
    sseupd_(rvec, All, select, d, z, ldz, sigma, bmat, n, which, nev, tol, resid, ncv, v, ldv, iparam, ipntr,
        workd, workl, lworkl, ierr);
  }
};

template <> struct arpack_wrapper<double, double>
{
  static inline void saupd(int *ido, char *bmat, int *n, char *which,
      int *nev, double *tol, double *resid, int *ncv,
      double *v, int *ldv, int *iparam, int *ipntr,
      double *workd, double *workl, int *lworkl, int *info)
  {
    dsaupd_(ido, bmat, n, which, nev, tol, resid, ncv, v, ldv, iparam, ipntr, workd, workl, lworkl, info);
  }

  static inline void seupd(int *rvec, char *All, int *select, double *d,
      double *z, int *ldz, double *sigma,
      char *bmat, int *n, char *which, int *nev,
      double *tol, double *resid, int *ncv, double *v,
      int *ldv, int *iparam, int *ipntr, double *workd,
      double *workl, int *lworkl, int *ierr)
  {
    dseupd_(rvec, All, select, d, v, ldv, sigma, bmat, n, which, nev, tol, resid, ncv, v, ldv, iparam, ipntr,
        workd, workl, lworkl, ierr);
  }
};


template<typename MatrixSolver, typename MatrixType, typename Scalar, bool BisSPD>
struct OP
{
    static inline void applyOP(MatrixSolver &OP, const MatrixType &A, int n, Scalar *in, Scalar *out);
    static inline void project(MatrixSolver &OP, int n, int k, Scalar *vecs);
};

template<typename MatrixSolver, typename MatrixType, typename Scalar>
struct OP<MatrixSolver, MatrixType, Scalar, true>
{
  static inline void applyOP(MatrixSolver &OP, const MatrixType &A, int n, Scalar *in, Scalar *out)
{
    
    
    
    
    Matrix<Scalar, Dynamic, 1>::Map(out, n) = OP.matrixU().solve(Matrix<Scalar, Dynamic, 1>::Map(in, n));
    Matrix<Scalar, Dynamic, 1>::Map(out, n) = OP.permutationPinv() * Matrix<Scalar, Dynamic, 1>::Map(out, n);

    
    
    Matrix<Scalar, Dynamic, 1>::Map(out, n) = A * Matrix<Scalar, Dynamic, 1>::Map(out, n);

    
    
    Matrix<Scalar, Dynamic, 1>::Map(out, n) = OP.permutationP() * Matrix<Scalar, Dynamic, 1>::Map(out, n);
    Matrix<Scalar, Dynamic, 1>::Map(out, n) = OP.matrixL().solve(Matrix<Scalar, Dynamic, 1>::Map(out, n));
}

  static inline void project(MatrixSolver &OP, int n, int k, Scalar *vecs)
{
    
    
    Matrix<Scalar, Dynamic, Dynamic>::Map(vecs, n, k) = OP.matrixU().solve(Matrix<Scalar, Dynamic, Dynamic>::Map(vecs, n, k));
    Matrix<Scalar, Dynamic, Dynamic>::Map(vecs, n, k) = OP.permutationPinv() * Matrix<Scalar, Dynamic, Dynamic>::Map(vecs, n, k);
}

};

template<typename MatrixSolver, typename MatrixType, typename Scalar>
struct OP<MatrixSolver, MatrixType, Scalar, false>
{
  static inline void applyOP(MatrixSolver &OP, const MatrixType &A, int n, Scalar *in, Scalar *out)
{
    eigen_assert(false && "Should never be in here...");
}

  static inline void project(MatrixSolver &OP, int n, int k, Scalar *vecs)
{
    eigen_assert(false && "Should never be in here...");
}

};

} 

} 

#endif 

