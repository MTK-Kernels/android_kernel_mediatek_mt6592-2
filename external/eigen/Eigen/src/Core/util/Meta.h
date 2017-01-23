// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_META_H
#define EIGEN_META_H

namespace Eigen {

namespace internal {


struct true_type {  enum { value = 1 }; };
struct false_type { enum { value = 0 }; };

template<bool Condition, typename Then, typename Else>
struct conditional { typedef Then type; };

template<typename Then, typename Else>
struct conditional <false, Then, Else> { typedef Else type; };

template<typename T, typename U> struct is_same { enum { value = 0 }; };
template<typename T> struct is_same<T,T> { enum { value = 1 }; };

template<typename T> struct remove_reference { typedef T type; };
template<typename T> struct remove_reference<T&> { typedef T type; };

template<typename T> struct remove_pointer { typedef T type; };
template<typename T> struct remove_pointer<T*> { typedef T type; };
template<typename T> struct remove_pointer<T*const> { typedef T type; };

template <class T> struct remove_const { typedef T type; };
template <class T> struct remove_const<const T> { typedef T type; };
template <class T> struct remove_const<const T[]> { typedef T type[]; };
template <class T, unsigned int Size> struct remove_const<const T[Size]> { typedef T type[Size]; };

template<typename T> struct remove_all { typedef T type; };
template<typename T> struct remove_all<const T>   { typedef typename remove_all<T>::type type; };
template<typename T> struct remove_all<T const&>  { typedef typename remove_all<T>::type type; };
template<typename T> struct remove_all<T&>        { typedef typename remove_all<T>::type type; };
template<typename T> struct remove_all<T const*>  { typedef typename remove_all<T>::type type; };
template<typename T> struct remove_all<T*>        { typedef typename remove_all<T>::type type; };

template<typename T> struct is_arithmetic      { enum { value = false }; };
template<> struct is_arithmetic<float>         { enum { value = true }; };
template<> struct is_arithmetic<double>        { enum { value = true }; };
template<> struct is_arithmetic<long double>   { enum { value = true }; };
template<> struct is_arithmetic<bool>          { enum { value = true }; };
template<> struct is_arithmetic<char>          { enum { value = true }; };
template<> struct is_arithmetic<signed char>   { enum { value = true }; };
template<> struct is_arithmetic<unsigned char> { enum { value = true }; };
template<> struct is_arithmetic<signed short>  { enum { value = true }; };
template<> struct is_arithmetic<unsigned short>{ enum { value = true }; };
template<> struct is_arithmetic<signed int>    { enum { value = true }; };
template<> struct is_arithmetic<unsigned int>  { enum { value = true }; };
template<> struct is_arithmetic<signed long>   { enum { value = true }; };
template<> struct is_arithmetic<unsigned long> { enum { value = true }; };

template <typename T> struct add_const { typedef const T type; };
template <typename T> struct add_const<T&> { typedef T& type; };

template <typename T> struct is_const { enum { value = 0 }; };
template <typename T> struct is_const<T const> { enum { value = 1 }; };

template<typename T> struct add_const_on_value_type            { typedef const T type;  };
template<typename T> struct add_const_on_value_type<T&>        { typedef T const& type; };
template<typename T> struct add_const_on_value_type<T*>        { typedef T const* type; };
template<typename T> struct add_const_on_value_type<T* const>  { typedef T const* const type; };
template<typename T> struct add_const_on_value_type<T const* const>  { typedef T const* const type; };

template<bool Condition, typename T> struct enable_if;

template<typename T> struct enable_if<true,T>
{ typedef T type; };



class noncopyable
{
  noncopyable(const noncopyable&);
  const noncopyable& operator=(const noncopyable&);
protected:
  noncopyable() {}
  ~noncopyable() {}
};


template<typename T> struct result_of {};

struct has_none {int a[1];};
struct has_std_result_type {int a[2];};
struct has_tr1_result {int a[3];};

template<typename Func, typename ArgType, int SizeOf=sizeof(has_none)>
struct unary_result_of_select {typedef ArgType type;};

template<typename Func, typename ArgType>
struct unary_result_of_select<Func, ArgType, sizeof(has_std_result_type)> {typedef typename Func::result_type type;};

template<typename Func, typename ArgType>
struct unary_result_of_select<Func, ArgType, sizeof(has_tr1_result)> {typedef typename Func::template result<Func(ArgType)>::type type;};

template<typename Func, typename ArgType>
struct result_of<Func(ArgType)> {
    template<typename T>
    static has_std_result_type testFunctor(T const *, typename T::result_type const * = 0);
    template<typename T>
    static has_tr1_result      testFunctor(T const *, typename T::template result<T(ArgType)>::type const * = 0);
    static has_none            testFunctor(...);

    
    enum {FunctorType = sizeof(testFunctor(static_cast<Func*>(0)))};
    typedef typename unary_result_of_select<Func, ArgType, FunctorType>::type type;
};

template<typename Func, typename ArgType0, typename ArgType1, int SizeOf=sizeof(has_none)>
struct binary_result_of_select {typedef ArgType0 type;};

template<typename Func, typename ArgType0, typename ArgType1>
struct binary_result_of_select<Func, ArgType0, ArgType1, sizeof(has_std_result_type)>
{typedef typename Func::result_type type;};

template<typename Func, typename ArgType0, typename ArgType1>
struct binary_result_of_select<Func, ArgType0, ArgType1, sizeof(has_tr1_result)>
{typedef typename Func::template result<Func(ArgType0,ArgType1)>::type type;};

template<typename Func, typename ArgType0, typename ArgType1>
struct result_of<Func(ArgType0,ArgType1)> {
    template<typename T>
    static has_std_result_type testFunctor(T const *, typename T::result_type const * = 0);
    template<typename T>
    static has_tr1_result      testFunctor(T const *, typename T::template result<T(ArgType0,ArgType1)>::type const * = 0);
    static has_none            testFunctor(...);

    
    enum {FunctorType = sizeof(testFunctor(static_cast<Func*>(0)))};
    typedef typename binary_result_of_select<Func, ArgType0, ArgType1, FunctorType>::type type;
};

template<int Y,
         int InfX = 0,
         int SupX = ((Y==1) ? 1 : Y/2),
         bool Done = ((SupX-InfX)<=1 ? true : ((SupX*SupX <= Y) && ((SupX+1)*(SupX+1) > Y))) >
                                
class meta_sqrt
{
    enum {
      MidX = (InfX+SupX)/2,
      TakeInf = MidX*MidX > Y ? 1 : 0,
      NewInf = int(TakeInf) ? InfX : int(MidX),
      NewSup = int(TakeInf) ? int(MidX) : SupX
    };
  public:
    enum { ret = meta_sqrt<Y,NewInf,NewSup>::ret };
};

template<int Y, int InfX, int SupX>
class meta_sqrt<Y, InfX, SupX, true> { public:  enum { ret = (SupX*SupX <= Y) ? SupX : InfX }; };

template<typename T, typename U> struct scalar_product_traits
{
  enum { Defined = 0 };
};

template<typename T> struct scalar_product_traits<T,T>
{
  enum {
    
    Defined = 1
  };
  typedef T ReturnType;
};

template<typename T> struct scalar_product_traits<T,std::complex<T> >
{
  enum {
    
    Defined = 1
  };
  typedef std::complex<T> ReturnType;
};

template<typename T> struct scalar_product_traits<std::complex<T>, T>
{
  enum {
    
    Defined = 1
  };
  typedef std::complex<T> ReturnType;
};


template<typename T> struct is_diagonal
{ enum { ret = false }; };

template<typename T> struct is_diagonal<DiagonalBase<T> >
{ enum { ret = true }; };

template<typename T> struct is_diagonal<DiagonalWrapper<T> >
{ enum { ret = true }; };

template<typename T, int S> struct is_diagonal<DiagonalMatrix<T,S> >
{ enum { ret = true }; };

} 

} 

#endif 
