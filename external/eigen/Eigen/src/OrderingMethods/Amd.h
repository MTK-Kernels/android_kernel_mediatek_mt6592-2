// Copyright (C) 2010 Gael Guennebaud <gael.guennebaud@inria.fr>

/*

NOTE: this routine has been adapted from the CSparse library:

Copyright (c) 2006, Timothy A. Davis.
http://www.cise.ufl.edu/research/sparse/CSparse

CSparse is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

CSparse is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this Module; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "../Core/util/NonMPL2.h"

#ifndef EIGEN_SPARSE_AMD_H
#define EIGEN_SPARSE_AMD_H

namespace Eigen { 

namespace internal {
  
template<typename T> inline T amd_flip(const T& i) { return -i-2; }
template<typename T> inline T amd_unflip(const T& i) { return i<0 ? amd_flip(i) : i; }
template<typename T0, typename T1> inline bool amd_marked(const T0* w, const T1& j) { return w[j]<0; }
template<typename T0, typename T1> inline void amd_mark(const T0* w, const T1& j) { return w[j] = amd_flip(w[j]); }

template<typename Index>
static int cs_wclear (Index mark, Index lemax, Index *w, Index n)
{
  Index k;
  if(mark < 2 || (mark + lemax < 0))
  {
    for(k = 0; k < n; k++)
      if(w[k] != 0)
        w[k] = 1;
    mark = 2;
  }
  return (mark);     
}

template<typename Index>
Index cs_tdfs(Index j, Index k, Index *head, const Index *next, Index *post, Index *stack)
{
  int i, p, top = 0;
  if(!head || !next || !post || !stack) return (-1);    
  stack[0] = j;                 
  while (top >= 0)                
  {
    p = stack[top];           
    i = head[p];              
    if(i == -1)
    {
      top--;                 
      post[k++] = p;        
    }
    else
    {
      head[p] = next[i];   
      stack[++top] = i;     
    }
  }
  return k;
}


template<typename Scalar, typename Index>
void minimum_degree_ordering(SparseMatrix<Scalar,ColMajor,Index>& C, PermutationMatrix<Dynamic,Dynamic,Index>& perm)
{
  using std::sqrt;
  
  int d, dk, dext, lemax = 0, e, elenk, eln, i, j, k, k1,
      k2, k3, jlast, ln, dense, nzmax, mindeg = 0, nvi, nvj, nvk, mark, wnvi,
      ok, nel = 0, p, p1, p2, p3, p4, pj, pk, pk1, pk2, pn, q, t;
  unsigned int h;
  
  Index n = C.cols();
  dense = std::max<Index> (16, Index(10 * sqrt(double(n))));   
  dense = std::min<Index> (n-2, dense);
  
  Index cnz = C.nonZeros();
  perm.resize(n+1);
  t = cnz + cnz/5 + 2*n;                 
  C.resizeNonZeros(t);
  
  Index* W       = new Index[8*(n+1)]; 
  Index* len     = W;
  Index* nv      = W +   (n+1);
  Index* next    = W + 2*(n+1);
  Index* head    = W + 3*(n+1);
  Index* elen    = W + 4*(n+1);
  Index* degree  = W + 5*(n+1);
  Index* w       = W + 6*(n+1);
  Index* hhead   = W + 7*(n+1);
  Index* last    = perm.indices().data();                              
  
  
  Index* Cp = C.outerIndexPtr();
  Index* Ci = C.innerIndexPtr();
  for(k = 0; k < n; k++)
    len[k] = Cp[k+1] - Cp[k];
  len[n] = 0;
  nzmax = t;
  
  for(i = 0; i <= n; i++)
  {
    head[i]   = -1;                     
    last[i]   = -1;
    next[i]   = -1;
    hhead[i]  = -1;                     
    nv[i]     = 1;                      
    w[i]      = 1;                      
    elen[i]   = 0;                      
    degree[i] = len[i];                 
  }
  mark = internal::cs_wclear<Index>(0, 0, w, n);         
  elen[n] = -2;                         
  Cp[n] = -1;                           
  w[n] = 0;                             
  
  
  for(i = 0; i < n; i++)
  {
    d = degree[i];
    if(d == 0)                         
    {
      elen[i] = -2;                 
      nel++;
      Cp[i] = -1;                   
      w[i] = 0;
    }
    else if(d > dense)                 
    {
      nv[i] = 0;                    
      elen[i] = -1;                 
      nel++;
      Cp[i] = amd_flip (n);
      nv[n]++;
    }
    else
    {
      if(head[d] != -1) last[head[d]] = i;
      next[i] = head[d];           
      head[d] = i;
    }
  }
  
  while (nel < n)                         
  {
    
    for(k = -1; mindeg < n && (k = head[mindeg]) == -1; mindeg++) {}
    if(next[k] != -1) last[next[k]] = -1;
    head[mindeg] = next[k];          
    elenk = elen[k];                  
    nvk = nv[k];                      
    nel += nvk;                        
    
    
    if(elenk > 0 && cnz + mindeg >= nzmax)
    {
      for(j = 0; j < n; j++)
      {
        if((p = Cp[j]) >= 0)      
        {
          Cp[j] = Ci[p];          
          Ci[p] = amd_flip (j);    
        }
      }
      for(q = 0, p = 0; p < cnz; ) 
      {
        if((j = amd_flip (Ci[p++])) >= 0)  
        {
          Ci[q] = Cp[j];       
          Cp[j] = q++;          
          for(k3 = 0; k3 < len[j]-1; k3++) Ci[q++] = Ci[p++];
        }
      }
      cnz = q;                       
    }
    
    
    dk = 0;
    nv[k] = -nvk;                     
    p = Cp[k];
    pk1 = (elenk == 0) ? p : cnz;      
    pk2 = pk1;
    for(k1 = 1; k1 <= elenk + 1; k1++)
    {
      if(k1 > elenk)
      {
        e = k;                     
        pj = p;                    
        ln = len[k] - elenk;      
      }
      else
      {
        e = Ci[p++];              
        pj = Cp[e];
        ln = len[e];              
      }
      for(k2 = 1; k2 <= ln; k2++)
      {
        i = Ci[pj++];
        if((nvi = nv[i]) <= 0) continue; 
        dk += nvi;                 
        nv[i] = -nvi;             
        Ci[pk2++] = i;            
        if(next[i] != -1) last[next[i]] = last[i];
        if(last[i] != -1)         
        {
          next[last[i]] = next[i];
        }
        else
        {
          head[degree[i]] = next[i];
        }
      }
      if(e != k)
      {
        Cp[e] = amd_flip (k);      
        w[e] = 0;                 
      }
    }
    if(elenk != 0) cnz = pk2;         
    degree[k] = dk;                   
    Cp[k] = pk1;                      
    len[k] = pk2 - pk1;
    elen[k] = -2;                     
    
    
    mark = internal::cs_wclear<Index>(mark, lemax, w, n);  
    for(pk = pk1; pk < pk2; pk++)    
    {
      i = Ci[pk];
      if((eln = elen[i]) <= 0) continue;
      nvi = -nv[i];                      
      wnvi = mark - nvi;
      for(p = Cp[i]; p <= Cp[i] + eln - 1; p++)  
      {
        e = Ci[p];
        if(w[e] >= mark)
        {
          w[e] -= nvi;          
        }
        else if(w[e] != 0)        
        {
          w[e] = degree[e] + wnvi; 
        }
      }
    }
    
    
    for(pk = pk1; pk < pk2; pk++)    
    {
      i = Ci[pk];                   
      p1 = Cp[i];
      p2 = p1 + elen[i] - 1;
      pn = p1;
      for(h = 0, d = 0, p = p1; p <= p2; p++)    
      {
        e = Ci[p];
        if(w[e] != 0)             
        {
          dext = w[e] - mark;   
          if(dext > 0)
          {
            d += dext;         
            Ci[pn++] = e;     
            h += e;            
          }
          else
          {
            Cp[e] = amd_flip (k);  
            w[e] = 0;             
          }
        }
      }
      elen[i] = pn - p1 + 1;        
      p3 = pn;
      p4 = p1 + len[i];
      for(p = p2 + 1; p < p4; p++) 
      {
        j = Ci[p];
        if((nvj = nv[j]) <= 0) continue; 
        d += nvj;                  
        Ci[pn++] = j;             
        h += j;                    
      }
      if(d == 0)                     
      {
        Cp[i] = amd_flip (k);      
        nvi = -nv[i];
        dk -= nvi;                 
        nvk += nvi;                
        nel += nvi;
        nv[i] = 0;
        elen[i] = -1;             
      }
      else
      {
        degree[i] = std::min<Index> (degree[i], d);   
        Ci[pn] = Ci[p3];         
        Ci[p3] = Ci[p1];         
        Ci[p1] = k;               
        len[i] = pn - p1 + 1;     
        h %= n;                    
        next[i] = hhead[h];      
        hhead[h] = i;
        last[i] = h;              
      }
    }                                   
    degree[k] = dk;                   
    lemax = std::max<Index>(lemax, dk);
    mark = internal::cs_wclear<Index>(mark+lemax, lemax, w, n);    
    
    
    for(pk = pk1; pk < pk2; pk++)
    {
      i = Ci[pk];
      if(nv[i] >= 0) continue;         
      h = last[i];                      
      i = hhead[h];
      hhead[h] = -1;                    
      for(; i != -1 && next[i] != -1; i = next[i], mark++)
      {
        ln = len[i];
        eln = elen[i];
        for(p = Cp[i]+1; p <= Cp[i] + ln-1; p++) w[Ci[p]] = mark;
        jlast = i;
        for(j = next[i]; j != -1; ) 
        {
          ok = (len[j] == ln) && (elen[j] == eln);
          for(p = Cp[j] + 1; ok && p <= Cp[j] + ln - 1; p++)
          {
            if(w[Ci[p]] != mark) ok = 0;    
          }
          if(ok)                     
          {
            Cp[j] = amd_flip (i);  
            nv[i] += nv[j];
            nv[j] = 0;
            elen[j] = -1;         
            j = next[j];          
            next[jlast] = j;
          }
          else
          {
            jlast = j;             
            j = next[j];
          }
        }
      }
    }
    
    
    for(p = pk1, pk = pk1; pk < pk2; pk++)   
    {
      i = Ci[pk];
      if((nvi = -nv[i]) <= 0) continue;
      nv[i] = nvi;                      
      d = degree[i] + dk - nvi;         
      d = std::min<Index> (d, n - nel - nvi);
      if(head[d] != -1) last[head[d]] = i;
      next[i] = head[d];               
      last[i] = -1;
      head[d] = i;
      mindeg = std::min<Index> (mindeg, d);       
      degree[i] = d;
      Ci[p++] = i;                      
    }
    nv[k] = nvk;                      
    if((len[k] = p-pk1) == 0)         
    {
      Cp[k] = -1;                   
      w[k] = 0;                     
    }
    if(elenk != 0) cnz = p;           
  }
  
  
  for(i = 0; i < n; i++) Cp[i] = amd_flip (Cp[i]);
  for(j = 0; j <= n; j++) head[j] = -1;
  for(j = n; j >= 0; j--)              
  {
    if(nv[j] > 0) continue;          
    next[j] = head[Cp[j]];          
    head[Cp[j]] = j;
  }
  for(e = n; e >= 0; e--)              
  {
    if(nv[e] <= 0) continue;         
    if(Cp[e] != -1)
    {
      next[e] = head[Cp[e]];      
      head[Cp[e]] = e;
    }
  }
  for(k = 0, i = 0; i <= n; i++)       
  {
    if(Cp[i] == -1) k = internal::cs_tdfs<Index>(i, k, head, next, perm.indices().data(), w);
  }
  
  perm.indices().conservativeResize(n);

  delete[] W;
}

} 

} 

#endif 
