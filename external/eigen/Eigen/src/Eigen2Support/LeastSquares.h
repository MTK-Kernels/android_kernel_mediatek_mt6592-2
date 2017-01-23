// Copyright (C) 2006-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN2_LEASTSQUARES_H
#define EIGEN2_LEASTSQUARES_H

namespace Eigen { 

template<typename VectorType>
void linearRegression(int numPoints,
                      VectorType **points,
                      VectorType *result,
                      int funcOfOthers )
{
  typedef typename VectorType::Scalar Scalar;
  typedef Hyperplane<Scalar, VectorType::SizeAtCompileTime> HyperplaneType;
  const int size = points[0]->size();
  result->resize(size);
  HyperplaneType h(size);
  fitHyperplane(numPoints, points, &h);
  for(int i = 0; i < funcOfOthers; i++)
    result->coeffRef(i) = - h.coeffs()[i] / h.coeffs()[funcOfOthers];
  for(int i = funcOfOthers; i < size; i++)
    result->coeffRef(i) = - h.coeffs()[i+1] / h.coeffs()[funcOfOthers];
}

template<typename VectorType, typename HyperplaneType>
void fitHyperplane(int numPoints,
                   VectorType **points,
                   HyperplaneType *result,
                   typename NumTraits<typename VectorType::Scalar>::Real* soundness = 0)
{
  typedef typename VectorType::Scalar Scalar;
  typedef Matrix<Scalar,VectorType::SizeAtCompileTime,VectorType::SizeAtCompileTime> CovMatrixType;
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(VectorType)
  ei_assert(numPoints >= 1);
  int size = points[0]->size();
  ei_assert(size+1 == result->coeffs().size());

  
  VectorType mean = VectorType::Zero(size);
  for(int i = 0; i < numPoints; ++i)
    mean += *(points[i]);
  mean /= numPoints;

  
  CovMatrixType covMat = CovMatrixType::Zero(size, size);
  for(int i = 0; i < numPoints; ++i)
  {
    VectorType diff = (*(points[i]) - mean).conjugate();
    covMat += diff * diff.adjoint();
  }

  
  SelfAdjointEigenSolver<CovMatrixType> eig(covMat);
  result->normal() = eig.eigenvectors().col(0);
  if (soundness)
    *soundness = eig.eigenvalues().coeff(0)/eig.eigenvalues().coeff(1);

  
  
  result->offset() = - (result->normal().cwise()* mean).sum();
}

} 

#endif 
