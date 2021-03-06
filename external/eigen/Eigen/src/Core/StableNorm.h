// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_STABLENORM_H
#define EIGEN_STABLENORM_H

namespace Eigen { 

namespace internal {

template<typename ExpressionType, typename Scalar>
inline void stable_norm_kernel(const ExpressionType& bl, Scalar& ssq, Scalar& scale, Scalar& invScale)
{
  using std::max;
  Scalar maxCoeff = bl.cwiseAbs().maxCoeff();
  
  if (maxCoeff>scale)
  {
    ssq = ssq * numext::abs2(scale/maxCoeff);
    Scalar tmp = Scalar(1)/maxCoeff;
    if(tmp > NumTraits<Scalar>::highest())
    {
      invScale = NumTraits<Scalar>::highest();
      scale = Scalar(1)/invScale;
    }
    else
    {
      scale = maxCoeff;
      invScale = tmp;
    }
  }
  
  
  
  if(scale>Scalar(0)) 
    ssq += (bl*invScale).squaredNorm();
}

template<typename Derived>
inline typename NumTraits<typename traits<Derived>::Scalar>::Real
blueNorm_impl(const EigenBase<Derived>& _vec)
{
  typedef typename Derived::RealScalar RealScalar;  
  typedef typename Derived::Index Index;
  using std::pow;
  using std::min;
  using std::max;
  using std::sqrt;
  using std::abs;
  const Derived& vec(_vec.derived());
  static bool initialized = false;
  static RealScalar b1, b2, s1m, s2m, overfl, rbig, relerr;
  if(!initialized)
  {
    int ibeta, it, iemin, iemax, iexp;
    RealScalar eps;
    
    
    
    
    
    
    
    
    ibeta = std::numeric_limits<RealScalar>::radix;                 
    it    = std::numeric_limits<RealScalar>::digits;                
    iemin = std::numeric_limits<RealScalar>::min_exponent;          
    iemax = std::numeric_limits<RealScalar>::max_exponent;          
    rbig  = (std::numeric_limits<RealScalar>::max)();               

    iexp  = -((1-iemin)/2);
    b1    = RealScalar(pow(RealScalar(ibeta),RealScalar(iexp)));    
    iexp  = (iemax + 1 - it)/2;
    b2    = RealScalar(pow(RealScalar(ibeta),RealScalar(iexp)));    

    iexp  = (2-iemin)/2;
    s1m   = RealScalar(pow(RealScalar(ibeta),RealScalar(iexp)));    
    iexp  = - ((iemax+it)/2);
    s2m   = RealScalar(pow(RealScalar(ibeta),RealScalar(iexp)));    

    overfl  = rbig*s2m;                                             
    eps     = RealScalar(pow(double(ibeta), 1-it));
    relerr  = sqrt(eps);                                            
    initialized = true;
  }
  Index n = vec.size();
  RealScalar ab2 = b2 / RealScalar(n);
  RealScalar asml = RealScalar(0);
  RealScalar amed = RealScalar(0);
  RealScalar abig = RealScalar(0);
  for(typename Derived::InnerIterator it(vec, 0); it; ++it)
  {
    RealScalar ax = abs(it.value());
    if(ax > ab2)     abig += numext::abs2(ax*s2m);
    else if(ax < b1) asml += numext::abs2(ax*s1m);
    else             amed += numext::abs2(ax);
  }
  if(abig > RealScalar(0))
  {
    abig = sqrt(abig);
    if(abig > overfl)
    {
      return rbig;
    }
    if(amed > RealScalar(0))
    {
      abig = abig/s2m;
      amed = sqrt(amed);
    }
    else
      return abig/s2m;
  }
  else if(asml > RealScalar(0))
  {
    if (amed > RealScalar(0))
    {
      abig = sqrt(amed);
      amed = sqrt(asml) / s1m;
    }
    else
      return sqrt(asml)/s1m;
  }
  else
    return sqrt(amed);
  asml = (min)(abig, amed);
  abig = (max)(abig, amed);
  if(asml <= abig*relerr)
    return abig;
  else
    return abig * sqrt(RealScalar(1) + numext::abs2(asml/abig));
}

} 

template<typename Derived>
inline typename NumTraits<typename internal::traits<Derived>::Scalar>::Real
MatrixBase<Derived>::stableNorm() const
{
  using std::min;
  using std::sqrt;
  const Index blockSize = 4096;
  RealScalar scale(0);
  RealScalar invScale(1);
  RealScalar ssq(0); 
  enum {
    Alignment = (int(Flags)&DirectAccessBit) || (int(Flags)&AlignedBit) ? 1 : 0
  };
  Index n = size();
  Index bi = internal::first_aligned(derived());
  if (bi>0)
    internal::stable_norm_kernel(this->head(bi), ssq, scale, invScale);
  for (; bi<n; bi+=blockSize)
    internal::stable_norm_kernel(this->segment(bi,(min)(blockSize, n - bi)).template forceAlignedAccessIf<Alignment>(), ssq, scale, invScale);
  return scale * sqrt(ssq);
}

template<typename Derived>
inline typename NumTraits<typename internal::traits<Derived>::Scalar>::Real
MatrixBase<Derived>::blueNorm() const
{
  return internal::blueNorm_impl(*this);
}

template<typename Derived>
inline typename NumTraits<typename internal::traits<Derived>::Scalar>::Real
MatrixBase<Derived>::hypotNorm() const
{
  return this->cwiseAbs().redux(internal::scalar_hypot_op<RealScalar>());
}

} 

#endif 
