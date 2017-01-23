// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SPARSETRIANGULARSOLVER_H
#define EIGEN_SPARSETRIANGULARSOLVER_H

namespace Eigen { 

namespace internal {

template<typename Lhs, typename Rhs, int Mode,
  int UpLo = (Mode & Lower)
           ? Lower
           : (Mode & Upper)
           ? Upper
           : -1,
  int StorageOrder = int(traits<Lhs>::Flags) & RowMajorBit>
struct sparse_solve_triangular_selector;

template<typename Lhs, typename Rhs, int Mode>
struct sparse_solve_triangular_selector<Lhs,Rhs,Mode,Lower,RowMajor>
{
  typedef typename Rhs::Scalar Scalar;
  static void run(const Lhs& lhs, Rhs& other)
  {
    for(int col=0 ; col<other.cols() ; ++col)
    {
      for(int i=0; i<lhs.rows(); ++i)
      {
        Scalar tmp = other.coeff(i,col);
        Scalar lastVal(0);
        int lastIndex = 0;
        for(typename Lhs::InnerIterator it(lhs, i); it; ++it)
        {
          lastVal = it.value();
          lastIndex = it.index();
          if(lastIndex==i)
            break;
          tmp -= lastVal * other.coeff(lastIndex,col);
        }
        if (Mode & UnitDiag)
          other.coeffRef(i,col) = tmp;
        else
        {
          eigen_assert(lastIndex==i);
          other.coeffRef(i,col) = tmp/lastVal;
        }
      }
    }
  }
};

template<typename Lhs, typename Rhs, int Mode>
struct sparse_solve_triangular_selector<Lhs,Rhs,Mode,Upper,RowMajor>
{
  typedef typename Rhs::Scalar Scalar;
  static void run(const Lhs& lhs, Rhs& other)
  {
    for(int col=0 ; col<other.cols() ; ++col)
    {
      for(int i=lhs.rows()-1 ; i>=0 ; --i)
      {
        Scalar tmp = other.coeff(i,col);
        Scalar l_ii = 0;
        typename Lhs::InnerIterator it(lhs, i);
        while(it && it.index()<i)
          ++it;
        if(!(Mode & UnitDiag))
        {
          eigen_assert(it && it.index()==i);
          l_ii = it.value();
          ++it;
        }
        else if (it && it.index() == i)
          ++it;
        for(; it; ++it)
        {
          tmp -= it.value() * other.coeff(it.index(),col);
        }

        if (Mode & UnitDiag)
          other.coeffRef(i,col) = tmp;
        else
          other.coeffRef(i,col) = tmp/l_ii;
      }
    }
  }
};

template<typename Lhs, typename Rhs, int Mode>
struct sparse_solve_triangular_selector<Lhs,Rhs,Mode,Lower,ColMajor>
{
  typedef typename Rhs::Scalar Scalar;
  static void run(const Lhs& lhs, Rhs& other)
  {
    for(int col=0 ; col<other.cols() ; ++col)
    {
      for(int i=0; i<lhs.cols(); ++i)
      {
        Scalar& tmp = other.coeffRef(i,col);
        if (tmp!=Scalar(0)) 
        {
          typename Lhs::InnerIterator it(lhs, i);
          while(it && it.index()<i)
            ++it;
          if(!(Mode & UnitDiag))
          {
            eigen_assert(it && it.index()==i);
            tmp /= it.value();
          }
          if (it && it.index()==i)
            ++it;
          for(; it; ++it)
            other.coeffRef(it.index(), col) -= tmp * it.value();
        }
      }
    }
  }
};

template<typename Lhs, typename Rhs, int Mode>
struct sparse_solve_triangular_selector<Lhs,Rhs,Mode,Upper,ColMajor>
{
  typedef typename Rhs::Scalar Scalar;
  static void run(const Lhs& lhs, Rhs& other)
  {
    for(int col=0 ; col<other.cols() ; ++col)
    {
      for(int i=lhs.cols()-1; i>=0; --i)
      {
        Scalar& tmp = other.coeffRef(i,col);
        if (tmp!=Scalar(0)) 
        {
          if(!(Mode & UnitDiag))
          {
            
            typename Lhs::ReverseInnerIterator it(lhs, i);
            while(it && it.index()!=i)
              --it;
            eigen_assert(it && it.index()==i);
            other.coeffRef(i,col) /= it.value();
          }
          typename Lhs::InnerIterator it(lhs, i);
          for(; it && it.index()<i; ++it)
            other.coeffRef(it.index(), col) -= tmp * it.value();
        }
      }
    }
  }
};

} 

template<typename ExpressionType,int Mode>
template<typename OtherDerived>
void SparseTriangularView<ExpressionType,Mode>::solveInPlace(MatrixBase<OtherDerived>& other) const
{
  eigen_assert(m_matrix.cols() == m_matrix.rows() && m_matrix.cols() == other.rows());
  eigen_assert((!(Mode & ZeroDiag)) && bool(Mode & (Upper|Lower)));

  enum { copy = internal::traits<OtherDerived>::Flags & RowMajorBit };

  typedef typename internal::conditional<copy,
    typename internal::plain_matrix_type_column_major<OtherDerived>::type, OtherDerived&>::type OtherCopy;
  OtherCopy otherCopy(other.derived());

  internal::sparse_solve_triangular_selector<ExpressionType, typename internal::remove_reference<OtherCopy>::type, Mode>::run(m_matrix, otherCopy);

  if (copy)
    other = otherCopy;
}

template<typename ExpressionType,int Mode>
template<typename OtherDerived>
typename internal::plain_matrix_type_column_major<OtherDerived>::type
SparseTriangularView<ExpressionType,Mode>::solve(const MatrixBase<OtherDerived>& other) const
{
  typename internal::plain_matrix_type_column_major<OtherDerived>::type res(other);
  solveInPlace(res);
  return res;
}


namespace internal {

template<typename Lhs, typename Rhs, int Mode,
  int UpLo = (Mode & Lower)
           ? Lower
           : (Mode & Upper)
           ? Upper
           : -1,
  int StorageOrder = int(Lhs::Flags) & (RowMajorBit)>
struct sparse_solve_triangular_sparse_selector;

template<typename Lhs, typename Rhs, int Mode, int UpLo>
struct sparse_solve_triangular_sparse_selector<Lhs,Rhs,Mode,UpLo,ColMajor>
{
  typedef typename Rhs::Scalar Scalar;
  typedef typename promote_index_type<typename traits<Lhs>::Index,
                                         typename traits<Rhs>::Index>::type Index;
  static void run(const Lhs& lhs, Rhs& other)
  {
    const bool IsLower = (UpLo==Lower);
    AmbiVector<Scalar,Index> tempVector(other.rows()*2);
    tempVector.setBounds(0,other.rows());

    Rhs res(other.rows(), other.cols());
    res.reserve(other.nonZeros());

    for(int col=0 ; col<other.cols() ; ++col)
    {
      
      tempVector.init(.99);
      tempVector.setZero();
      tempVector.restart();
      for (typename Rhs::InnerIterator rhsIt(other, col); rhsIt; ++rhsIt)
      {
        tempVector.coeffRef(rhsIt.index()) = rhsIt.value();
      }

      for(int i=IsLower?0:lhs.cols()-1;
          IsLower?i<lhs.cols():i>=0;
          i+=IsLower?1:-1)
      {
        tempVector.restart();
        Scalar& ci = tempVector.coeffRef(i);
        if (ci!=Scalar(0))
        {
          
          typename Lhs::InnerIterator it(lhs, i);
          if(!(Mode & UnitDiag))
          {
            if (IsLower)
            {
              eigen_assert(it.index()==i);
              ci /= it.value();
            }
            else
              ci /= lhs.coeff(i,i);
          }
          tempVector.restart();
          if (IsLower)
          {
            if (it.index()==i)
              ++it;
            for(; it; ++it)
              tempVector.coeffRef(it.index()) -= ci * it.value();
          }
          else
          {
            for(; it && it.index()<i; ++it)
              tempVector.coeffRef(it.index()) -= ci * it.value();
          }
        }
      }


      int count = 0;
      
      for (typename AmbiVector<Scalar,Index>::Iterator it(tempVector); it; ++it)
      {
        ++ count;
        
        res.insert(it.index(), col) = it.value();
      }
    }
    res.finalize();
    other = res.markAsRValue();
  }
};

} 

template<typename ExpressionType,int Mode>
template<typename OtherDerived>
void SparseTriangularView<ExpressionType,Mode>::solveInPlace(SparseMatrixBase<OtherDerived>& other) const
{
  eigen_assert(m_matrix.cols() == m_matrix.rows() && m_matrix.cols() == other.rows());
  eigen_assert( (!(Mode & ZeroDiag)) && bool(Mode & (Upper|Lower)));



  internal::sparse_solve_triangular_sparse_selector<ExpressionType, OtherDerived, Mode>::run(m_matrix, other.derived());

}

#ifdef EIGEN2_SUPPORT


template<typename Derived>
template<typename OtherDerived>
void SparseMatrixBase<Derived>::solveTriangularInPlace(MatrixBase<OtherDerived>& other) const
{
  this->template triangular<Flags&(Upper|Lower)>().solveInPlace(other);
}

template<typename Derived>
template<typename OtherDerived>
typename internal::plain_matrix_type_column_major<OtherDerived>::type
SparseMatrixBase<Derived>::solveTriangular(const MatrixBase<OtherDerived>& other) const
{
  typename internal::plain_matrix_type_column_major<OtherDerived>::type res(other);
  derived().solveTriangularInPlace(res);
  return res;
}
#endif 

} 

#endif 
