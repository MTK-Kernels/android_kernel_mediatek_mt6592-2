// Copyright (C) 2012 Desire NUENTSA WAKAM <desire.nuentsa_wakam@inria.fr
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ITERSCALING_H
#define EIGEN_ITERSCALING_H
namespace Eigen {
using std::abs; 
template<typename _MatrixType>
class IterScaling
{
  public:
    typedef _MatrixType MatrixType; 
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;
    
  public:
    IterScaling() { init(); }
    
    IterScaling(const MatrixType& matrix)
    {
      init();
      compute(matrix);
    }
    
    ~IterScaling() { }
    
    void compute (const MatrixType& mat)
    {
      int m = mat.rows(); 
      int n = mat.cols();
      eigen_assert((m>0 && m == n) && "Please give a non - empty matrix");
      m_left.resize(m); 
      m_right.resize(n);
      m_left.setOnes();
      m_right.setOnes();
      m_matrix = mat;
      VectorXd Dr, Dc, DrRes, DcRes; 
      Dr.resize(m); Dc.resize(n);
      DrRes.resize(m); DcRes.resize(n);
      double EpsRow = 1.0, EpsCol = 1.0;
      int its = 0; 
      do
      { 
        
        Dr.setZero(); Dc.setZero();
        for (int k=0; k<m_matrix.outerSize(); ++k)
        {
          for (typename MatrixType::InnerIterator it(m_matrix, k); it; ++it)
          {
            if ( Dr(it.row()) < abs(it.value()) )
              Dr(it.row()) = abs(it.value());
            
            if ( Dc(it.col()) < abs(it.value()) )
              Dc(it.col()) = abs(it.value());
          }
        }
        for (int i = 0; i < m; ++i) 
        {
          Dr(i) = std::sqrt(Dr(i));
          Dc(i) = std::sqrt(Dc(i));
        }
        
        for (int i = 0; i < m; ++i) 
        {
          m_left(i) /= Dr(i);
          m_right(i) /= Dc(i);
        }
        
        DrRes.setZero(); DcRes.setZero(); 
        for (int k=0; k<m_matrix.outerSize(); ++k)
        {
          for (typename MatrixType::InnerIterator it(m_matrix, k); it; ++it)
          {
            it.valueRef() = it.value()/( Dr(it.row()) * Dc(it.col()) );
            
            if ( DrRes(it.row()) < abs(it.value()) )
              DrRes(it.row()) = abs(it.value());
            
            if ( DcRes(it.col()) < abs(it.value()) )
              DcRes(it.col()) = abs(it.value());
          }
        }  
        DrRes.array() = (1-DrRes.array()).abs();
        EpsRow = DrRes.maxCoeff();
        DcRes.array() = (1-DcRes.array()).abs();
        EpsCol = DcRes.maxCoeff();
        its++;
      }while ( (EpsRow >m_tol || EpsCol > m_tol) && (its < m_maxits) );
      m_isInitialized = true;
    }
    void computeRef (MatrixType& mat)
    {
      compute (mat);
      mat = m_matrix;
    }
    VectorXd& LeftScaling()
    {
      return m_left;
    }
    
    VectorXd& RightScaling()
    {
      return m_right;
    }
    
    void setTolerance(double tol)
    {
      m_tol = tol; 
    }
      
  protected:
    
    void init()
    {
      m_tol = 1e-10;
      m_maxits = 5;
      m_isInitialized = false;
    }
    
    MatrixType m_matrix;
    mutable ComputationInfo m_info; 
    bool m_isInitialized; 
    VectorXd m_left; 
    VectorXd m_right; 
    double m_tol; 
    int m_maxits; 
};
}
#endif
