// Copyright (C) 2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_EIGENBASE_H
#define EIGEN_EIGENBASE_H

namespace Eigen {

template<typename Derived> struct EigenBase
{

  typedef typename internal::traits<Derived>::StorageKind StorageKind;
  typedef typename internal::traits<Derived>::Index Index;

  
  Derived& derived() { return *static_cast<Derived*>(this); }
  
  const Derived& derived() const { return *static_cast<const Derived*>(this); }

  inline Derived& const_cast_derived() const
  { return *static_cast<Derived*>(const_cast<EigenBase*>(this)); }
  inline const Derived& const_derived() const
  { return *static_cast<const Derived*>(this); }

  
  inline Index rows() const { return derived().rows(); }
  
  inline Index cols() const { return derived().cols(); }
  inline Index size() const { return rows() * cols(); }

  
  template<typename Dest> inline void evalTo(Dest& dst) const
  { derived().evalTo(dst); }

  
  template<typename Dest> inline void addTo(Dest& dst) const
  {
    
    
    typename Dest::PlainObject res(rows(),cols());
    evalTo(res);
    dst += res;
  }

  
  template<typename Dest> inline void subTo(Dest& dst) const
  {
    
    
    typename Dest::PlainObject res(rows(),cols());
    evalTo(res);
    dst -= res;
  }

  
  template<typename Dest> inline void applyThisOnTheRight(Dest& dst) const
  {
    
    
    dst = dst * this->derived();
  }

  
  template<typename Dest> inline void applyThisOnTheLeft(Dest& dst) const
  {
    
    
    dst = this->derived() * dst;
  }

};


template<typename Derived>
template<typename OtherDerived>
Derived& DenseBase<Derived>::operator=(const EigenBase<OtherDerived> &other)
{
  other.derived().evalTo(derived());
  return derived();
}

template<typename Derived>
template<typename OtherDerived>
Derived& DenseBase<Derived>::operator+=(const EigenBase<OtherDerived> &other)
{
  other.derived().addTo(derived());
  return derived();
}

template<typename Derived>
template<typename OtherDerived>
Derived& DenseBase<Derived>::operator-=(const EigenBase<OtherDerived> &other)
{
  other.derived().subTo(derived());
  return derived();
}

} 

#endif 
