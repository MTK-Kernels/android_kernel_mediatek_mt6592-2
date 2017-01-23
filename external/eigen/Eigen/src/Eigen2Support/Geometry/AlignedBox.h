// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed


namespace Eigen { 

template <typename _Scalar, int _AmbientDim>
class AlignedBox
{
public:
EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_AmbientDim==Dynamic ? Dynamic : _AmbientDim+1)
  enum { AmbientDimAtCompileTime = _AmbientDim };
  typedef _Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  typedef Matrix<Scalar,AmbientDimAtCompileTime,1> VectorType;

  
  inline AlignedBox()
  { if (AmbientDimAtCompileTime!=Dynamic) setNull(); }

  
  inline explicit AlignedBox(int _dim) : m_min(_dim), m_max(_dim)
  { setNull(); }

  
  inline AlignedBox(const VectorType& _min, const VectorType& _max) : m_min(_min), m_max(_max) {}

  
  inline explicit AlignedBox(const VectorType& p) : m_min(p), m_max(p) {}

  ~AlignedBox() {}

  
  inline int dim() const { return AmbientDimAtCompileTime==Dynamic ? m_min.size()-1 : AmbientDimAtCompileTime; }

  
  inline bool isNull() const { return (m_min.cwise() > m_max).any(); }

  
  inline void setNull()
  {
    m_min.setConstant( (std::numeric_limits<Scalar>::max)());
    m_max.setConstant(-(std::numeric_limits<Scalar>::max)());
  }

  
  inline const VectorType& (min)() const { return m_min; }
  
  inline VectorType& (min)() { return m_min; }
  
  inline const VectorType& (max)() const { return m_max; }
  
  inline VectorType& (max)() { return m_max; }

  
  inline bool contains(const VectorType& p) const
  { return (m_min.cwise()<=p).all() && (p.cwise()<=m_max).all(); }

  
  inline bool contains(const AlignedBox& b) const
  { return (m_min.cwise()<=(b.min)()).all() && ((b.max)().cwise()<=m_max).all(); }

  
  inline AlignedBox& extend(const VectorType& p)
  { m_min = (m_min.cwise().min)(p); m_max = (m_max.cwise().max)(p); return *this; }

  
  inline AlignedBox& extend(const AlignedBox& b)
  { m_min = (m_min.cwise().min)(b.m_min); m_max = (m_max.cwise().max)(b.m_max); return *this; }

  
  inline AlignedBox& clamp(const AlignedBox& b)
  { m_min = (m_min.cwise().max)(b.m_min); m_max = (m_max.cwise().min)(b.m_max); return *this; }

  
  inline AlignedBox& translate(const VectorType& t)
  { m_min += t; m_max += t; return *this; }

  inline Scalar squaredExteriorDistance(const VectorType& p) const;

  inline Scalar exteriorDistance(const VectorType& p) const
  { return ei_sqrt(squaredExteriorDistance(p)); }

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

  bool isApprox(const AlignedBox& other, typename NumTraits<Scalar>::Real prec = precision<Scalar>()) const
  { return m_min.isApprox(other.m_min, prec) && m_max.isApprox(other.m_max, prec); }

protected:

  VectorType m_min, m_max;
};

template<typename Scalar,int AmbiantDim>
inline Scalar AlignedBox<Scalar,AmbiantDim>::squaredExteriorDistance(const VectorType& p) const
{
  Scalar dist2(0);
  Scalar aux;
  for (int k=0; k<dim(); ++k)
  {
    if ((aux = (p[k]-m_min[k]))<Scalar(0))
      dist2 += aux*aux;
    else if ( (aux = (m_max[k]-p[k]))<Scalar(0))
      dist2 += aux*aux;
  }
  return dist2;
}

} 
