// Copyright (C) 20010-2011 Hauke Heibel <hauke.heibel@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SPLINES_FWD_H
#define EIGEN_SPLINES_FWD_H

#include <Eigen/Core>

namespace Eigen
{
    template <typename Scalar, int Dim, int Degree = Dynamic> class Spline;

    template < typename SplineType, int DerivativeOrder = Dynamic > struct SplineTraits {};

    template <typename _Scalar, int _Dim, int _Degree>
    struct SplineTraits< Spline<_Scalar, _Dim, _Degree>, Dynamic >
    {
      typedef _Scalar Scalar; 
      enum { Dimension = _Dim  };
      enum { Degree = _Degree  };

      enum { OrderAtCompileTime = _Degree==Dynamic ? Dynamic : _Degree+1  };
      enum { NumOfDerivativesAtCompileTime = OrderAtCompileTime  };

      
      typedef Array<Scalar,1,OrderAtCompileTime> BasisVectorType;

      
      typedef Array<Scalar,Dynamic,Dynamic,RowMajor,NumOfDerivativesAtCompileTime,OrderAtCompileTime> BasisDerivativeType;
      
      
      typedef Array<Scalar,Dimension,Dynamic,ColMajor,Dimension,NumOfDerivativesAtCompileTime> DerivativeType;

      
      typedef Array<Scalar,Dimension,1> PointType;
      
      
      typedef Array<Scalar,1,Dynamic> KnotVectorType;
      
      
      typedef Array<Scalar,Dimension,Dynamic> ControlPointVectorType;
    };

    template < typename _Scalar, int _Dim, int _Degree, int _DerivativeOrder >
    struct SplineTraits< Spline<_Scalar, _Dim, _Degree>, _DerivativeOrder > : public SplineTraits< Spline<_Scalar, _Dim, _Degree> >
    {
      enum { OrderAtCompileTime = _Degree==Dynamic ? Dynamic : _Degree+1  };
      enum { NumOfDerivativesAtCompileTime = _DerivativeOrder==Dynamic ? Dynamic : _DerivativeOrder+1  };

      
      typedef Array<_Scalar,Dynamic,Dynamic,RowMajor,NumOfDerivativesAtCompileTime,OrderAtCompileTime> BasisDerivativeType;
      
            
      typedef Array<_Scalar,_Dim,Dynamic,ColMajor,_Dim,NumOfDerivativesAtCompileTime> DerivativeType;
    };

    
    typedef Spline<float,2> Spline2f;
    
    
    typedef Spline<float,3> Spline3f;

    
    typedef Spline<double,2> Spline2d;
    
    
    typedef Spline<double,3> Spline3d;
}

#endif 
