// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_IO_H
#define EIGEN_IO_H

namespace Eigen { 

enum { DontAlignCols = 1 };
enum { StreamPrecision = -1,
       FullPrecision = -2 };

namespace internal {
template<typename Derived>
std::ostream & print_matrix(std::ostream & s, const Derived& _m, const IOFormat& fmt);
}

struct IOFormat
{
  
  IOFormat(int _precision = StreamPrecision, int _flags = 0,
    const std::string& _coeffSeparator = " ",
    const std::string& _rowSeparator = "\n", const std::string& _rowPrefix="", const std::string& _rowSuffix="",
    const std::string& _matPrefix="", const std::string& _matSuffix="")
  : matPrefix(_matPrefix), matSuffix(_matSuffix), rowPrefix(_rowPrefix), rowSuffix(_rowSuffix), rowSeparator(_rowSeparator),
    rowSpacer(""), coeffSeparator(_coeffSeparator), precision(_precision), flags(_flags)
  {
    int i = int(matSuffix.length())-1;
    while (i>=0 && matSuffix[i]!='\n')
    {
      rowSpacer += ' ';
      i--;
    }
  }
  std::string matPrefix, matSuffix;
  std::string rowPrefix, rowSuffix, rowSeparator, rowSpacer;
  std::string coeffSeparator;
  int precision;
  int flags;
};

template<typename ExpressionType>
class WithFormat
{
  public:

    WithFormat(const ExpressionType& matrix, const IOFormat& format)
      : m_matrix(matrix), m_format(format)
    {}

    friend std::ostream & operator << (std::ostream & s, const WithFormat& wf)
    {
      return internal::print_matrix(s, wf.m_matrix.eval(), wf.m_format);
    }

  protected:
    const typename ExpressionType::Nested m_matrix;
    IOFormat m_format;
};

template<typename Derived>
inline const WithFormat<Derived>
DenseBase<Derived>::format(const IOFormat& fmt) const
{
  return WithFormat<Derived>(derived(), fmt);
}

namespace internal {

template<typename Scalar, bool IsInteger>
struct significant_decimals_default_impl
{
  typedef typename NumTraits<Scalar>::Real RealScalar;
  static inline int run()
  {
    using std::ceil;
    using std::log;
    return cast<RealScalar,int>(ceil(-log(NumTraits<RealScalar>::epsilon())/log(RealScalar(10))));
  }
};

template<typename Scalar>
struct significant_decimals_default_impl<Scalar, true>
{
  static inline int run()
  {
    return 0;
  }
};

template<typename Scalar>
struct significant_decimals_impl
  : significant_decimals_default_impl<Scalar, NumTraits<Scalar>::IsInteger>
{};

template<typename Derived>
std::ostream & print_matrix(std::ostream & s, const Derived& _m, const IOFormat& fmt)
{
  if(_m.size() == 0)
  {
    s << fmt.matPrefix << fmt.matSuffix;
    return s;
  }
  
  typename Derived::Nested m = _m;
  typedef typename Derived::Scalar Scalar;
  typedef typename Derived::Index Index;

  Index width = 0;

  std::streamsize explicit_precision;
  if(fmt.precision == StreamPrecision)
  {
    explicit_precision = 0;
  }
  else if(fmt.precision == FullPrecision)
  {
    if (NumTraits<Scalar>::IsInteger)
    {
      explicit_precision = 0;
    }
    else
    {
      explicit_precision = significant_decimals_impl<Scalar>::run();
    }
  }
  else
  {
    explicit_precision = fmt.precision;
  }

  std::streamsize old_precision = 0;
  if(explicit_precision) old_precision = s.precision(explicit_precision);

  bool align_cols = !(fmt.flags & DontAlignCols);
  if(align_cols)
  {
    
    for(Index j = 0; j < m.cols(); ++j)
      for(Index i = 0; i < m.rows(); ++i)
      {
        std::stringstream sstr;
        sstr.copyfmt(s);
        sstr << m.coeff(i,j);
        width = std::max<Index>(width, Index(sstr.str().length()));
      }
  }
  s << fmt.matPrefix;
  for(Index i = 0; i < m.rows(); ++i)
  {
    if (i)
      s << fmt.rowSpacer;
    s << fmt.rowPrefix;
    if(width) s.width(width);
    s << m.coeff(i, 0);
    for(Index j = 1; j < m.cols(); ++j)
    {
      s << fmt.coeffSeparator;
      if (width) s.width(width);
      s << m.coeff(i, j);
    }
    s << fmt.rowSuffix;
    if( i < m.rows() - 1)
      s << fmt.rowSeparator;
  }
  s << fmt.matSuffix;
  if(explicit_precision) s.precision(old_precision);
  return s;
}

} 

template<typename Derived>
std::ostream & operator <<
(std::ostream & s,
 const DenseBase<Derived> & m)
{
  return internal::print_matrix(s, m.eval(), EIGEN_DEFAULT_IO_FORMAT);
}

} 

#endif 
