// Copyright (C) 2009 Thomas Capricelli <orzel@freehackers.org>
// Copyright (C) 2012 Desire Nuentsa <desire.nuentsa_wakam@inria.fr>
// Copyright Jorge More - Argonne National Laboratory
// Copyright Burt Garbow - Argonne National Laboratory
// Copyright Ken Hillstrom - Argonne National Laboratory
// This Source Code Form is subject to the terms of the Minpack license
// (a BSD-like license) described in the campaigned CopyrightMINPACK.txt file.
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_LEVENBERGMARQUARDT_H
#define EIGEN_LEVENBERGMARQUARDT_H


namespace Eigen {
namespace LevenbergMarquardtSpace {
    enum Status {
        NotStarted = -2,
        Running = -1,
        ImproperInputParameters = 0,
        RelativeReductionTooSmall = 1,
        RelativeErrorTooSmall = 2,
        RelativeErrorAndReductionTooSmall = 3,
        CosinusTooSmall = 4,
        TooManyFunctionEvaluation = 5,
        FtolTooSmall = 6,
        XtolTooSmall = 7,
        GtolTooSmall = 8,
        UserAsked = 9
    };
}

template <typename _Scalar, int NX=Dynamic, int NY=Dynamic>
struct DenseFunctor
{
  typedef _Scalar Scalar;
  enum {
    InputsAtCompileTime = NX,
    ValuesAtCompileTime = NY
  };
  typedef Matrix<Scalar,InputsAtCompileTime,1> InputType;
  typedef Matrix<Scalar,ValuesAtCompileTime,1> ValueType;
  typedef Matrix<Scalar,ValuesAtCompileTime,InputsAtCompileTime> JacobianType;
  typedef ColPivHouseholderQR<JacobianType> QRSolver;
  const int m_inputs, m_values;

  DenseFunctor() : m_inputs(InputsAtCompileTime), m_values(ValuesAtCompileTime) {}
  DenseFunctor(int inputs, int values) : m_inputs(inputs), m_values(values) {}

  int inputs() const { return m_inputs; }
  int values() const { return m_values; }

  
  
  
  
  
};

template <typename _Scalar, typename _Index>
struct SparseFunctor
{
  typedef _Scalar Scalar;
  typedef _Index Index;
  typedef Matrix<Scalar,Dynamic,1> InputType;
  typedef Matrix<Scalar,Dynamic,1> ValueType;
  typedef SparseMatrix<Scalar, ColMajor, Index> JacobianType;
  typedef SparseQR<JacobianType, COLAMDOrdering<int> > QRSolver;
  enum {
    InputsAtCompileTime = Dynamic,
    ValuesAtCompileTime = Dynamic
  };
  
  SparseFunctor(int inputs, int values) : m_inputs(inputs), m_values(values) {}

  int inputs() const { return m_inputs; }
  int values() const { return m_values; }
  
  const int m_inputs, m_values;
  
  
  
  
  
  
};
namespace internal {
template <typename QRSolver, typename VectorType>
void lmpar2(const QRSolver &qr, const VectorType  &diag, const VectorType  &qtb,
	    typename VectorType::Scalar m_delta, typename VectorType::Scalar &par,
	    VectorType  &x);
    }
template<typename _FunctorType>
class LevenbergMarquardt : internal::no_assignment_operator
{
  public:
    typedef _FunctorType FunctorType;
    typedef typename FunctorType::QRSolver QRSolver;
    typedef typename FunctorType::JacobianType JacobianType;
    typedef typename JacobianType::Scalar Scalar;
    typedef typename JacobianType::RealScalar RealScalar; 
    typedef typename JacobianType::Index Index;
    typedef typename QRSolver::Index PermIndex;
    typedef Matrix<Scalar,Dynamic,1> FVectorType;
    typedef PermutationMatrix<Dynamic,Dynamic> PermutationType;
  public:
    LevenbergMarquardt(FunctorType& functor) 
    : m_functor(functor),m_nfev(0),m_njev(0),m_fnorm(0.0),m_gnorm(0),
      m_isInitialized(false),m_info(InvalidInput)
    {
      resetParameters();
      m_useExternalScaling=false; 
    }
    
    LevenbergMarquardtSpace::Status minimize(FVectorType &x);
    LevenbergMarquardtSpace::Status minimizeInit(FVectorType &x);
    LevenbergMarquardtSpace::Status minimizeOneStep(FVectorType &x);
    LevenbergMarquardtSpace::Status lmder1(
      FVectorType  &x, 
      const Scalar tol = std::sqrt(NumTraits<Scalar>::epsilon())
    );
    static LevenbergMarquardtSpace::Status lmdif1(
            FunctorType &functor,
            FVectorType  &x,
            Index *nfev,
            const Scalar tol = std::sqrt(NumTraits<Scalar>::epsilon())
            );
    
    
    void resetParameters() 
    { 
      m_factor = 100.; 
      m_maxfev = 400; 
      m_ftol = std::sqrt(NumTraits<RealScalar>::epsilon());
      m_xtol = std::sqrt(NumTraits<RealScalar>::epsilon());
      m_gtol = 0. ; 
      m_epsfcn = 0. ;
    }
    
    
    void setXtol(RealScalar xtol) { m_xtol = xtol; }
    
    
    void setFtol(RealScalar ftol) { m_ftol = ftol; }
    
    
    void setGtol(RealScalar gtol) { m_gtol = gtol; }
    
    
    void setFactor(RealScalar factor) { m_factor = factor; }    
    
    
    void setEpsilon (RealScalar epsfcn) { m_epsfcn = epsfcn; }
    
    
    void setMaxfev(Index maxfev) {m_maxfev = maxfev; }
    
    
    void setExternalScaling(bool value) {m_useExternalScaling  = value; }
    
    
    FVectorType& diag() {return m_diag; }
    
    
    Index iterations() { return m_iter; }
    
    
    Index nfev() { return m_nfev; }
    
    
    Index njev() { return m_njev; }
    
    
    RealScalar fnorm() {return m_fnorm; }
    
    
    RealScalar gnorm() {return m_gnorm; }
    
    
    RealScalar lm_param(void) { return m_par; }
    
    FVectorType& fvec() {return m_fvec; }
    
    JacobianType& jacobian() {return m_fjac; }
    
    JacobianType& matrixR() {return m_rfactor; }
    
    PermutationType permutation() {return m_permutation; }
    
    ComputationInfo info() const
    {
      
      return m_info;
    }
  private:
    JacobianType m_fjac; 
    JacobianType m_rfactor; 
    FunctorType &m_functor;
    FVectorType m_fvec, m_qtf, m_diag; 
    Index n;
    Index m; 
    Index m_nfev;
    Index m_njev; 
    RealScalar m_fnorm; 
    RealScalar m_gnorm; 
    RealScalar m_factor; 
    Index m_maxfev; 
    RealScalar m_ftol; 
    RealScalar m_xtol; 
    RealScalar m_gtol; 
    RealScalar m_epsfcn; 
    Index m_iter; 
    RealScalar m_delta;
    bool m_useExternalScaling;
    PermutationType m_permutation;
    FVectorType m_wa1, m_wa2, m_wa3, m_wa4; 
    RealScalar m_par;
    bool m_isInitialized; 
    ComputationInfo m_info; 
};

template<typename FunctorType>
LevenbergMarquardtSpace::Status
LevenbergMarquardt<FunctorType>::minimize(FVectorType  &x)
{
    LevenbergMarquardtSpace::Status status = minimizeInit(x);
    if (status==LevenbergMarquardtSpace::ImproperInputParameters) {
      m_isInitialized = true;
      return status;
    }
    do {
        status = minimizeOneStep(x);
    } while (status==LevenbergMarquardtSpace::Running);
     m_isInitialized = true;
     return status;
}

template<typename FunctorType>
LevenbergMarquardtSpace::Status
LevenbergMarquardt<FunctorType>::minimizeInit(FVectorType  &x)
{
    n = x.size();
    m = m_functor.values();

    m_wa1.resize(n); m_wa2.resize(n); m_wa3.resize(n);
    m_wa4.resize(m);
    m_fvec.resize(m);
    
    m_fjac.resize(m, n);
    if (!m_useExternalScaling)
        m_diag.resize(n);
    eigen_assert( (!m_useExternalScaling || m_diag.size()==n) || "When m_useExternalScaling is set, the caller must provide a valid 'm_diag'");
    m_qtf.resize(n);

    
    m_nfev = 0;
    m_njev = 0;

    
    if (n <= 0 || m < n || m_ftol < 0. || m_xtol < 0. || m_gtol < 0. || m_maxfev <= 0 || m_factor <= 0.){
      m_info = InvalidInput;
      return LevenbergMarquardtSpace::ImproperInputParameters;
    }

    if (m_useExternalScaling)
        for (Index j = 0; j < n; ++j)
            if (m_diag[j] <= 0.) 
            {
              m_info = InvalidInput;
              return LevenbergMarquardtSpace::ImproperInputParameters;
            }

    
    
    m_nfev = 1;
    if ( m_functor(x, m_fvec) < 0)
        return LevenbergMarquardtSpace::UserAsked;
    m_fnorm = m_fvec.stableNorm();

    
    m_par = 0.;
    m_iter = 1;

    return LevenbergMarquardtSpace::NotStarted;
}

template<typename FunctorType>
LevenbergMarquardtSpace::Status
LevenbergMarquardt<FunctorType>::lmder1(
        FVectorType  &x,
        const Scalar tol
        )
{
    n = x.size();
    m = m_functor.values();

    
    if (n <= 0 || m < n || tol < 0.)
        return LevenbergMarquardtSpace::ImproperInputParameters;

    resetParameters();
    m_ftol = tol;
    m_xtol = tol;
    m_maxfev = 100*(n+1);

    return minimize(x);
}


template<typename FunctorType>
LevenbergMarquardtSpace::Status
LevenbergMarquardt<FunctorType>::lmdif1(
        FunctorType &functor,
        FVectorType  &x,
        Index *nfev,
        const Scalar tol
        )
{
    Index n = x.size();
    Index m = functor.values();

    
    if (n <= 0 || m < n || tol < 0.)
        return LevenbergMarquardtSpace::ImproperInputParameters;

    NumericalDiff<FunctorType> numDiff(functor);
    
    LevenbergMarquardt<NumericalDiff<FunctorType> > lm(numDiff);
    lm.setFtol(tol);
    lm.setXtol(tol);
    lm.setMaxfev(200*(n+1));

    LevenbergMarquardtSpace::Status info = LevenbergMarquardtSpace::Status(lm.minimize(x));
    if (nfev)
        * nfev = lm.nfev();
    return info;
}

} 

#endif 
