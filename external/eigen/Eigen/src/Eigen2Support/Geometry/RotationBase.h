// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed


namespace Eigen { 


template<typename Derived, int _Dim>
class RotationBase
{
  public:
    enum { Dim = _Dim };
    
    typedef typename ei_traits<Derived>::Scalar Scalar;
    
    
    typedef Matrix<Scalar,Dim,Dim> RotationMatrixType;

    inline const Derived& derived() const { return *static_cast<const Derived*>(this); }
    inline Derived& derived() { return *static_cast<Derived*>(this); }

    
    inline RotationMatrixType toRotationMatrix() const { return derived().toRotationMatrix(); }

    
    inline Derived inverse() const { return derived().inverse(); }

    
    inline Transform<Scalar,Dim> operator*(const Translation<Scalar,Dim>& t) const
    { return toRotationMatrix() * t; }

    
    inline RotationMatrixType operator*(const Scaling<Scalar,Dim>& s) const
    { return toRotationMatrix() * s; }

    
    inline Transform<Scalar,Dim> operator*(const Transform<Scalar,Dim>& t) const
    { return toRotationMatrix() * t; }
};

template<typename _Scalar, int _Rows, int _Cols, int _Storage, int _MaxRows, int _MaxCols>
template<typename OtherDerived>
Matrix<_Scalar, _Rows, _Cols, _Storage, _MaxRows, _MaxCols>
::Matrix(const RotationBase<OtherDerived,ColsAtCompileTime>& r)
{
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE(Matrix,int(OtherDerived::Dim),int(OtherDerived::Dim))
  *this = r.toRotationMatrix();
}

template<typename _Scalar, int _Rows, int _Cols, int _Storage, int _MaxRows, int _MaxCols>
template<typename OtherDerived>
Matrix<_Scalar, _Rows, _Cols, _Storage, _MaxRows, _MaxCols>&
Matrix<_Scalar, _Rows, _Cols, _Storage, _MaxRows, _MaxCols>
::operator=(const RotationBase<OtherDerived,ColsAtCompileTime>& r)
{
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE(Matrix,int(OtherDerived::Dim),int(OtherDerived::Dim))
  return *this = r.toRotationMatrix();
}

template<typename Scalar, int Dim>
static inline Matrix<Scalar,2,2> ei_toRotationMatrix(const Scalar& s)
{
  EIGEN_STATIC_ASSERT(Dim==2,YOU_MADE_A_PROGRAMMING_MISTAKE)
  return Rotation2D<Scalar>(s).toRotationMatrix();
}

template<typename Scalar, int Dim, typename OtherDerived>
static inline Matrix<Scalar,Dim,Dim> ei_toRotationMatrix(const RotationBase<OtherDerived,Dim>& r)
{
  return r.toRotationMatrix();
}

template<typename Scalar, int Dim, typename OtherDerived>
static inline const MatrixBase<OtherDerived>& ei_toRotationMatrix(const MatrixBase<OtherDerived>& mat)
{
  EIGEN_STATIC_ASSERT(OtherDerived::RowsAtCompileTime==Dim && OtherDerived::ColsAtCompileTime==Dim,
    YOU_MADE_A_PROGRAMMING_MISTAKE)
  return mat;
}

} 
