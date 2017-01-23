// Copyright (C) 20010-2011 Hauke Heibel <hauke.heibel@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SPLINE_FITTING_H
#define EIGEN_SPLINE_FITTING_H

#include <numeric>

#include "SplineFwd.h"

#include <Eigen/QR>

namespace Eigen
{
  template <typename KnotVectorType>
  void KnotAveraging(const KnotVectorType& parameters, DenseIndex degree, KnotVectorType& knots)
  {
    knots.resize(parameters.size()+degree+1);      

    for (DenseIndex j=1; j<parameters.size()-degree; ++j)
      knots(j+degree) = parameters.segment(j,degree).mean();

    knots.segment(0,degree+1) = KnotVectorType::Zero(degree+1);
    knots.segment(knots.size()-degree-1,degree+1) = KnotVectorType::Ones(degree+1);
  }

   
  template <typename PointArrayType, typename KnotVectorType>
  void ChordLengths(const PointArrayType& pts, KnotVectorType& chord_lengths)
  {
    typedef typename KnotVectorType::Scalar Scalar;

    const DenseIndex n = pts.cols();

    
    chord_lengths.resize(pts.cols());
    chord_lengths[0] = 0;
    chord_lengths.rightCols(n-1) = (pts.array().leftCols(n-1) - pts.array().rightCols(n-1)).matrix().colwise().norm();

    
    std::partial_sum(chord_lengths.data(), chord_lengths.data()+n, chord_lengths.data());

    
    chord_lengths /= chord_lengths(n-1);
    chord_lengths(n-1) = Scalar(1);
  }

     
  template <typename SplineType>
  struct SplineFitting
  {
    typedef typename SplineType::KnotVectorType KnotVectorType;

    template <typename PointArrayType>
    static SplineType Interpolate(const PointArrayType& pts, DenseIndex degree);

    template <typename PointArrayType>
    static SplineType Interpolate(const PointArrayType& pts, DenseIndex degree, const KnotVectorType& knot_parameters);
  };

  template <typename SplineType>
  template <typename PointArrayType>
  SplineType SplineFitting<SplineType>::Interpolate(const PointArrayType& pts, DenseIndex degree, const KnotVectorType& knot_parameters)
  {
    typedef typename SplineType::KnotVectorType::Scalar Scalar;      
    typedef typename SplineType::ControlPointVectorType ControlPointVectorType;      

    typedef Matrix<Scalar,Dynamic,Dynamic> MatrixType;

    KnotVectorType knots;
    KnotAveraging(knot_parameters, degree, knots);

    DenseIndex n = pts.cols();
    MatrixType A = MatrixType::Zero(n,n);
    for (DenseIndex i=1; i<n-1; ++i)
    {
      const DenseIndex span = SplineType::Span(knot_parameters[i], degree, knots);

      
      A.row(i).segment(span-degree, degree+1) = SplineType::BasisFunctions(knot_parameters[i], degree, knots);
    }
    A(0,0) = 1.0;
    A(n-1,n-1) = 1.0;

    HouseholderQR<MatrixType> qr(A);

    
    ControlPointVectorType ctrls = qr.solve(MatrixType(pts.transpose())).transpose();

    return SplineType(knots, ctrls);
  }

  template <typename SplineType>
  template <typename PointArrayType>
  SplineType SplineFitting<SplineType>::Interpolate(const PointArrayType& pts, DenseIndex degree)
  {
    KnotVectorType chord_lengths; 
    ChordLengths(pts, chord_lengths);
    return Interpolate(pts, degree, chord_lengths);
  }
}

#endif 
