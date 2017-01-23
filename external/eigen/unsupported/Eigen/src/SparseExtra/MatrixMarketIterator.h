
// Copyright (C) 2012 Desire NUENTSA WAKAM <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_BROWSE_MATRICES_H
#define EIGEN_BROWSE_MATRICES_H

namespace Eigen {

enum {
  SPD = 0x100,
  NonSymmetric = 0x0
}; 

template <typename Scalar>
class MatrixMarketIterator 
{
  public:
    typedef Matrix<Scalar,Dynamic,1> VectorType; 
    typedef SparseMatrix<Scalar,ColMajor> MatrixType; 
  
  public:
    MatrixMarketIterator(const std::string folder):m_sym(0),m_isvalid(false),m_matIsLoaded(false),m_hasRhs(false),m_hasrefX(false),m_folder(folder)
    {
      m_folder_id = opendir(folder.c_str());
      if (!m_folder_id){
        m_isvalid = false;
        std::cerr << "The provided Matrix folder could not be opened \n\n";
        abort();
      }
      Getnextvalidmatrix();
    }
    
    ~MatrixMarketIterator()
    {
      if (m_folder_id) closedir(m_folder_id); 
    }
    
    inline MatrixMarketIterator& operator++()
    {
      m_matIsLoaded = false;
      m_hasrefX = false;
      m_hasRhs = false;
      Getnextvalidmatrix();
      return *this;
    }
    inline operator bool() const { return m_isvalid;}
    
    
    inline MatrixType& matrix() 
    { 
      
      if (m_matIsLoaded) return m_mat;
      
      std::string matrix_file = m_folder + "/" + m_matname + ".mtx";
      if ( !loadMarket(m_mat, matrix_file)) 
      {
        m_matIsLoaded = false;
        return m_mat;
      }
      m_matIsLoaded = true; 
      
      if (m_sym != NonSymmetric) 
      { 
        MatrixType B; 
        B = m_mat;
        m_mat = B.template selfadjointView<Lower>();
      }
      return m_mat; 
    }
    
    inline VectorType& rhs() 
    { 
       
      if (m_hasRhs) return m_rhs;
      
      std::string rhs_file;
      rhs_file = m_folder + "/" + m_matname + "_b.mtx"; 
      m_hasRhs = Fileexists(rhs_file);
      if (m_hasRhs)
      {
        m_rhs.resize(m_mat.cols());
        m_hasRhs = loadMarketVector(m_rhs, rhs_file);
      }
      if (!m_hasRhs)
      {
        
        if (!m_matIsLoaded) this->matrix(); 
        m_refX.resize(m_mat.cols());
        m_refX.setRandom();
        m_rhs = m_mat * m_refX;
        m_hasrefX = true;
        m_hasRhs = true;
      }
      return m_rhs; 
    }
    
    inline VectorType& refX() 
    { 
      
      if (m_hasrefX) return m_refX;
      
      std::string lhs_file;
      lhs_file = m_folder + "/" + m_matname + "_x.mtx"; 
      m_hasrefX = Fileexists(lhs_file);
      if (m_hasrefX)
      {
        m_refX.resize(m_mat.cols());
        m_hasrefX = loadMarketVector(m_refX, lhs_file);
      }
      return m_refX; 
    }
    
    inline std::string& matname() { return m_matname; }
    
    inline int sym() { return m_sym; }
    
    inline bool hasRhs() {return m_hasRhs; }
    inline bool hasrefX() {return m_hasrefX; }
    
  protected:
    
    inline bool Fileexists(std::string file)
    {
      std::ifstream file_id(file.c_str());
      if (!file_id.good() ) 
      {
        return false;
      }
      else 
      {
        file_id.close();
        return true;
      }
    }
    
    void Getnextvalidmatrix( )
    {
      m_isvalid = false;
      
      while ( (m_curs_id = readdir(m_folder_id)) != NULL) {
        m_isvalid = false;
        std::string curfile;
        curfile = m_folder + "/" + m_curs_id->d_name;
        
        if (m_curs_id->d_type == DT_DIR) continue; 
        
        
        bool isvector,iscomplex=false;
        if(!getMarketHeader(curfile,m_sym,iscomplex,isvector)) continue;
        if(isvector) continue;
        if (!iscomplex)
        {
          if(internal::is_same<Scalar, std::complex<float> >::value || internal::is_same<Scalar, std::complex<double> >::value)
            continue; 
        }
        if (iscomplex)
        {
          if(internal::is_same<Scalar, float>::value || internal::is_same<Scalar, double>::value)
            continue; 
        }
        
        
        
        std::string filename = m_curs_id->d_name;
        m_matname = filename.substr(0, filename.length()-4); 
        
        
        size_t found = m_matname.find("SPD");
        if( (found!=std::string::npos) && (m_sym != NonSymmetric) )
          m_sym = SPD;
       
        m_isvalid = true;
        break; 
      }
    }
    int m_sym; 
    MatrixType m_mat; 
    VectorType m_rhs;  
    VectorType m_refX; 
    std::string m_matname; 
    bool m_isvalid; 
    bool m_matIsLoaded; 
    bool m_hasRhs; 
    bool m_hasrefX; 
    std::string m_folder;
    DIR * m_folder_id;
    struct dirent *m_curs_id; 
    
};

} 

#endif
