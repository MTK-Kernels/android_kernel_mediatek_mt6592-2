// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed


namespace Eigen { 

template <typename _Scalar, int _AmbientDim>
class ParametrizedLine
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_AmbientDim)
  enum { AmbientDimAtCompileTime = _AmbientDim };
  typedef _Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  typedef Matrix<Scalar,AmbientDimAtCompileTime,1> VectorType;

  
  inline ParametrizedLine() {}

  inline explicit ParametrizedLine(int _dim) : m_origin(_dim), m_direction(_dim) {}

  ParametrizedLine(const VectorType& origin, const VectorType& direction)
    : m_origin(origin), m_direction(direction) {}

  explicit ParametrizedLine(const Hyperplane<_Scalar, _AmbientDim>& hyperplane);

  
  static inline ParametrizedLine Through(const VectorType& p0, const VectorType& p1)
  { return ParametrizedLine(p0, (p1-p0).normalized()); }

  ~ParametrizedLine() {}

  
  inline int dim() const { return m_direction.size(); }

  const VectorType& origin() const { return m_origin; }
  VectorType& origin() { return m_origin; }

  const VectorType& direction() const { return m_direction; }
  VectorType& direction() { return m_direction; }

  RealScalar squaredDistance(const VectorType& p) const
  {
    VectorType diff = p-origin();
    return (diff - diff.eigen2_dot(direction())* direction()).squaredNorm();
  }
  RealScalar distance(const VectorType& p) const { return ei_sqrt(squaredDistance(p)); }

  
  VectorType projection(const VectorType& p) const
  { return origin() + (p-origin()).eigen2_dot(direction()) * direction(); }

  Scalar intersection(const Hyperplane<_Scalar, _AmbientDim>& hyperplane);

  template<typename NewScalarType>
  inline typename internal::cast_return_type<ParametrizedLine,
           ParametrizedLine<NewScalarType,AmbientDimAtCompileTime> >::type cast() const
  {
    return typename internal::cast_return_type<ParametrizedLine,
                    ParametrizedLine<NewScalarType,AmbientDimAtCompileTime> >::type(*this);
  }

  
  template<typename OtherScalarType>
  inline explicit ParametrizedLine(const ParametrizedLine<OtherScalarType,AmbientDimAtCompileTime>& other)
  {
    m_origin = other.origin().template cast<Scalar>();
    m_direction = other.direction().template cast<Scalar>();
  }

  bool isApprox(const ParametrizedLine& other, typename NumTraits<Scalar>::Real prec = precision<Scalar>()) const
  { return m_origin.isApprox(other.m_origin, prec) && m_direction.isApprox(other.m_direction, prec); }

protected:

  VectorType m_origin, m_direction;
};

template <typename _Scalar, int _AmbientDim>
inline ParametrizedLine<_Scalar, _AmbientDim>::ParametrizedLine(const Hyperplane<_Scalar, _AmbientDim>& hyperplane)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(VectorType, 2)
  direction() = hyperplane.normal().unitOrthogonal();
  origin() = -hyperplane.normal()*hyperplane.offset();
}

template <typename _Scalar, int _AmbientDim>
inline _Scalar ParametrizedLine<_Scalar, _AmbientDim>::intersection(const Hyperplane<_Scalar, _AmbientDim>& hyperplane)
{
  return -(hyperplane.offset()+origin().eigen2_dot(hyperplane.normal()))
          /(direction().eigen2_dot(hyperplane.normal()));
}

} 
