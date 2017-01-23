// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_BLOCK2_H
#define EIGEN_BLOCK2_H

namespace Eigen { 

template<typename Derived>
inline Block<Derived> DenseBase<Derived>
  ::corner(CornerType type, Index cRows, Index cCols)
{
  switch(type)
  {
    default:
      eigen_assert(false && "Bad corner type.");
    case TopLeft:
      return Block<Derived>(derived(), 0, 0, cRows, cCols);
    case TopRight:
      return Block<Derived>(derived(), 0, cols() - cCols, cRows, cCols);
    case BottomLeft:
      return Block<Derived>(derived(), rows() - cRows, 0, cRows, cCols);
    case BottomRight:
      return Block<Derived>(derived(), rows() - cRows, cols() - cCols, cRows, cCols);
  }
}

template<typename Derived>
inline const Block<Derived>
DenseBase<Derived>::corner(CornerType type, Index cRows, Index cCols) const
{
  switch(type)
  {
    default:
      eigen_assert(false && "Bad corner type.");
    case TopLeft:
      return Block<Derived>(derived(), 0, 0, cRows, cCols);
    case TopRight:
      return Block<Derived>(derived(), 0, cols() - cCols, cRows, cCols);
    case BottomLeft:
      return Block<Derived>(derived(), rows() - cRows, 0, cRows, cCols);
    case BottomRight:
      return Block<Derived>(derived(), rows() - cRows, cols() - cCols, cRows, cCols);
  }
}

template<typename Derived>
template<int CRows, int CCols>
inline Block<Derived, CRows, CCols>
DenseBase<Derived>::corner(CornerType type)
{
  switch(type)
  {
    default:
      eigen_assert(false && "Bad corner type.");
    case TopLeft:
      return Block<Derived, CRows, CCols>(derived(), 0, 0);
    case TopRight:
      return Block<Derived, CRows, CCols>(derived(), 0, cols() - CCols);
    case BottomLeft:
      return Block<Derived, CRows, CCols>(derived(), rows() - CRows, 0);
    case BottomRight:
      return Block<Derived, CRows, CCols>(derived(), rows() - CRows, cols() - CCols);
  }
}

template<typename Derived>
template<int CRows, int CCols>
inline const Block<Derived, CRows, CCols>
DenseBase<Derived>::corner(CornerType type) const
{
  switch(type)
  {
    default:
      eigen_assert(false && "Bad corner type.");
    case TopLeft:
      return Block<Derived, CRows, CCols>(derived(), 0, 0);
    case TopRight:
      return Block<Derived, CRows, CCols>(derived(), 0, cols() - CCols);
    case BottomLeft:
      return Block<Derived, CRows, CCols>(derived(), rows() - CRows, 0);
    case BottomRight:
      return Block<Derived, CRows, CCols>(derived(), rows() - CRows, cols() - CCols);
  }
}

} 

#endif 
