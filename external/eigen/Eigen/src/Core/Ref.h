// Copyright (C) 2012 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_REF_H
#define EIGEN_REF_H

namespace Eigen { 

template<typename Derived> class RefBase;
template<typename PlainObjectType, int Options = 0,
         typename StrideType = typename internal::conditional<PlainObjectType::IsVectorAtCompileTime,InnerStride<1>,OuterStride<> >::type > class Ref;


namespace internal {

template<typename _PlainObjectType, int _Options, typename _StrideType>
struct traits<Ref<_PlainObjectType, _Options, _StrideType> >
  : public traits<Map<_PlainObjectType, _Options, _StrideType> >
{
  typedef _PlainObjectType PlainObjectType;
  typedef _StrideType StrideType;
  enum {
    Options = _Options,
    Flags = traits<Map<_PlainObjectType, _Options, _StrideType> >::Flags | NestByRefBit
  };

  template<typename Derived> struct match {
    enum {
      HasDirectAccess = internal::has_direct_access<Derived>::ret,
      StorageOrderMatch = PlainObjectType::IsVectorAtCompileTime || Derived::IsVectorAtCompileTime || ((PlainObjectType::Flags&RowMajorBit)==(Derived::Flags&RowMajorBit)),
      InnerStrideMatch = int(StrideType::InnerStrideAtCompileTime)==int(Dynamic)
                      || int(StrideType::InnerStrideAtCompileTime)==int(Derived::InnerStrideAtCompileTime)
                      || (int(StrideType::InnerStrideAtCompileTime)==0 && int(Derived::InnerStrideAtCompileTime)==1),
      OuterStrideMatch = Derived::IsVectorAtCompileTime
                      || int(StrideType::OuterStrideAtCompileTime)==int(Dynamic) || int(StrideType::OuterStrideAtCompileTime)==int(Derived::OuterStrideAtCompileTime),
      AlignmentMatch = (_Options!=Aligned) || ((PlainObjectType::Flags&AlignedBit)==0) || ((traits<Derived>::Flags&AlignedBit)==AlignedBit),
      MatchAtCompileTime = HasDirectAccess && StorageOrderMatch && InnerStrideMatch && OuterStrideMatch && AlignmentMatch
    };
    typedef typename internal::conditional<MatchAtCompileTime,internal::true_type,internal::false_type>::type type;
  };
  
};

template<typename Derived>
struct traits<RefBase<Derived> > : public traits<Derived> {};

}

template<typename Derived> class RefBase
 : public MapBase<Derived>
{
  typedef typename internal::traits<Derived>::PlainObjectType PlainObjectType;
  typedef typename internal::traits<Derived>::StrideType StrideType;

public:

  typedef MapBase<Derived> Base;
  EIGEN_DENSE_PUBLIC_INTERFACE(RefBase)

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

  RefBase()
    : Base(0,RowsAtCompileTime==Dynamic?0:RowsAtCompileTime,ColsAtCompileTime==Dynamic?0:ColsAtCompileTime),
      
      m_stride(StrideType::OuterStrideAtCompileTime==Dynamic?0:StrideType::OuterStrideAtCompileTime,
               StrideType::InnerStrideAtCompileTime==Dynamic?0:StrideType::InnerStrideAtCompileTime)
  {}
  
  EIGEN_INHERIT_ASSIGNMENT_OPERATORS(RefBase)

protected:

  typedef Stride<StrideType::OuterStrideAtCompileTime,StrideType::InnerStrideAtCompileTime> StrideBase;

  template<typename Expression>
  void construct(Expression& expr)
  {
    if(PlainObjectType::RowsAtCompileTime==1)
    {
      eigen_assert(expr.rows()==1 || expr.cols()==1);
      ::new (static_cast<Base*>(this)) Base(expr.data(), 1, expr.size());
    }
    else if(PlainObjectType::ColsAtCompileTime==1)
    {
      eigen_assert(expr.rows()==1 || expr.cols()==1);
      ::new (static_cast<Base*>(this)) Base(expr.data(), expr.size(), 1);
    }
    else
      ::new (static_cast<Base*>(this)) Base(expr.data(), expr.rows(), expr.cols());
    
    if(Expression::IsVectorAtCompileTime && (!PlainObjectType::IsVectorAtCompileTime) && ((Expression::Flags&RowMajorBit)!=(PlainObjectType::Flags&RowMajorBit)))
      ::new (&m_stride) StrideBase(expr.innerStride(), StrideType::InnerStrideAtCompileTime==0?0:1);
    else
      ::new (&m_stride) StrideBase(StrideType::OuterStrideAtCompileTime==0?0:expr.outerStride(),
                                   StrideType::InnerStrideAtCompileTime==0?0:expr.innerStride());    
  }

  StrideBase m_stride;
};


template<typename PlainObjectType, int Options, typename StrideType> class Ref
  : public RefBase<Ref<PlainObjectType, Options, StrideType> >
{
    typedef internal::traits<Ref> Traits;
    template<typename Derived>
    inline Ref(const PlainObjectBase<Derived>& expr);
  public:

    typedef RefBase<Ref> Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(Ref)


    #ifndef EIGEN_PARSED_BY_DOXYGEN
    template<typename Derived>
    inline Ref(PlainObjectBase<Derived>& expr)
    {
      EIGEN_STATIC_ASSERT(static_cast<bool>(Traits::template match<Derived>::MatchAtCompileTime), STORAGE_LAYOUT_DOES_NOT_MATCH);
      Base::construct(expr.derived());
    }
    template<typename Derived>
    inline Ref(const DenseBase<Derived>& expr)
    #else
    template<typename Derived>
    inline Ref(DenseBase<Derived>& expr)
    #endif
    {
      EIGEN_STATIC_ASSERT(static_cast<bool>(internal::is_lvalue<Derived>::value), THIS_EXPRESSION_IS_NOT_A_LVALUE__IT_IS_READ_ONLY);
      EIGEN_STATIC_ASSERT(static_cast<bool>(Traits::template match<Derived>::MatchAtCompileTime), STORAGE_LAYOUT_DOES_NOT_MATCH);
      enum { THIS_EXPRESSION_IS_NOT_A_LVALUE__IT_IS_READ_ONLY = Derived::ThisConstantIsPrivateInPlainObjectBase};
      Base::construct(expr.const_cast_derived());
    }

    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(Ref)

};

template<typename TPlainObjectType, int Options, typename StrideType> class Ref<const TPlainObjectType, Options, StrideType>
  : public RefBase<Ref<const TPlainObjectType, Options, StrideType> >
{
    typedef internal::traits<Ref> Traits;
  public:

    typedef RefBase<Ref> Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(Ref)

    template<typename Derived>
    inline Ref(const DenseBase<Derived>& expr)
    {
      construct(expr.derived(), typename Traits::template match<Derived>::type());
    }

  protected:

    template<typename Expression>
    void construct(const Expression& expr,internal::true_type)
    {
      Base::construct(expr);
    }

    template<typename Expression>
    void construct(const Expression& expr, internal::false_type)
    {
      m_object.lazyAssign(expr);
      Base::construct(m_object);
    }

  protected:
    TPlainObjectType m_object;
};

} 

#endif 
