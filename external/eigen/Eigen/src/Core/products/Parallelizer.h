// Copyright (C) 2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_PARALLELIZER_H
#define EIGEN_PARALLELIZER_H

namespace Eigen { 

namespace internal {

inline void manage_multi_threading(Action action, int* v)
{
  static EIGEN_UNUSED int m_maxThreads = -1;

  if(action==SetAction)
  {
    eigen_internal_assert(v!=0);
    m_maxThreads = *v;
  }
  else if(action==GetAction)
  {
    eigen_internal_assert(v!=0);
    #ifdef EIGEN_HAS_OPENMP
    if(m_maxThreads>0)
      *v = m_maxThreads;
    else
      *v = omp_get_max_threads();
    #else
    *v = 1;
    #endif
  }
  else
  {
    eigen_internal_assert(false);
  }
}

}

inline void initParallel()
{
  int nbt;
  internal::manage_multi_threading(GetAction, &nbt);
  std::ptrdiff_t l1, l2;
  internal::manage_caching_sizes(GetAction, &l1, &l2);
}

inline int nbThreads()
{
  int ret;
  internal::manage_multi_threading(GetAction, &ret);
  return ret;
}

inline void setNbThreads(int v)
{
  internal::manage_multi_threading(SetAction, &v);
}

namespace internal {

template<typename Index> struct GemmParallelInfo
{
  GemmParallelInfo() : sync(-1), users(0), rhs_start(0), rhs_length(0) {}

  int volatile sync;
  int volatile users;

  Index rhs_start;
  Index rhs_length;
};

template<bool Condition, typename Functor, typename Index>
void parallelize_gemm(const Functor& func, Index rows, Index cols, bool transpose)
{
  
  
#if !(defined (EIGEN_HAS_OPENMP)) || defined (EIGEN_USE_BLAS)
  
  
  
  
  EIGEN_UNUSED_VARIABLE(transpose);
  func(0,rows, 0,cols);
#else

  
  
  
  
  

  
  
  if((!Condition) || (omp_get_num_threads()>1))
    return func(0,rows, 0,cols);

  Index size = transpose ? cols : rows;

  
  
  Index max_threads = std::max<Index>(1,size / 32);

  
  Index threads = std::min<Index>(nbThreads(), max_threads);

  if(threads==1)
    return func(0,rows, 0,cols);

  Eigen::initParallel();
  func.initParallelSession();

  if(transpose)
    std::swap(rows,cols);

  Index blockCols = (cols / threads) & ~Index(0x3);
  Index blockRows = (rows / threads) & ~Index(0x7);
  
  GemmParallelInfo<Index>* info = new GemmParallelInfo<Index>[threads];

  #pragma omp parallel for schedule(static,1) num_threads(threads)
  for(Index i=0; i<threads; ++i)
  {
    Index r0 = i*blockRows;
    Index actualBlockRows = (i+1==threads) ? rows-r0 : blockRows;

    Index c0 = i*blockCols;
    Index actualBlockCols = (i+1==threads) ? cols-c0 : blockCols;

    info[i].rhs_start = c0;
    info[i].rhs_length = actualBlockCols;

    if(transpose)
      func(0, cols, r0, actualBlockRows, info);
    else
      func(r0, actualBlockRows, 0,cols, info);
  }

  delete[] info;
#endif
}

} 

} 

#endif 
