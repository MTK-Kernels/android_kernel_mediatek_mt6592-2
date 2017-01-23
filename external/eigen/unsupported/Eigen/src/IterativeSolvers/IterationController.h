// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>


// Copyright (C) 1997-2001
// You should have received a copy of the License Agreement for the
// file LICENSE.  
// the code was modified is included with the above COPYRIGHT NOTICE and
// with the COPYRIGHT NOTICE in the LICENSE file, and that the LICENSE
// PARTICULAR PURPOSE OR THAT THE USE OF THE LICENSED SOFTWARE COMPONENTS
// OR DOCUMENTATION WILL NOT INFRINGE ANY PATENTS, COPYRIGHTS, TRADEMARKS

// Copyright (C) 2002-2007 Yves Renard
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; version 2.1 of the License.
// GNU Lesser General Public License for more details.
// License along with this program; if not, write to the Free Software

#include "../../../../Eigen/src/Core/util/NonMPL2.h"

#ifndef EIGEN_ITERATION_CONTROLLER_H
#define EIGEN_ITERATION_CONTROLLER_H

namespace Eigen { 

class IterationController
{
  protected :
    double m_rhsn;        
    size_t m_maxiter;     
    int m_noise;          
    double m_resmax;      
    double m_resminreach, m_resadd;
    size_t m_nit;         
    double m_res;         
    bool m_written;
    void (*m_callback)(const IterationController&);
  public :

    void init()
    {
      m_nit = 0; m_res = 0.0; m_written = false;
      m_resminreach = 1E50; m_resadd = 0.0;
      m_callback = 0;
    }

    IterationController(double r = 1.0E-8, int noi = 0, size_t mit = size_t(-1))
      : m_rhsn(1.0), m_maxiter(mit), m_noise(noi), m_resmax(r) { init(); }

    void operator ++(int) { m_nit++; m_written = false; m_resadd += m_res; }
    void operator ++() { (*this)++; }

    bool first() { return m_nit == 0; }

    
    int noiseLevel() const { return m_noise; }
    void setNoiseLevel(int n) { m_noise = n; }
    void reduceNoiseLevel() { if (m_noise > 0) m_noise--; }

    double maxResidual() const { return m_resmax; }
    void setMaxResidual(double r) { m_resmax = r; }

    double residual() const { return m_res; }

    
    void setCallback(void (*t)(const IterationController&))
    {
      m_callback = t;
    }

    size_t iteration() const { return m_nit; }
    void setIteration(size_t i) { m_nit = i; }

    size_t maxIterarions() const { return m_maxiter; }
    void setMaxIterations(size_t i) { m_maxiter = i; }

    double rhsNorm() const { return m_rhsn; }
    void setRhsNorm(double r) { m_rhsn = r; }

    bool converged() const { return m_res <= m_rhsn * m_resmax; }
    bool converged(double nr)
    {
      using std::abs;
      m_res = abs(nr); 
      m_resminreach = (std::min)(m_resminreach, m_res);
      return converged();
    }
    template<typename VectorType> bool converged(const VectorType &v)
    { return converged(v.squaredNorm()); }

    bool finished(double nr)
    {
      if (m_callback) m_callback(*this);
      if (m_noise > 0 && !m_written)
      {
        converged(nr);
        m_written = true;
      }
      return (m_nit >= m_maxiter || converged(nr));
    }
    template <typename VectorType>
    bool finished(const MatrixBase<VectorType> &v)
    { return finished(double(v.squaredNorm())); }

};

} 

#endif 
