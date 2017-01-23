// Copyright (C) 2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_STRIDE_H
#define EIGEN_STRIDE_H

namespace Eigen { 

template<int _OuterStrideAtCompileTime, int _InnerStrideAtCompileTime>
class Stride
{
  public:
    typedef DenseIndex Index;
    enum {
      InnerStrideAtCompileTime = _InnerStrideAtCompileTime,
      OuterStrideAtCompileTime = _OuterStrideAtCompileTime
    };

    
    Stride()
      : m_outer(OuterStrideAtCompileTime), m_inner(InnerStrideAtCompileTime)
    {
      eigen_assert(InnerStrideAtCompileTime != Dynamic && OuterStrideAtCompileTime != Dynamic);
    }

    
    Stride(Index outerStride, Index innerStride)
      : m_outer(outerStride), m_inner(innerStride)
    {
      eigen_assert(innerStride>=0 && outerStride>=0);
    }

    
    Stride(const Stride& other)
      : m_outer(other.outer()), m_inner(other.inner())
    {}

    
    inline Index outer() const { return m_outer.value(); }
    
    inline Index inner() const { return m_inner.value(); }

  protected:
    internal::variable_if_dynamic<Index, OuterStrideAtCompileTime> m_outer;
    internal::variable_if_dynamic<Index, InnerStrideAtCompileTime> m_inner;
};

template<int Value = Dynamic>
class InnerStride : public Stride<0, Value>
{
    typedef Stride<0, Value> Base;
  public:
    typedef DenseIndex Index;
    InnerStride() : Base() {}
    InnerStride(Index v) : Base(0, v) {}
};

template<int Value = Dynamic>
class OuterStride : public Stride<Value, 0>
{
    typedef Stride<Value, 0> Base;
  public:
    typedef DenseIndex Index;
    OuterStride() : Base() {}
    OuterStride(Index v) : Base(v,0) {}
};

} 

#endif 
