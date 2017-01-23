// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2009 Mathieu Gautier <mathieu.gautier@cea.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_QUATERNION_H
#define EIGEN_QUATERNION_H
namespace Eigen { 



namespace internal {
template<typename Other,
         int OtherRows=Other::RowsAtCompileTime,
         int OtherCols=Other::ColsAtCompileTime>
struct quaternionbase_assign_impl;
}

template<class Derived>
class QuaternionBase : public RotationBase<Derived, 3>
{
  typedef RotationBase<Derived, 3> Base;
public:
  using Base::operator*;
  using Base::derived;

  typedef typename internal::traits<Derived>::Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  typedef typename internal::traits<Derived>::Coefficients Coefficients;
  enum {
    Flags = Eigen::internal::traits<Derived>::Flags
  };

 
  
  typedef Matrix<Scalar,3,1> Vector3;
  
  typedef Matrix<Scalar,3,3> Matrix3;
  
  typedef AngleAxis<Scalar> AngleAxisType;



  
  inline Scalar x() const { return this->derived().coeffs().coeff(0); }
  
  inline Scalar y() const { return this->derived().coeffs().coeff(1); }
  
  inline Scalar z() const { return this->derived().coeffs().coeff(2); }
  
  inline Scalar w() const { return this->derived().coeffs().coeff(3); }

  
  inline Scalar& x() { return this->derived().coeffs().coeffRef(0); }
  
  inline Scalar& y() { return this->derived().coeffs().coeffRef(1); }
  
  inline Scalar& z() { return this->derived().coeffs().coeffRef(2); }
  
  inline Scalar& w() { return this->derived().coeffs().coeffRef(3); }

  
  inline const VectorBlock<const Coefficients,3> vec() const { return coeffs().template head<3>(); }

  
  inline VectorBlock<Coefficients,3> vec() { return coeffs().template head<3>(); }

  
  inline const typename internal::traits<Derived>::Coefficients& coeffs() const { return derived().coeffs(); }

  
  inline typename internal::traits<Derived>::Coefficients& coeffs() { return derived().coeffs(); }

  EIGEN_STRONG_INLINE QuaternionBase<Derived>& operator=(const QuaternionBase<Derived>& other);
  template<class OtherDerived> EIGEN_STRONG_INLINE Derived& operator=(const QuaternionBase<OtherDerived>& other);


  Derived& operator=(const AngleAxisType& aa);
  template<class OtherDerived> Derived& operator=(const MatrixBase<OtherDerived>& m);

  static inline Quaternion<Scalar> Identity() { return Quaternion<Scalar>(1, 0, 0, 0); }

  inline QuaternionBase& setIdentity() { coeffs() << 0, 0, 0, 1; return *this; }

  inline Scalar squaredNorm() const { return coeffs().squaredNorm(); }

  inline Scalar norm() const { return coeffs().norm(); }

  inline void normalize() { coeffs().normalize(); }
  inline Quaternion<Scalar> normalized() const { return Quaternion<Scalar>(coeffs().normalized()); }

  template<class OtherDerived> inline Scalar dot(const QuaternionBase<OtherDerived>& other) const { return coeffs().dot(other.coeffs()); }

  template<class OtherDerived> Scalar angularDistance(const QuaternionBase<OtherDerived>& other) const;

  
  Matrix3 toRotationMatrix() const;

  
  template<typename Derived1, typename Derived2>
  Derived& setFromTwoVectors(const MatrixBase<Derived1>& a, const MatrixBase<Derived2>& b);

  template<class OtherDerived> EIGEN_STRONG_INLINE Quaternion<Scalar> operator* (const QuaternionBase<OtherDerived>& q) const;
  template<class OtherDerived> EIGEN_STRONG_INLINE Derived& operator*= (const QuaternionBase<OtherDerived>& q);

  
  Quaternion<Scalar> inverse() const;

  
  Quaternion<Scalar> conjugate() const;

  template<class OtherDerived> Quaternion<Scalar> slerp(const Scalar& t, const QuaternionBase<OtherDerived>& other) const;

  template<class OtherDerived>
  bool isApprox(const QuaternionBase<OtherDerived>& other, const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const
  { return coeffs().isApprox(other.coeffs(), prec); }

	
  EIGEN_STRONG_INLINE Vector3 _transformVector(Vector3 v) const;

  template<typename NewScalarType>
  inline typename internal::cast_return_type<Derived,Quaternion<NewScalarType> >::type cast() const
  {
    return typename internal::cast_return_type<Derived,Quaternion<NewScalarType> >::type(derived());
  }

#ifdef EIGEN_QUATERNIONBASE_PLUGIN
# include EIGEN_QUATERNIONBASE_PLUGIN
#endif
};



namespace internal {
template<typename _Scalar,int _Options>
struct traits<Quaternion<_Scalar,_Options> >
{
  typedef Quaternion<_Scalar,_Options> PlainObject;
  typedef _Scalar Scalar;
  typedef Matrix<_Scalar,4,1,_Options> Coefficients;
  enum{
    IsAligned = internal::traits<Coefficients>::Flags & AlignedBit,
    Flags = IsAligned ? (AlignedBit | LvalueBit) : LvalueBit
  };
};
}

template<typename _Scalar, int _Options>
class Quaternion : public QuaternionBase<Quaternion<_Scalar,_Options> >
{
  typedef QuaternionBase<Quaternion<_Scalar,_Options> > Base;
  enum { IsAligned = internal::traits<Quaternion>::IsAligned };

public:
  typedef _Scalar Scalar;

  EIGEN_INHERIT_ASSIGNMENT_EQUAL_OPERATOR(Quaternion)
  using Base::operator*=;

  typedef typename internal::traits<Quaternion>::Coefficients Coefficients;
  typedef typename Base::AngleAxisType AngleAxisType;

  
  inline Quaternion() {}

  inline Quaternion(const Scalar& w, const Scalar& x, const Scalar& y, const Scalar& z) : m_coeffs(x, y, z, w){}

  
  inline Quaternion(const Scalar* data) : m_coeffs(data) {}

  
  template<class Derived> EIGEN_STRONG_INLINE Quaternion(const QuaternionBase<Derived>& other) { this->Base::operator=(other); }

  
  explicit inline Quaternion(const AngleAxisType& aa) { *this = aa; }

  template<typename Derived>
  explicit inline Quaternion(const MatrixBase<Derived>& other) { *this = other; }

  
  template<typename OtherScalar, int OtherOptions>
  explicit inline Quaternion(const Quaternion<OtherScalar, OtherOptions>& other)
  { m_coeffs = other.coeffs().template cast<Scalar>(); }

  template<typename Derived1, typename Derived2>
  static Quaternion FromTwoVectors(const MatrixBase<Derived1>& a, const MatrixBase<Derived2>& b);

  inline Coefficients& coeffs() { return m_coeffs;}
  inline const Coefficients& coeffs() const { return m_coeffs;}

  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(IsAligned)

protected:
  Coefficients m_coeffs;
  
#ifndef EIGEN_PARSED_BY_DOXYGEN
    static EIGEN_STRONG_INLINE void _check_template_params()
    {
      EIGEN_STATIC_ASSERT( (_Options & DontAlign) == _Options,
        INVALID_MATRIX_TEMPLATE_PARAMETERS)
    }
#endif
};

typedef Quaternion<float> Quaternionf;
typedef Quaternion<double> Quaterniond;


namespace internal {
  template<typename _Scalar, int _Options>
  struct traits<Map<Quaternion<_Scalar>, _Options> > : traits<Quaternion<_Scalar, (int(_Options)&Aligned)==Aligned ? AutoAlign : DontAlign> >
  {
    typedef Map<Matrix<_Scalar,4,1>, _Options> Coefficients;
  };
}

namespace internal {
  template<typename _Scalar, int _Options>
  struct traits<Map<const Quaternion<_Scalar>, _Options> > : traits<Quaternion<_Scalar, (int(_Options)&Aligned)==Aligned ? AutoAlign : DontAlign> >
  {
    typedef Map<const Matrix<_Scalar,4,1>, _Options> Coefficients;
    typedef traits<Quaternion<_Scalar, (int(_Options)&Aligned)==Aligned ? AutoAlign : DontAlign> > TraitsBase;
    enum {
      Flags = TraitsBase::Flags & ~LvalueBit
    };
  };
}

template<typename _Scalar, int _Options>
class Map<const Quaternion<_Scalar>, _Options >
  : public QuaternionBase<Map<const Quaternion<_Scalar>, _Options> >
{
    typedef QuaternionBase<Map<const Quaternion<_Scalar>, _Options> > Base;

  public:
    typedef _Scalar Scalar;
    typedef typename internal::traits<Map>::Coefficients Coefficients;
    EIGEN_INHERIT_ASSIGNMENT_EQUAL_OPERATOR(Map)
    using Base::operator*=;

    EIGEN_STRONG_INLINE Map(const Scalar* coeffs) : m_coeffs(coeffs) {}

    inline const Coefficients& coeffs() const { return m_coeffs;}

  protected:
    const Coefficients m_coeffs;
};

template<typename _Scalar, int _Options>
class Map<Quaternion<_Scalar>, _Options >
  : public QuaternionBase<Map<Quaternion<_Scalar>, _Options> >
{
    typedef QuaternionBase<Map<Quaternion<_Scalar>, _Options> > Base;

  public:
    typedef _Scalar Scalar;
    typedef typename internal::traits<Map>::Coefficients Coefficients;
    EIGEN_INHERIT_ASSIGNMENT_EQUAL_OPERATOR(Map)
    using Base::operator*=;

    EIGEN_STRONG_INLINE Map(Scalar* coeffs) : m_coeffs(coeffs) {}

    inline Coefficients& coeffs() { return m_coeffs; }
    inline const Coefficients& coeffs() const { return m_coeffs; }

  protected:
    Coefficients m_coeffs;
};

typedef Map<Quaternion<float>, 0>         QuaternionMapf;
typedef Map<Quaternion<double>, 0>        QuaternionMapd;
typedef Map<Quaternion<float>, Aligned>   QuaternionMapAlignedf;
typedef Map<Quaternion<double>, Aligned>  QuaternionMapAlignedd;


namespace internal {
template<int Arch, class Derived1, class Derived2, typename Scalar, int _Options> struct quat_product
{
  static EIGEN_STRONG_INLINE Quaternion<Scalar> run(const QuaternionBase<Derived1>& a, const QuaternionBase<Derived2>& b){
    return Quaternion<Scalar>
    (
      a.w() * b.w() - a.x() * b.x() - a.y() * b.y() - a.z() * b.z(),
      a.w() * b.x() + a.x() * b.w() + a.y() * b.z() - a.z() * b.y(),
      a.w() * b.y() + a.y() * b.w() + a.z() * b.x() - a.x() * b.z(),
      a.w() * b.z() + a.z() * b.w() + a.x() * b.y() - a.y() * b.x()
    );
  }
};
}

template <class Derived>
template <class OtherDerived>
EIGEN_STRONG_INLINE Quaternion<typename internal::traits<Derived>::Scalar>
QuaternionBase<Derived>::operator* (const QuaternionBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT((internal::is_same<typename Derived::Scalar, typename OtherDerived::Scalar>::value),
   YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)
  return internal::quat_product<Architecture::Target, Derived, OtherDerived,
                         typename internal::traits<Derived>::Scalar,
                         internal::traits<Derived>::IsAligned && internal::traits<OtherDerived>::IsAligned>::run(*this, other);
}

template <class Derived>
template <class OtherDerived>
EIGEN_STRONG_INLINE Derived& QuaternionBase<Derived>::operator*= (const QuaternionBase<OtherDerived>& other)
{
  derived() = derived() * other.derived();
  return derived();
}

template <class Derived>
EIGEN_STRONG_INLINE typename QuaternionBase<Derived>::Vector3
QuaternionBase<Derived>::_transformVector(Vector3 v) const
{
    
    
    
    
    
    Vector3 uv = this->vec().cross(v);
    uv += uv;
    return v + this->w() * uv + this->vec().cross(uv);
}

template<class Derived>
EIGEN_STRONG_INLINE QuaternionBase<Derived>& QuaternionBase<Derived>::operator=(const QuaternionBase<Derived>& other)
{
  coeffs() = other.coeffs();
  return derived();
}

template<class Derived>
template<class OtherDerived>
EIGEN_STRONG_INLINE Derived& QuaternionBase<Derived>::operator=(const QuaternionBase<OtherDerived>& other)
{
  coeffs() = other.coeffs();
  return derived();
}

template<class Derived>
EIGEN_STRONG_INLINE Derived& QuaternionBase<Derived>::operator=(const AngleAxisType& aa)
{
  using std::cos;
  using std::sin;
  Scalar ha = Scalar(0.5)*aa.angle(); 
  this->w() = cos(ha);
  this->vec() = sin(ha) * aa.axis();
  return derived();
}


template<class Derived>
template<class MatrixDerived>
inline Derived& QuaternionBase<Derived>::operator=(const MatrixBase<MatrixDerived>& xpr)
{
  EIGEN_STATIC_ASSERT((internal::is_same<typename Derived::Scalar, typename MatrixDerived::Scalar>::value),
   YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)
  internal::quaternionbase_assign_impl<MatrixDerived>::run(*this, xpr.derived());
  return derived();
}

template<class Derived>
inline typename QuaternionBase<Derived>::Matrix3
QuaternionBase<Derived>::toRotationMatrix(void) const
{
  
  
  
  
  Matrix3 res;

  const Scalar tx  = Scalar(2)*this->x();
  const Scalar ty  = Scalar(2)*this->y();
  const Scalar tz  = Scalar(2)*this->z();
  const Scalar twx = tx*this->w();
  const Scalar twy = ty*this->w();
  const Scalar twz = tz*this->w();
  const Scalar txx = tx*this->x();
  const Scalar txy = ty*this->x();
  const Scalar txz = tz*this->x();
  const Scalar tyy = ty*this->y();
  const Scalar tyz = tz*this->y();
  const Scalar tzz = tz*this->z();

  res.coeffRef(0,0) = Scalar(1)-(tyy+tzz);
  res.coeffRef(0,1) = txy-twz;
  res.coeffRef(0,2) = txz+twy;
  res.coeffRef(1,0) = txy+twz;
  res.coeffRef(1,1) = Scalar(1)-(txx+tzz);
  res.coeffRef(1,2) = tyz-twx;
  res.coeffRef(2,0) = txz-twy;
  res.coeffRef(2,1) = tyz+twx;
  res.coeffRef(2,2) = Scalar(1)-(txx+tyy);

  return res;
}

template<class Derived>
template<typename Derived1, typename Derived2>
inline Derived& QuaternionBase<Derived>::setFromTwoVectors(const MatrixBase<Derived1>& a, const MatrixBase<Derived2>& b)
{
  using std::max;
  using std::sqrt;
  Vector3 v0 = a.normalized();
  Vector3 v1 = b.normalized();
  Scalar c = v1.dot(v0);

  
  
  
  
  
  
  
  
  if (c < Scalar(-1)+NumTraits<Scalar>::dummy_precision())
  {
    c = (max)(c,Scalar(-1));
    Matrix<Scalar,2,3> m; m << v0.transpose(), v1.transpose();
    JacobiSVD<Matrix<Scalar,2,3> > svd(m, ComputeFullV);
    Vector3 axis = svd.matrixV().col(2);

    Scalar w2 = (Scalar(1)+c)*Scalar(0.5);
    this->w() = sqrt(w2);
    this->vec() = axis * sqrt(Scalar(1) - w2);
    return derived();
  }
  Vector3 axis = v0.cross(v1);
  Scalar s = sqrt((Scalar(1)+c)*Scalar(2));
  Scalar invs = Scalar(1)/s;
  this->vec() = axis * invs;
  this->w() = s * Scalar(0.5);

  return derived();
}


template<typename Scalar, int Options>
template<typename Derived1, typename Derived2>
Quaternion<Scalar,Options> Quaternion<Scalar,Options>::FromTwoVectors(const MatrixBase<Derived1>& a, const MatrixBase<Derived2>& b)
{
    Quaternion quat;
    quat.setFromTwoVectors(a, b);
    return quat;
}


template <class Derived>
inline Quaternion<typename internal::traits<Derived>::Scalar> QuaternionBase<Derived>::inverse() const
{
  
  Scalar n2 = this->squaredNorm();
  if (n2 > 0)
    return Quaternion<Scalar>(conjugate().coeffs() / n2);
  else
  {
    
    return Quaternion<Scalar>(Coefficients::Zero());
  }
}

template <class Derived>
inline Quaternion<typename internal::traits<Derived>::Scalar>
QuaternionBase<Derived>::conjugate() const
{
  return Quaternion<Scalar>(this->w(),-this->x(),-this->y(),-this->z());
}

template <class Derived>
template <class OtherDerived>
inline typename internal::traits<Derived>::Scalar
QuaternionBase<Derived>::angularDistance(const QuaternionBase<OtherDerived>& other) const
{
  using std::acos;
  using std::abs;
  Scalar d = abs(this->dot(other));
  if (d>=Scalar(1))
    return Scalar(0);
  return Scalar(2) * acos(d);
}

 
    
template <class Derived>
template <class OtherDerived>
Quaternion<typename internal::traits<Derived>::Scalar>
QuaternionBase<Derived>::slerp(const Scalar& t, const QuaternionBase<OtherDerived>& other) const
{
  using std::acos;
  using std::sin;
  using std::abs;
  static const Scalar one = Scalar(1) - NumTraits<Scalar>::epsilon();
  Scalar d = this->dot(other);
  Scalar absD = abs(d);

  Scalar scale0;
  Scalar scale1;

  if(absD>=one)
  {
    scale0 = Scalar(1) - t;
    scale1 = t;
  }
  else
  {
    
    Scalar theta = acos(absD);
    Scalar sinTheta = sin(theta);

    scale0 = sin( ( Scalar(1) - t ) * theta) / sinTheta;
    scale1 = sin( ( t * theta) ) / sinTheta;
  }
  if(d<0) scale1 = -scale1;

  return Quaternion<Scalar>(scale0 * coeffs() + scale1 * other.coeffs());
}

namespace internal {

template<typename Other>
struct quaternionbase_assign_impl<Other,3,3>
{
  typedef typename Other::Scalar Scalar;
  typedef DenseIndex Index;
  template<class Derived> static inline void run(QuaternionBase<Derived>& q, const Other& mat)
  {
    using std::sqrt;
    
    
    Scalar t = mat.trace();
    if (t > Scalar(0))
    {
      t = sqrt(t + Scalar(1.0));
      q.w() = Scalar(0.5)*t;
      t = Scalar(0.5)/t;
      q.x() = (mat.coeff(2,1) - mat.coeff(1,2)) * t;
      q.y() = (mat.coeff(0,2) - mat.coeff(2,0)) * t;
      q.z() = (mat.coeff(1,0) - mat.coeff(0,1)) * t;
    }
    else
    {
      DenseIndex i = 0;
      if (mat.coeff(1,1) > mat.coeff(0,0))
        i = 1;
      if (mat.coeff(2,2) > mat.coeff(i,i))
        i = 2;
      DenseIndex j = (i+1)%3;
      DenseIndex k = (j+1)%3;

      t = sqrt(mat.coeff(i,i)-mat.coeff(j,j)-mat.coeff(k,k) + Scalar(1.0));
      q.coeffs().coeffRef(i) = Scalar(0.5) * t;
      t = Scalar(0.5)/t;
      q.w() = (mat.coeff(k,j)-mat.coeff(j,k))*t;
      q.coeffs().coeffRef(j) = (mat.coeff(j,i)+mat.coeff(i,j))*t;
      q.coeffs().coeffRef(k) = (mat.coeff(k,i)+mat.coeff(i,k))*t;
    }
  }
};

template<typename Other>
struct quaternionbase_assign_impl<Other,4,1>
{
  typedef typename Other::Scalar Scalar;
  template<class Derived> static inline void run(QuaternionBase<Derived>& q, const Other& vec)
  {
    q.coeffs() = vec;
  }
};

} 

} 

#endif 
