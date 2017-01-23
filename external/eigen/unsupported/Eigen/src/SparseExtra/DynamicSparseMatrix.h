// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_DYNAMIC_SPARSEMATRIX_H
#define EIGEN_DYNAMIC_SPARSEMATRIX_H

namespace Eigen { 


namespace internal {
template<typename _Scalar, int _Options, typename _Index>
struct traits<DynamicSparseMatrix<_Scalar, _Options, _Index> >
{
  typedef _Scalar Scalar;
  typedef _Index Index;
  typedef Sparse StorageKind;
  typedef MatrixXpr XprKind;
  enum {
    RowsAtCompileTime = Dynamic,
    ColsAtCompileTime = Dynamic,
    MaxRowsAtCompileTime = Dynamic,
    MaxColsAtCompileTime = Dynamic,
    Flags = _Options | NestByRefBit | LvalueBit,
    CoeffReadCost = NumTraits<Scalar>::ReadCost,
    SupportedAccessPatterns = OuterRandomAccessPattern
  };
};
}

template<typename _Scalar, int _Options, typename _Index>
 class  DynamicSparseMatrix
  : public SparseMatrixBase<DynamicSparseMatrix<_Scalar, _Options, _Index> >
{
  public:
    EIGEN_SPARSE_PUBLIC_INTERFACE(DynamicSparseMatrix)
    
    
    
    typedef MappedSparseMatrix<Scalar,Flags> Map;
    using Base::IsRowMajor;
    using Base::operator=;
    enum {
      Options = _Options
    };

  protected:

    typedef DynamicSparseMatrix<Scalar,(Flags&~RowMajorBit)|(IsRowMajor?RowMajorBit:0)> TransposedSparseMatrix;

    Index m_innerSize;
    std::vector<internal::CompressedStorage<Scalar,Index> > m_data;

  public:

    inline Index rows() const { return IsRowMajor ? outerSize() : m_innerSize; }
    inline Index cols() const { return IsRowMajor ? m_innerSize : outerSize(); }
    inline Index innerSize() const { return m_innerSize; }
    inline Index outerSize() const { return static_cast<Index>(m_data.size()); }
    inline Index innerNonZeros(Index j) const { return m_data[j].size(); }

    std::vector<internal::CompressedStorage<Scalar,Index> >& _data() { return m_data; }
    const std::vector<internal::CompressedStorage<Scalar,Index> >& _data() const { return m_data; }

    inline Scalar coeff(Index row, Index col) const
    {
      const Index outer = IsRowMajor ? row : col;
      const Index inner = IsRowMajor ? col : row;
      return m_data[outer].at(inner);
    }

    inline Scalar& coeffRef(Index row, Index col)
    {
      const Index outer = IsRowMajor ? row : col;
      const Index inner = IsRowMajor ? col : row;
      return m_data[outer].atWithInsertion(inner);
    }

    class InnerIterator;
    class ReverseInnerIterator;

    void setZero()
    {
      for (Index j=0; j<outerSize(); ++j)
        m_data[j].clear();
    }

    
    Index nonZeros() const
    {
      Index res = 0;
      for (Index j=0; j<outerSize(); ++j)
        res += static_cast<Index>(m_data[j].size());
      return res;
    }



    void reserve(Index reserveSize = 1000)
    {
      if (outerSize()>0)
      {
        Index reserveSizePerVector = (std::max)(reserveSize/outerSize(),Index(4));
        for (Index j=0; j<outerSize(); ++j)
        {
          m_data[j].reserve(reserveSizePerVector);
        }
      }
    }

    
    inline void startVec(Index ) {}

    inline Scalar& insertBack(Index row, Index col)
    {
      return insertBackByOuterInner(IsRowMajor?row:col, IsRowMajor?col:row);
    }

    
    inline Scalar& insertBackByOuterInner(Index outer, Index inner)
    {
      eigen_assert(outer<Index(m_data.size()) && inner<m_innerSize && "out of range");
      eigen_assert(((m_data[outer].size()==0) || (m_data[outer].index(m_data[outer].size()-1)<inner))
                && "wrong sorted insertion");
      m_data[outer].append(0, inner);
      return m_data[outer].value(m_data[outer].size()-1);
    }

    inline Scalar& insert(Index row, Index col)
    {
      const Index outer = IsRowMajor ? row : col;
      const Index inner = IsRowMajor ? col : row;

      Index startId = 0;
      Index id = static_cast<Index>(m_data[outer].size()) - 1;
      m_data[outer].resize(id+2,1);

      while ( (id >= startId) && (m_data[outer].index(id) > inner) )
      {
        m_data[outer].index(id+1) = m_data[outer].index(id);
        m_data[outer].value(id+1) = m_data[outer].value(id);
        --id;
      }
      m_data[outer].index(id+1) = inner;
      m_data[outer].value(id+1) = 0;
      return m_data[outer].value(id+1);
    }

    
    inline void finalize() {}

    
    void prune(Scalar reference, RealScalar epsilon = NumTraits<RealScalar>::dummy_precision())
    {
      for (Index j=0; j<outerSize(); ++j)
        m_data[j].prune(reference,epsilon);
    }

    void resize(Index rows, Index cols)
    {
      const Index outerSize = IsRowMajor ? rows : cols;
      m_innerSize = IsRowMajor ? cols : rows;
      setZero();
      if (Index(m_data.size()) != outerSize)
      {
        m_data.resize(outerSize);
      }
    }

    void resizeAndKeepData(Index rows, Index cols)
    {
      const Index outerSize = IsRowMajor ? rows : cols;
      const Index innerSize = IsRowMajor ? cols : rows;
      if (m_innerSize>innerSize)
      {
        
        
        
        exit(2);
      }
      if (m_data.size() != outerSize)
      {
        m_data.resize(outerSize);
      }
    }

    
    EIGEN_DEPRECATED inline DynamicSparseMatrix()
      : m_innerSize(0), m_data(0)
    {
      eigen_assert(innerSize()==0 && outerSize()==0);
    }

    
    EIGEN_DEPRECATED inline DynamicSparseMatrix(Index rows, Index cols)
      : m_innerSize(0)
    {
      resize(rows, cols);
    }

    
    template<typename OtherDerived>
    EIGEN_DEPRECATED explicit inline DynamicSparseMatrix(const SparseMatrixBase<OtherDerived>& other)
      : m_innerSize(0)
    {
    Base::operator=(other.derived());
    }

    inline DynamicSparseMatrix(const DynamicSparseMatrix& other)
      : Base(), m_innerSize(0)
    {
      *this = other.derived();
    }

    inline void swap(DynamicSparseMatrix& other)
    {
      
      std::swap(m_innerSize, other.m_innerSize);
      
      m_data.swap(other.m_data);
    }

    inline DynamicSparseMatrix& operator=(const DynamicSparseMatrix& other)
    {
      if (other.isRValue())
      {
        swap(other.const_cast_derived());
      }
      else
      {
        resize(other.rows(), other.cols());
        m_data = other.m_data;
      }
      return *this;
    }

    
    inline ~DynamicSparseMatrix() {}

  public:

    EIGEN_DEPRECATED void startFill(Index reserveSize = 1000)
    {
      setZero();
      reserve(reserveSize);
    }

    EIGEN_DEPRECATED Scalar& fill(Index row, Index col)
    {
      const Index outer = IsRowMajor ? row : col;
      const Index inner = IsRowMajor ? col : row;
      return insertBack(outer,inner);
    }

    EIGEN_DEPRECATED Scalar& fillrand(Index row, Index col)
    {
      return insert(row,col);
    }

    EIGEN_DEPRECATED void endFill() {}
    
#   ifdef EIGEN_DYNAMICSPARSEMATRIX_PLUGIN
#     include EIGEN_DYNAMICSPARSEMATRIX_PLUGIN
#   endif
 };

template<typename Scalar, int _Options, typename _Index>
class DynamicSparseMatrix<Scalar,_Options,_Index>::InnerIterator : public SparseVector<Scalar,_Options,_Index>::InnerIterator
{
    typedef typename SparseVector<Scalar,_Options,_Index>::InnerIterator Base;
  public:
    InnerIterator(const DynamicSparseMatrix& mat, Index outer)
      : Base(mat.m_data[outer]), m_outer(outer)
    {}

    inline Index row() const { return IsRowMajor ? m_outer : Base::index(); }
    inline Index col() const { return IsRowMajor ? Base::index() : m_outer; }

  protected:
    const Index m_outer;
};

template<typename Scalar, int _Options, typename _Index>
class DynamicSparseMatrix<Scalar,_Options,_Index>::ReverseInnerIterator : public SparseVector<Scalar,_Options,_Index>::ReverseInnerIterator
{
    typedef typename SparseVector<Scalar,_Options,_Index>::ReverseInnerIterator Base;
  public:
    ReverseInnerIterator(const DynamicSparseMatrix& mat, Index outer)
      : Base(mat.m_data[outer]), m_outer(outer)
    {}

    inline Index row() const { return IsRowMajor ? m_outer : Base::index(); }
    inline Index col() const { return IsRowMajor ? Base::index() : m_outer; }

  protected:
    const Index m_outer;
};

} 

#endif 
