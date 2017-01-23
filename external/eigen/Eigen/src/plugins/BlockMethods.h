// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed


#ifndef EIGEN_PARSED_BY_DOXYGEN

typedef Block<Derived, internal::traits<Derived>::RowsAtCompileTime, 1, !IsRowMajor> ColXpr;
typedef const Block<const Derived, internal::traits<Derived>::RowsAtCompileTime, 1, !IsRowMajor> ConstColXpr;
typedef Block<Derived, 1, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> RowXpr;
typedef const Block<const Derived, 1, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> ConstRowXpr;
typedef Block<Derived, internal::traits<Derived>::RowsAtCompileTime, Dynamic, !IsRowMajor> ColsBlockXpr;
typedef const Block<const Derived, internal::traits<Derived>::RowsAtCompileTime, Dynamic, !IsRowMajor> ConstColsBlockXpr;
typedef Block<Derived, Dynamic, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> RowsBlockXpr;
typedef const Block<const Derived, Dynamic, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> ConstRowsBlockXpr;
template<int N> struct NColsBlockXpr { typedef Block<Derived, internal::traits<Derived>::RowsAtCompileTime, N, !IsRowMajor> Type; };
template<int N> struct ConstNColsBlockXpr { typedef const Block<const Derived, internal::traits<Derived>::RowsAtCompileTime, N, !IsRowMajor> Type; };
template<int N> struct NRowsBlockXpr { typedef Block<Derived, N, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> Type; };
template<int N> struct ConstNRowsBlockXpr { typedef const Block<const Derived, N, internal::traits<Derived>::ColsAtCompileTime, IsRowMajor> Type; };

typedef VectorBlock<Derived> SegmentReturnType;
typedef const VectorBlock<const Derived> ConstSegmentReturnType;
template<int Size> struct FixedSegmentReturnType { typedef VectorBlock<Derived, Size> Type; };
template<int Size> struct ConstFixedSegmentReturnType { typedef const VectorBlock<const Derived, Size> Type; };

#endif 

inline Block<Derived> block(Index startRow, Index startCol, Index blockRows, Index blockCols)
{
  return Block<Derived>(derived(), startRow, startCol, blockRows, blockCols);
}

inline const Block<const Derived> block(Index startRow, Index startCol, Index blockRows, Index blockCols) const
{
  return Block<const Derived>(derived(), startRow, startCol, blockRows, blockCols);
}




inline Block<Derived> topRightCorner(Index cRows, Index cCols)
{
  return Block<Derived>(derived(), 0, cols() - cCols, cRows, cCols);
}

inline const Block<const Derived> topRightCorner(Index cRows, Index cCols) const
{
  return Block<const Derived>(derived(), 0, cols() - cCols, cRows, cCols);
}

template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> topRightCorner()
{
  return Block<Derived, CRows, CCols>(derived(), 0, cols() - CCols);
}

template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> topRightCorner() const
{
  return Block<const Derived, CRows, CCols>(derived(), 0, cols() - CCols);
}

template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> topRightCorner(Index cRows, Index cCols)
{
  return Block<Derived, CRows, CCols>(derived(), 0, cols() - cCols, cRows, cCols);
}

template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> topRightCorner(Index cRows, Index cCols) const
{
  return Block<const Derived, CRows, CCols>(derived(), 0, cols() - cCols, cRows, cCols);
}



inline Block<Derived> topLeftCorner(Index cRows, Index cCols)
{
  return Block<Derived>(derived(), 0, 0, cRows, cCols);
}

inline const Block<const Derived> topLeftCorner(Index cRows, Index cCols) const
{
  return Block<const Derived>(derived(), 0, 0, cRows, cCols);
}

template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> topLeftCorner()
{
  return Block<Derived, CRows, CCols>(derived(), 0, 0);
}

template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> topLeftCorner() const
{
  return Block<const Derived, CRows, CCols>(derived(), 0, 0);
}

template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> topLeftCorner(Index cRows, Index cCols)
{
  return Block<Derived, CRows, CCols>(derived(), 0, 0, cRows, cCols);
}

template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> topLeftCorner(Index cRows, Index cCols) const
{
  return Block<const Derived, CRows, CCols>(derived(), 0, 0, cRows, cCols);
}



inline Block<Derived> bottomRightCorner(Index cRows, Index cCols)
{
  return Block<Derived>(derived(), rows() - cRows, cols() - cCols, cRows, cCols);
}

inline const Block<const Derived> bottomRightCorner(Index cRows, Index cCols) const
{
  return Block<const Derived>(derived(), rows() - cRows, cols() - cCols, cRows, cCols);
}

template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> bottomRightCorner()
{
  return Block<Derived, CRows, CCols>(derived(), rows() - CRows, cols() - CCols);
}

template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> bottomRightCorner() const
{
  return Block<const Derived, CRows, CCols>(derived(), rows() - CRows, cols() - CCols);
}

template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> bottomRightCorner(Index cRows, Index cCols)
{
  return Block<Derived, CRows, CCols>(derived(), rows() - cRows, cols() - cCols, cRows, cCols);
}

template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> bottomRightCorner(Index cRows, Index cCols) const
{
  return Block<const Derived, CRows, CCols>(derived(), rows() - cRows, cols() - cCols, cRows, cCols);
}



inline Block<Derived> bottomLeftCorner(Index cRows, Index cCols)
{
  return Block<Derived>(derived(), rows() - cRows, 0, cRows, cCols);
}

inline const Block<const Derived> bottomLeftCorner(Index cRows, Index cCols) const
{
  return Block<const Derived>(derived(), rows() - cRows, 0, cRows, cCols);
}

template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> bottomLeftCorner()
{
  return Block<Derived, CRows, CCols>(derived(), rows() - CRows, 0);
}

template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> bottomLeftCorner() const
{
  return Block<const Derived, CRows, CCols>(derived(), rows() - CRows, 0);
}

template<int CRows, int CCols>
inline Block<Derived, CRows, CCols> bottomLeftCorner(Index cRows, Index cCols)
{
  return Block<Derived, CRows, CCols>(derived(), rows() - cRows, 0, cRows, cCols);
}

template<int CRows, int CCols>
inline const Block<const Derived, CRows, CCols> bottomLeftCorner(Index cRows, Index cCols) const
{
  return Block<const Derived, CRows, CCols>(derived(), rows() - cRows, 0, cRows, cCols);
}



inline RowsBlockXpr topRows(Index n)
{
  return RowsBlockXpr(derived(), 0, 0, n, cols());
}

inline ConstRowsBlockXpr topRows(Index n) const
{
  return ConstRowsBlockXpr(derived(), 0, 0, n, cols());
}

template<int N>
inline typename NRowsBlockXpr<N>::Type topRows(Index n = N)
{
  return typename NRowsBlockXpr<N>::Type(derived(), 0, 0, n, cols());
}

template<int N>
inline typename ConstNRowsBlockXpr<N>::Type topRows(Index n = N) const
{
  return typename ConstNRowsBlockXpr<N>::Type(derived(), 0, 0, n, cols());
}



inline RowsBlockXpr bottomRows(Index n)
{
  return RowsBlockXpr(derived(), rows() - n, 0, n, cols());
}

inline ConstRowsBlockXpr bottomRows(Index n) const
{
  return ConstRowsBlockXpr(derived(), rows() - n, 0, n, cols());
}

template<int N>
inline typename NRowsBlockXpr<N>::Type bottomRows(Index n = N)
{
  return typename NRowsBlockXpr<N>::Type(derived(), rows() - n, 0, n, cols());
}

template<int N>
inline typename ConstNRowsBlockXpr<N>::Type bottomRows(Index n = N) const
{
  return typename ConstNRowsBlockXpr<N>::Type(derived(), rows() - n, 0, n, cols());
}



inline RowsBlockXpr middleRows(Index startRow, Index n)
{
  return RowsBlockXpr(derived(), startRow, 0, n, cols());
}

inline ConstRowsBlockXpr middleRows(Index startRow, Index n) const
{
  return ConstRowsBlockXpr(derived(), startRow, 0, n, cols());
}

template<int N>
inline typename NRowsBlockXpr<N>::Type middleRows(Index startRow, Index n = N)
{
  return typename NRowsBlockXpr<N>::Type(derived(), startRow, 0, n, cols());
}

template<int N>
inline typename ConstNRowsBlockXpr<N>::Type middleRows(Index startRow, Index n = N) const
{
  return typename ConstNRowsBlockXpr<N>::Type(derived(), startRow, 0, n, cols());
}



inline ColsBlockXpr leftCols(Index n)
{
  return ColsBlockXpr(derived(), 0, 0, rows(), n);
}

inline ConstColsBlockXpr leftCols(Index n) const
{
  return ConstColsBlockXpr(derived(), 0, 0, rows(), n);
}

template<int N>
inline typename NColsBlockXpr<N>::Type leftCols(Index n = N)
{
  return typename NColsBlockXpr<N>::Type(derived(), 0, 0, rows(), n);
}

template<int N>
inline typename ConstNColsBlockXpr<N>::Type leftCols(Index n = N) const
{
  return typename ConstNColsBlockXpr<N>::Type(derived(), 0, 0, rows(), n);
}



inline ColsBlockXpr rightCols(Index n)
{
  return ColsBlockXpr(derived(), 0, cols() - n, rows(), n);
}

inline ConstColsBlockXpr rightCols(Index n) const
{
  return ConstColsBlockXpr(derived(), 0, cols() - n, rows(), n);
}

template<int N>
inline typename NColsBlockXpr<N>::Type rightCols(Index n = N)
{
  return typename NColsBlockXpr<N>::Type(derived(), 0, cols() - n, rows(), n);
}

template<int N>
inline typename ConstNColsBlockXpr<N>::Type rightCols(Index n = N) const
{
  return typename ConstNColsBlockXpr<N>::Type(derived(), 0, cols() - n, rows(), n);
}



inline ColsBlockXpr middleCols(Index startCol, Index numCols)
{
  return ColsBlockXpr(derived(), 0, startCol, rows(), numCols);
}

inline ConstColsBlockXpr middleCols(Index startCol, Index numCols) const
{
  return ConstColsBlockXpr(derived(), 0, startCol, rows(), numCols);
}

template<int N>
inline typename NColsBlockXpr<N>::Type middleCols(Index startCol, Index n = N)
{
  return typename NColsBlockXpr<N>::Type(derived(), 0, startCol, rows(), n);
}

template<int N>
inline typename ConstNColsBlockXpr<N>::Type middleCols(Index startCol, Index n = N) const
{
  return typename ConstNColsBlockXpr<N>::Type(derived(), 0, startCol, rows(), n);
}



template<int BlockRows, int BlockCols>
inline Block<Derived, BlockRows, BlockCols> block(Index startRow, Index startCol)
{
  return Block<Derived, BlockRows, BlockCols>(derived(), startRow, startCol);
}

template<int BlockRows, int BlockCols>
inline const Block<const Derived, BlockRows, BlockCols> block(Index startRow, Index startCol) const
{
  return Block<const Derived, BlockRows, BlockCols>(derived(), startRow, startCol);
}

template<int BlockRows, int BlockCols>
inline Block<Derived, BlockRows, BlockCols> block(Index startRow, Index startCol, 
                                                  Index blockRows, Index blockCols)
{
  return Block<Derived, BlockRows, BlockCols>(derived(), startRow, startCol, blockRows, blockCols);
}

template<int BlockRows, int BlockCols>
inline const Block<const Derived, BlockRows, BlockCols> block(Index startRow, Index startCol,
                                                              Index blockRows, Index blockCols) const
{
  return Block<const Derived, BlockRows, BlockCols>(derived(), startRow, startCol, blockRows, blockCols);
}

inline ColXpr col(Index i)
{
  return ColXpr(derived(), i);
}

inline ConstColXpr col(Index i) const
{
  return ConstColXpr(derived(), i);
}

inline RowXpr row(Index i)
{
  return RowXpr(derived(), i);
}

inline ConstRowXpr row(Index i) const
{
  return ConstRowXpr(derived(), i);
}

inline SegmentReturnType segment(Index start, Index n)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return SegmentReturnType(derived(), start, n);
}


inline ConstSegmentReturnType segment(Index start, Index n) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return ConstSegmentReturnType(derived(), start, n);
}

inline SegmentReturnType head(Index n)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return SegmentReturnType(derived(), 0, n);
}

inline ConstSegmentReturnType head(Index n) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return ConstSegmentReturnType(derived(), 0, n);
}

inline SegmentReturnType tail(Index n)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return SegmentReturnType(derived(), this->size() - n, n);
}

inline ConstSegmentReturnType tail(Index n) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return ConstSegmentReturnType(derived(), this->size() - n, n);
}

template<int N>
inline typename FixedSegmentReturnType<N>::Type segment(Index start, Index n = N)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename FixedSegmentReturnType<N>::Type(derived(), start, n);
}

template<int N>
inline typename ConstFixedSegmentReturnType<N>::Type segment(Index start, Index n = N) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename ConstFixedSegmentReturnType<N>::Type(derived(), start, n);
}

template<int N>
inline typename FixedSegmentReturnType<N>::Type head(Index n = N)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename FixedSegmentReturnType<N>::Type(derived(), 0, n);
}

template<int N>
inline typename ConstFixedSegmentReturnType<N>::Type head(Index n = N) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename ConstFixedSegmentReturnType<N>::Type(derived(), 0, n);
}

template<int N>
inline typename FixedSegmentReturnType<N>::Type tail(Index n = N)
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename FixedSegmentReturnType<N>::Type(derived(), size() - n);
}

template<int N>
inline typename ConstFixedSegmentReturnType<N>::Type tail(Index n = N) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return typename ConstFixedSegmentReturnType<N>::Type(derived(), size() - n);
}
