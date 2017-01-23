// Copyright (C) 2009-2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_JACOBISVD_H
#define EIGEN_JACOBISVD_H

namespace Eigen { 

namespace internal {
template<typename MatrixType, int QRPreconditioner,
         bool IsComplex = NumTraits<typename MatrixType::Scalar>::IsComplex>
struct svd_precondition_2x2_block_to_be_real {};


enum { PreconditionIfMoreColsThanRows, PreconditionIfMoreRowsThanCols };

template<typename MatrixType, int QRPreconditioner, int Case>
struct qr_preconditioner_should_do_anything
{
  enum { a = MatrixType::RowsAtCompileTime != Dynamic &&
             MatrixType::ColsAtCompileTime != Dynamic &&
             MatrixType::ColsAtCompileTime <= MatrixType::RowsAtCompileTime,
         b = MatrixType::RowsAtCompileTime != Dynamic &&
             MatrixType::ColsAtCompileTime != Dynamic &&
             MatrixType::RowsAtCompileTime <= MatrixType::ColsAtCompileTime,
         ret = !( (QRPreconditioner == NoQRPreconditioner) ||
                  (Case == PreconditionIfMoreColsThanRows && bool(a)) ||
                  (Case == PreconditionIfMoreRowsThanCols && bool(b)) )
  };
};

template<typename MatrixType, int QRPreconditioner, int Case,
         bool DoAnything = qr_preconditioner_should_do_anything<MatrixType, QRPreconditioner, Case>::ret
> struct qr_preconditioner_impl {};

template<typename MatrixType, int QRPreconditioner, int Case>
class qr_preconditioner_impl<MatrixType, QRPreconditioner, Case, false>
{
public:
  typedef typename MatrixType::Index Index;
  void allocate(const JacobiSVD<MatrixType, QRPreconditioner>&) {}
  bool run(JacobiSVD<MatrixType, QRPreconditioner>&, const MatrixType&)
  {
    return false;
  }
};


template<typename MatrixType>
class qr_preconditioner_impl<MatrixType, FullPivHouseholderQRPreconditioner, PreconditionIfMoreRowsThanCols, true>
{
public:
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::Scalar Scalar;
  enum
  {
    RowsAtCompileTime = MatrixType::RowsAtCompileTime,
    MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime
  };
  typedef Matrix<Scalar, 1, RowsAtCompileTime, RowMajor, 1, MaxRowsAtCompileTime> WorkspaceType;

  void allocate(const JacobiSVD<MatrixType, FullPivHouseholderQRPreconditioner>& svd)
  {
    if (svd.rows() != m_qr.rows() || svd.cols() != m_qr.cols())
    {
      m_qr.~QRType();
      ::new (&m_qr) QRType(svd.rows(), svd.cols());
    }
    if (svd.m_computeFullU) m_workspace.resize(svd.rows());
  }

  bool run(JacobiSVD<MatrixType, FullPivHouseholderQRPreconditioner>& svd, const MatrixType& matrix)
  {
    if(matrix.rows() > matrix.cols())
    {
      m_qr.compute(matrix);
      svd.m_workMatrix = m_qr.matrixQR().block(0,0,matrix.cols(),matrix.cols()).template triangularView<Upper>();
      if(svd.m_computeFullU) m_qr.matrixQ().evalTo(svd.m_matrixU, m_workspace);
      if(svd.computeV()) svd.m_matrixV = m_qr.colsPermutation();
      return true;
    }
    return false;
  }
private:
  typedef FullPivHouseholderQR<MatrixType> QRType;
  QRType m_qr;
  WorkspaceType m_workspace;
};

template<typename MatrixType>
class qr_preconditioner_impl<MatrixType, FullPivHouseholderQRPreconditioner, PreconditionIfMoreColsThanRows, true>
{
public:
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::Scalar Scalar;
  enum
  {
    RowsAtCompileTime = MatrixType::RowsAtCompileTime,
    ColsAtCompileTime = MatrixType::ColsAtCompileTime,
    MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime,
    Options = MatrixType::Options
  };
  typedef Matrix<Scalar, ColsAtCompileTime, RowsAtCompileTime, Options, MaxColsAtCompileTime, MaxRowsAtCompileTime>
          TransposeTypeWithSameStorageOrder;

  void allocate(const JacobiSVD<MatrixType, FullPivHouseholderQRPreconditioner>& svd)
  {
    if (svd.cols() != m_qr.rows() || svd.rows() != m_qr.cols())
    {
      m_qr.~QRType();
      ::new (&m_qr) QRType(svd.cols(), svd.rows());
    }
    m_adjoint.resize(svd.cols(), svd.rows());
    if (svd.m_computeFullV) m_workspace.resize(svd.cols());
  }

  bool run(JacobiSVD<MatrixType, FullPivHouseholderQRPreconditioner>& svd, const MatrixType& matrix)
  {
    if(matrix.cols() > matrix.rows())
    {
      m_adjoint = matrix.adjoint();
      m_qr.compute(m_adjoint);
      svd.m_workMatrix = m_qr.matrixQR().block(0,0,matrix.rows(),matrix.rows()).template triangularView<Upper>().adjoint();
      if(svd.m_computeFullV) m_qr.matrixQ().evalTo(svd.m_matrixV, m_workspace);
      if(svd.computeU()) svd.m_matrixU = m_qr.colsPermutation();
      return true;
    }
    else return false;
  }
private:
  typedef FullPivHouseholderQR<TransposeTypeWithSameStorageOrder> QRType;
  QRType m_qr;
  TransposeTypeWithSameStorageOrder m_adjoint;
  typename internal::plain_row_type<MatrixType>::type m_workspace;
};


template<typename MatrixType>
class qr_preconditioner_impl<MatrixType, ColPivHouseholderQRPreconditioner, PreconditionIfMoreRowsThanCols, true>
{
public:
  typedef typename MatrixType::Index Index;

  void allocate(const JacobiSVD<MatrixType, ColPivHouseholderQRPreconditioner>& svd)
  {
    if (svd.rows() != m_qr.rows() || svd.cols() != m_qr.cols())
    {
      m_qr.~QRType();
      ::new (&m_qr) QRType(svd.rows(), svd.cols());
    }
    if (svd.m_computeFullU) m_workspace.resize(svd.rows());
    else if (svd.m_computeThinU) m_workspace.resize(svd.cols());
  }

  bool run(JacobiSVD<MatrixType, ColPivHouseholderQRPreconditioner>& svd, const MatrixType& matrix)
  {
    if(matrix.rows() > matrix.cols())
    {
      m_qr.compute(matrix);
      svd.m_workMatrix = m_qr.matrixQR().block(0,0,matrix.cols(),matrix.cols()).template triangularView<Upper>();
      if(svd.m_computeFullU) m_qr.householderQ().evalTo(svd.m_matrixU, m_workspace);
      else if(svd.m_computeThinU)
      {
        svd.m_matrixU.setIdentity(matrix.rows(), matrix.cols());
        m_qr.householderQ().applyThisOnTheLeft(svd.m_matrixU, m_workspace);
      }
      if(svd.computeV()) svd.m_matrixV = m_qr.colsPermutation();
      return true;
    }
    return false;
  }

private:
  typedef ColPivHouseholderQR<MatrixType> QRType;
  QRType m_qr;
  typename internal::plain_col_type<MatrixType>::type m_workspace;
};

template<typename MatrixType>
class qr_preconditioner_impl<MatrixType, ColPivHouseholderQRPreconditioner, PreconditionIfMoreColsThanRows, true>
{
public:
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::Scalar Scalar;
  enum
  {
    RowsAtCompileTime = MatrixType::RowsAtCompileTime,
    ColsAtCompileTime = MatrixType::ColsAtCompileTime,
    MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime,
    Options = MatrixType::Options
  };

  typedef Matrix<Scalar, ColsAtCompileTime, RowsAtCompileTime, Options, MaxColsAtCompileTime, MaxRowsAtCompileTime>
          TransposeTypeWithSameStorageOrder;

  void allocate(const JacobiSVD<MatrixType, ColPivHouseholderQRPreconditioner>& svd)
  {
    if (svd.cols() != m_qr.rows() || svd.rows() != m_qr.cols())
    {
      m_qr.~QRType();
      ::new (&m_qr) QRType(svd.cols(), svd.rows());
    }
    if (svd.m_computeFullV) m_workspace.resize(svd.cols());
    else if (svd.m_computeThinV) m_workspace.resize(svd.rows());
    m_adjoint.resize(svd.cols(), svd.rows());
  }

  bool run(JacobiSVD<MatrixType, ColPivHouseholderQRPreconditioner>& svd, const MatrixType& matrix)
  {
    if(matrix.cols() > matrix.rows())
    {
      m_adjoint = matrix.adjoint();
      m_qr.compute(m_adjoint);

      svd.m_workMatrix = m_qr.matrixQR().block(0,0,matrix.rows(),matrix.rows()).template triangularView<Upper>().adjoint();
      if(svd.m_computeFullV) m_qr.householderQ().evalTo(svd.m_matrixV, m_workspace);
      else if(svd.m_computeThinV)
      {
        svd.m_matrixV.setIdentity(matrix.cols(), matrix.rows());
        m_qr.householderQ().applyThisOnTheLeft(svd.m_matrixV, m_workspace);
      }
      if(svd.computeU()) svd.m_matrixU = m_qr.colsPermutation();
      return true;
    }
    else return false;
  }

private:
  typedef ColPivHouseholderQR<TransposeTypeWithSameStorageOrder> QRType;
  QRType m_qr;
  TransposeTypeWithSameStorageOrder m_adjoint;
  typename internal::plain_row_type<MatrixType>::type m_workspace;
};


template<typename MatrixType>
class qr_preconditioner_impl<MatrixType, HouseholderQRPreconditioner, PreconditionIfMoreRowsThanCols, true>
{
public:
  typedef typename MatrixType::Index Index;

  void allocate(const JacobiSVD<MatrixType, HouseholderQRPreconditioner>& svd)
  {
    if (svd.rows() != m_qr.rows() || svd.cols() != m_qr.cols())
    {
      m_qr.~QRType();
      ::new (&m_qr) QRType(svd.rows(), svd.cols());
    }
    if (svd.m_computeFullU) m_workspace.resize(svd.rows());
    else if (svd.m_computeThinU) m_workspace.resize(svd.cols());
  }

  bool run(JacobiSVD<MatrixType, HouseholderQRPreconditioner>& svd, const MatrixType& matrix)
  {
    if(matrix.rows() > matrix.cols())
    {
      m_qr.compute(matrix);
      svd.m_workMatrix = m_qr.matrixQR().block(0,0,matrix.cols(),matrix.cols()).template triangularView<Upper>();
      if(svd.m_computeFullU) m_qr.householderQ().evalTo(svd.m_matrixU, m_workspace);
      else if(svd.m_computeThinU)
      {
        svd.m_matrixU.setIdentity(matrix.rows(), matrix.cols());
        m_qr.householderQ().applyThisOnTheLeft(svd.m_matrixU, m_workspace);
      }
      if(svd.computeV()) svd.m_matrixV.setIdentity(matrix.cols(), matrix.cols());
      return true;
    }
    return false;
  }
private:
  typedef HouseholderQR<MatrixType> QRType;
  QRType m_qr;
  typename internal::plain_col_type<MatrixType>::type m_workspace;
};

template<typename MatrixType>
class qr_preconditioner_impl<MatrixType, HouseholderQRPreconditioner, PreconditionIfMoreColsThanRows, true>
{
public:
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::Scalar Scalar;
  enum
  {
    RowsAtCompileTime = MatrixType::RowsAtCompileTime,
    ColsAtCompileTime = MatrixType::ColsAtCompileTime,
    MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime,
    Options = MatrixType::Options
  };

  typedef Matrix<Scalar, ColsAtCompileTime, RowsAtCompileTime, Options, MaxColsAtCompileTime, MaxRowsAtCompileTime>
          TransposeTypeWithSameStorageOrder;

  void allocate(const JacobiSVD<MatrixType, HouseholderQRPreconditioner>& svd)
  {
    if (svd.cols() != m_qr.rows() || svd.rows() != m_qr.cols())
    {
      m_qr.~QRType();
      ::new (&m_qr) QRType(svd.cols(), svd.rows());
    }
    if (svd.m_computeFullV) m_workspace.resize(svd.cols());
    else if (svd.m_computeThinV) m_workspace.resize(svd.rows());
    m_adjoint.resize(svd.cols(), svd.rows());
  }

  bool run(JacobiSVD<MatrixType, HouseholderQRPreconditioner>& svd, const MatrixType& matrix)
  {
    if(matrix.cols() > matrix.rows())
    {
      m_adjoint = matrix.adjoint();
      m_qr.compute(m_adjoint);

      svd.m_workMatrix = m_qr.matrixQR().block(0,0,matrix.rows(),matrix.rows()).template triangularView<Upper>().adjoint();
      if(svd.m_computeFullV) m_qr.householderQ().evalTo(svd.m_matrixV, m_workspace);
      else if(svd.m_computeThinV)
      {
        svd.m_matrixV.setIdentity(matrix.cols(), matrix.rows());
        m_qr.householderQ().applyThisOnTheLeft(svd.m_matrixV, m_workspace);
      }
      if(svd.computeU()) svd.m_matrixU.setIdentity(matrix.rows(), matrix.rows());
      return true;
    }
    else return false;
  }

private:
  typedef HouseholderQR<TransposeTypeWithSameStorageOrder> QRType;
  QRType m_qr;
  TransposeTypeWithSameStorageOrder m_adjoint;
  typename internal::plain_row_type<MatrixType>::type m_workspace;
};


template<typename MatrixType, int QRPreconditioner>
struct svd_precondition_2x2_block_to_be_real<MatrixType, QRPreconditioner, false>
{
  typedef JacobiSVD<MatrixType, QRPreconditioner> SVD;
  typedef typename SVD::Index Index;
  static void run(typename SVD::WorkMatrixType&, SVD&, Index, Index) {}
};

template<typename MatrixType, int QRPreconditioner>
struct svd_precondition_2x2_block_to_be_real<MatrixType, QRPreconditioner, true>
{
  typedef JacobiSVD<MatrixType, QRPreconditioner> SVD;
  typedef typename MatrixType::Scalar Scalar;
  typedef typename MatrixType::RealScalar RealScalar;
  typedef typename SVD::Index Index;
  static void run(typename SVD::WorkMatrixType& work_matrix, SVD& svd, Index p, Index q)
  {
    using std::sqrt;
    Scalar z;
    JacobiRotation<Scalar> rot;
    RealScalar n = sqrt(numext::abs2(work_matrix.coeff(p,p)) + numext::abs2(work_matrix.coeff(q,p)));
    
    if(n==0)
    {
      z = abs(work_matrix.coeff(p,q)) / work_matrix.coeff(p,q);
      work_matrix.row(p) *= z;
      if(svd.computeU()) svd.m_matrixU.col(p) *= conj(z);
      if(work_matrix.coeff(q,q)!=Scalar(0))
      {
        z = abs(work_matrix.coeff(q,q)) / work_matrix.coeff(q,q);
        work_matrix.row(q) *= z;
        if(svd.computeU()) svd.m_matrixU.col(q) *= conj(z);
      }
      
    }
    else
    {
      rot.c() = conj(work_matrix.coeff(p,p)) / n;
      rot.s() = work_matrix.coeff(q,p) / n;
      work_matrix.applyOnTheLeft(p,q,rot);
      if(svd.computeU()) svd.m_matrixU.applyOnTheRight(p,q,rot.adjoint());
      if(work_matrix.coeff(p,q) != Scalar(0))
      {
        Scalar z = abs(work_matrix.coeff(p,q)) / work_matrix.coeff(p,q);
        work_matrix.col(q) *= z;
        if(svd.computeV()) svd.m_matrixV.col(q) *= z;
      }
      if(work_matrix.coeff(q,q) != Scalar(0))
      {
        z = abs(work_matrix.coeff(q,q)) / work_matrix.coeff(q,q);
        work_matrix.row(q) *= z;
        if(svd.computeU()) svd.m_matrixU.col(q) *= conj(z);
      }
    }
  }
};

template<typename MatrixType, typename RealScalar, typename Index>
void real_2x2_jacobi_svd(const MatrixType& matrix, Index p, Index q,
                            JacobiRotation<RealScalar> *j_left,
                            JacobiRotation<RealScalar> *j_right)
{
  using std::sqrt;
  using std::abs;
  Matrix<RealScalar,2,2> m;
  m << numext::real(matrix.coeff(p,p)), numext::real(matrix.coeff(p,q)),
       numext::real(matrix.coeff(q,p)), numext::real(matrix.coeff(q,q));
  JacobiRotation<RealScalar> rot1;
  RealScalar t = m.coeff(0,0) + m.coeff(1,1);
  RealScalar d = m.coeff(1,0) - m.coeff(0,1);
  if(t == RealScalar(0))
  {
    rot1.c() = RealScalar(0);
    rot1.s() = d > RealScalar(0) ? RealScalar(1) : RealScalar(-1);
  }
  else
  {
    RealScalar t2d2 = numext::hypot(t,d);
    rot1.c() = abs(t)/t2d2;
    rot1.s() = d/t2d2;
    if(t<RealScalar(0))
      rot1.s() = -rot1.s();
  }
  m.applyOnTheLeft(0,1,rot1);
  j_right->makeJacobi(m,0,1);
  *j_left  = rot1 * j_right->transpose();
}

} 

template<typename _MatrixType, int QRPreconditioner> class JacobiSVD
{
  public:

    typedef _MatrixType MatrixType;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef typename MatrixType::Index Index;
    enum {
      RowsAtCompileTime = MatrixType::RowsAtCompileTime,
      ColsAtCompileTime = MatrixType::ColsAtCompileTime,
      DiagSizeAtCompileTime = EIGEN_SIZE_MIN_PREFER_DYNAMIC(RowsAtCompileTime,ColsAtCompileTime),
      MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
      MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime,
      MaxDiagSizeAtCompileTime = EIGEN_SIZE_MIN_PREFER_FIXED(MaxRowsAtCompileTime,MaxColsAtCompileTime),
      MatrixOptions = MatrixType::Options
    };

    typedef Matrix<Scalar, RowsAtCompileTime, RowsAtCompileTime,
                   MatrixOptions, MaxRowsAtCompileTime, MaxRowsAtCompileTime>
            MatrixUType;
    typedef Matrix<Scalar, ColsAtCompileTime, ColsAtCompileTime,
                   MatrixOptions, MaxColsAtCompileTime, MaxColsAtCompileTime>
            MatrixVType;
    typedef typename internal::plain_diag_type<MatrixType, RealScalar>::type SingularValuesType;
    typedef typename internal::plain_row_type<MatrixType>::type RowType;
    typedef typename internal::plain_col_type<MatrixType>::type ColType;
    typedef Matrix<Scalar, DiagSizeAtCompileTime, DiagSizeAtCompileTime,
                   MatrixOptions, MaxDiagSizeAtCompileTime, MaxDiagSizeAtCompileTime>
            WorkMatrixType;

    JacobiSVD()
      : m_isInitialized(false),
        m_isAllocated(false),
        m_usePrescribedThreshold(false),
        m_computationOptions(0),
        m_rows(-1), m_cols(-1), m_diagSize(0)
    {}


    JacobiSVD(Index rows, Index cols, unsigned int computationOptions = 0)
      : m_isInitialized(false),
        m_isAllocated(false),
        m_usePrescribedThreshold(false),
        m_computationOptions(0),
        m_rows(-1), m_cols(-1)
    {
      allocate(rows, cols, computationOptions);
    }

    JacobiSVD(const MatrixType& matrix, unsigned int computationOptions = 0)
      : m_isInitialized(false),
        m_isAllocated(false),
        m_usePrescribedThreshold(false),
        m_computationOptions(0),
        m_rows(-1), m_cols(-1)
    {
      compute(matrix, computationOptions);
    }

    JacobiSVD& compute(const MatrixType& matrix, unsigned int computationOptions);

    JacobiSVD& compute(const MatrixType& matrix)
    {
      return compute(matrix, m_computationOptions);
    }

    const MatrixUType& matrixU() const
    {
      eigen_assert(m_isInitialized && "JacobiSVD is not initialized.");
      eigen_assert(computeU() && "This JacobiSVD decomposition didn't compute U. Did you ask for it?");
      return m_matrixU;
    }

    const MatrixVType& matrixV() const
    {
      eigen_assert(m_isInitialized && "JacobiSVD is not initialized.");
      eigen_assert(computeV() && "This JacobiSVD decomposition didn't compute V. Did you ask for it?");
      return m_matrixV;
    }

    const SingularValuesType& singularValues() const
    {
      eigen_assert(m_isInitialized && "JacobiSVD is not initialized.");
      return m_singularValues;
    }

    
    inline bool computeU() const { return m_computeFullU || m_computeThinU; }
    
    inline bool computeV() const { return m_computeFullV || m_computeThinV; }

    template<typename Rhs>
    inline const internal::solve_retval<JacobiSVD, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "JacobiSVD is not initialized.");
      eigen_assert(computeU() && computeV() && "JacobiSVD::solve() requires both unitaries U and V to be computed (thin unitaries suffice).");
      return internal::solve_retval<JacobiSVD, Rhs>(*this, b.derived());
    }

    
    Index nonzeroSingularValues() const
    {
      eigen_assert(m_isInitialized && "JacobiSVD is not initialized.");
      return m_nonzeroSingularValues;
    }
    
    inline Index rank() const
    {
      using std::abs;
      eigen_assert(m_isInitialized && "JacobiSVD is not initialized.");
      if(m_singularValues.size()==0) return 0;
      RealScalar premultiplied_threshold = m_singularValues.coeff(0) * threshold();
      Index i = m_nonzeroSingularValues-1;
      while(i>=0 && m_singularValues.coeff(i) < premultiplied_threshold) --i;
      return i+1;
    }
    
    JacobiSVD& setThreshold(const RealScalar& threshold)
    {
      m_usePrescribedThreshold = true;
      m_prescribedThreshold = threshold;
      return *this;
    }

    JacobiSVD& setThreshold(Default_t)
    {
      m_usePrescribedThreshold = false;
      return *this;
    }

    RealScalar threshold() const
    {
      eigen_assert(m_isInitialized || m_usePrescribedThreshold);
      return m_usePrescribedThreshold ? m_prescribedThreshold
                                      : (std::max<Index>)(1,m_diagSize)*NumTraits<Scalar>::epsilon();
    }

    inline Index rows() const { return m_rows; }
    inline Index cols() const { return m_cols; }

  private:
    void allocate(Index rows, Index cols, unsigned int computationOptions);

  protected:
    MatrixUType m_matrixU;
    MatrixVType m_matrixV;
    SingularValuesType m_singularValues;
    WorkMatrixType m_workMatrix;
    bool m_isInitialized, m_isAllocated, m_usePrescribedThreshold;
    bool m_computeFullU, m_computeThinU;
    bool m_computeFullV, m_computeThinV;
    unsigned int m_computationOptions;
    Index m_nonzeroSingularValues, m_rows, m_cols, m_diagSize;
    RealScalar m_prescribedThreshold;

    template<typename __MatrixType, int _QRPreconditioner, bool _IsComplex>
    friend struct internal::svd_precondition_2x2_block_to_be_real;
    template<typename __MatrixType, int _QRPreconditioner, int _Case, bool _DoAnything>
    friend struct internal::qr_preconditioner_impl;

    internal::qr_preconditioner_impl<MatrixType, QRPreconditioner, internal::PreconditionIfMoreColsThanRows> m_qr_precond_morecols;
    internal::qr_preconditioner_impl<MatrixType, QRPreconditioner, internal::PreconditionIfMoreRowsThanCols> m_qr_precond_morerows;
    MatrixType m_scaledMatrix;
};

template<typename MatrixType, int QRPreconditioner>
void JacobiSVD<MatrixType, QRPreconditioner>::allocate(Index rows, Index cols, unsigned int computationOptions)
{
  eigen_assert(rows >= 0 && cols >= 0);

  if (m_isAllocated &&
      rows == m_rows &&
      cols == m_cols &&
      computationOptions == m_computationOptions)
  {
    return;
  }

  m_rows = rows;
  m_cols = cols;
  m_isInitialized = false;
  m_isAllocated = true;
  m_computationOptions = computationOptions;
  m_computeFullU = (computationOptions & ComputeFullU) != 0;
  m_computeThinU = (computationOptions & ComputeThinU) != 0;
  m_computeFullV = (computationOptions & ComputeFullV) != 0;
  m_computeThinV = (computationOptions & ComputeThinV) != 0;
  eigen_assert(!(m_computeFullU && m_computeThinU) && "JacobiSVD: you can't ask for both full and thin U");
  eigen_assert(!(m_computeFullV && m_computeThinV) && "JacobiSVD: you can't ask for both full and thin V");
  eigen_assert(EIGEN_IMPLIES(m_computeThinU || m_computeThinV, MatrixType::ColsAtCompileTime==Dynamic) &&
              "JacobiSVD: thin U and V are only available when your matrix has a dynamic number of columns.");
  if (QRPreconditioner == FullPivHouseholderQRPreconditioner)
  {
      eigen_assert(!(m_computeThinU || m_computeThinV) &&
              "JacobiSVD: can't compute thin U or thin V with the FullPivHouseholderQR preconditioner. "
              "Use the ColPivHouseholderQR preconditioner instead.");
  }
  m_diagSize = (std::min)(m_rows, m_cols);
  m_singularValues.resize(m_diagSize);
  if(RowsAtCompileTime==Dynamic)
    m_matrixU.resize(m_rows, m_computeFullU ? m_rows
                            : m_computeThinU ? m_diagSize
                            : 0);
  if(ColsAtCompileTime==Dynamic)
    m_matrixV.resize(m_cols, m_computeFullV ? m_cols
                            : m_computeThinV ? m_diagSize
                            : 0);
  m_workMatrix.resize(m_diagSize, m_diagSize);
  
  if(m_cols>m_rows)   m_qr_precond_morecols.allocate(*this);
  if(m_rows>m_cols)   m_qr_precond_morerows.allocate(*this);
  if(m_cols!=m_cols)  m_scaledMatrix.resize(rows,cols);
}

template<typename MatrixType, int QRPreconditioner>
JacobiSVD<MatrixType, QRPreconditioner>&
JacobiSVD<MatrixType, QRPreconditioner>::compute(const MatrixType& matrix, unsigned int computationOptions)
{
  using std::abs;
  allocate(matrix.rows(), matrix.cols(), computationOptions);

  
  
  const RealScalar precision = RealScalar(2) * NumTraits<Scalar>::epsilon();

  
  const RealScalar considerAsZero = RealScalar(2) * std::numeric_limits<RealScalar>::denorm_min();

  
  RealScalar scale = matrix.cwiseAbs().maxCoeff();
  if(scale==RealScalar(0)) scale = RealScalar(1);
  
  

  if(m_rows!=m_cols)
  {
    m_scaledMatrix = matrix / scale;
    m_qr_precond_morecols.run(*this, m_scaledMatrix);
    m_qr_precond_morerows.run(*this, m_scaledMatrix);
  }
  else
  {
    m_workMatrix = matrix.block(0,0,m_diagSize,m_diagSize) / scale;
    if(m_computeFullU) m_matrixU.setIdentity(m_rows,m_rows);
    if(m_computeThinU) m_matrixU.setIdentity(m_rows,m_diagSize);
    if(m_computeFullV) m_matrixV.setIdentity(m_cols,m_cols);
    if(m_computeThinV) m_matrixV.setIdentity(m_cols, m_diagSize);
  }

  

  bool finished = false;
  while(!finished)
  {
    finished = true;

    

    for(Index p = 1; p < m_diagSize; ++p)
    {
      for(Index q = 0; q < p; ++q)
      {
        
        
        
        using std::max;
        RealScalar threshold = (max)(considerAsZero, precision * (max)(abs(m_workMatrix.coeff(p,p)),
                                                                       abs(m_workMatrix.coeff(q,q))));
        
        if(abs(m_workMatrix.coeff(p,q))>threshold || abs(m_workMatrix.coeff(q,p)) > threshold)
        {
          finished = false;

          
          internal::svd_precondition_2x2_block_to_be_real<MatrixType, QRPreconditioner>::run(m_workMatrix, *this, p, q);
          JacobiRotation<RealScalar> j_left, j_right;
          internal::real_2x2_jacobi_svd(m_workMatrix, p, q, &j_left, &j_right);

          
          m_workMatrix.applyOnTheLeft(p,q,j_left);
          if(computeU()) m_matrixU.applyOnTheRight(p,q,j_left.transpose());

          m_workMatrix.applyOnTheRight(p,q,j_right);
          if(computeV()) m_matrixV.applyOnTheRight(p,q,j_right);
        }
      }
    }
  }

  

  for(Index i = 0; i < m_diagSize; ++i)
  {
    RealScalar a = abs(m_workMatrix.coeff(i,i));
    m_singularValues.coeffRef(i) = a;
    if(computeU() && (a!=RealScalar(0))) m_matrixU.col(i) *= m_workMatrix.coeff(i,i)/a;
  }

  

  m_nonzeroSingularValues = m_diagSize;
  for(Index i = 0; i < m_diagSize; i++)
  {
    Index pos;
    RealScalar maxRemainingSingularValue = m_singularValues.tail(m_diagSize-i).maxCoeff(&pos);
    if(maxRemainingSingularValue == RealScalar(0))
    {
      m_nonzeroSingularValues = i;
      break;
    }
    if(pos)
    {
      pos += i;
      std::swap(m_singularValues.coeffRef(i), m_singularValues.coeffRef(pos));
      if(computeU()) m_matrixU.col(pos).swap(m_matrixU.col(i));
      if(computeV()) m_matrixV.col(pos).swap(m_matrixV.col(i));
    }
  }
  
  m_singularValues *= scale;

  m_isInitialized = true;
  return *this;
}

namespace internal {
template<typename _MatrixType, int QRPreconditioner, typename Rhs>
struct solve_retval<JacobiSVD<_MatrixType, QRPreconditioner>, Rhs>
  : solve_retval_base<JacobiSVD<_MatrixType, QRPreconditioner>, Rhs>
{
  typedef JacobiSVD<_MatrixType, QRPreconditioner> JacobiSVDType;
  EIGEN_MAKE_SOLVE_HELPERS(JacobiSVDType,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    eigen_assert(rhs().rows() == dec().rows());

    
    

    Matrix<Scalar, Dynamic, Rhs::ColsAtCompileTime, 0, _MatrixType::MaxRowsAtCompileTime, Rhs::MaxColsAtCompileTime> tmp;
    Index rank = dec().rank();
    
    tmp.noalias() = dec().matrixU().leftCols(rank).adjoint() * rhs();
    tmp = dec().singularValues().head(rank).asDiagonal().inverse() * tmp;
    dst = dec().matrixV().leftCols(rank) * tmp;
  }
};
} 

template<typename Derived>
JacobiSVD<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::jacobiSvd(unsigned int computationOptions) const
{
  return JacobiSVD<PlainObject>(*this, computationOptions);
}

} 

#endif 
