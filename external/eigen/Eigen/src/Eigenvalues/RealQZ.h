// Copyright (C) 2012 Alexey Korepanov <kaikaikai@yandex.ru>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_REAL_QZ_H
#define EIGEN_REAL_QZ_H

namespace Eigen {


  template<typename _MatrixType> class RealQZ
  {
    public:
      typedef _MatrixType MatrixType;
      enum {
        RowsAtCompileTime = MatrixType::RowsAtCompileTime,
        ColsAtCompileTime = MatrixType::ColsAtCompileTime,
        Options = MatrixType::Options,
        MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
        MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime
      };
      typedef typename MatrixType::Scalar Scalar;
      typedef std::complex<typename NumTraits<Scalar>::Real> ComplexScalar;
      typedef typename MatrixType::Index Index;

      typedef Matrix<ComplexScalar, ColsAtCompileTime, 1, Options & ~RowMajor, MaxColsAtCompileTime, 1> EigenvalueType;
      typedef Matrix<Scalar, ColsAtCompileTime, 1, Options & ~RowMajor, MaxColsAtCompileTime, 1> ColumnVectorType;

      RealQZ(Index size = RowsAtCompileTime==Dynamic ? 1 : RowsAtCompileTime) : 
        m_S(size, size),
        m_T(size, size),
        m_Q(size, size),
        m_Z(size, size),
        m_workspace(size*2),
        m_maxIters(400),
        m_isInitialized(false)
        { }

      RealQZ(const MatrixType& A, const MatrixType& B, bool computeQZ = true) :
        m_S(A.rows(),A.cols()),
        m_T(A.rows(),A.cols()),
        m_Q(A.rows(),A.cols()),
        m_Z(A.rows(),A.cols()),
        m_workspace(A.rows()*2),
        m_maxIters(400),
        m_isInitialized(false) {
          compute(A, B, computeQZ);
        }

      const MatrixType& matrixQ() const {
        eigen_assert(m_isInitialized && "RealQZ is not initialized.");
        eigen_assert(m_computeQZ && "The matrices Q and Z have not been computed during the QZ decomposition.");
        return m_Q;
      }

      const MatrixType& matrixZ() const {
        eigen_assert(m_isInitialized && "RealQZ is not initialized.");
        eigen_assert(m_computeQZ && "The matrices Q and Z have not been computed during the QZ decomposition.");
        return m_Z;
      }

      const MatrixType& matrixS() const {
        eigen_assert(m_isInitialized && "RealQZ is not initialized.");
        return m_S;
      }

      const MatrixType& matrixT() const {
        eigen_assert(m_isInitialized && "RealQZ is not initialized.");
        return m_T;
      }

      RealQZ& compute(const MatrixType& A, const MatrixType& B, bool computeQZ = true);

      ComputationInfo info() const
      {
        eigen_assert(m_isInitialized && "RealQZ is not initialized.");
        return m_info;
      }

      Index iterations() const
      {
        eigen_assert(m_isInitialized && "RealQZ is not initialized.");
        return m_global_iter;
      }

      RealQZ& setMaxIterations(Index maxIters)
      {
        m_maxIters = maxIters;
        return *this;
      }

    private:

      MatrixType m_S, m_T, m_Q, m_Z;
      Matrix<Scalar,Dynamic,1> m_workspace;
      ComputationInfo m_info;
      Index m_maxIters;
      bool m_isInitialized;
      bool m_computeQZ;
      Scalar m_normOfT, m_normOfS;
      Index m_global_iter;

      typedef Matrix<Scalar,3,1> Vector3s;
      typedef Matrix<Scalar,2,1> Vector2s;
      typedef Matrix<Scalar,2,2> Matrix2s;
      typedef JacobiRotation<Scalar> JRs;

      void hessenbergTriangular();
      void computeNorms();
      Index findSmallSubdiagEntry(Index iu);
      Index findSmallDiagEntry(Index f, Index l);
      void splitOffTwoRows(Index i);
      void pushDownZero(Index z, Index f, Index l);
      void step(Index f, Index l, Index iter);

  }; 

  
  template<typename MatrixType>
    void RealQZ<MatrixType>::hessenbergTriangular()
    {

      const Index dim = m_S.cols();

      
      HouseholderQR<MatrixType> qrT(m_T);
      m_T = qrT.matrixQR();
      m_T.template triangularView<StrictlyLower>().setZero();
      m_Q = qrT.householderQ();
      
      m_S.applyOnTheLeft(m_Q.adjoint());
      
      if (m_computeQZ)
        m_Z = MatrixType::Identity(dim,dim);
      
      for (Index j=0; j<=dim-3; j++) {
        for (Index i=dim-1; i>=j+2; i--) {
          JRs G;
          
          if(m_S.coeff(i,j) != 0)
          {
            G.makeGivens(m_S.coeff(i-1,j), m_S.coeff(i,j), &m_S.coeffRef(i-1, j));
            m_S.coeffRef(i,j) = Scalar(0.0);
            m_S.rightCols(dim-j-1).applyOnTheLeft(i-1,i,G.adjoint());
            m_T.rightCols(dim-i+1).applyOnTheLeft(i-1,i,G.adjoint());
          }
          
          if (m_computeQZ)
            m_Q.applyOnTheRight(i-1,i,G);
          
          if(m_T.coeff(i,i-1)!=Scalar(0))
          {
            G.makeGivens(m_T.coeff(i,i), m_T.coeff(i,i-1), &m_T.coeffRef(i,i));
            m_T.coeffRef(i,i-1) = Scalar(0.0);
            m_S.applyOnTheRight(i,i-1,G);
            m_T.topRows(i).applyOnTheRight(i,i-1,G);
          }
          
          if (m_computeQZ)
            m_Z.applyOnTheLeft(i,i-1,G.adjoint());
        }
      }
    }

  
  template<typename MatrixType>
    inline void RealQZ<MatrixType>::computeNorms()
    {
      const Index size = m_S.cols();
      m_normOfS = Scalar(0.0);
      m_normOfT = Scalar(0.0);
      for (Index j = 0; j < size; ++j)
      {
        m_normOfS += m_S.col(j).segment(0, (std::min)(size,j+2)).cwiseAbs().sum();
        m_normOfT += m_T.row(j).segment(j, size - j).cwiseAbs().sum();
      }
    }


  
  template<typename MatrixType>
    inline typename MatrixType::Index RealQZ<MatrixType>::findSmallSubdiagEntry(Index iu)
    {
      using std::abs;
      Index res = iu;
      while (res > 0)
      {
        Scalar s = abs(m_S.coeff(res-1,res-1)) + abs(m_S.coeff(res,res));
        if (s == Scalar(0.0))
          s = m_normOfS;
        if (abs(m_S.coeff(res,res-1)) < NumTraits<Scalar>::epsilon() * s)
          break;
        res--;
      }
      return res;
    }

  
  template<typename MatrixType>
    inline typename MatrixType::Index RealQZ<MatrixType>::findSmallDiagEntry(Index f, Index l)
    {
      using std::abs;
      Index res = l;
      while (res >= f) {
        if (abs(m_T.coeff(res,res)) <= NumTraits<Scalar>::epsilon() * m_normOfT)
          break;
        res--;
      }
      return res;
    }

  
  template<typename MatrixType>
    inline void RealQZ<MatrixType>::splitOffTwoRows(Index i)
    {
      using std::abs;
      using std::sqrt;
      const Index dim=m_S.cols();
      if (abs(m_S.coeff(i+1,i))==Scalar(0))
        return;
      Index z = findSmallDiagEntry(i,i+1);
      if (z==i-1)
      {
        
        Matrix2s STi = m_T.template block<2,2>(i,i).template triangularView<Upper>().
          template solve<OnTheRight>(m_S.template block<2,2>(i,i));
        Scalar p = Scalar(0.5)*(STi(0,0)-STi(1,1));
        Scalar q = p*p + STi(1,0)*STi(0,1);
        if (q>=0) {
          Scalar z = sqrt(q);
          
          
          
          JRs G;
          if (p>=0)
            G.makeGivens(p + z, STi(1,0));
          else
            G.makeGivens(p - z, STi(1,0));
          m_S.rightCols(dim-i).applyOnTheLeft(i,i+1,G.adjoint());
          m_T.rightCols(dim-i).applyOnTheLeft(i,i+1,G.adjoint());
          
          if (m_computeQZ)
            m_Q.applyOnTheRight(i,i+1,G);

          G.makeGivens(m_T.coeff(i+1,i+1), m_T.coeff(i+1,i));
          m_S.topRows(i+2).applyOnTheRight(i+1,i,G);
          m_T.topRows(i+2).applyOnTheRight(i+1,i,G);
          
          if (m_computeQZ)
            m_Z.applyOnTheLeft(i+1,i,G.adjoint());

          m_S.coeffRef(i+1,i) = Scalar(0.0);
          m_T.coeffRef(i+1,i) = Scalar(0.0);
        }
      }
      else
      {
        pushDownZero(z,i,i+1);
      }
    }

  
  template<typename MatrixType>
    inline void RealQZ<MatrixType>::pushDownZero(Index z, Index f, Index l)
    {
      JRs G;
      const Index dim = m_S.cols();
      for (Index zz=z; zz<l; zz++)
      {
        
        Index firstColS = zz>f ? (zz-1) : zz;
        G.makeGivens(m_T.coeff(zz, zz+1), m_T.coeff(zz+1, zz+1));
        m_S.rightCols(dim-firstColS).applyOnTheLeft(zz,zz+1,G.adjoint());
        m_T.rightCols(dim-zz).applyOnTheLeft(zz,zz+1,G.adjoint());
        m_T.coeffRef(zz+1,zz+1) = Scalar(0.0);
        
        if (m_computeQZ)
          m_Q.applyOnTheRight(zz,zz+1,G);
        
        if (zz>f)
        {
          G.makeGivens(m_S.coeff(zz+1, zz), m_S.coeff(zz+1,zz-1));
          m_S.topRows(zz+2).applyOnTheRight(zz, zz-1,G);
          m_T.topRows(zz+1).applyOnTheRight(zz, zz-1,G);
          m_S.coeffRef(zz+1,zz-1) = Scalar(0.0);
          
          if (m_computeQZ)
            m_Z.applyOnTheLeft(zz,zz-1,G.adjoint());
        }
      }
      
      G.makeGivens(m_S.coeff(l,l), m_S.coeff(l,l-1));
      m_S.applyOnTheRight(l,l-1,G);
      m_T.applyOnTheRight(l,l-1,G);
      m_S.coeffRef(l,l-1)=Scalar(0.0);
      
      if (m_computeQZ)
        m_Z.applyOnTheLeft(l,l-1,G.adjoint());
    }

  
  template<typename MatrixType>
    inline void RealQZ<MatrixType>::step(Index f, Index l, Index iter)
    {
      using std::abs;
      const Index dim = m_S.cols();

      
      Scalar x, y, z;
      if (iter==10)
      {
        
        const Scalar
          a11=m_S.coeff(f+0,f+0), a12=m_S.coeff(f+0,f+1),
          a21=m_S.coeff(f+1,f+0), a22=m_S.coeff(f+1,f+1), a32=m_S.coeff(f+2,f+1),
          b12=m_T.coeff(f+0,f+1),
          b11i=Scalar(1.0)/m_T.coeff(f+0,f+0),
          b22i=Scalar(1.0)/m_T.coeff(f+1,f+1),
          a87=m_S.coeff(l-1,l-2),
          a98=m_S.coeff(l-0,l-1),
          b77i=Scalar(1.0)/m_T.coeff(l-2,l-2),
          b88i=Scalar(1.0)/m_T.coeff(l-1,l-1);
        Scalar ss = abs(a87*b77i) + abs(a98*b88i),
               lpl = Scalar(1.5)*ss,
               ll = ss*ss;
        x = ll + a11*a11*b11i*b11i - lpl*a11*b11i + a12*a21*b11i*b22i
          - a11*a21*b12*b11i*b11i*b22i;
        y = a11*a21*b11i*b11i - lpl*a21*b11i + a21*a22*b11i*b22i 
          - a21*a21*b12*b11i*b11i*b22i;
        z = a21*a32*b11i*b22i;
      }
      else if (iter==16)
      {
        
        x = m_S.coeff(f,f)/m_T.coeff(f,f)-m_S.coeff(l,l)/m_T.coeff(l,l) + m_S.coeff(l,l-1)*m_T.coeff(l-1,l) /
          (m_T.coeff(l-1,l-1)*m_T.coeff(l,l));
        y = m_S.coeff(f+1,f)/m_T.coeff(f,f);
        z = 0;
      }
      else if (iter>23 && !(iter%8))
      {
        
        x = internal::random<Scalar>(-1.0,1.0);
        y = internal::random<Scalar>(-1.0,1.0);
        z = internal::random<Scalar>(-1.0,1.0);
      }
      else
      {
        
        
        
        
        
        
        const Scalar
          a11 = m_S.coeff(f,f),     a12 = m_S.coeff(f,f+1),
          a21 = m_S.coeff(f+1,f),   a22 = m_S.coeff(f+1,f+1),
                                    a32 = m_S.coeff(f+2,f+1),

          a88 = m_S.coeff(l-1,l-1), a89 = m_S.coeff(l-1,l),
          a98 = m_S.coeff(l,l-1),   a99 = m_S.coeff(l,l),

          b11 = m_T.coeff(f,f),     b12 = m_T.coeff(f,f+1),
                                    b22 = m_T.coeff(f+1,f+1),

          b88 = m_T.coeff(l-1,l-1), b89 = m_T.coeff(l-1,l),
                                    b99 = m_T.coeff(l,l);

        x = ( (a88/b88 - a11/b11)*(a99/b99 - a11/b11) - (a89/b99)*(a98/b88) + (a98/b88)*(b89/b99)*(a11/b11) ) * (b11/a21)
          + a12/b22 - (a11/b11)*(b12/b22);
        y = (a22/b22-a11/b11) - (a21/b11)*(b12/b22) - (a88/b88-a11/b11) - (a99/b99-a11/b11) + (a98/b88)*(b89/b99);
        z = a32/b22;
      }

      JRs G;

      for (Index k=f; k<=l-2; k++)
      {
        
        Vector2s essential2;
        Scalar tau, beta;

        Vector3s hr(x,y,z);

        
        hr.makeHouseholderInPlace(tau, beta);
        essential2 = hr.template bottomRows<2>();
        Index fc=(std::max)(k-1,Index(0));  
        m_S.template middleRows<3>(k).rightCols(dim-fc).applyHouseholderOnTheLeft(essential2, tau, m_workspace.data());
        m_T.template middleRows<3>(k).rightCols(dim-fc).applyHouseholderOnTheLeft(essential2, tau, m_workspace.data());
        if (m_computeQZ)
          m_Q.template middleCols<3>(k).applyHouseholderOnTheRight(essential2, tau, m_workspace.data());
        if (k>f)
          m_S.coeffRef(k+2,k-1) = m_S.coeffRef(k+1,k-1) = Scalar(0.0);

        
        hr << m_T.coeff(k+2,k+2),m_T.coeff(k+2,k),m_T.coeff(k+2,k+1);
        hr.makeHouseholderInPlace(tau, beta);
        essential2 = hr.template bottomRows<2>();
        {
          Index lr = (std::min)(k+4,dim); 
          Map<Matrix<Scalar,Dynamic,1> > tmp(m_workspace.data(),lr);
          
          tmp = m_S.template middleCols<2>(k).topRows(lr) * essential2;
          tmp += m_S.col(k+2).head(lr);
          m_S.col(k+2).head(lr) -= tau*tmp;
          m_S.template middleCols<2>(k).topRows(lr) -= (tau*tmp) * essential2.adjoint();
          
          tmp = m_T.template middleCols<2>(k).topRows(lr) * essential2;
          tmp += m_T.col(k+2).head(lr);
          m_T.col(k+2).head(lr) -= tau*tmp;
          m_T.template middleCols<2>(k).topRows(lr) -= (tau*tmp) * essential2.adjoint();
        }
        if (m_computeQZ)
        {
          
          Map<Matrix<Scalar,1,Dynamic> > tmp(m_workspace.data(),dim);
          tmp = essential2.adjoint()*(m_Z.template middleRows<2>(k));
          tmp += m_Z.row(k+2);
          m_Z.row(k+2) -= tau*tmp;
          m_Z.template middleRows<2>(k) -= essential2 * (tau*tmp);
        }
        m_T.coeffRef(k+2,k) = m_T.coeffRef(k+2,k+1) = Scalar(0.0);

        
        G.makeGivens(m_T.coeff(k+1,k+1), m_T.coeff(k+1,k));
        m_S.applyOnTheRight(k+1,k,G);
        m_T.applyOnTheRight(k+1,k,G);
        
        if (m_computeQZ)
          m_Z.applyOnTheLeft(k+1,k,G.adjoint());
        m_T.coeffRef(k+1,k) = Scalar(0.0);

        
        x = m_S.coeff(k+1,k);
        y = m_S.coeff(k+2,k);
        if (k < l-2)
          z = m_S.coeff(k+3,k);
      } 

      
      G.makeGivens(x,y);
      m_S.applyOnTheLeft(l-1,l,G.adjoint());
      m_T.applyOnTheLeft(l-1,l,G.adjoint());
      if (m_computeQZ)
        m_Q.applyOnTheRight(l-1,l,G);
      m_S.coeffRef(l,l-2) = Scalar(0.0);

      
      G.makeGivens(m_T.coeff(l,l),m_T.coeff(l,l-1));
      m_S.applyOnTheRight(l,l-1,G);
      m_T.applyOnTheRight(l,l-1,G);
      if (m_computeQZ)
        m_Z.applyOnTheLeft(l,l-1,G.adjoint());
      m_T.coeffRef(l,l-1) = Scalar(0.0);
    }


  template<typename MatrixType>
    RealQZ<MatrixType>& RealQZ<MatrixType>::compute(const MatrixType& A_in, const MatrixType& B_in, bool computeQZ)
    {

      const Index dim = A_in.cols();

      eigen_assert (A_in.rows()==dim && A_in.cols()==dim 
          && B_in.rows()==dim && B_in.cols()==dim 
          && "Need square matrices of the same dimension");

      m_isInitialized = true;
      m_computeQZ = computeQZ;
      m_S = A_in; m_T = B_in;
      m_workspace.resize(dim*2);
      m_global_iter = 0;

      
      hessenbergTriangular();
      
      computeNorms();

      Index l = dim-1, 
            f, 
            local_iter = 0;

      while (l>0 && local_iter<m_maxIters)
      {
        f = findSmallSubdiagEntry(l);
        
        if (f>0) m_S.coeffRef(f,f-1) = Scalar(0.0);
        if (f == l) 
        {
          l--;
          local_iter = 0;
        }
        else if (f == l-1) 
        {
          splitOffTwoRows(f);
          l -= 2;
          local_iter = 0;
        }
        else 
        {
          
          Index z = findSmallDiagEntry(f,l);
          if (z>=f)
          {
            
            pushDownZero(z,f,l);
          }
          else
          {
            
            
            
            step(f,l, local_iter);
            local_iter++;
            m_global_iter++;
          }
        }
      }
      
      m_info = (local_iter<m_maxIters) ? Success : NoConvergence;
      return *this;
    } 

} 

#endif 
