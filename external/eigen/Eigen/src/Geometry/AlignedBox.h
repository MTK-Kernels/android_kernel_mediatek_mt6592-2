// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ALIGNEDBOX_H
#define EIGEN_ALIGNEDBOX_H

namespace Eigen { 

template <typename _Scalar, int _AmbientDim>
class AlignedBox
{
public:
EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_AmbientDim)
  enum { AmbientDimAtCompileTime = _AmbientDim };
  typedef _Scalar                                   Scalar;
  typedef NumTraits<Scalar>                         ScalarTraits;
  typedef DenseIndex                                Index;
  typedef typename ScalarTraits::Real               RealScalar;
  typedef typename ScalarTraits::NonInteger      NonInteger;
  typedef Matrix<Scalar,AmbientDimAtCompileTime,1>  VectorType;

  
  enum CornerType
  {
    
    Min=0, Max=1,

    
    BottomLeft=0, BottomRight=1,
    TopLeft=2, TopRight=3,

    
    BottomLeftFloor=0, BottomRightFloor=1,
    TopLeftFloor=2, TopRightFloor=3,
    BottomLeftCeil=4, BottomRightCeil=5,
    TopLeftCeil=6, TopRightCeil=7
  };


  
  inline AlignedBox()
  { if (AmbientDimAtCompileTime!=Dynamic) setEmpty(); }

  
  inline explicit AlignedBox(Index _dim) : m_min(_dim), m_max(_dim)
  { setEmpty(); }

  
  template<typename OtherVectorType1, typename OtherVectorType2>
  inline AlignedBox(const OtherVectorType1& _min, const OtherVectorType2& _max) : m_min(_min), m_max(_max) {}

  
  template<typename Derived>
  inline explicit AlignedBox(const MatrixBase<Derived>& a_p)
  {
    typename internal::nested<Derived,2>::type p(a_p.derived());
    m_min = p;
    m_max = p;
  }

  ~AlignedBox() {}

  
  inline Index dim() const { return AmbientDimAtCompileTime==Dynamic ? m_min.size() : Index(AmbientDimAtCompileTime); }

  
  inline bool isNull() const { return isEmpty(); }

  
  inline void setNull() { setEmpty(); }

  
  inline bool isEmpty() const { return (m_min.array() > m_max.array()).any(); }

  
  inline void setEmpty()
  {
    m_min.setConstant( ScalarTraits::highest() );
    m_max.setConstant( ScalarTraits::lowest() );
  }

  
  inline const VectorType& (min)() const { return m_min; }
  
  inline VectorType& (min)() { return m_min; }
  
  inline const VectorType& (max)() const { return m_max; }
  
  inline VectorType& (max)() { return m_max; }

  
  inline const CwiseUnaryOp<internal::scalar_quotient1_op<Scalar>,
                            const CwiseBinaryOp<internal::scalar_sum_op<Scalar>, const VectorType, const VectorType> >
  center() const
  { return (m_min+m_max)/2; }

  inline const CwiseBinaryOp< internal::scalar_difference_op<Scalar>, const VectorType, const VectorType> sizes() const
  { return m_max - m_min; }

  
  inline Scalar volume() const
  { return sizes().prod(); }

  inline CwiseBinaryOp< internal::scalar_difference_op<Scalar>, const VectorType, const VectorType> diagonal() const
  { return sizes(); }

  inline VectorType corner(CornerType corner) const
  {
    EIGEN_STATIC_ASSERT(_AmbientDim <= 3, THIS_METHOD_IS_ONLY_FOR_VECTORS_OF_A_SPECIFIC_SIZE);

    VectorType res;

    Index mult = 1;
    for(Index d=0; d<dim(); ++d)
    {
      if( mult & corner ) res[d] = m_max[d];
      else                res[d] = m_min[d];
      mult *= 2;
    }
    return res;
  }

  inline VectorType sample() const
  {
    VectorType r;
    for(Index d=0; d<dim(); ++d)
    {
      if(!ScalarTraits::IsInteger)
      {
        r[d] = m_min[d] + (m_max[d]-m_min[d])
             * internal::random<Scalar>(Scalar(0), Scalar(1));
      }
      else
        r[d] = internal::random(m_min[d], m_max[d]);
    }
    return r;
  }

  
  template<typename Derived>
  inline bool contains(const MatrixBase<Derived>& a_p) const
  {
    typename internal::nested<Derived,2>::type p(a_p.derived());
    return (m_min.array()<=p.array()).all() && (p.array()<=m_max.array()).all();
  }

  
  inline bool contains(const AlignedBox& b) const
  { return (m_min.array()<=(b.min)().array()).all() && ((b.max)().array()<=m_max.array()).all(); }

  
  template<typename Derived>
  inline AlignedBox& extend(const MatrixBase<Derived>& a_p)
  {
    typename internal::nested<Derived,2>::type p(a_p.derived());
    m_min = m_min.cwiseMin(p);
    m_max = m_max.cwiseMax(p);
    return *this;
  }

  
  inline AlignedBox& extend(const AlignedBox& b)
  {
    m_min = m_min.cwiseMin(b.m_min);
    m_max = m_max.cwiseMax(b.m_max);
    return *this;
  }

  
  inline AlignedBox& clamp(const AlignedBox& b)
  {
    m_min = m_min.cwiseMax(b.m_min);
    m_max = m_max.cwiseMin(b.m_max);
    return *this;
  }

  
  inline AlignedBox intersection(const AlignedBox& b) const
  {return AlignedBox(m_min.cwiseMax(b.m_min), m_max.cwiseMin(b.m_max)); }

  
  inline AlignedBox merged(const AlignedBox& b) const
  { return AlignedBox(m_min.cwiseMin(b.m_min), m_max.cwiseMax(b.m_max)); }

  
  template<typename Derived>
  inline AlignedBox& translate(const MatrixBase<Derived>& a_t)
  {
    const typename internal::nested<Derived,2>::type t(a_t.derived());
    m_min += t;
    m_max += t;
    return *this;
  }

  template<typename Derived>
  inline Scalar squaredExteriorDistance(const MatrixBase<Derived>& a_p) const;

  inline Scalar squaredExteriorDistance(const AlignedBox& b) const;

  template<typename Derived>
  inline NonInteger exteriorDistance(const MatrixBase<Derived>& p) const
  { using std::sqrt; return sqrt(NonInteger(squaredExteriorDistance(p))); }

  inline NonInteger exteriorDistance(const AlignedBox& b) const
  { using std::sqrt; return sqrt(NonInteger(squaredExteriorDistance(b))); }

  template<typename NewScalarType>
  inline typename internal::cast_return_type<AlignedBox,
           AlignedBox<NewScalarType,AmbientDimAtCompileTime> >::type cast() const
  {
    return typename internal::cast_return_type<AlignedBox,
                    AlignedBox<NewScalarType,AmbientDimAtCompileTime> >::type(*this);
  }

  
  template<typename OtherScalarType>
  inline explicit AlignedBox(const AlignedBox<OtherScalarType,AmbientDimAtCompileTime>& other)
  {
    m_min = (other.min)().template cast<Scalar>();
    m_max = (other.max)().template cast<Scalar>();
  }

  bool isApprox(const AlignedBox& other, const RealScalar& prec = ScalarTraits::dummy_precision()) const
  { return m_min.isApprox(other.m_min, prec) && m_max.isApprox(other.m_max, prec); }

protected:

  VectorType m_min, m_max;
};



template<typename Scalar,int AmbientDim>
template<typename Derived>
inline Scalar AlignedBox<Scalar,AmbientDim>::squaredExteriorDistance(const MatrixBase<Derived>& a_p) const
{
  typename internal::nested<Derived,2*AmbientDim>::type p(a_p.derived());
  Scalar dist2(0);
  Scalar aux;
  for (Index k=0; k<dim(); ++k)
  {
    if( m_min[k] > p[k] )
    {
      aux = m_min[k] - p[k];
      dist2 += aux*aux;
    }
    else if( p[k] > m_max[k] )
    {
      aux = p[k] - m_max[k];
      dist2 += aux*aux;
    }
  }
  return dist2;
}

template<typename Scalar,int AmbientDim>
inline Scalar AlignedBox<Scalar,AmbientDim>::squaredExteriorDistance(const AlignedBox& b) const
{
  Scalar dist2(0);
  Scalar aux;
  for (Index k=0; k<dim(); ++k)
  {
    if( m_min[k] > b.m_max[k] )
    {
      aux = m_min[k] - b.m_max[k];
      dist2 += aux*aux;
    }
    else if( b.m_min[k] > m_max[k] )
    {
      aux = b.m_min[k] - m_max[k];
      dist2 += aux*aux;
    }
  }
  return dist2;
}


#define EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, Size, SizeSuffix)    \
                                 \
typedef AlignedBox<Type, Size>   AlignedBox##SizeSuffix##TypeSuffix;

#define EIGEN_MAKE_TYPEDEFS_ALL_SIZES(Type, TypeSuffix) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, 1, 1) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, 2, 2) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, 3, 3) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, 4, 4) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, Dynamic, X)

EIGEN_MAKE_TYPEDEFS_ALL_SIZES(int,                  i)
EIGEN_MAKE_TYPEDEFS_ALL_SIZES(float,                f)
EIGEN_MAKE_TYPEDEFS_ALL_SIZES(double,               d)

#undef EIGEN_MAKE_TYPEDEFS_ALL_SIZES
#undef EIGEN_MAKE_TYPEDEFS

} 

#endif 
