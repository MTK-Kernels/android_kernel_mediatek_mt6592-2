// Copyright (C) 2010 Manuel Yguel <manuel.yguel@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_POLYNOMIAL_UTILS_H
#define EIGEN_POLYNOMIAL_UTILS_H

namespace Eigen { 

template <typename Polynomials, typename T>
inline
T poly_eval_horner( const Polynomials& poly, const T& x )
{
  T val=poly[poly.size()-1];
  for(DenseIndex i=poly.size()-2; i>=0; --i ){
    val = val*x + poly[i]; }
  return val;
}

template <typename Polynomials, typename T>
inline
T poly_eval( const Polynomials& poly, const T& x )
{
  typedef typename NumTraits<T>::Real Real;

  if( numext::abs2( x ) <= Real(1) ){
    return poly_eval_horner( poly, x ); }
  else
  {
    T val=poly[0];
    T inv_x = T(1)/x;
    for( DenseIndex i=1; i<poly.size(); ++i ){
      val = val*inv_x + poly[i]; }

    return std::pow(x,(T)(poly.size()-1)) * val;
  }
}

template <typename Polynomial>
inline
typename NumTraits<typename Polynomial::Scalar>::Real cauchy_max_bound( const Polynomial& poly )
{
  using std::abs;
  typedef typename Polynomial::Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real Real;

  eigen_assert( Scalar(0) != poly[poly.size()-1] );
  const Scalar inv_leading_coeff = Scalar(1)/poly[poly.size()-1];
  Real cb(0);

  for( DenseIndex i=0; i<poly.size()-1; ++i ){
    cb += abs(poly[i]*inv_leading_coeff); }
  return cb + Real(1);
}

template <typename Polynomial>
inline
typename NumTraits<typename Polynomial::Scalar>::Real cauchy_min_bound( const Polynomial& poly )
{
  using std::abs;
  typedef typename Polynomial::Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real Real;

  DenseIndex i=0;
  while( i<poly.size()-1 && Scalar(0) == poly(i) ){ ++i; }
  if( poly.size()-1 == i ){
    return Real(1); }

  const Scalar inv_min_coeff = Scalar(1)/poly[i];
  Real cb(1);
  for( DenseIndex j=i+1; j<poly.size(); ++j ){
    cb += abs(poly[j]*inv_min_coeff); }
  return Real(1)/cb;
}

template <typename RootVector, typename Polynomial>
void roots_to_monicPolynomial( const RootVector& rv, Polynomial& poly )
{

  typedef typename Polynomial::Scalar Scalar;

  poly.setZero( rv.size()+1 );
  poly[0] = -rv[0]; poly[1] = Scalar(1);
  for( DenseIndex i=1; i< rv.size(); ++i )
  {
    for( DenseIndex j=i+1; j>0; --j ){ poly[j] = poly[j-1] - rv[i]*poly[j]; }
    poly[0] = -rv[i]*poly[0];
  }
}

} 

#endif 
