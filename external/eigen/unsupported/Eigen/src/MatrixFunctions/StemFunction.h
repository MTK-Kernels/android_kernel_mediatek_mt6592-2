// Copyright (C) 2010 Jitse Niesen <jitse@maths.leeds.ac.uk>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_STEM_FUNCTION
#define EIGEN_STEM_FUNCTION

namespace Eigen { 

template <typename Scalar>
class StdStemFunctions
{
  public:

    
    static Scalar exp(Scalar x, int)
    {
      return std::exp(x);
    }

    
    static Scalar cos(Scalar x, int n)
    {
      Scalar res;
      switch (n % 4) {
      case 0: 
	res = std::cos(x);
	break;
      case 1:
	res = -std::sin(x);
	break;
      case 2:
	res = -std::cos(x);
	break;
      case 3:
	res = std::sin(x);
	break;
      }
      return res;
    }

    
    static Scalar sin(Scalar x, int n)
    {
      Scalar res;
      switch (n % 4) {
      case 0:
	res = std::sin(x);
	break;
      case 1:
	res = std::cos(x);
	break;
      case 2:
	res = -std::sin(x);
	break;
      case 3:
	res = -std::cos(x);
	break;
      }
      return res;
    }

    
    static Scalar cosh(Scalar x, int n)
    {
      Scalar res;
      switch (n % 2) {
      case 0:
	res = std::cosh(x);
	break;
      case 1:
	res = std::sinh(x);
	break;
      }
      return res;
    }
	
    
    static Scalar sinh(Scalar x, int n)
    {
      Scalar res;
      switch (n % 2) {
      case 0:
	res = std::sinh(x);
	break;
      case 1:
	res = std::cosh(x);
	break;
      }
      return res;
    }

}; 

} 

#endif 
