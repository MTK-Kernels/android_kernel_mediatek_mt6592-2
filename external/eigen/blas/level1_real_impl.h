// Copyright (C) 2009-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#include "common.h"

RealScalar EIGEN_BLAS_FUNC(asum)(int *n, RealScalar *px, int *incx)
{

  Scalar* x = reinterpret_cast<Scalar*>(px);

  if(*n<=0) return 0;

  if(*incx==1)  return vector(x,*n).cwiseAbs().sum();
  else          return vector(x,*n,std::abs(*incx)).cwiseAbs().sum();
}

Scalar EIGEN_BLAS_FUNC(dot)(int *n, RealScalar *px, int *incx, RealScalar *py, int *incy)
{

  if(*n<=0) return 0;

  Scalar* x = reinterpret_cast<Scalar*>(px);
  Scalar* y = reinterpret_cast<Scalar*>(py);

  if(*incx==1 && *incy==1)    return (vector(x,*n).cwiseProduct(vector(y,*n))).sum();
  else if(*incx>0 && *incy>0) return (vector(x,*n,*incx).cwiseProduct(vector(y,*n,*incy))).sum();
  else if(*incx<0 && *incy>0) return (vector(x,*n,-*incx).reverse().cwiseProduct(vector(y,*n,*incy))).sum();
  else if(*incx>0 && *incy<0) return (vector(x,*n,*incx).cwiseProduct(vector(y,*n,-*incy).reverse())).sum();
  else if(*incx<0 && *incy<0) return (vector(x,*n,-*incx).reverse().cwiseProduct(vector(y,*n,-*incy).reverse())).sum();
  else return 0;
}

Scalar EIGEN_BLAS_FUNC(nrm2)(int *n, RealScalar *px, int *incx)
{
  if(*n<=0) return 0;

  Scalar* x = reinterpret_cast<Scalar*>(px);

  if(*incx==1)  return vector(x,*n).stableNorm();
  else          return vector(x,*n,std::abs(*incx)).stableNorm();
}

int EIGEN_BLAS_FUNC(rot)(int *n, RealScalar *px, int *incx, RealScalar *py, int *incy, RealScalar *pc, RealScalar *ps)
{
  if(*n<=0) return 0;

  Scalar* x = reinterpret_cast<Scalar*>(px);
  Scalar* y = reinterpret_cast<Scalar*>(py);
  Scalar c = *reinterpret_cast<Scalar*>(pc);
  Scalar s = *reinterpret_cast<Scalar*>(ps);

  StridedVectorType vx(vector(x,*n,std::abs(*incx)));
  StridedVectorType vy(vector(y,*n,std::abs(*incy)));

  Reverse<StridedVectorType> rvx(vx);
  Reverse<StridedVectorType> rvy(vy);

       if(*incx<0 && *incy>0) internal::apply_rotation_in_the_plane(rvx, vy, JacobiRotation<Scalar>(c,s));
  else if(*incx>0 && *incy<0) internal::apply_rotation_in_the_plane(vx, rvy, JacobiRotation<Scalar>(c,s));
  else                        internal::apply_rotation_in_the_plane(vx, vy,  JacobiRotation<Scalar>(c,s));


  return 0;
}

