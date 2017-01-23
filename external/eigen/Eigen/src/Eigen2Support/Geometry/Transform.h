// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Copyright (C) 2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed


namespace Eigen { 

template< typename Other,
          int Dim,
          int HDim,
          int OtherRows=Other::RowsAtCompileTime,
          int OtherCols=Other::ColsAtCompileTime>
struct ei_transform_product_impl;

template<typename _Scalar, int _Dim>
class Transform
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_Dim==Dynamic ? Dynamic : (_Dim+1)*(_Dim+1))
  enum {
    Dim = _Dim,     
    HDim = _Dim+1   
  };
  
  typedef _Scalar Scalar;
  
  typedef Matrix<Scalar,HDim,HDim> MatrixType;
  
  typedef Matrix<Scalar,Dim,Dim> LinearMatrixType;
  
  typedef Block<MatrixType,Dim,Dim> LinearPart;
  
  typedef const Block<const MatrixType,Dim,Dim> ConstLinearPart;
  
  typedef Matrix<Scalar,Dim,1> VectorType;
  
  typedef Block<MatrixType,Dim,1> TranslationPart;
  
  typedef const Block<const MatrixType,Dim,1> ConstTranslationPart;
  
  typedef Translation<Scalar,Dim> TranslationType;
  
  typedef Scaling<Scalar,Dim> ScalingType;

protected:

  MatrixType m_matrix;

public:

  
  inline Transform() { }

  inline Transform(const Transform& other)
  {
    m_matrix = other.m_matrix;
  }

  inline explicit Transform(const TranslationType& t) { *this = t; }
  inline explicit Transform(const ScalingType& s) { *this = s; }
  template<typename Derived>
  inline explicit Transform(const RotationBase<Derived, Dim>& r) { *this = r; }

  inline Transform& operator=(const Transform& other)
  { m_matrix = other.m_matrix; return *this; }

  template<typename OtherDerived, bool BigMatrix> 
  struct construct_from_matrix
  {
    static inline void run(Transform *transform, const MatrixBase<OtherDerived>& other)
    {
      transform->matrix() = other;
    }
  };

  template<typename OtherDerived> struct construct_from_matrix<OtherDerived, true>
  {
    static inline void run(Transform *transform, const MatrixBase<OtherDerived>& other)
    {
      transform->linear() = other;
      transform->translation().setZero();
      transform->matrix()(Dim,Dim) = Scalar(1);
      transform->matrix().template block<1,Dim>(Dim,0).setZero();
    }
  };

  
  template<typename OtherDerived>
  inline explicit Transform(const MatrixBase<OtherDerived>& other)
  {
    construct_from_matrix<OtherDerived, int(OtherDerived::RowsAtCompileTime) == Dim>::run(this, other);
  }

  
  template<typename OtherDerived>
  inline Transform& operator=(const MatrixBase<OtherDerived>& other)
  { m_matrix = other; return *this; }

  #ifdef EIGEN_QT_SUPPORT
  inline Transform(const QMatrix& other);
  inline Transform& operator=(const QMatrix& other);
  inline QMatrix toQMatrix(void) const;
  inline Transform(const QTransform& other);
  inline Transform& operator=(const QTransform& other);
  inline QTransform toQTransform(void) const;
  #endif

  inline Scalar operator() (int row, int col) const { return m_matrix(row,col); }
  inline Scalar& operator() (int row, int col) { return m_matrix(row,col); }

  
  inline const MatrixType& matrix() const { return m_matrix; }
  
  inline MatrixType& matrix() { return m_matrix; }

  
  inline ConstLinearPart linear() const { return m_matrix.template block<Dim,Dim>(0,0); }
  
  inline LinearPart linear() { return m_matrix.template block<Dim,Dim>(0,0); }

  
  inline ConstTranslationPart translation() const { return m_matrix.template block<Dim,1>(0,Dim); }
  
  inline TranslationPart translation() { return m_matrix.template block<Dim,1>(0,Dim); }

  
  template<typename OtherDerived>
  inline const typename ei_transform_product_impl<OtherDerived,_Dim,_Dim+1>::ResultType
  operator * (const MatrixBase<OtherDerived> &other) const
  { return ei_transform_product_impl<OtherDerived,Dim,HDim>::run(*this,other.derived()); }

  template<typename OtherDerived>
  friend inline const typename ProductReturnType<OtherDerived,MatrixType>::Type
  operator * (const MatrixBase<OtherDerived> &a, const Transform &b)
  { return a.derived() * b.matrix(); }

  
  inline const Transform
  operator * (const Transform& other) const
  { return Transform(m_matrix * other.matrix()); }

  
  void setIdentity() { m_matrix.setIdentity(); }
  static const typename MatrixType::IdentityReturnType Identity()
  {
    return MatrixType::Identity();
  }

  template<typename OtherDerived>
  inline Transform& scale(const MatrixBase<OtherDerived> &other);

  template<typename OtherDerived>
  inline Transform& prescale(const MatrixBase<OtherDerived> &other);

  inline Transform& scale(Scalar s);
  inline Transform& prescale(Scalar s);

  template<typename OtherDerived>
  inline Transform& translate(const MatrixBase<OtherDerived> &other);

  template<typename OtherDerived>
  inline Transform& pretranslate(const MatrixBase<OtherDerived> &other);

  template<typename RotationType>
  inline Transform& rotate(const RotationType& rotation);

  template<typename RotationType>
  inline Transform& prerotate(const RotationType& rotation);

  Transform& shear(Scalar sx, Scalar sy);
  Transform& preshear(Scalar sx, Scalar sy);

  inline Transform& operator=(const TranslationType& t);
  inline Transform& operator*=(const TranslationType& t) { return translate(t.vector()); }
  inline Transform operator*(const TranslationType& t) const;

  inline Transform& operator=(const ScalingType& t);
  inline Transform& operator*=(const ScalingType& s) { return scale(s.coeffs()); }
  inline Transform operator*(const ScalingType& s) const;
  friend inline Transform operator*(const LinearMatrixType& mat, const Transform& t)
  {
    Transform res = t;
    res.matrix().row(Dim) = t.matrix().row(Dim);
    res.matrix().template block<Dim,HDim>(0,0) = (mat * t.matrix().template block<Dim,HDim>(0,0)).lazy();
    return res;
  }

  template<typename Derived>
  inline Transform& operator=(const RotationBase<Derived,Dim>& r);
  template<typename Derived>
  inline Transform& operator*=(const RotationBase<Derived,Dim>& r) { return rotate(r.toRotationMatrix()); }
  template<typename Derived>
  inline Transform operator*(const RotationBase<Derived,Dim>& r) const;

  LinearMatrixType rotation() const;
  template<typename RotationMatrixType, typename ScalingMatrixType>
  void computeRotationScaling(RotationMatrixType *rotation, ScalingMatrixType *scaling) const;
  template<typename ScalingMatrixType, typename RotationMatrixType>
  void computeScalingRotation(ScalingMatrixType *scaling, RotationMatrixType *rotation) const;

  template<typename PositionDerived, typename OrientationType, typename ScaleDerived>
  Transform& fromPositionOrientationScale(const MatrixBase<PositionDerived> &position,
    const OrientationType& orientation, const MatrixBase<ScaleDerived> &scale);

  inline const MatrixType inverse(TransformTraits traits = Affine) const;

  
  const Scalar* data() const { return m_matrix.data(); }
  
  Scalar* data() { return m_matrix.data(); }

  template<typename NewScalarType>
  inline typename internal::cast_return_type<Transform,Transform<NewScalarType,Dim> >::type cast() const
  { return typename internal::cast_return_type<Transform,Transform<NewScalarType,Dim> >::type(*this); }

  
  template<typename OtherScalarType>
  inline explicit Transform(const Transform<OtherScalarType,Dim>& other)
  { m_matrix = other.matrix().template cast<Scalar>(); }

  bool isApprox(const Transform& other, typename NumTraits<Scalar>::Real prec = precision<Scalar>()) const
  { return m_matrix.isApprox(other.m_matrix, prec); }

  #ifdef EIGEN_TRANSFORM_PLUGIN
  #include EIGEN_TRANSFORM_PLUGIN
  #endif

protected:

};

typedef Transform<float,2> Transform2f;
typedef Transform<float,3> Transform3f;
typedef Transform<double,2> Transform2d;
typedef Transform<double,3> Transform3d;


#ifdef EIGEN_QT_SUPPORT
template<typename Scalar, int Dim>
Transform<Scalar,Dim>::Transform(const QMatrix& other)
{
  *this = other;
}

template<typename Scalar, int Dim>
Transform<Scalar,Dim>& Transform<Scalar,Dim>::operator=(const QMatrix& other)
{
  EIGEN_STATIC_ASSERT(Dim==2, YOU_MADE_A_PROGRAMMING_MISTAKE)
  m_matrix << other.m11(), other.m21(), other.dx(),
              other.m12(), other.m22(), other.dy(),
              0, 0, 1;
   return *this;
}

template<typename Scalar, int Dim>
QMatrix Transform<Scalar,Dim>::toQMatrix(void) const
{
  EIGEN_STATIC_ASSERT(Dim==2, YOU_MADE_A_PROGRAMMING_MISTAKE)
  return QMatrix(m_matrix.coeff(0,0), m_matrix.coeff(1,0),
                 m_matrix.coeff(0,1), m_matrix.coeff(1,1),
                 m_matrix.coeff(0,2), m_matrix.coeff(1,2));
}

template<typename Scalar, int Dim>
Transform<Scalar,Dim>::Transform(const QTransform& other)
{
  *this = other;
}

template<typename Scalar, int Dim>
Transform<Scalar,Dim>& Transform<Scalar,Dim>::operator=(const QTransform& other)
{
  EIGEN_STATIC_ASSERT(Dim==2, YOU_MADE_A_PROGRAMMING_MISTAKE)
  m_matrix << other.m11(), other.m21(), other.dx(),
              other.m12(), other.m22(), other.dy(),
              other.m13(), other.m23(), other.m33();
   return *this;
}

template<typename Scalar, int Dim>
QTransform Transform<Scalar,Dim>::toQTransform(void) const
{
  EIGEN_STATIC_ASSERT(Dim==2, YOU_MADE_A_PROGRAMMING_MISTAKE)
  return QTransform(m_matrix.coeff(0,0), m_matrix.coeff(1,0), m_matrix.coeff(2,0),
                    m_matrix.coeff(0,1), m_matrix.coeff(1,1), m_matrix.coeff(2,1),
                    m_matrix.coeff(0,2), m_matrix.coeff(1,2), m_matrix.coeff(2,2));
}
#endif


template<typename Scalar, int Dim>
template<typename OtherDerived>
Transform<Scalar,Dim>&
Transform<Scalar,Dim>::scale(const MatrixBase<OtherDerived> &other)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(OtherDerived,int(Dim))
  linear() = (linear() * other.asDiagonal()).lazy();
  return *this;
}

template<typename Scalar, int Dim>
inline Transform<Scalar,Dim>& Transform<Scalar,Dim>::scale(Scalar s)
{
  linear() *= s;
  return *this;
}

template<typename Scalar, int Dim>
template<typename OtherDerived>
Transform<Scalar,Dim>&
Transform<Scalar,Dim>::prescale(const MatrixBase<OtherDerived> &other)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(OtherDerived,int(Dim))
  m_matrix.template block<Dim,HDim>(0,0) = (other.asDiagonal() * m_matrix.template block<Dim,HDim>(0,0)).lazy();
  return *this;
}

template<typename Scalar, int Dim>
inline Transform<Scalar,Dim>& Transform<Scalar,Dim>::prescale(Scalar s)
{
  m_matrix.template corner<Dim,HDim>(TopLeft) *= s;
  return *this;
}

template<typename Scalar, int Dim>
template<typename OtherDerived>
Transform<Scalar,Dim>&
Transform<Scalar,Dim>::translate(const MatrixBase<OtherDerived> &other)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(OtherDerived,int(Dim))
  translation() += linear() * other;
  return *this;
}

template<typename Scalar, int Dim>
template<typename OtherDerived>
Transform<Scalar,Dim>&
Transform<Scalar,Dim>::pretranslate(const MatrixBase<OtherDerived> &other)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(OtherDerived,int(Dim))
  translation() += other;
  return *this;
}

template<typename Scalar, int Dim>
template<typename RotationType>
Transform<Scalar,Dim>&
Transform<Scalar,Dim>::rotate(const RotationType& rotation)
{
  linear() *= ei_toRotationMatrix<Scalar,Dim>(rotation);
  return *this;
}

template<typename Scalar, int Dim>
template<typename RotationType>
Transform<Scalar,Dim>&
Transform<Scalar,Dim>::prerotate(const RotationType& rotation)
{
  m_matrix.template block<Dim,HDim>(0,0) = ei_toRotationMatrix<Scalar,Dim>(rotation)
                                         * m_matrix.template block<Dim,HDim>(0,0);
  return *this;
}

template<typename Scalar, int Dim>
Transform<Scalar,Dim>&
Transform<Scalar,Dim>::shear(Scalar sx, Scalar sy)
{
  EIGEN_STATIC_ASSERT(int(Dim)==2, YOU_MADE_A_PROGRAMMING_MISTAKE)
  VectorType tmp = linear().col(0)*sy + linear().col(1);
  linear() << linear().col(0) + linear().col(1)*sx, tmp;
  return *this;
}

template<typename Scalar, int Dim>
Transform<Scalar,Dim>&
Transform<Scalar,Dim>::preshear(Scalar sx, Scalar sy)
{
  EIGEN_STATIC_ASSERT(int(Dim)==2, YOU_MADE_A_PROGRAMMING_MISTAKE)
  m_matrix.template block<Dim,HDim>(0,0) = LinearMatrixType(1, sx, sy, 1) * m_matrix.template block<Dim,HDim>(0,0);
  return *this;
}


template<typename Scalar, int Dim>
inline Transform<Scalar,Dim>& Transform<Scalar,Dim>::operator=(const TranslationType& t)
{
  linear().setIdentity();
  translation() = t.vector();
  m_matrix.template block<1,Dim>(Dim,0).setZero();
  m_matrix(Dim,Dim) = Scalar(1);
  return *this;
}

template<typename Scalar, int Dim>
inline Transform<Scalar,Dim> Transform<Scalar,Dim>::operator*(const TranslationType& t) const
{
  Transform res = *this;
  res.translate(t.vector());
  return res;
}

template<typename Scalar, int Dim>
inline Transform<Scalar,Dim>& Transform<Scalar,Dim>::operator=(const ScalingType& s)
{
  m_matrix.setZero();
  linear().diagonal() = s.coeffs();
  m_matrix.coeffRef(Dim,Dim) = Scalar(1);
  return *this;
}

template<typename Scalar, int Dim>
inline Transform<Scalar,Dim> Transform<Scalar,Dim>::operator*(const ScalingType& s) const
{
  Transform res = *this;
  res.scale(s.coeffs());
  return res;
}

template<typename Scalar, int Dim>
template<typename Derived>
inline Transform<Scalar,Dim>& Transform<Scalar,Dim>::operator=(const RotationBase<Derived,Dim>& r)
{
  linear() = ei_toRotationMatrix<Scalar,Dim>(r);
  translation().setZero();
  m_matrix.template block<1,Dim>(Dim,0).setZero();
  m_matrix.coeffRef(Dim,Dim) = Scalar(1);
  return *this;
}

template<typename Scalar, int Dim>
template<typename Derived>
inline Transform<Scalar,Dim> Transform<Scalar,Dim>::operator*(const RotationBase<Derived,Dim>& r) const
{
  Transform res = *this;
  res.rotate(r.derived());
  return res;
}


template<typename Scalar, int Dim>
typename Transform<Scalar,Dim>::LinearMatrixType
Transform<Scalar,Dim>::rotation() const
{
  LinearMatrixType result;
  computeRotationScaling(&result, (LinearMatrixType*)0);
  return result;
}


template<typename Scalar, int Dim>
template<typename RotationMatrixType, typename ScalingMatrixType>
void Transform<Scalar,Dim>::computeRotationScaling(RotationMatrixType *rotation, ScalingMatrixType *scaling) const
{
  JacobiSVD<LinearMatrixType> svd(linear(), ComputeFullU|ComputeFullV);
  Scalar x = (svd.matrixU() * svd.matrixV().adjoint()).determinant(); 
  Matrix<Scalar, Dim, 1> sv(svd.singularValues());
  sv.coeffRef(0) *= x;
  if(scaling)
  {
    scaling->noalias() = svd.matrixV() * sv.asDiagonal() * svd.matrixV().adjoint();
  }
  if(rotation)
  {
    LinearMatrixType m(svd.matrixU());
    m.col(0) /= x;
    rotation->noalias() = m * svd.matrixV().adjoint();
  }
}

template<typename Scalar, int Dim>
template<typename ScalingMatrixType, typename RotationMatrixType>
void Transform<Scalar,Dim>::computeScalingRotation(ScalingMatrixType *scaling, RotationMatrixType *rotation) const
{
  JacobiSVD<LinearMatrixType> svd(linear(), ComputeFullU|ComputeFullV);
  Scalar x = (svd.matrixU() * svd.matrixV().adjoint()).determinant(); 
  Matrix<Scalar, Dim, 1> sv(svd.singularValues());
  sv.coeffRef(0) *= x;
  if(scaling)
  {
    scaling->noalias() = svd.matrixU() * sv.asDiagonal() * svd.matrixU().adjoint();
  }
  if(rotation)
  {
    LinearMatrixType m(svd.matrixU());
    m.col(0) /= x;
    rotation->noalias() = m * svd.matrixV().adjoint();
  }
}

template<typename Scalar, int Dim>
template<typename PositionDerived, typename OrientationType, typename ScaleDerived>
Transform<Scalar,Dim>&
Transform<Scalar,Dim>::fromPositionOrientationScale(const MatrixBase<PositionDerived> &position,
  const OrientationType& orientation, const MatrixBase<ScaleDerived> &scale)
{
  linear() = ei_toRotationMatrix<Scalar,Dim>(orientation);
  linear() *= scale.asDiagonal();
  translation() = position;
  m_matrix.template block<1,Dim>(Dim,0).setZero();
  m_matrix(Dim,Dim) = Scalar(1);
  return *this;
}

template<typename Scalar, int Dim>
inline const typename Transform<Scalar,Dim>::MatrixType
Transform<Scalar,Dim>::inverse(TransformTraits traits) const
{
  if (traits == Projective)
  {
    return m_matrix.inverse();
  }
  else
  {
    MatrixType res;
    if (traits == Affine)
    {
      res.template corner<Dim,Dim>(TopLeft) = linear().inverse();
    }
    else if (traits == Isometry)
    {
      res.template corner<Dim,Dim>(TopLeft) = linear().transpose();
    }
    else
    {
      ei_assert("invalid traits value in Transform::inverse()");
    }
    
    res.template corner<Dim,1>(TopRight) = - res.template corner<Dim,Dim>(TopLeft) * translation();
    res.template corner<1,Dim>(BottomLeft).setZero();
    res.coeffRef(Dim,Dim) = Scalar(1);
    return res;
  }
}


template<typename Other, int Dim, int HDim>
struct ei_transform_product_impl<Other,Dim,HDim, HDim,HDim>
{
  typedef Transform<typename Other::Scalar,Dim> TransformType;
  typedef typename TransformType::MatrixType MatrixType;
  typedef typename ProductReturnType<MatrixType,Other>::Type ResultType;
  static ResultType run(const TransformType& tr, const Other& other)
  { return tr.matrix() * other; }
};

template<typename Other, int Dim, int HDim>
struct ei_transform_product_impl<Other,Dim,HDim, Dim,Dim>
{
  typedef Transform<typename Other::Scalar,Dim> TransformType;
  typedef typename TransformType::MatrixType MatrixType;
  typedef TransformType ResultType;
  static ResultType run(const TransformType& tr, const Other& other)
  {
    TransformType res;
    res.translation() = tr.translation();
    res.matrix().row(Dim) = tr.matrix().row(Dim);
    res.linear() = (tr.linear() * other).lazy();
    return res;
  }
};

template<typename Other, int Dim, int HDim>
struct ei_transform_product_impl<Other,Dim,HDim, HDim,1>
{
  typedef Transform<typename Other::Scalar,Dim> TransformType;
  typedef typename TransformType::MatrixType MatrixType;
  typedef typename ProductReturnType<MatrixType,Other>::Type ResultType;
  static ResultType run(const TransformType& tr, const Other& other)
  { return tr.matrix() * other; }
};

template<typename Other, int Dim, int HDim>
struct ei_transform_product_impl<Other,Dim,HDim, Dim,1>
{
  typedef typename Other::Scalar Scalar;
  typedef Transform<Scalar,Dim> TransformType;
  typedef Matrix<Scalar,Dim,1> ResultType;
  static ResultType run(const TransformType& tr, const Other& other)
  { return ((tr.linear() * other) + tr.translation())
          * (Scalar(1) / ( (tr.matrix().template block<1,Dim>(Dim,0) * other).coeff(0) + tr.matrix().coeff(Dim,Dim))); }
};

} 
