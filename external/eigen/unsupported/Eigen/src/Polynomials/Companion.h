// Copyright (C) 2010 Manuel Yguel <manuel.yguel@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_COMPANION_H
#define EIGEN_COMPANION_H


namespace Eigen { 

namespace internal {

#ifndef EIGEN_PARSED_BY_DOXYGEN

template <typename T>
T radix(){ return 2; }

template <typename T>
T radix2(){ return radix<T>()*radix<T>(); }

template<int Size>
struct decrement_if_fixed_size
{
  enum {
    ret = (Size == Dynamic) ? Dynamic : Size-1 };
};

#endif

template< typename _Scalar, int _Deg >
class companion
{
  public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_Deg==Dynamic ? Dynamic : _Deg)

    enum {
      Deg = _Deg,
      Deg_1=decrement_if_fixed_size<Deg>::ret
    };

    typedef _Scalar                                Scalar;
    typedef typename NumTraits<Scalar>::Real       RealScalar;
    typedef Matrix<Scalar, Deg, 1>                 RightColumn;
    
    typedef Matrix<Scalar, Deg_1, 1>               BottomLeftDiagonal;

    typedef Matrix<Scalar, Deg, Deg>               DenseCompanionMatrixType;
    typedef Matrix< Scalar, _Deg, Deg_1 >          LeftBlock;
    typedef Matrix< Scalar, Deg_1, Deg_1 >         BottomLeftBlock;
    typedef Matrix< Scalar, 1, Deg_1 >             LeftBlockFirstRow;

    typedef DenseIndex Index;

  public:
    EIGEN_STRONG_INLINE const _Scalar operator()(Index row, Index col ) const
    {
      if( m_bl_diag.rows() > col )
      {
        if( 0 < row ){ return m_bl_diag[col]; }
        else{ return 0; }
      }
      else{ return m_monic[row]; }
    }

  public:
    template<typename VectorType>
    void setPolynomial( const VectorType& poly )
    {
      const Index deg = poly.size()-1;
      m_monic = -1/poly[deg] * poly.head(deg);
      
      m_bl_diag.setOnes(deg-1);
    }

    template<typename VectorType>
    companion( const VectorType& poly ){
      setPolynomial( poly ); }

  public:
    DenseCompanionMatrixType denseMatrix() const
    {
      const Index deg   = m_monic.size();
      const Index deg_1 = deg-1;
      DenseCompanionMatrixType companion(deg,deg);
      companion <<
        ( LeftBlock(deg,deg_1)
          << LeftBlockFirstRow::Zero(1,deg_1),
          BottomLeftBlock::Identity(deg-1,deg-1)*m_bl_diag.asDiagonal() ).finished()
        , m_monic;
      return companion;
    }



  protected:
    bool balanced( Scalar colNorm, Scalar rowNorm,
        bool& isBalanced, Scalar& colB, Scalar& rowB );

    bool balancedR( Scalar colNorm, Scalar rowNorm,
        bool& isBalanced, Scalar& colB, Scalar& rowB );

  public:
    void balance();

  protected:
      RightColumn                m_monic;
      BottomLeftDiagonal         m_bl_diag;
};



template< typename _Scalar, int _Deg >
inline
bool companion<_Scalar,_Deg>::balanced( Scalar colNorm, Scalar rowNorm,
    bool& isBalanced, Scalar& colB, Scalar& rowB )
{
  if( Scalar(0) == colNorm || Scalar(0) == rowNorm ){ return true; }
  else
  {
    
    
    
    
    
    rowB = rowNorm / radix<Scalar>();
    colB = Scalar(1);
    const Scalar s = colNorm + rowNorm;

    while (colNorm < rowB)
    {
      colB *= radix<Scalar>();
      colNorm *= radix2<Scalar>();
    }

    rowB = rowNorm * radix<Scalar>();

    while (colNorm >= rowB)
    {
      colB /= radix<Scalar>();
      colNorm /= radix2<Scalar>();
    }

    
    if ((rowNorm + colNorm) < Scalar(0.95) * s * colB)
    {
      isBalanced = false;
      rowB = Scalar(1) / colB;
      return false;
    }
    else{
      return true; }
  }
}

template< typename _Scalar, int _Deg >
inline
bool companion<_Scalar,_Deg>::balancedR( Scalar colNorm, Scalar rowNorm,
    bool& isBalanced, Scalar& colB, Scalar& rowB )
{
  if( Scalar(0) == colNorm || Scalar(0) == rowNorm ){ return true; }
  else
  {
    const _Scalar q = colNorm/rowNorm;
    if( !isApprox( q, _Scalar(1) ) )
    {
      rowB = sqrt( colNorm/rowNorm );
      colB = Scalar(1)/rowB;

      isBalanced = false;
      return false;
    }
    else{
      return true; }
  }
}


template< typename _Scalar, int _Deg >
void companion<_Scalar,_Deg>::balance()
{
  using std::abs;
  EIGEN_STATIC_ASSERT( Deg == Dynamic || 1 < Deg, YOU_MADE_A_PROGRAMMING_MISTAKE );
  const Index deg   = m_monic.size();
  const Index deg_1 = deg-1;

  bool hasConverged=false;
  while( !hasConverged )
  {
    hasConverged = true;
    Scalar colNorm,rowNorm;
    Scalar colB,rowB;

    
    
    colNorm = abs(m_bl_diag[0]);
    rowNorm = abs(m_monic[0]);

    
    if( !balanced( colNorm, rowNorm, hasConverged, colB, rowB ) )
    {
      m_bl_diag[0] *= colB;
      m_monic[0] *= rowB;
    }

    
    
    for( Index i=1; i<deg_1; ++i )
    {
      
      colNorm = abs(m_bl_diag[i]);

      
      rowNorm = abs(m_bl_diag[i-1]) + abs(m_monic[i]);

      
      if( !balanced( colNorm, rowNorm, hasConverged, colB, rowB ) )
      {
        m_bl_diag[i]   *= colB;
        m_bl_diag[i-1] *= rowB;
        m_monic[i]     *= rowB;
      }
    }

    
    
    const Index ebl = m_bl_diag.size()-1;
    VectorBlock<RightColumn,Deg_1> headMonic( m_monic, 0, deg_1 );
    colNorm = headMonic.array().abs().sum();
    rowNorm = abs( m_bl_diag[ebl] );

    
    if( !balanced( colNorm, rowNorm, hasConverged, colB, rowB ) )
    {
      headMonic      *= colB;
      m_bl_diag[ebl] *= rowB;
    }
  }
}

} 

} 

#endif 
