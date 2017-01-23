// Copyright (C) 2009-2011 Jitse Niesen <jitse@maths.leeds.ac.uk>
// Public License v. 2.0. If a copy of the MPL was not distributed

#include "main.h"
#include <unsupported/Eigen/MatrixFunctions>

template <typename MatrixType, int IsComplex = NumTraits<typename internal::traits<MatrixType>::Scalar>::IsComplex>
struct generateTestMatrix;

template <typename MatrixType>
struct generateTestMatrix<MatrixType,0>
{
  static void run(MatrixType& result, typename MatrixType::Index size)
  {
    MatrixType mat = MatrixType::Random(size, size);
    EigenSolver<MatrixType> es(mat);
    typename EigenSolver<MatrixType>::EigenvalueType eivals = es.eigenvalues();
    for (typename MatrixType::Index i = 0; i < size; ++i) {
      if (eivals(i).imag() == 0 && eivals(i).real() < 0)
	eivals(i) = -eivals(i);
    }
    result = (es.eigenvectors() * eivals.asDiagonal() * es.eigenvectors().inverse()).real();
  }
};

template <typename MatrixType>
struct generateTestMatrix<MatrixType,1>
{
  static void run(MatrixType& result, typename MatrixType::Index size)
  {
    result = MatrixType::Random(size, size);
  }
};

template <typename Derived, typename OtherDerived>
double relerr(const MatrixBase<Derived>& A, const MatrixBase<OtherDerived>& B)
{
  return std::sqrt((A - B).cwiseAbs2().sum() / (std::min)(A.cwiseAbs2().sum(), B.cwiseAbs2().sum()));
}
