// Copyright (C) 20010-2011 Hauke Heibel <hauke.heibel@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SPLINE_H
#define EIGEN_SPLINE_H

#include "SplineFwd.h"

namespace Eigen
{
  template <typename _Scalar, int _Dim, int _Degree>
  class Spline
  {
  public:
    typedef _Scalar Scalar; 
    enum { Dimension = _Dim  };
    enum { Degree = _Degree  };

    
    typedef typename SplineTraits<Spline>::PointType PointType;
    
    
    typedef typename SplineTraits<Spline>::KnotVectorType KnotVectorType;
    
    
    typedef typename SplineTraits<Spline>::BasisVectorType BasisVectorType;
    
    
    typedef typename SplineTraits<Spline>::ControlPointVectorType ControlPointVectorType;
    
    Spline() 
    : m_knots(1, (Degree==Dynamic ? 2 : 2*Degree+2))
    , m_ctrls(ControlPointVectorType::Zero(2,(Degree==Dynamic ? 1 : Degree+1))) 
    {
      
      
      enum { MinDegree = (Degree==Dynamic ? 0 : Degree) };
      m_knots.template segment<MinDegree+1>(0) = Array<Scalar,1,MinDegree+1>::Zero();
      m_knots.template segment<MinDegree+1>(MinDegree+1) = Array<Scalar,1,MinDegree+1>::Ones();
    }

    template <typename OtherVectorType, typename OtherArrayType>
    Spline(const OtherVectorType& knots, const OtherArrayType& ctrls) : m_knots(knots), m_ctrls(ctrls) {}

    template <int OtherDegree>
    Spline(const Spline<Scalar, Dimension, OtherDegree>& spline) : 
    m_knots(spline.knots()), m_ctrls(spline.ctrls()) {}

    const KnotVectorType& knots() const { return m_knots; }
    
    
    const ControlPointVectorType& ctrls() const { return m_ctrls; }

    PointType operator()(Scalar u) const;

    typename SplineTraits<Spline>::DerivativeType
      derivatives(Scalar u, DenseIndex order) const;

    
    template <int DerivativeOrder>
    typename SplineTraits<Spline,DerivativeOrder>::DerivativeType
      derivatives(Scalar u, DenseIndex order = DerivativeOrder) const;

    typename SplineTraits<Spline>::BasisVectorType
      basisFunctions(Scalar u) const;

    typename SplineTraits<Spline>::BasisDerivativeType
      basisFunctionDerivatives(Scalar u, DenseIndex order) const;

    
    template <int DerivativeOrder>
    typename SplineTraits<Spline,DerivativeOrder>::BasisDerivativeType
      basisFunctionDerivatives(Scalar u, DenseIndex order = DerivativeOrder) const;

 
    DenseIndex degree() const;

    DenseIndex span(Scalar u) const;

    static DenseIndex Span(typename SplineTraits<Spline>::Scalar u, DenseIndex degree, const typename SplineTraits<Spline>::KnotVectorType& knots);
    
    static BasisVectorType BasisFunctions(Scalar u, DenseIndex degree, const KnotVectorType& knots);


  private:
    KnotVectorType m_knots; 
    ControlPointVectorType  m_ctrls; 
  };

  template <typename _Scalar, int _Dim, int _Degree>
  DenseIndex Spline<_Scalar, _Dim, _Degree>::Span(
    typename SplineTraits< Spline<_Scalar, _Dim, _Degree> >::Scalar u,
    DenseIndex degree,
    const typename SplineTraits< Spline<_Scalar, _Dim, _Degree> >::KnotVectorType& knots)
  {
    
    if (u <= knots(0)) return degree;
    const Scalar* pos = std::upper_bound(knots.data()+degree-1, knots.data()+knots.size()-degree-1, u);
    return static_cast<DenseIndex>( std::distance(knots.data(), pos) - 1 );
  }

  template <typename _Scalar, int _Dim, int _Degree>
  typename Spline<_Scalar, _Dim, _Degree>::BasisVectorType
    Spline<_Scalar, _Dim, _Degree>::BasisFunctions(
    typename Spline<_Scalar, _Dim, _Degree>::Scalar u,
    DenseIndex degree,
    const typename Spline<_Scalar, _Dim, _Degree>::KnotVectorType& knots)
  {
    typedef typename Spline<_Scalar, _Dim, _Degree>::BasisVectorType BasisVectorType;

    const DenseIndex p = degree;
    const DenseIndex i = Spline::Span(u, degree, knots);

    const KnotVectorType& U = knots;

    BasisVectorType left(p+1); left(0) = Scalar(0);
    BasisVectorType right(p+1); right(0) = Scalar(0);        

    VectorBlock<BasisVectorType,Degree>(left,1,p) = u - VectorBlock<const KnotVectorType,Degree>(U,i+1-p,p).reverse();
    VectorBlock<BasisVectorType,Degree>(right,1,p) = VectorBlock<const KnotVectorType,Degree>(U,i+1,p) - u;

    BasisVectorType N(1,p+1);
    N(0) = Scalar(1);
    for (DenseIndex j=1; j<=p; ++j)
    {
      Scalar saved = Scalar(0);
      for (DenseIndex r=0; r<j; r++)
      {
        const Scalar tmp = N(r)/(right(r+1)+left(j-r));
        N[r] = saved + right(r+1)*tmp;
        saved = left(j-r)*tmp;
      }
      N(j) = saved;
    }
    return N;
  }

  template <typename _Scalar, int _Dim, int _Degree>
  DenseIndex Spline<_Scalar, _Dim, _Degree>::degree() const
  {
    if (_Degree == Dynamic)
      return m_knots.size() - m_ctrls.cols() - 1;
    else
      return _Degree;
  }

  template <typename _Scalar, int _Dim, int _Degree>
  DenseIndex Spline<_Scalar, _Dim, _Degree>::span(Scalar u) const
  {
    return Spline::Span(u, degree(), knots());
  }

  template <typename _Scalar, int _Dim, int _Degree>
  typename Spline<_Scalar, _Dim, _Degree>::PointType Spline<_Scalar, _Dim, _Degree>::operator()(Scalar u) const
  {
    enum { Order = SplineTraits<Spline>::OrderAtCompileTime };

    const DenseIndex span = this->span(u);
    const DenseIndex p = degree();
    const BasisVectorType basis_funcs = basisFunctions(u);

    const Replicate<BasisVectorType,Dimension,1> ctrl_weights(basis_funcs);
    const Block<const ControlPointVectorType,Dimension,Order> ctrl_pts(ctrls(),0,span-p,Dimension,p+1);
    return (ctrl_weights * ctrl_pts).rowwise().sum();
  }

  

  template <typename SplineType, typename DerivativeType>
  void derivativesImpl(const SplineType& spline, typename SplineType::Scalar u, DenseIndex order, DerivativeType& der)
  {    
    enum { Dimension = SplineTraits<SplineType>::Dimension };
    enum { Order = SplineTraits<SplineType>::OrderAtCompileTime };
    enum { DerivativeOrder = DerivativeType::ColsAtCompileTime };

    typedef typename SplineTraits<SplineType>::ControlPointVectorType ControlPointVectorType;
    typedef typename SplineTraits<SplineType,DerivativeOrder>::BasisDerivativeType BasisDerivativeType;
    typedef typename BasisDerivativeType::ConstRowXpr BasisDerivativeRowXpr;    

    const DenseIndex p = spline.degree();
    const DenseIndex span = spline.span(u);

    const DenseIndex n = (std::min)(p, order);

    der.resize(Dimension,n+1);

    
    const BasisDerivativeType basis_func_ders = spline.template basisFunctionDerivatives<DerivativeOrder>(u, n+1);

    
    for (DenseIndex der_order=0; der_order<n+1; ++der_order)
    {
      const Replicate<BasisDerivativeRowXpr,Dimension,1> ctrl_weights( basis_func_ders.row(der_order) );
      const Block<const ControlPointVectorType,Dimension,Order> ctrl_pts(spline.ctrls(),0,span-p,Dimension,p+1);
      der.col(der_order) = (ctrl_weights * ctrl_pts).rowwise().sum();
    }
  }

  template <typename _Scalar, int _Dim, int _Degree>
  typename SplineTraits< Spline<_Scalar, _Dim, _Degree> >::DerivativeType
    Spline<_Scalar, _Dim, _Degree>::derivatives(Scalar u, DenseIndex order) const
  {
    typename SplineTraits< Spline >::DerivativeType res;
    derivativesImpl(*this, u, order, res);
    return res;
  }

  template <typename _Scalar, int _Dim, int _Degree>
  template <int DerivativeOrder>
  typename SplineTraits< Spline<_Scalar, _Dim, _Degree>, DerivativeOrder >::DerivativeType
    Spline<_Scalar, _Dim, _Degree>::derivatives(Scalar u, DenseIndex order) const
  {
    typename SplineTraits< Spline, DerivativeOrder >::DerivativeType res;
    derivativesImpl(*this, u, order, res);
    return res;
  }

  template <typename _Scalar, int _Dim, int _Degree>
  typename SplineTraits< Spline<_Scalar, _Dim, _Degree> >::BasisVectorType
    Spline<_Scalar, _Dim, _Degree>::basisFunctions(Scalar u) const
  {
    return Spline::BasisFunctions(u, degree(), knots());
  }

  

  template <typename SplineType, typename DerivativeType>
  void basisFunctionDerivativesImpl(const SplineType& spline, typename SplineType::Scalar u, DenseIndex order, DerivativeType& N_)
  {
    enum { Order = SplineTraits<SplineType>::OrderAtCompileTime };

    typedef typename SplineTraits<SplineType>::Scalar Scalar;
    typedef typename SplineTraits<SplineType>::BasisVectorType BasisVectorType;
    typedef typename SplineTraits<SplineType>::KnotVectorType KnotVectorType;

    const KnotVectorType& U = spline.knots();

    const DenseIndex p = spline.degree();
    const DenseIndex span = spline.span(u);

    const DenseIndex n = (std::min)(p, order);

    N_.resize(n+1, p+1);

    BasisVectorType left = BasisVectorType::Zero(p+1);
    BasisVectorType right = BasisVectorType::Zero(p+1);

    Matrix<Scalar,Order,Order> ndu(p+1,p+1);

    double saved, temp;

    ndu(0,0) = 1.0;

    DenseIndex j;
    for (j=1; j<=p; ++j)
    {
      left[j] = u-U[span+1-j];
      right[j] = U[span+j]-u;
      saved = 0.0;

      for (DenseIndex r=0; r<j; ++r)
      {
        
        ndu(j,r) = right[r+1]+left[j-r];
        temp = ndu(r,j-1)/ndu(j,r);
        
        ndu(r,j) = static_cast<Scalar>(saved+right[r+1] * temp);
        saved = left[j-r] * temp;
      }

      ndu(j,j) = static_cast<Scalar>(saved);
    }

    for (j = p; j>=0; --j) 
      N_(0,j) = ndu(j,p);

    
    DerivativeType a(n+1,p+1);
    DenseIndex r=0;
    for (; r<=p; ++r)
    {
      DenseIndex s1,s2;
      s1 = 0; s2 = 1; 
      a(0,0) = 1.0;

      
      for (DenseIndex k=1; k<=static_cast<DenseIndex>(n); ++k)
      {
        double d = 0.0;
        DenseIndex rk,pk,j1,j2;
        rk = r-k; pk = p-k;

        if (r>=k)
        {
          a(s2,0) = a(s1,0)/ndu(pk+1,rk);
          d = a(s2,0)*ndu(rk,pk);
        }

        if (rk>=-1) j1 = 1;
        else        j1 = -rk;

        if (r-1 <= pk) j2 = k-1;
        else           j2 = p-r;

        for (j=j1; j<=j2; ++j)
        {
          a(s2,j) = (a(s1,j)-a(s1,j-1))/ndu(pk+1,rk+j);
          d += a(s2,j)*ndu(rk+j,pk);
        }

        if (r<=pk)
        {
          a(s2,k) = -a(s1,k-1)/ndu(pk+1,r);
          d += a(s2,k)*ndu(r,pk);
        }

        N_(k,r) = static_cast<Scalar>(d);
        j = s1; s1 = s2; s2 = j; 
      }
    }

    
    
    r = p;
    for (DenseIndex k=1; k<=static_cast<DenseIndex>(n); ++k)
    {
      for (DenseIndex j=p; j>=0; --j) N_(k,j) *= r;
      r *= p-k;
    }
  }

  template <typename _Scalar, int _Dim, int _Degree>
  typename SplineTraits< Spline<_Scalar, _Dim, _Degree> >::BasisDerivativeType
    Spline<_Scalar, _Dim, _Degree>::basisFunctionDerivatives(Scalar u, DenseIndex order) const
  {
    typename SplineTraits< Spline >::BasisDerivativeType der;
    basisFunctionDerivativesImpl(*this, u, order, der);
    return der;
  }

  template <typename _Scalar, int _Dim, int _Degree>
  template <int DerivativeOrder>
  typename SplineTraits< Spline<_Scalar, _Dim, _Degree>, DerivativeOrder >::BasisDerivativeType
    Spline<_Scalar, _Dim, _Degree>::basisFunctionDerivatives(Scalar u, DenseIndex order) const
  {
    typename SplineTraits< Spline, DerivativeOrder >::BasisDerivativeType der;
    basisFunctionDerivativesImpl(*this, u, order, der);
    return der;
  }
}

#endif 
