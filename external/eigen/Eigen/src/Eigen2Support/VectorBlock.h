// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN2_VECTORBLOCK_H
#define EIGEN2_VECTORBLOCK_H

namespace Eigen { 

template<typename Derived>
inline VectorBlock<Derived>
MatrixBase<Derived>::start(Index size)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<Derived>(derived(), 0, size);
}

template<typename Derived>
inline const VectorBlock<const Derived>
MatrixBase<Derived>::start(Index size) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<const Derived>(derived(), 0, size);
}

template<typename Derived>
inline VectorBlock<Derived>
MatrixBase<Derived>::end(Index size)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<Derived>(derived(), this->size() - size, size);
}

template<typename Derived>
inline const VectorBlock<const Derived>
MatrixBase<Derived>::end(Index size) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<const Derived>(derived(), this->size() - size, size);
}

template<typename Derived>
template<int Size>
inline VectorBlock<Derived,Size>
MatrixBase<Derived>::start()
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<Derived,Size>(derived(), 0);
}

template<typename Derived>
template<int Size>
inline const VectorBlock<const Derived,Size>
MatrixBase<Derived>::start() const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<const Derived,Size>(derived(), 0);
}

template<typename Derived>
template<int Size>
inline VectorBlock<Derived,Size>
MatrixBase<Derived>::end()
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<Derived, Size>(derived(), size() - Size);
}

template<typename Derived>
template<int Size>
inline const VectorBlock<const Derived,Size>
MatrixBase<Derived>::end() const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return VectorBlock<const Derived, Size>(derived(), size() - Size);
}

} 

#endif 
