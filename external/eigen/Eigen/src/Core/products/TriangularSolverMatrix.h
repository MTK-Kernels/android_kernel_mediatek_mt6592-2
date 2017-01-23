// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_TRIANGULAR_SOLVER_MATRIX_H
#define EIGEN_TRIANGULAR_SOLVER_MATRIX_H

namespace Eigen { 

namespace internal {

template <typename Scalar, typename Index, int Side, int Mode, bool Conjugate, int TriStorageOrder>
struct triangular_solve_matrix<Scalar,Index,Side,Mode,Conjugate,TriStorageOrder,RowMajor>
{
  static void run(
    Index size, Index cols,
    const Scalar*  tri, Index triStride,
    Scalar* _other, Index otherStride,
    level3_blocking<Scalar,Scalar>& blocking)
  {
    triangular_solve_matrix<
      Scalar, Index, Side==OnTheLeft?OnTheRight:OnTheLeft,
      (Mode&UnitDiag) | ((Mode&Upper) ? Lower : Upper),
      NumTraits<Scalar>::IsComplex && Conjugate,
      TriStorageOrder==RowMajor ? ColMajor : RowMajor, ColMajor>
      ::run(size, cols, tri, triStride, _other, otherStride, blocking);
  }
};

template <typename Scalar, typename Index, int Mode, bool Conjugate, int TriStorageOrder>
struct triangular_solve_matrix<Scalar,Index,OnTheLeft,Mode,Conjugate,TriStorageOrder,ColMajor>
{
  static EIGEN_DONT_INLINE void run(
    Index size, Index otherSize,
    const Scalar* _tri, Index triStride,
    Scalar* _other, Index otherStride,
    level3_blocking<Scalar,Scalar>& blocking);
};
template <typename Scalar, typename Index, int Mode, bool Conjugate, int TriStorageOrder>
EIGEN_DONT_INLINE void triangular_solve_matrix<Scalar,Index,OnTheLeft,Mode,Conjugate,TriStorageOrder,ColMajor>::run(
    Index size, Index otherSize,
    const Scalar* _tri, Index triStride,
    Scalar* _other, Index otherStride,
    level3_blocking<Scalar,Scalar>& blocking)
  {
    Index cols = otherSize;
    const_blas_data_mapper<Scalar, Index, TriStorageOrder> tri(_tri,triStride);
    blas_data_mapper<Scalar, Index, ColMajor> other(_other,otherStride);

    typedef gebp_traits<Scalar,Scalar> Traits;
    enum {
      SmallPanelWidth   = EIGEN_PLAIN_ENUM_MAX(Traits::mr,Traits::nr),
      IsLower = (Mode&Lower) == Lower
    };

    Index kc = blocking.kc();                   
    Index mc = (std::min)(size,blocking.mc());  

    std::size_t sizeA = kc*mc;
    std::size_t sizeB = kc*cols;
    std::size_t sizeW = kc*Traits::WorkSpaceFactor;

    ei_declare_aligned_stack_constructed_variable(Scalar, blockA, sizeA, blocking.blockA());
    ei_declare_aligned_stack_constructed_variable(Scalar, blockB, sizeB, blocking.blockB());
    ei_declare_aligned_stack_constructed_variable(Scalar, blockW, sizeW, blocking.blockW());

    conj_if<Conjugate> conj;
    gebp_kernel<Scalar, Scalar, Index, Traits::mr, Traits::nr, Conjugate, false> gebp_kernel;
    gemm_pack_lhs<Scalar, Index, Traits::mr, Traits::LhsProgress, TriStorageOrder> pack_lhs;
    gemm_pack_rhs<Scalar, Index, Traits::nr, ColMajor, false, true> pack_rhs;

    
    
    std::ptrdiff_t l1, l2;
    manage_caching_sizes(GetAction, &l1, &l2);
    Index subcols = cols>0 ? l2/(4 * sizeof(Scalar) * otherStride) : 0;
    subcols = std::max<Index>((subcols/Traits::nr)*Traits::nr, Traits::nr);

    for(Index k2=IsLower ? 0 : size;
        IsLower ? k2<size : k2>0;
        IsLower ? k2+=kc : k2-=kc)
    {
      const Index actual_kc = (std::min)(IsLower ? size-k2 : k2, kc);

      
      
      
      
      
      
      
      

      
      
      
      
      for(Index j2=0; j2<cols; j2+=subcols)
      {
        Index actual_cols = (std::min)(cols-j2,subcols);
        
        for (Index k1=0; k1<actual_kc; k1+=SmallPanelWidth)
        {
          Index actualPanelWidth = std::min<Index>(actual_kc-k1, SmallPanelWidth);
          
          for (Index k=0; k<actualPanelWidth; ++k)
          {
            
            Index i  = IsLower ? k2+k1+k : k2-k1-k-1;
            Index s  = IsLower ? k2+k1 : i+1;
            Index rs = actualPanelWidth - k - 1; 

            Scalar a = (Mode & UnitDiag) ? Scalar(1) : Scalar(1)/conj(tri(i,i));
            for (Index j=j2; j<j2+actual_cols; ++j)
            {
              if (TriStorageOrder==RowMajor)
              {
                Scalar b(0);
                const Scalar* l = &tri(i,s);
                Scalar* r = &other(s,j);
                for (Index i3=0; i3<k; ++i3)
                  b += conj(l[i3]) * r[i3];

                other(i,j) = (other(i,j) - b)*a;
              }
              else
              {
                Index s = IsLower ? i+1 : i-rs;
                Scalar b = (other(i,j) *= a);
                Scalar* r = &other(s,j);
                const Scalar* l = &tri(s,i);
                for (Index i3=0;i3<rs;++i3)
                  r[i3] -= b * conj(l[i3]);
              }
            }
          }

          Index lengthTarget = actual_kc-k1-actualPanelWidth;
          Index startBlock   = IsLower ? k2+k1 : k2-k1-actualPanelWidth;
          Index blockBOffset = IsLower ? k1 : lengthTarget;

          
          pack_rhs(blockB+actual_kc*j2, &other(startBlock,j2), otherStride, actualPanelWidth, actual_cols, actual_kc, blockBOffset);

          
          if (lengthTarget>0)
          {
            Index startTarget  = IsLower ? k2+k1+actualPanelWidth : k2-actual_kc;

            pack_lhs(blockA, &tri(startTarget,startBlock), triStride, actualPanelWidth, lengthTarget);

            gebp_kernel(&other(startTarget,j2), otherStride, blockA, blockB+actual_kc*j2, lengthTarget, actualPanelWidth, actual_cols, Scalar(-1),
                        actualPanelWidth, actual_kc, 0, blockBOffset, blockW);
          }
        }
      }
      
      
      {
        Index start = IsLower ? k2+kc : 0;
        Index end   = IsLower ? size : k2-kc;
        for(Index i2=start; i2<end; i2+=mc)
        {
          const Index actual_mc = (std::min)(mc,end-i2);
          if (actual_mc>0)
          {
            pack_lhs(blockA, &tri(i2, IsLower ? k2 : k2-kc), triStride, actual_kc, actual_mc);

            gebp_kernel(_other+i2, otherStride, blockA, blockB, actual_mc, actual_kc, cols, Scalar(-1), -1, -1, 0, 0, blockW);
          }
        }
      }
    }
  }

template <typename Scalar, typename Index, int Mode, bool Conjugate, int TriStorageOrder>
struct triangular_solve_matrix<Scalar,Index,OnTheRight,Mode,Conjugate,TriStorageOrder,ColMajor>
{
  static EIGEN_DONT_INLINE void run(
    Index size, Index otherSize,
    const Scalar* _tri, Index triStride,
    Scalar* _other, Index otherStride,
    level3_blocking<Scalar,Scalar>& blocking);
};
template <typename Scalar, typename Index, int Mode, bool Conjugate, int TriStorageOrder>
EIGEN_DONT_INLINE void triangular_solve_matrix<Scalar,Index,OnTheRight,Mode,Conjugate,TriStorageOrder,ColMajor>::run(
    Index size, Index otherSize,
    const Scalar* _tri, Index triStride,
    Scalar* _other, Index otherStride,
    level3_blocking<Scalar,Scalar>& blocking)
  {
    Index rows = otherSize;
    const_blas_data_mapper<Scalar, Index, TriStorageOrder> rhs(_tri,triStride);
    blas_data_mapper<Scalar, Index, ColMajor> lhs(_other,otherStride);

    typedef gebp_traits<Scalar,Scalar> Traits;
    enum {
      RhsStorageOrder   = TriStorageOrder,
      SmallPanelWidth   = EIGEN_PLAIN_ENUM_MAX(Traits::mr,Traits::nr),
      IsLower = (Mode&Lower) == Lower
    };

    Index kc = blocking.kc();                   
    Index mc = (std::min)(rows,blocking.mc());  

    std::size_t sizeA = kc*mc;
    std::size_t sizeB = kc*size;
    std::size_t sizeW = kc*Traits::WorkSpaceFactor;

    ei_declare_aligned_stack_constructed_variable(Scalar, blockA, sizeA, blocking.blockA());
    ei_declare_aligned_stack_constructed_variable(Scalar, blockB, sizeB, blocking.blockB());
    ei_declare_aligned_stack_constructed_variable(Scalar, blockW, sizeW, blocking.blockW());

    conj_if<Conjugate> conj;
    gebp_kernel<Scalar,Scalar, Index, Traits::mr, Traits::nr, false, Conjugate> gebp_kernel;
    gemm_pack_rhs<Scalar, Index, Traits::nr,RhsStorageOrder> pack_rhs;
    gemm_pack_rhs<Scalar, Index, Traits::nr,RhsStorageOrder,false,true> pack_rhs_panel;
    gemm_pack_lhs<Scalar, Index, Traits::mr, Traits::LhsProgress, ColMajor, false, true> pack_lhs_panel;

    for(Index k2=IsLower ? size : 0;
        IsLower ? k2>0 : k2<size;
        IsLower ? k2-=kc : k2+=kc)
    {
      const Index actual_kc = (std::min)(IsLower ? k2 : size-k2, kc);
      Index actual_k2 = IsLower ? k2-actual_kc : k2 ;

      Index startPanel = IsLower ? 0 : k2+actual_kc;
      Index rs = IsLower ? actual_k2 : size - actual_k2 - actual_kc;
      Scalar* geb = blockB+actual_kc*actual_kc;

      if (rs>0) pack_rhs(geb, &rhs(actual_k2,startPanel), triStride, actual_kc, rs);

      
      
      {
        for (Index j2=0; j2<actual_kc; j2+=SmallPanelWidth)
        {
          Index actualPanelWidth = std::min<Index>(actual_kc-j2, SmallPanelWidth);
          Index actual_j2 = actual_k2 + j2;
          Index panelOffset = IsLower ? j2+actualPanelWidth : 0;
          Index panelLength = IsLower ? actual_kc-j2-actualPanelWidth : j2;

          if (panelLength>0)
          pack_rhs_panel(blockB+j2*actual_kc,
                         &rhs(actual_k2+panelOffset, actual_j2), triStride,
                         panelLength, actualPanelWidth,
                         actual_kc, panelOffset);
        }
      }

      for(Index i2=0; i2<rows; i2+=mc)
      {
        const Index actual_mc = (std::min)(mc,rows-i2);

        
        {
          
          for (Index j2 = IsLower
                      ? (actual_kc - ((actual_kc%SmallPanelWidth) ? Index(actual_kc%SmallPanelWidth)
                                                                  : Index(SmallPanelWidth)))
                      : 0;
               IsLower ? j2>=0 : j2<actual_kc;
               IsLower ? j2-=SmallPanelWidth : j2+=SmallPanelWidth)
          {
            Index actualPanelWidth = std::min<Index>(actual_kc-j2, SmallPanelWidth);
            Index absolute_j2 = actual_k2 + j2;
            Index panelOffset = IsLower ? j2+actualPanelWidth : 0;
            Index panelLength = IsLower ? actual_kc - j2 - actualPanelWidth : j2;

            
            if(panelLength>0)
            {
              gebp_kernel(&lhs(i2,absolute_j2), otherStride,
                          blockA, blockB+j2*actual_kc,
                          actual_mc, panelLength, actualPanelWidth,
                          Scalar(-1),
                          actual_kc, actual_kc, 
                          panelOffset, panelOffset, 
                          blockW);  
            }

            
            for (Index k=0; k<actualPanelWidth; ++k)
            {
              Index j = IsLower ? absolute_j2+actualPanelWidth-k-1 : absolute_j2+k;

              Scalar* r = &lhs(i2,j);
              for (Index k3=0; k3<k; ++k3)
              {
                Scalar b = conj(rhs(IsLower ? j+1+k3 : absolute_j2+k3,j));
                Scalar* a = &lhs(i2,IsLower ? j+1+k3 : absolute_j2+k3);
                for (Index i=0; i<actual_mc; ++i)
                  r[i] -= a[i] * b;
              }
              Scalar b = (Mode & UnitDiag) ? Scalar(1) : Scalar(1)/conj(rhs(j,j));
              for (Index i=0; i<actual_mc; ++i)
                r[i] *= b;
            }

            
            pack_lhs_panel(blockA, _other+absolute_j2*otherStride+i2, otherStride,
                           actualPanelWidth, actual_mc,
                           actual_kc, j2);
          }
        }

        if (rs>0)
          gebp_kernel(_other+i2+startPanel*otherStride, otherStride, blockA, geb,
                      actual_mc, actual_kc, rs, Scalar(-1),
                      -1, -1, 0, 0, blockW);
      }
    }
  }

} 

} 

#endif 
