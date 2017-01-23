// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_AMBIVECTOR_H
#define EIGEN_AMBIVECTOR_H

namespace Eigen { 

namespace internal {

template<typename _Scalar, typename _Index>
class AmbiVector
{
  public:
    typedef _Scalar Scalar;
    typedef _Index Index;
    typedef typename NumTraits<Scalar>::Real RealScalar;

    AmbiVector(Index size)
      : m_buffer(0), m_zero(0), m_size(0), m_allocatedSize(0), m_allocatedElements(0), m_mode(-1)
    {
      resize(size);
    }

    void init(double estimatedDensity);
    void init(int mode);

    Index nonZeros() const;

    
    void setBounds(Index start, Index end) { m_start = start; m_end = end; }

    void setZero();

    void restart();
    Scalar& coeffRef(Index i);
    Scalar& coeff(Index i);

    class Iterator;

    ~AmbiVector() { delete[] m_buffer; }

    void resize(Index size)
    {
      if (m_allocatedSize < size)
        reallocate(size);
      m_size = size;
    }

    Index size() const { return m_size; }

  protected:

    void reallocate(Index size)
    {
      
      
      delete[] m_buffer;
      if (size<1000)
      {
        Index allocSize = (size * sizeof(ListEl) + sizeof(Scalar) - 1)/sizeof(Scalar);
        m_allocatedElements = (allocSize*sizeof(Scalar))/sizeof(ListEl);
        m_buffer = new Scalar[allocSize];
      }
      else
      {
        m_allocatedElements = (size*sizeof(Scalar))/sizeof(ListEl);
        m_buffer = new Scalar[size];
      }
      m_size = size;
      m_start = 0;
      m_end = m_size;
    }

    void reallocateSparse()
    {
      Index copyElements = m_allocatedElements;
      m_allocatedElements = (std::min)(Index(m_allocatedElements*1.5),m_size);
      Index allocSize = m_allocatedElements * sizeof(ListEl);
      allocSize = (allocSize + sizeof(Scalar) - 1)/sizeof(Scalar);
      Scalar* newBuffer = new Scalar[allocSize];
      memcpy(newBuffer,  m_buffer,  copyElements * sizeof(ListEl));
      delete[] m_buffer;
      m_buffer = newBuffer;
    }

  protected:
    
    struct ListEl
    {
      Index next;
      Index index;
      Scalar value;
    };

    
    Scalar* m_buffer;
    Scalar m_zero;
    Index m_size;
    Index m_start;
    Index m_end;
    Index m_allocatedSize;
    Index m_allocatedElements;
    Index m_mode;

    
    Index m_llStart;
    Index m_llCurrent;
    Index m_llSize;
};

template<typename _Scalar,typename _Index>
_Index AmbiVector<_Scalar,_Index>::nonZeros() const
{
  if (m_mode==IsSparse)
    return m_llSize;
  else
    return m_end - m_start;
}

template<typename _Scalar,typename _Index>
void AmbiVector<_Scalar,_Index>::init(double estimatedDensity)
{
  if (estimatedDensity>0.1)
    init(IsDense);
  else
    init(IsSparse);
}

template<typename _Scalar,typename _Index>
void AmbiVector<_Scalar,_Index>::init(int mode)
{
  m_mode = mode;
  if (m_mode==IsSparse)
  {
    m_llSize = 0;
    m_llStart = -1;
  }
}

template<typename _Scalar,typename _Index>
void AmbiVector<_Scalar,_Index>::restart()
{
  m_llCurrent = m_llStart;
}

template<typename _Scalar,typename _Index>
void AmbiVector<_Scalar,_Index>::setZero()
{
  if (m_mode==IsDense)
  {
    for (Index i=m_start; i<m_end; ++i)
      m_buffer[i] = Scalar(0);
  }
  else
  {
    eigen_assert(m_mode==IsSparse);
    m_llSize = 0;
    m_llStart = -1;
  }
}

template<typename _Scalar,typename _Index>
_Scalar& AmbiVector<_Scalar,_Index>::coeffRef(_Index i)
{
  if (m_mode==IsDense)
    return m_buffer[i];
  else
  {
    ListEl* EIGEN_RESTRICT llElements = reinterpret_cast<ListEl*>(m_buffer);
    
    eigen_assert(m_mode==IsSparse);
    if (m_llSize==0)
    {
      
      m_llStart = 0;
      m_llCurrent = 0;
      ++m_llSize;
      llElements[0].value = Scalar(0);
      llElements[0].index = i;
      llElements[0].next = -1;
      return llElements[0].value;
    }
    else if (i<llElements[m_llStart].index)
    {
      
      ListEl& el = llElements[m_llSize];
      el.value = Scalar(0);
      el.index = i;
      el.next = m_llStart;
      m_llStart = m_llSize;
      ++m_llSize;
      m_llCurrent = m_llStart;
      return el.value;
    }
    else
    {
      Index nextel = llElements[m_llCurrent].next;
      eigen_assert(i>=llElements[m_llCurrent].index && "you must call restart() before inserting an element with lower or equal index");
      while (nextel >= 0 && llElements[nextel].index<=i)
      {
        m_llCurrent = nextel;
        nextel = llElements[nextel].next;
      }

      if (llElements[m_llCurrent].index==i)
      {
        
        return llElements[m_llCurrent].value;
      }
      else
      {
        if (m_llSize>=m_allocatedElements)
        {
          reallocateSparse();
          llElements = reinterpret_cast<ListEl*>(m_buffer);
        }
        eigen_internal_assert(m_llSize<m_allocatedElements && "internal error: overflow in sparse mode");
        
        ListEl& el = llElements[m_llSize];
        el.value = Scalar(0);
        el.index = i;
        el.next = llElements[m_llCurrent].next;
        llElements[m_llCurrent].next = m_llSize;
        ++m_llSize;
        return el.value;
      }
    }
  }
}

template<typename _Scalar,typename _Index>
_Scalar& AmbiVector<_Scalar,_Index>::coeff(_Index i)
{
  if (m_mode==IsDense)
    return m_buffer[i];
  else
  {
    ListEl* EIGEN_RESTRICT llElements = reinterpret_cast<ListEl*>(m_buffer);
    eigen_assert(m_mode==IsSparse);
    if ((m_llSize==0) || (i<llElements[m_llStart].index))
    {
      return m_zero;
    }
    else
    {
      Index elid = m_llStart;
      while (elid >= 0 && llElements[elid].index<i)
        elid = llElements[elid].next;

      if (llElements[elid].index==i)
        return llElements[m_llCurrent].value;
      else
        return m_zero;
    }
  }
}

template<typename _Scalar,typename _Index>
class AmbiVector<_Scalar,_Index>::Iterator
{
  public:
    typedef _Scalar Scalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;

    Iterator(const AmbiVector& vec, const RealScalar& epsilon = 0)
      : m_vector(vec)
    {
      using std::abs;
      m_epsilon = epsilon;
      m_isDense = m_vector.m_mode==IsDense;
      if (m_isDense)
      {
        m_currentEl = 0;   
        m_cachedValue = 0; 
        m_cachedIndex = m_vector.m_start-1;
        ++(*this);
      }
      else
      {
        ListEl* EIGEN_RESTRICT llElements = reinterpret_cast<ListEl*>(m_vector.m_buffer);
        m_currentEl = m_vector.m_llStart;
        while (m_currentEl>=0 && abs(llElements[m_currentEl].value)<=m_epsilon)
          m_currentEl = llElements[m_currentEl].next;
        if (m_currentEl<0)
        {
          m_cachedValue = 0; 
          m_cachedIndex = -1;
        }
        else
        {
          m_cachedIndex = llElements[m_currentEl].index;
          m_cachedValue = llElements[m_currentEl].value;
        }
      }
    }

    Index index() const { return m_cachedIndex; }
    Scalar value() const { return m_cachedValue; }

    operator bool() const { return m_cachedIndex>=0; }

    Iterator& operator++()
    {
      using std::abs;
      if (m_isDense)
      {
        do {
          ++m_cachedIndex;
        } while (m_cachedIndex<m_vector.m_end && abs(m_vector.m_buffer[m_cachedIndex])<m_epsilon);
        if (m_cachedIndex<m_vector.m_end)
          m_cachedValue = m_vector.m_buffer[m_cachedIndex];
        else
          m_cachedIndex=-1;
      }
      else
      {
        ListEl* EIGEN_RESTRICT llElements = reinterpret_cast<ListEl*>(m_vector.m_buffer);
        do {
          m_currentEl = llElements[m_currentEl].next;
        } while (m_currentEl>=0 && abs(llElements[m_currentEl].value)<m_epsilon);
        if (m_currentEl<0)
        {
          m_cachedIndex = -1;
        }
        else
        {
          m_cachedIndex = llElements[m_currentEl].index;
          m_cachedValue = llElements[m_currentEl].value;
        }
      }
      return *this;
    }

  protected:
    const AmbiVector& m_vector; 
    Index m_currentEl;            
    RealScalar m_epsilon;       
    Index m_cachedIndex;          
    Scalar m_cachedValue;       
    bool m_isDense;             
};

} 

} 

#endif 
