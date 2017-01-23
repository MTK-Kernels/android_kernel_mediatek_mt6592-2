// Copyright (C) 2010 Manuel Yguel <manuel.yguel@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_POLYNOMIAL_SOLVER_H
#define EIGEN_POLYNOMIAL_SOLVER_H

namespace Eigen { 

template< typename _Scalar, int _Deg >
class PolynomialSolverBase
{
  public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_Deg==Dynamic ? Dynamic : _Deg)

    typedef _Scalar                             Scalar;
    typedef typename NumTraits<Scalar>::Real    RealScalar;
    typedef std::complex<RealScalar>            RootType;
    typedef Matrix<RootType,_Deg,1>             RootsType;

    typedef DenseIndex Index;

  protected:
    template< typename OtherPolynomial >
    inline void setPolynomial( const OtherPolynomial& poly ){
      m_roots.resize(poly.size()); }

  public:
    template< typename OtherPolynomial >
    inline PolynomialSolverBase( const OtherPolynomial& poly ){
      setPolynomial( poly() ); }

    inline PolynomialSolverBase(){}

  public:
    
    inline const RootsType& roots() const { return m_roots; }

  public:
    template<typename Stl_back_insertion_sequence>
    inline void realRoots( Stl_back_insertion_sequence& bi_seq,
        const RealScalar& absImaginaryThreshold = NumTraits<Scalar>::dummy_precision() ) const
    {
      using std::abs;
      bi_seq.clear();
      for(Index i=0; i<m_roots.size(); ++i )
      {
        if( abs( m_roots[i].imag() ) < absImaginaryThreshold ){
          bi_seq.push_back( m_roots[i].real() ); }
      }
    }

  protected:
    template<typename squaredNormBinaryPredicate>
    inline const RootType& selectComplexRoot_withRespectToNorm( squaredNormBinaryPredicate& pred ) const
    {
      Index res=0;
      RealScalar norm2 = numext::abs2( m_roots[0] );
      for( Index i=1; i<m_roots.size(); ++i )
      {
        const RealScalar currNorm2 = numext::abs2( m_roots[i] );
        if( pred( currNorm2, norm2 ) ){
          res=i; norm2=currNorm2; }
      }
      return m_roots[res];
    }

  public:
    inline const RootType& greatestRoot() const
    {
      std::greater<Scalar> greater;
      return selectComplexRoot_withRespectToNorm( greater );
    }

    inline const RootType& smallestRoot() const
    {
      std::less<Scalar> less;
      return selectComplexRoot_withRespectToNorm( less );
    }

  protected:
    template<typename squaredRealPartBinaryPredicate>
    inline const RealScalar& selectRealRoot_withRespectToAbsRealPart(
        squaredRealPartBinaryPredicate& pred,
        bool& hasArealRoot,
        const RealScalar& absImaginaryThreshold = NumTraits<Scalar>::dummy_precision() ) const
    {
      using std::abs;
      hasArealRoot = false;
      Index res=0;
      RealScalar abs2(0);

      for( Index i=0; i<m_roots.size(); ++i )
      {
        if( abs( m_roots[i].imag() ) < absImaginaryThreshold )
        {
          if( !hasArealRoot )
          {
            hasArealRoot = true;
            res = i;
            abs2 = m_roots[i].real() * m_roots[i].real();
          }
          else
          {
            const RealScalar currAbs2 = m_roots[i].real() * m_roots[i].real();
            if( pred( currAbs2, abs2 ) )
            {
              abs2 = currAbs2;
              res = i;
            }
          }
        }
        else
        {
          if( abs( m_roots[i].imag() ) < abs( m_roots[res].imag() ) ){
            res = i; }
        }
      }
      return numext::real_ref(m_roots[res]);
    }


    template<typename RealPartBinaryPredicate>
    inline const RealScalar& selectRealRoot_withRespectToRealPart(
        RealPartBinaryPredicate& pred,
        bool& hasArealRoot,
        const RealScalar& absImaginaryThreshold = NumTraits<Scalar>::dummy_precision() ) const
    {
      using std::abs;
      hasArealRoot = false;
      Index res=0;
      RealScalar val(0);

      for( Index i=0; i<m_roots.size(); ++i )
      {
        if( abs( m_roots[i].imag() ) < absImaginaryThreshold )
        {
          if( !hasArealRoot )
          {
            hasArealRoot = true;
            res = i;
            val = m_roots[i].real();
          }
          else
          {
            const RealScalar curr = m_roots[i].real();
            if( pred( curr, val ) )
            {
              val = curr;
              res = i;
            }
          }
        }
        else
        {
          if( abs( m_roots[i].imag() ) < abs( m_roots[res].imag() ) ){
            res = i; }
        }
      }
      return numext::real_ref(m_roots[res]);
    }

  public:
    inline const RealScalar& absGreatestRealRoot(
        bool& hasArealRoot,
        const RealScalar& absImaginaryThreshold = NumTraits<Scalar>::dummy_precision() ) const
    {
      std::greater<Scalar> greater;
      return selectRealRoot_withRespectToAbsRealPart( greater, hasArealRoot, absImaginaryThreshold );
    }


    inline const RealScalar& absSmallestRealRoot(
        bool& hasArealRoot,
        const RealScalar& absImaginaryThreshold = NumTraits<Scalar>::dummy_precision() ) const
    {
      std::less<Scalar> less;
      return selectRealRoot_withRespectToAbsRealPart( less, hasArealRoot, absImaginaryThreshold );
    }


    inline const RealScalar& greatestRealRoot(
        bool& hasArealRoot,
        const RealScalar& absImaginaryThreshold = NumTraits<Scalar>::dummy_precision() ) const
    {
      std::greater<Scalar> greater;
      return selectRealRoot_withRespectToRealPart( greater, hasArealRoot, absImaginaryThreshold );
    }


    inline const RealScalar& smallestRealRoot(
        bool& hasArealRoot,
        const RealScalar& absImaginaryThreshold = NumTraits<Scalar>::dummy_precision() ) const
    {
      std::less<Scalar> less;
      return selectRealRoot_withRespectToRealPart( less, hasArealRoot, absImaginaryThreshold );
    }

  protected:
    RootsType               m_roots;
};

#define EIGEN_POLYNOMIAL_SOLVER_BASE_INHERITED_TYPES( BASE )  \
  typedef typename BASE::Scalar                 Scalar;       \
  typedef typename BASE::RealScalar             RealScalar;   \
  typedef typename BASE::RootType               RootType;     \
  typedef typename BASE::RootsType              RootsType;



template< typename _Scalar, int _Deg >
class PolynomialSolver : public PolynomialSolverBase<_Scalar,_Deg>
{
  public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_Deg==Dynamic ? Dynamic : _Deg)

    typedef PolynomialSolverBase<_Scalar,_Deg>    PS_Base;
    EIGEN_POLYNOMIAL_SOLVER_BASE_INHERITED_TYPES( PS_Base )

    typedef Matrix<Scalar,_Deg,_Deg>                 CompanionMatrixType;
    typedef EigenSolver<CompanionMatrixType>         EigenSolverType;

  public:
    
    template< typename OtherPolynomial >
    void compute( const OtherPolynomial& poly )
    {
      eigen_assert( Scalar(0) != poly[poly.size()-1] );
      internal::companion<Scalar,_Deg> companion( poly );
      companion.balance();
      m_eigenSolver.compute( companion.denseMatrix() );
      m_roots = m_eigenSolver.eigenvalues();
    }

  public:
    template< typename OtherPolynomial >
    inline PolynomialSolver( const OtherPolynomial& poly ){
      compute( poly ); }

    inline PolynomialSolver(){}

  protected:
    using                   PS_Base::m_roots;
    EigenSolverType         m_eigenSolver;
};


template< typename _Scalar >
class PolynomialSolver<_Scalar,1> : public PolynomialSolverBase<_Scalar,1>
{
  public:
    typedef PolynomialSolverBase<_Scalar,1>    PS_Base;
    EIGEN_POLYNOMIAL_SOLVER_BASE_INHERITED_TYPES( PS_Base )

  public:
    
    template< typename OtherPolynomial >
    void compute( const OtherPolynomial& poly )
    {
      eigen_assert( Scalar(0) != poly[poly.size()-1] );
      m_roots[0] = -poly[0]/poly[poly.size()-1];
    }

  protected:
    using                   PS_Base::m_roots;
};

} 

#endif 
