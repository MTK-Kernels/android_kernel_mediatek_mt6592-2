// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_HYPERPLANE_H
#define EIGEN_HYPERPLANE_H

namespace Eigen { 

template <typename _Scalar, int _AmbientDim, int _Options>
class Hyperplane
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_AmbientDim==Dynamic ? Dynamic : _AmbientDim+1)
  enum {
    AmbientDimAtCompileTime = _AmbientDim,
    Options = _Options
  };
  typedef _Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  typedef DenseIndex Index;
  typedef Matrix<Scalar,AmbientDimAtCompileTime,1> VectorType;
  typedef Matrix<Scalar,Index(AmbientDimAtCompileTime)==Dynamic
                        ? Dynamic
                        : Index(AmbientDimAtCompileTime)+1,1,Options> Coefficients;
  typedef Block<Coefficients,AmbientDimAtCompileTime,1> NormalReturnType;
  typedef const Block<const Coefficients,AmbientDimAtCompileTime,1> ConstNormalReturnType;

  
  inline Hyperplane() {}
  
  template<int OtherOptions>
  Hyperplane(const Hyperplane<Scalar,AmbientDimAtCompileTime,OtherOptions>& other)
   : m_coeffs(other.coeffs())
  {}

  inline explicit Hyperplane(Index _dim) : m_coeffs(_dim+1) {}

  inline Hyperplane(const VectorType& n, const VectorType& e)
    : m_coeffs(n.size()+1)
  {
    normal() = n;
    offset() = -n.dot(e);
  }

  inline Hyperplane(const VectorType& n, const Scalar& d)
    : m_coeffs(n.size()+1)
  {
    normal() = n;
    offset() = d;
  }

  static inline Hyperplane Through(const VectorType& p0, const VectorType& p1)
  {
    Hyperplane result(p0.size());
    result.normal() = (p1 - p0).unitOrthogonal();
    result.offset() = -p0.dot(result.normal());
    return result;
  }

  static inline Hyperplane Through(const VectorType& p0, const VectorType& p1, const VectorType& p2)
  {
    EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(VectorType, 3)
    Hyperplane result(p0.size());
    VectorType v0(p2 - p0), v1(p1 - p0);
    result.normal() = v0.cross(v1);
    RealScalar norm = result.normal().norm();
    if(norm <= v0.norm() * v1.norm() * NumTraits<RealScalar>::epsilon())
    {
      Matrix<Scalar,2,3> m; m << v0.transpose(), v1.transpose();
      JacobiSVD<Matrix<Scalar,2,3> > svd(m, ComputeFullV);
      result.normal() = svd.matrixV().col(2);
    }
    else
      result.normal() /= norm;
    result.offset() = -p0.dot(result.normal());
    return result;
  }

  
  explicit Hyperplane(const ParametrizedLine<Scalar, AmbientDimAtCompileTime>& parametrized)
  {
    normal() = parametrized.direction().unitOrthogonal();
    offset() = -parametrized.origin().dot(normal());
  }

  ~Hyperplane() {}

  
  inline Index dim() const { return AmbientDimAtCompileTime==Dynamic ? m_coeffs.size()-1 : Index(AmbientDimAtCompileTime); }

  
  void normalize(void)
  {
    m_coeffs /= normal().norm();
  }

  inline Scalar signedDistance(const VectorType& p) const { return normal().dot(p) + offset(); }

  inline Scalar absDistance(const VectorType& p) const { using std::abs; return abs(signedDistance(p)); }

  inline VectorType projection(const VectorType& p) const { return p - signedDistance(p) * normal(); }

  inline ConstNormalReturnType normal() const { return ConstNormalReturnType(m_coeffs,0,0,dim(),1); }

  inline NormalReturnType normal() { return NormalReturnType(m_coeffs,0,0,dim(),1); }

  inline const Scalar& offset() const { return m_coeffs.coeff(dim()); }

  inline Scalar& offset() { return m_coeffs(dim()); }

  inline const Coefficients& coeffs() const { return m_coeffs; }

  inline Coefficients& coeffs() { return m_coeffs; }

  VectorType intersection(const Hyperplane& other) const
  {
    using std::abs;
    EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(VectorType, 2)
    Scalar det = coeffs().coeff(0) * other.coeffs().coeff(1) - coeffs().coeff(1) * other.coeffs().coeff(0);
    
    
    if(internal::isMuchSmallerThan(det, Scalar(1)))
    {   
        if(abs(coeffs().coeff(1))>abs(coeffs().coeff(0)))
            return VectorType(coeffs().coeff(1), -coeffs().coeff(2)/coeffs().coeff(1)-coeffs().coeff(0));
        else
            return VectorType(-coeffs().coeff(2)/coeffs().coeff(0)-coeffs().coeff(1), coeffs().coeff(0));
    }
    else
    {   
        Scalar invdet = Scalar(1) / det;
        return VectorType(invdet*(coeffs().coeff(1)*other.coeffs().coeff(2)-other.coeffs().coeff(1)*coeffs().coeff(2)),
                          invdet*(other.coeffs().coeff(0)*coeffs().coeff(2)-coeffs().coeff(0)*other.coeffs().coeff(2)));
    }
  }

  template<typename XprType>
  inline Hyperplane& transform(const MatrixBase<XprType>& mat, TransformTraits traits = Affine)
  {
    if (traits==Affine)
      normal() = mat.inverse().transpose() * normal();
    else if (traits==Isometry)
      normal() = mat * normal();
    else
    {
      eigen_assert(0 && "invalid traits value in Hyperplane::transform()");
    }
    return *this;
  }

  template<int TrOptions>
  inline Hyperplane& transform(const Transform<Scalar,AmbientDimAtCompileTime,Affine,TrOptions>& t,
                                TransformTraits traits = Affine)
  {
    transform(t.linear(), traits);
    offset() -= normal().dot(t.translation());
    return *this;
  }

  template<typename NewScalarType>
  inline typename internal::cast_return_type<Hyperplane,
           Hyperplane<NewScalarType,AmbientDimAtCompileTime,Options> >::type cast() const
  {
    return typename internal::cast_return_type<Hyperplane,
                    Hyperplane<NewScalarType,AmbientDimAtCompileTime,Options> >::type(*this);
  }

  
  template<typename OtherScalarType,int OtherOptions>
  inline explicit Hyperplane(const Hyperplane<OtherScalarType,AmbientDimAtCompileTime,OtherOptions>& other)
  { m_coeffs = other.coeffs().template cast<Scalar>(); }

  template<int OtherOptions>
  bool isApprox(const Hyperplane<Scalar,AmbientDimAtCompileTime,OtherOptions>& other, const typename NumTraits<Scalar>::Real& prec = NumTraits<Scalar>::dummy_precision()) const
  { return m_coeffs.isApprox(other.m_coeffs, prec); }

protected:

  Coefficients m_coeffs;
};

} 

#endif 
