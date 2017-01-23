#ifndef EIGEN_WARNINGS_DISABLED
#define EIGEN_WARNINGS_DISABLED

#ifdef _MSC_VER
  
  
  
  
  
  
  
  
  
  
  
  
  #ifndef EIGEN_PERMANENTLY_DISABLE_STUPID_WARNINGS
    #pragma warning( push )
  #endif
  #pragma warning( disable : 4100 4101 4127 4181 4211 4244 4273 4324 4512 4522 4700 4717 )
#elif defined __INTEL_COMPILER
  
  
  
  
  
  #ifndef EIGEN_PERMANENTLY_DISABLE_STUPID_WARNINGS
    #pragma warning push
  #endif
  #pragma warning disable 2196 279
#elif defined __clang__
  
  
  #ifndef EIGEN_PERMANENTLY_DISABLE_STUPID_WARNINGS
    #pragma clang diagnostic push
  #endif
  #pragma clang diagnostic ignored "-Wconstant-logical-operand"
#endif

#endif 
