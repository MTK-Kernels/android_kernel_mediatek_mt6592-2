// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_PARAMETRIZEDLINE_H
#define EIGEN_PARAMETRIZEDLINE_H

namespace Eigen { 

template <typename _Scalar, int _AmbientDim, int _Options>
class ParametrizedLine
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_AmbientDim)
  enum {
    AmbientDimAtCompileTime = _AmbientDim,
    Options = _Options
  };
  typedef _Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  typedef DenseIndex Index;
  typedef Matrix<Scalar,AmbientDimAtCompileTime,1,Options> VectorType;

  
  inline ParametrizedLine() {}
  
  template<int OtherOptions>
  ParametrizedLine(const ParametrizedLine<Scalar,AmbientDimAtCompileTime,OtherOptions>& other)
   : m_origin(other.origin()), m_direction(other.direction())
  {}

  inline explicit ParametrizedLine(Index _dim) : m_origin(_dim), m_direction(_dim) {}

  ParametrizedLine(const VectorType& origin, const VectorType& direction)
    : m_origin(origin), m_direction(direction) {}

  template <int OtherOptions>
  explicit ParametrizedLine(const Hyperplane<_Scalar, _AmbientDim, OtherOptions>& hyperplane);

  
  static inline ParametrizedLine Through(const VectorType& p0, const VectorType& p1)
  { return ParametrizedLine(p0, (p1-p0).normalized()); }

  ~ParametrizedLine() {}

  
  inline Index dim() const { return m_direction.size(); }

  const VectorType& origin() const { return m_origin; }
  VectorType& origin() { return m_origin; }

  const VectorType& direction() const { return m_direction; }
  VectorType& direction() { return m_direction; }

  RealScalar squaredDistance(const VectorType& p) const
  {
    VectorType diff = p - origin();
    return (diff - direction().dot(diff) * direction()).squaredNorm();
  }
  RealScalar distance(const VectorType& p) const { using std::sqrt; return sqrt(squaredDistance(p)); }

  
  VectorType projection(const VectorType& p) const
  { return origin() + direction().dot(p-origin()) * direction(); }

  VectorType pointAt(const Scalar& t) const;
  
  template <int OtherOptions>
  Scalar intersectionParameter(const Hyperplane<_Scalar, _AmbientDim, OtherOptions>& hyperplane) const;
 
  template <int OtherOptions>
  Scalar intersection(const Hyperplane<_Scalar, _AmbientDim, OtherOptions>& hyperplane) const;
  
  template <int OtherOptions>
  VectorType intersectionPoint(const Hyperplane<_Scalar, _AmbientDim, OtherOptions>& hyperplane) const;

  template<typename NewScalarType>
  inline typename internal::cast_return_type<ParametrizedLine,
           ParametrizedLine<NewScalarType,AmbientDimAtCompileTime,Options> >::type cast() const
  {
    return typename internal::cast_return_type<ParametrizedLine,
                    ParametrizedLine<NewScalarType,AmbientDimAtCompileTime,Options> >::type(*this);
  }

  
  template<typename OtherScalarType,int OtherOptions>
  inline explicit ParametrizedLine(const ParametrizedLine<OtherScalarType,AmbientDimAtCompileTime,OtherOptions>& other)
  {
    m_origin = other.origin().template cast<Scalar>();
    m_direction = other.direction().template cast<Scalar>();
  }

  bool isApprox(const ParametrizedLine& other, typename NumTraits<Scalar>::Real prec = NumTraits<Scalar>::dummy_precision()) const
  { return m_origin.isApprox(other.m_origin, prec) && m_direction.isApprox(other.m_direction, prec); }

protected:

  VectorType m_origin, m_direction;
};

template <typename _Scalar, int _AmbientDim, int _Options>
template <int OtherOptions>
inline ParametrizedLine<_Scalar, _AmbientDim,_Options>::ParametrizedLine(const Hyperplane<_Scalar, _AmbientDim,OtherOptions>& hyperplane)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(VectorType, 2)
  direction() = hyperplane.normal().unitOrthogonal();
  origin() = -hyperplane.normal()*hyperplane.offset();
}

template <typename _Scalar, int _AmbientDim, int _Options>
inline typename ParametrizedLine<_Scalar, _AmbientDim,_Options>::VectorType
ParametrizedLine<_Scalar, _AmbientDim,_Options>::pointAt(const _Scalar& t) const
{
  return origin() + (direction()*t); 
}

template <typename _Scalar, int _AmbientDim, int _Options>
template <int OtherOptions>
inline _Scalar ParametrizedLine<_Scalar, _AmbientDim,_Options>::intersectionParameter(const Hyperplane<_Scalar, _AmbientDim, OtherOptions>& hyperplane) const
{
  return -(hyperplane.offset()+hyperplane.normal().dot(origin()))
          / hyperplane.normal().dot(direction());
}


template <typename _Scalar, int _AmbientDim, int _Options>
template <int OtherOptions>
inline _Scalar ParametrizedLine<_Scalar, _AmbientDim,_Options>::intersection(const Hyperplane<_Scalar, _AmbientDim, OtherOptions>& hyperplane) const
{
  return intersectionParameter(hyperplane);
}

template <typename _Scalar, int _AmbientDim, int _Options>
template <int OtherOptions>
inline typename ParametrizedLine<_Scalar, _AmbientDim,_Options>::VectorType
ParametrizedLine<_Scalar, _AmbientDim,_Options>::intersectionPoint(const Hyperplane<_Scalar, _AmbientDim, OtherOptions>& hyperplane) const
{
  return pointAt(intersectionParameter(hyperplane));
}

} 

#endif 
