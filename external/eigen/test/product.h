// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#include "main.h"
#include <Eigen/QR>

template<typename Derived1, typename Derived2>
bool areNotApprox(const MatrixBase<Derived1>& m1, const MatrixBase<Derived2>& m2, typename Derived1::RealScalar epsilon = NumTraits<typename Derived1::RealScalar>::dummy_precision())
{
  return !((m1-m2).cwiseAbs2().maxCoeff() < epsilon * epsilon
                          * (std::max)(m1.cwiseAbs2().maxCoeff(), m2.cwiseAbs2().maxCoeff()));
}

template<typename MatrixType> void product(const MatrixType& m)
{
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::Scalar Scalar;
  typedef Matrix<Scalar, MatrixType::RowsAtCompileTime, 1> RowVectorType;
  typedef Matrix<Scalar, MatrixType::ColsAtCompileTime, 1> ColVectorType;
  typedef Matrix<Scalar, MatrixType::RowsAtCompileTime, MatrixType::RowsAtCompileTime> RowSquareMatrixType;
  typedef Matrix<Scalar, MatrixType::ColsAtCompileTime, MatrixType::ColsAtCompileTime> ColSquareMatrixType;
  typedef Matrix<Scalar, MatrixType::RowsAtCompileTime, MatrixType::ColsAtCompileTime,
                         MatrixType::Flags&RowMajorBit?ColMajor:RowMajor> OtherMajorMatrixType;

  Index rows = m.rows();
  Index cols = m.cols();

  
  
  MatrixType m1 = MatrixType::Random(rows, cols),
             m2 = MatrixType::Random(rows, cols),
             m3(rows, cols);
  RowSquareMatrixType
             identity = RowSquareMatrixType::Identity(rows, rows),
             square = RowSquareMatrixType::Random(rows, rows),
             res = RowSquareMatrixType::Random(rows, rows);
  ColSquareMatrixType
             square2 = ColSquareMatrixType::Random(cols, cols),
             res2 = ColSquareMatrixType::Random(cols, cols);
  RowVectorType v1 = RowVectorType::Random(rows);
  ColVectorType vc2 = ColVectorType::Random(cols), vcres(cols);
  OtherMajorMatrixType tm1 = m1;

  Scalar s1 = internal::random<Scalar>();

  Index r  = internal::random<Index>(0, rows-1),
        c  = internal::random<Index>(0, cols-1),
        c2 = internal::random<Index>(0, cols-1);

  
  
  VERIFY_IS_APPROX((m1*m1.transpose())*m2,  m1*(m1.transpose()*m2));
  m3 = m1;
  m3 *= m1.transpose() * m2;
  VERIFY_IS_APPROX(m3,                      m1 * (m1.transpose()*m2));
  VERIFY_IS_APPROX(m3,                      m1 * (m1.transpose()*m2));

  
  VERIFY_IS_APPROX(square*(m1 + m2),        square*m1+square*m2);
  VERIFY_IS_APPROX(square*(m1 - m2),        square*m1-square*m2);

  
  VERIFY_IS_APPROX(s1*(square*m1),          (s1*square)*m1);
  VERIFY_IS_APPROX(s1*(square*m1),          square*(m1*s1));

  
  VERIFY_IS_APPROX(v1,                      identity*v1);
  VERIFY_IS_APPROX(v1.transpose(),          v1.transpose() * identity);
  
  VERIFY_IS_APPROX(MatrixType::Identity(rows, cols)(r,c), static_cast<Scalar>(r==c));

  if (rows!=cols)
     VERIFY_RAISES_ASSERT(m3 = m1*m1);

  
  
  if (!NumTraits<Scalar>::IsInteger && (std::min)(rows,cols)>1)
  {
    VERIFY(areNotApprox(m1.transpose()*m2,m2.transpose()*m1));
  }

  
  res = square;
  res.noalias() += m1 * m2.transpose();
  VERIFY_IS_APPROX(res, square + m1 * m2.transpose());
  if (!NumTraits<Scalar>::IsInteger && (std::min)(rows,cols)>1)
  {
    VERIFY(areNotApprox(res,square + m2 * m1.transpose()));
  }
  vcres = vc2;
  vcres.noalias() += m1.transpose() * v1;
  VERIFY_IS_APPROX(vcres, vc2 + m1.transpose() * v1);

  
  res = square;
  res.noalias() -= m1 * m2.transpose();
  VERIFY_IS_APPROX(res, square - (m1 * m2.transpose()));
  if (!NumTraits<Scalar>::IsInteger && (std::min)(rows,cols)>1)
  {
    VERIFY(areNotApprox(res,square - m2 * m1.transpose()));
  }
  vcres = vc2;
  vcres.noalias() -= m1.transpose() * v1;
  VERIFY_IS_APPROX(vcres, vc2 - m1.transpose() * v1);

  tm1 = m1;
  VERIFY_IS_APPROX(tm1.transpose() * v1, m1.transpose() * v1);
  VERIFY_IS_APPROX(v1.transpose() * tm1, v1.transpose() * m1);

  
  for (int i=0; i<rows; ++i)
    res.row(i) = m1.row(i) * m2.transpose();
  VERIFY_IS_APPROX(res, m1 * m2.transpose());
  
  for (int i=0; i<rows; ++i)
    res.col(i) = m1 * m2.transpose().col(i);
  VERIFY_IS_APPROX(res, m1 * m2.transpose());

  res2 = square2;
  res2.noalias() += m1.transpose() * m2;
  VERIFY_IS_APPROX(res2, square2 + m1.transpose() * m2);
  if (!NumTraits<Scalar>::IsInteger && (std::min)(rows,cols)>1)
  {
    VERIFY(areNotApprox(res2,square2 + m2.transpose() * m1));
  }

  VERIFY_IS_APPROX(res.col(r).noalias() = square.adjoint() * square.col(r), (square.adjoint() * square.col(r)).eval());
  VERIFY_IS_APPROX(res.col(r).noalias() = square * square.col(r), (square * square.col(r)).eval());

  
  Scalar x = square2.row(c) * square2.col(c2);
  VERIFY_IS_APPROX(x, square2.row(c).transpose().cwiseProduct(square2.col(c2)).sum());
  
  
  VERIFY_IS_APPROX(m1.col(c) * m1.row(r), m1.block(0,c,rows,1) * m1.block(r,0,1,cols));
  VERIFY_IS_APPROX(m1.row(r).transpose() * m1.col(c).transpose(), m1.block(r,0,1,cols).transpose() * m1.block(0,c,rows,1).transpose());
  VERIFY_IS_APPROX(m1.block(0,c,rows,1) * m1.row(r), m1.block(0,c,rows,1) * m1.block(r,0,1,cols));
  VERIFY_IS_APPROX(m1.col(c) * m1.block(r,0,1,cols), m1.block(0,c,rows,1) * m1.block(r,0,1,cols));
  VERIFY_IS_APPROX(m1.leftCols(1) * m1.row(r), m1.block(0,0,rows,1) * m1.block(r,0,1,cols));
  VERIFY_IS_APPROX(m1.col(c) * m1.topRows(1), m1.block(0,c,rows,1) * m1.block(0,0,1,cols));  
}
