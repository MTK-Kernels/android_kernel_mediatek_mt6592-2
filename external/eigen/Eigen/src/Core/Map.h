// Copyright (C) 2007-2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_MAP_H
#define EIGEN_MAP_H

namespace Eigen { 


namespace internal {
template<typename PlainObjectType, int MapOptions, typename StrideType>
struct traits<Map<PlainObjectType, MapOptions, StrideType> >
  : public traits<PlainObjectType>
{
  typedef traits<PlainObjectType> TraitsBase;
  typedef typename PlainObjectType::Index Index;
  typedef typename PlainObjectType::Scalar Scalar;
  enum {
    InnerStrideAtCompileTime = StrideType::InnerStrideAtCompileTime == 0
                             ? int(PlainObjectType::InnerStrideAtCompileTime)
                             : int(StrideType::InnerStrideAtCompileTime),
    OuterStrideAtCompileTime = StrideType::OuterStrideAtCompileTime == 0
                             ? int(PlainObjectType::OuterStrideAtCompileTime)
                             : int(StrideType::OuterStrideAtCompileTime),
    HasNoInnerStride = InnerStrideAtCompileTime == 1,
    HasNoOuterStride = StrideType::OuterStrideAtCompileTime == 0,
    HasNoStride = HasNoInnerStride && HasNoOuterStride,
    IsAligned = bool(EIGEN_ALIGN) && ((int(MapOptions)&Aligned)==Aligned),
    IsDynamicSize = PlainObjectType::SizeAtCompileTime==Dynamic,
    KeepsPacketAccess = bool(HasNoInnerStride)
                        && ( bool(IsDynamicSize)
                           || HasNoOuterStride
                           || ( OuterStrideAtCompileTime!=Dynamic
                           && ((static_cast<int>(sizeof(Scalar))*OuterStrideAtCompileTime)%16)==0 ) ),
    Flags0 = TraitsBase::Flags & (~NestByRefBit),
    Flags1 = IsAligned ? (int(Flags0) | AlignedBit) : (int(Flags0) & ~AlignedBit),
    Flags2 = (bool(HasNoStride) || bool(PlainObjectType::IsVectorAtCompileTime))
           ? int(Flags1) : int(Flags1 & ~LinearAccessBit),
    Flags3 = is_lvalue<PlainObjectType>::value ? int(Flags2) : (int(Flags2) & ~LvalueBit),
    Flags = KeepsPacketAccess ? int(Flags3) : (int(Flags3) & ~PacketAccessBit)
  };
private:
  enum { Options }; 
};
}

template<typename PlainObjectType, int MapOptions, typename StrideType> class Map
  : public MapBase<Map<PlainObjectType, MapOptions, StrideType> >
{
  public:

    typedef MapBase<Map> Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(Map)

    typedef typename Base::PointerType PointerType;
#if EIGEN2_SUPPORT_STAGE <= STAGE30_FULL_EIGEN3_API
    typedef const Scalar* PointerArgType;
    inline PointerType cast_to_pointer_type(PointerArgType ptr) { return const_cast<PointerType>(ptr); }
#else
    typedef PointerType PointerArgType;
    inline PointerType cast_to_pointer_type(PointerArgType ptr) { return ptr; }
#endif

    inline Index innerStride() const
    {
      return StrideType::InnerStrideAtCompileTime != 0 ? m_stride.inner() : 1;
    }

    inline Index outerStride() const
    {
      return StrideType::OuterStrideAtCompileTime != 0 ? m_stride.outer()
           : IsVectorAtCompileTime ? this->size()
           : int(Flags)&RowMajorBit ? this->cols()
           : this->rows();
    }

    inline Map(PointerArgType dataPtr, const StrideType& a_stride = StrideType())
      : Base(cast_to_pointer_type(dataPtr)), m_stride(a_stride)
    {
      PlainObjectType::Base::_check_template_params();
    }

    inline Map(PointerArgType dataPtr, Index a_size, const StrideType& a_stride = StrideType())
      : Base(cast_to_pointer_type(dataPtr), a_size), m_stride(a_stride)
    {
      PlainObjectType::Base::_check_template_params();
    }

    inline Map(PointerArgType dataPtr, Index nbRows, Index nbCols, const StrideType& a_stride = StrideType())
      : Base(cast_to_pointer_type(dataPtr), nbRows, nbCols), m_stride(a_stride)
    {
      PlainObjectType::Base::_check_template_params();
    }

    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(Map)

  protected:
    StrideType m_stride;
};

template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
inline Array<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>
  ::Array(const Scalar *data)
{
  this->_set_noalias(Eigen::Map<const Array>(data));
}

template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
inline Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>
  ::Matrix(const Scalar *data)
{
  this->_set_noalias(Eigen::Map<const Matrix>(data));
}

} 

#endif 
