// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ROTATIONBASE_H
#define EIGEN_ROTATIONBASE_H

namespace Eigen { 

namespace internal {
template<typename RotationDerived, typename MatrixType, bool IsVector=MatrixType::IsVectorAtCompileTime>
struct rotation_base_generic_product_selector;
}

template<typename Derived, int _Dim>
class RotationBase
{
  public:
    enum { Dim = _Dim };
    
    typedef typename internal::traits<Derived>::Scalar Scalar;

    
    typedef Matrix<Scalar,Dim,Dim> RotationMatrixType;
    typedef Matrix<Scalar,Dim,1> VectorType;

  public:
    inline const Derived& derived() const { return *static_cast<const Derived*>(this); }
    inline Derived& derived() { return *static_cast<Derived*>(this); }

    
    inline RotationMatrixType toRotationMatrix() const { return derived().toRotationMatrix(); }

    inline RotationMatrixType matrix() const { return derived().toRotationMatrix(); }

    
    inline Derived inverse() const { return derived().inverse(); }

    
    inline Transform<Scalar,Dim,Isometry> operator*(const Translation<Scalar,Dim>& t) const
    { return Transform<Scalar,Dim,Isometry>(*this) * t; }

    
    inline RotationMatrixType operator*(const UniformScaling<Scalar>& s) const
    { return toRotationMatrix() * s.factor(); }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE typename internal::rotation_base_generic_product_selector<Derived,OtherDerived,OtherDerived::IsVectorAtCompileTime>::ReturnType
    operator*(const EigenBase<OtherDerived>& e) const
    { return internal::rotation_base_generic_product_selector<Derived,OtherDerived>::run(derived(), e.derived()); }

    
    template<typename OtherDerived> friend
    inline RotationMatrixType operator*(const EigenBase<OtherDerived>& l, const Derived& r)
    { return l.derived() * r.toRotationMatrix(); }

    
    friend inline Transform<Scalar,Dim,Affine> operator*(const DiagonalMatrix<Scalar,Dim>& l, const Derived& r)
    { 
      Transform<Scalar,Dim,Affine> res(r);
      res.linear().applyOnTheLeft(l);
      return res;
    }

    
    template<int Mode, int Options>
    inline Transform<Scalar,Dim,Mode> operator*(const Transform<Scalar,Dim,Mode,Options>& t) const
    { return toRotationMatrix() * t; }

    template<typename OtherVectorType>
    inline VectorType _transformVector(const OtherVectorType& v) const
    { return toRotationMatrix() * v; }
};

namespace internal {

template<typename RotationDerived, typename MatrixType>
struct rotation_base_generic_product_selector<RotationDerived,MatrixType,false>
{
  enum { Dim = RotationDerived::Dim };
  typedef Matrix<typename RotationDerived::Scalar,Dim,Dim> ReturnType;
  static inline ReturnType run(const RotationDerived& r, const MatrixType& m)
  { return r.toRotationMatrix() * m; }
};

template<typename RotationDerived, typename Scalar, int Dim, int MaxDim>
struct rotation_base_generic_product_selector< RotationDerived, DiagonalMatrix<Scalar,Dim,MaxDim>, false >
{
  typedef Transform<Scalar,Dim,Affine> ReturnType;
  static inline ReturnType run(const RotationDerived& r, const DiagonalMatrix<Scalar,Dim,MaxDim>& m)
  {
    ReturnType res(r);
    res.linear() *= m;
    return res;
  }
};

template<typename RotationDerived,typename OtherVectorType>
struct rotation_base_generic_product_selector<RotationDerived,OtherVectorType,true>
{
  enum { Dim = RotationDerived::Dim };
  typedef Matrix<typename RotationDerived::Scalar,Dim,1> ReturnType;
  static EIGEN_STRONG_INLINE ReturnType run(const RotationDerived& r, const OtherVectorType& v)
  {
    return r._transformVector(v);
  }
};

} 

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

namespace internal {

template<typename Scalar, int Dim>
static inline Matrix<Scalar,2,2> toRotationMatrix(const Scalar& s)
{
  EIGEN_STATIC_ASSERT(Dim==2,YOU_MADE_A_PROGRAMMING_MISTAKE)
  return Rotation2D<Scalar>(s).toRotationMatrix();
}

template<typename Scalar, int Dim, typename OtherDerived>
static inline Matrix<Scalar,Dim,Dim> toRotationMatrix(const RotationBase<OtherDerived,Dim>& r)
{
  return r.toRotationMatrix();
}

template<typename Scalar, int Dim, typename OtherDerived>
static inline const MatrixBase<OtherDerived>& toRotationMatrix(const MatrixBase<OtherDerived>& mat)
{
  EIGEN_STATIC_ASSERT(OtherDerived::RowsAtCompileTime==Dim && OtherDerived::ColsAtCompileTime==Dim,
    YOU_MADE_A_PROGRAMMING_MISTAKE)
  return mat;
}

} 

} 

#endif 
