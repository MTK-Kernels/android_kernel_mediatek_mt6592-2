// Copyright (C) 2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_BASIC_PRECONDITIONERS_H
#define EIGEN_BASIC_PRECONDITIONERS_H

namespace Eigen { 

template <typename _Scalar>
class DiagonalPreconditioner
{
    typedef _Scalar Scalar;
    typedef Matrix<Scalar,Dynamic,1> Vector;
    typedef typename Vector::Index Index;

  public:
    
    typedef Matrix<Scalar,Dynamic,Dynamic> MatrixType;

    DiagonalPreconditioner() : m_isInitialized(false) {}

    template<typename MatType>
    DiagonalPreconditioner(const MatType& mat) : m_invdiag(mat.cols())
    {
      compute(mat);
    }

    Index rows() const { return m_invdiag.size(); }
    Index cols() const { return m_invdiag.size(); }
    
    template<typename MatType>
    DiagonalPreconditioner& analyzePattern(const MatType& )
    {
      return *this;
    }
    
    template<typename MatType>
    DiagonalPreconditioner& factorize(const MatType& mat)
    {
      m_invdiag.resize(mat.cols());
      for(int j=0; j<mat.outerSize(); ++j)
      {
        typename MatType::InnerIterator it(mat,j);
        while(it && it.index()!=j) ++it;
        if(it && it.index()==j)
          m_invdiag(j) = Scalar(1)/it.value();
        else
          m_invdiag(j) = 0;
      }
      m_isInitialized = true;
      return *this;
    }
    
    template<typename MatType>
    DiagonalPreconditioner& compute(const MatType& mat)
    {
      return factorize(mat);
    }

    template<typename Rhs, typename Dest>
    void _solve(const Rhs& b, Dest& x) const
    {
      x = m_invdiag.array() * b.array() ;
    }

    template<typename Rhs> inline const internal::solve_retval<DiagonalPreconditioner, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "DiagonalPreconditioner is not initialized.");
      eigen_assert(m_invdiag.size()==b.rows()
                && "DiagonalPreconditioner::solve(): invalid number of rows of the right hand side matrix b");
      return internal::solve_retval<DiagonalPreconditioner, Rhs>(*this, b.derived());
    }

  protected:
    Vector m_invdiag;
    bool m_isInitialized;
};

namespace internal {

template<typename _MatrixType, typename Rhs>
struct solve_retval<DiagonalPreconditioner<_MatrixType>, Rhs>
  : solve_retval_base<DiagonalPreconditioner<_MatrixType>, Rhs>
{
  typedef DiagonalPreconditioner<_MatrixType> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dec()._solve(rhs(),dst);
  }
};

}

class IdentityPreconditioner
{
  public:

    IdentityPreconditioner() {}

    template<typename MatrixType>
    IdentityPreconditioner(const MatrixType& ) {}
    
    template<typename MatrixType>
    IdentityPreconditioner& analyzePattern(const MatrixType& ) { return *this; }
    
    template<typename MatrixType>
    IdentityPreconditioner& factorize(const MatrixType& ) { return *this; }

    template<typename MatrixType>
    IdentityPreconditioner& compute(const MatrixType& ) { return *this; }
    
    template<typename Rhs>
    inline const Rhs& solve(const Rhs& b) const { return b; }
};

} 

#endif 
