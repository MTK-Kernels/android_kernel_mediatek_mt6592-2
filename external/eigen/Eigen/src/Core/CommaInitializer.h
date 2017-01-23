// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_COMMAINITIALIZER_H
#define EIGEN_COMMAINITIALIZER_H

namespace Eigen { 

template<typename XprType>
struct CommaInitializer
{
  typedef typename XprType::Scalar Scalar;
  typedef typename XprType::Index Index;

  inline CommaInitializer(XprType& xpr, const Scalar& s)
    : m_xpr(xpr), m_row(0), m_col(1), m_currentBlockRows(1)
  {
    m_xpr.coeffRef(0,0) = s;
  }

  template<typename OtherDerived>
  inline CommaInitializer(XprType& xpr, const DenseBase<OtherDerived>& other)
    : m_xpr(xpr), m_row(0), m_col(other.cols()), m_currentBlockRows(other.rows())
  {
    m_xpr.block(0, 0, other.rows(), other.cols()) = other;
  }

  
  inline CommaInitializer(const CommaInitializer& o)
  : m_xpr(o.m_xpr), m_row(o.m_row), m_col(o.m_col), m_currentBlockRows(o.m_currentBlockRows) {
    
    const_cast<CommaInitializer&>(o).m_row = m_xpr.rows();
    const_cast<CommaInitializer&>(o).m_col = m_xpr.cols();
    const_cast<CommaInitializer&>(o).m_currentBlockRows = 0;
  }

  
  CommaInitializer& operator,(const Scalar& s)
  {
    if (m_col==m_xpr.cols())
    {
      m_row+=m_currentBlockRows;
      m_col = 0;
      m_currentBlockRows = 1;
      eigen_assert(m_row<m_xpr.rows()
        && "Too many rows passed to comma initializer (operator<<)");
    }
    eigen_assert(m_col<m_xpr.cols()
      && "Too many coefficients passed to comma initializer (operator<<)");
    eigen_assert(m_currentBlockRows==1);
    m_xpr.coeffRef(m_row, m_col++) = s;
    return *this;
  }

  
  template<typename OtherDerived>
  CommaInitializer& operator,(const DenseBase<OtherDerived>& other)
  {
    if(other.cols()==0 || other.rows()==0)
      return *this;
    if (m_col==m_xpr.cols())
    {
      m_row+=m_currentBlockRows;
      m_col = 0;
      m_currentBlockRows = other.rows();
      eigen_assert(m_row+m_currentBlockRows<=m_xpr.rows()
        && "Too many rows passed to comma initializer (operator<<)");
    }
    eigen_assert(m_col<m_xpr.cols()
      && "Too many coefficients passed to comma initializer (operator<<)");
    eigen_assert(m_currentBlockRows==other.rows());
    if (OtherDerived::SizeAtCompileTime != Dynamic)
      m_xpr.template block<OtherDerived::RowsAtCompileTime != Dynamic ? OtherDerived::RowsAtCompileTime : 1,
                              OtherDerived::ColsAtCompileTime != Dynamic ? OtherDerived::ColsAtCompileTime : 1>
                    (m_row, m_col) = other;
    else
      m_xpr.block(m_row, m_col, other.rows(), other.cols()) = other;
    m_col += other.cols();
    return *this;
  }

  inline ~CommaInitializer()
  {
    eigen_assert((m_row+m_currentBlockRows) == m_xpr.rows()
         && m_col == m_xpr.cols()
         && "Too few coefficients passed to comma initializer (operator<<)");
  }

  inline XprType& finished() { return m_xpr; }

  XprType& m_xpr;   
  Index m_row;              
  Index m_col;              
  Index m_currentBlockRows; 
};

template<typename Derived>
inline CommaInitializer<Derived> DenseBase<Derived>::operator<< (const Scalar& s)
{
  return CommaInitializer<Derived>(*static_cast<Derived*>(this), s);
}

template<typename Derived>
template<typename OtherDerived>
inline CommaInitializer<Derived>
DenseBase<Derived>::operator<<(const DenseBase<OtherDerived>& other)
{
  return CommaInitializer<Derived>(*static_cast<Derived *>(this), other);
}

} 

#endif 
