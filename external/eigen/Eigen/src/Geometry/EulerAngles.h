// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_EULERANGLES_H
#define EIGEN_EULERANGLES_H

namespace Eigen { 

template<typename Derived>
inline Matrix<typename MatrixBase<Derived>::Scalar,3,1>
MatrixBase<Derived>::eulerAngles(Index a0, Index a1, Index a2) const
{
  using std::atan2;
  using std::sin;
  using std::cos;
  
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE(Derived,3,3)

  Matrix<Scalar,3,1> res;
  typedef Matrix<typename Derived::Scalar,2,1> Vector2;

  const Index odd = ((a0+1)%3 == a1) ? 0 : 1;
  const Index i = a0;
  const Index j = (a0 + 1 + odd)%3;
  const Index k = (a0 + 2 - odd)%3;
  
  if (a0==a2)
  {
    res[0] = atan2(coeff(j,i), coeff(k,i));
    if((odd && res[0]<Scalar(0)) || ((!odd) && res[0]>Scalar(0)))
    {
      res[0] = (res[0] > Scalar(0)) ? res[0] - Scalar(M_PI) : res[0] + Scalar(M_PI);
      Scalar s2 = Vector2(coeff(j,i), coeff(k,i)).norm();
      res[1] = -atan2(s2, coeff(i,i));
    }
    else
    {
      Scalar s2 = Vector2(coeff(j,i), coeff(k,i)).norm();
      res[1] = atan2(s2, coeff(i,i));
    }
    
    
    
    
    
    
    
    
    
    
    
    Scalar s1 = sin(res[0]);
    Scalar c1 = cos(res[0]);
    res[2] = atan2(c1*coeff(j,k)-s1*coeff(k,k), c1*coeff(j,j) - s1 * coeff(k,j));
  } 
  else
  {
    res[0] = atan2(coeff(j,k), coeff(k,k));
    Scalar c2 = Vector2(coeff(i,i), coeff(i,j)).norm();
    if((odd && res[0]<Scalar(0)) || ((!odd) && res[0]>Scalar(0))) {
      res[0] = (res[0] > Scalar(0)) ? res[0] - Scalar(M_PI) : res[0] + Scalar(M_PI);
      res[1] = atan2(-coeff(i,k), -c2);
    }
    else
      res[1] = atan2(-coeff(i,k), c2);
    Scalar s1 = sin(res[0]);
    Scalar c1 = cos(res[0]);
    res[2] = atan2(s1*coeff(k,i)-c1*coeff(j,i), c1*coeff(j,j) - s1 * coeff(k,j));
  }
  if (!odd)
    res = -res;
  
  return res;
}

} 

#endif 
