// Copyright (C) 2010-2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_TRANSPOSITIONS_H
#define EIGEN_TRANSPOSITIONS_H

namespace Eigen { 


namespace internal {
template<typename TranspositionType, typename MatrixType, int Side, bool Transposed=false> struct transposition_matrix_product_retval;
}

template<typename Derived>
class TranspositionsBase
{
    typedef internal::traits<Derived> Traits;
    
  public:

    typedef typename Traits::IndicesType IndicesType;
    typedef typename IndicesType::Scalar Index;

    Derived& derived() { return *static_cast<Derived*>(this); }
    const Derived& derived() const { return *static_cast<const Derived*>(this); }

    
    template<typename OtherDerived>
    Derived& operator=(const TranspositionsBase<OtherDerived>& other)
    {
      indices() = other.indices();
      return derived();
    }

    #ifndef EIGEN_PARSED_BY_DOXYGEN
    Derived& operator=(const TranspositionsBase& other)
    {
      indices() = other.indices();
      return derived();
    }
    #endif

    
    inline Index size() const { return indices().size(); }

    
    inline const Index& coeff(Index i) const { return indices().coeff(i); }
    
    inline Index& coeffRef(Index i) { return indices().coeffRef(i); }
    
    inline const Index& operator()(Index i) const { return indices()(i); }
    
    inline Index& operator()(Index i) { return indices()(i); }
    
    inline const Index& operator[](Index i) const { return indices()(i); }
    
    inline Index& operator[](Index i) { return indices()(i); }

    
    const IndicesType& indices() const { return derived().indices(); }
    
    IndicesType& indices() { return derived().indices(); }

    
    inline void resize(int newSize)
    {
      indices().resize(newSize);
    }

    
    void setIdentity()
    {
      for(int i = 0; i < indices().size(); ++i)
        coeffRef(i) = i;
    }

    
    
    

    
    inline Transpose<TranspositionsBase> inverse() const
    { return Transpose<TranspositionsBase>(derived()); }

    
    inline Transpose<TranspositionsBase> transpose() const
    { return Transpose<TranspositionsBase>(derived()); }

  protected:
};

namespace internal {
template<int SizeAtCompileTime, int MaxSizeAtCompileTime, typename IndexType>
struct traits<Transpositions<SizeAtCompileTime,MaxSizeAtCompileTime,IndexType> >
{
  typedef IndexType Index;
  typedef Matrix<Index, SizeAtCompileTime, 1, 0, MaxSizeAtCompileTime, 1> IndicesType;
};
}

template<int SizeAtCompileTime, int MaxSizeAtCompileTime, typename IndexType>
class Transpositions : public TranspositionsBase<Transpositions<SizeAtCompileTime,MaxSizeAtCompileTime,IndexType> >
{
    typedef internal::traits<Transpositions> Traits;
  public:

    typedef TranspositionsBase<Transpositions> Base;
    typedef typename Traits::IndicesType IndicesType;
    typedef typename IndicesType::Scalar Index;

    inline Transpositions() {}

    
    template<typename OtherDerived>
    inline Transpositions(const TranspositionsBase<OtherDerived>& other)
      : m_indices(other.indices()) {}

    #ifndef EIGEN_PARSED_BY_DOXYGEN
    inline Transpositions(const Transpositions& other) : m_indices(other.indices()) {}
    #endif

    
    template<typename Other>
    explicit inline Transpositions(const MatrixBase<Other>& a_indices) : m_indices(a_indices)
    {}

    
    template<typename OtherDerived>
    Transpositions& operator=(const TranspositionsBase<OtherDerived>& other)
    {
      return Base::operator=(other);
    }

    #ifndef EIGEN_PARSED_BY_DOXYGEN
    Transpositions& operator=(const Transpositions& other)
    {
      m_indices = other.m_indices;
      return *this;
    }
    #endif

    inline Transpositions(Index size) : m_indices(size)
    {}

    
    const IndicesType& indices() const { return m_indices; }
    
    IndicesType& indices() { return m_indices; }

  protected:

    IndicesType m_indices;
};


namespace internal {
template<int SizeAtCompileTime, int MaxSizeAtCompileTime, typename IndexType, int _PacketAccess>
struct traits<Map<Transpositions<SizeAtCompileTime,MaxSizeAtCompileTime,IndexType>,_PacketAccess> >
{
  typedef IndexType Index;
  typedef Map<const Matrix<Index,SizeAtCompileTime,1,0,MaxSizeAtCompileTime,1>, _PacketAccess> IndicesType;
};
}

template<int SizeAtCompileTime, int MaxSizeAtCompileTime, typename IndexType, int PacketAccess>
class Map<Transpositions<SizeAtCompileTime,MaxSizeAtCompileTime,IndexType>,PacketAccess>
 : public TranspositionsBase<Map<Transpositions<SizeAtCompileTime,MaxSizeAtCompileTime,IndexType>,PacketAccess> >
{
    typedef internal::traits<Map> Traits;
  public:

    typedef TranspositionsBase<Map> Base;
    typedef typename Traits::IndicesType IndicesType;
    typedef typename IndicesType::Scalar Index;

    inline Map(const Index* indicesPtr)
      : m_indices(indicesPtr)
    {}

    inline Map(const Index* indicesPtr, Index size)
      : m_indices(indicesPtr,size)
    {}

    
    template<typename OtherDerived>
    Map& operator=(const TranspositionsBase<OtherDerived>& other)
    {
      return Base::operator=(other);
    }

    #ifndef EIGEN_PARSED_BY_DOXYGEN
    Map& operator=(const Map& other)
    {
      m_indices = other.m_indices;
      return *this;
    }
    #endif

    
    const IndicesType& indices() const { return m_indices; }
    
    
    IndicesType& indices() { return m_indices; }

  protected:

    IndicesType m_indices;
};

namespace internal {
template<typename _IndicesType>
struct traits<TranspositionsWrapper<_IndicesType> >
{
  typedef typename _IndicesType::Scalar Index;
  typedef _IndicesType IndicesType;
};
}

template<typename _IndicesType>
class TranspositionsWrapper
 : public TranspositionsBase<TranspositionsWrapper<_IndicesType> >
{
    typedef internal::traits<TranspositionsWrapper> Traits;
  public:

    typedef TranspositionsBase<TranspositionsWrapper> Base;
    typedef typename Traits::IndicesType IndicesType;
    typedef typename IndicesType::Scalar Index;

    inline TranspositionsWrapper(IndicesType& a_indices)
      : m_indices(a_indices)
    {}

    
    template<typename OtherDerived>
    TranspositionsWrapper& operator=(const TranspositionsBase<OtherDerived>& other)
    {
      return Base::operator=(other);
    }

    #ifndef EIGEN_PARSED_BY_DOXYGEN
    TranspositionsWrapper& operator=(const TranspositionsWrapper& other)
    {
      m_indices = other.m_indices;
      return *this;
    }
    #endif

    
    const IndicesType& indices() const { return m_indices; }

    
    IndicesType& indices() { return m_indices; }

  protected:

    const typename IndicesType::Nested m_indices;
};

template<typename Derived, typename TranspositionsDerived>
inline const internal::transposition_matrix_product_retval<TranspositionsDerived, Derived, OnTheRight>
operator*(const MatrixBase<Derived>& matrix,
          const TranspositionsBase<TranspositionsDerived> &transpositions)
{
  return internal::transposition_matrix_product_retval
           <TranspositionsDerived, Derived, OnTheRight>
           (transpositions.derived(), matrix.derived());
}

template<typename Derived, typename TranspositionDerived>
inline const internal::transposition_matrix_product_retval
               <TranspositionDerived, Derived, OnTheLeft>
operator*(const TranspositionsBase<TranspositionDerived> &transpositions,
          const MatrixBase<Derived>& matrix)
{
  return internal::transposition_matrix_product_retval
           <TranspositionDerived, Derived, OnTheLeft>
           (transpositions.derived(), matrix.derived());
}

namespace internal {

template<typename TranspositionType, typename MatrixType, int Side, bool Transposed>
struct traits<transposition_matrix_product_retval<TranspositionType, MatrixType, Side, Transposed> >
{
  typedef typename MatrixType::PlainObject ReturnType;
};

template<typename TranspositionType, typename MatrixType, int Side, bool Transposed>
struct transposition_matrix_product_retval
 : public ReturnByValue<transposition_matrix_product_retval<TranspositionType, MatrixType, Side, Transposed> >
{
    typedef typename remove_all<typename MatrixType::Nested>::type MatrixTypeNestedCleaned;
    typedef typename TranspositionType::Index Index;

    transposition_matrix_product_retval(const TranspositionType& tr, const MatrixType& matrix)
      : m_transpositions(tr), m_matrix(matrix)
    {}

    inline int rows() const { return m_matrix.rows(); }
    inline int cols() const { return m_matrix.cols(); }

    template<typename Dest> inline void evalTo(Dest& dst) const
    {
      const int size = m_transpositions.size();
      Index j = 0;

      if(!(is_same<MatrixTypeNestedCleaned,Dest>::value && extract_data(dst) == extract_data(m_matrix)))
        dst = m_matrix;

      for(int k=(Transposed?size-1:0) ; Transposed?k>=0:k<size ; Transposed?--k:++k)
        if((j=m_transpositions.coeff(k))!=k)
        {
          if(Side==OnTheLeft)
            dst.row(k).swap(dst.row(j));
          else if(Side==OnTheRight)
            dst.col(k).swap(dst.col(j));
        }
    }

  protected:
    const TranspositionType& m_transpositions;
    typename MatrixType::Nested m_matrix;
};

} 


template<typename TranspositionsDerived>
class Transpose<TranspositionsBase<TranspositionsDerived> >
{
    typedef TranspositionsDerived TranspositionType;
    typedef typename TranspositionType::IndicesType IndicesType;
  public:

    Transpose(const TranspositionType& t) : m_transpositions(t) {}

    inline int size() const { return m_transpositions.size(); }

    template<typename Derived> friend
    inline const internal::transposition_matrix_product_retval<TranspositionType, Derived, OnTheRight, true>
    operator*(const MatrixBase<Derived>& matrix, const Transpose& trt)
    {
      return internal::transposition_matrix_product_retval<TranspositionType, Derived, OnTheRight, true>(trt.m_transpositions, matrix.derived());
    }

    template<typename Derived>
    inline const internal::transposition_matrix_product_retval<TranspositionType, Derived, OnTheLeft, true>
    operator*(const MatrixBase<Derived>& matrix) const
    {
      return internal::transposition_matrix_product_retval<TranspositionType, Derived, OnTheLeft, true>(m_transpositions, matrix.derived());
    }

  protected:
    const TranspositionType& m_transpositions;
};

} 

#endif 
