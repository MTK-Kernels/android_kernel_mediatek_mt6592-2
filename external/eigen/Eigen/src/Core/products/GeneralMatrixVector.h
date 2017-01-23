// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_GENERAL_MATRIX_VECTOR_H
#define EIGEN_GENERAL_MATRIX_VECTOR_H

namespace Eigen { 

namespace internal {

template<typename Index, typename LhsScalar, bool ConjugateLhs, typename RhsScalar, bool ConjugateRhs, int Version>
struct general_matrix_vector_product<Index,LhsScalar,ColMajor,ConjugateLhs,RhsScalar,ConjugateRhs,Version>
{
typedef typename scalar_product_traits<LhsScalar, RhsScalar>::ReturnType ResScalar;

enum {
  Vectorizable = packet_traits<LhsScalar>::Vectorizable && packet_traits<RhsScalar>::Vectorizable
              && int(packet_traits<LhsScalar>::size)==int(packet_traits<RhsScalar>::size),
  LhsPacketSize = Vectorizable ? packet_traits<LhsScalar>::size : 1,
  RhsPacketSize = Vectorizable ? packet_traits<RhsScalar>::size : 1,
  ResPacketSize = Vectorizable ? packet_traits<ResScalar>::size : 1
};

typedef typename packet_traits<LhsScalar>::type  _LhsPacket;
typedef typename packet_traits<RhsScalar>::type  _RhsPacket;
typedef typename packet_traits<ResScalar>::type  _ResPacket;

typedef typename conditional<Vectorizable,_LhsPacket,LhsScalar>::type LhsPacket;
typedef typename conditional<Vectorizable,_RhsPacket,RhsScalar>::type RhsPacket;
typedef typename conditional<Vectorizable,_ResPacket,ResScalar>::type ResPacket;

EIGEN_DONT_INLINE static void run(
  Index rows, Index cols,
  const LhsScalar* lhs, Index lhsStride,
  const RhsScalar* rhs, Index rhsIncr,
  ResScalar* res, Index resIncr, RhsScalar alpha);
};

template<typename Index, typename LhsScalar, bool ConjugateLhs, typename RhsScalar, bool ConjugateRhs, int Version>
EIGEN_DONT_INLINE void general_matrix_vector_product<Index,LhsScalar,ColMajor,ConjugateLhs,RhsScalar,ConjugateRhs,Version>::run(
  Index rows, Index cols,
  const LhsScalar* lhs, Index lhsStride,
  const RhsScalar* rhs, Index rhsIncr,
  ResScalar* res, Index resIncr, RhsScalar alpha)
{
  EIGEN_UNUSED_VARIABLE(resIncr)
  eigen_internal_assert(resIncr==1);
  #ifdef _EIGEN_ACCUMULATE_PACKETS
  #error _EIGEN_ACCUMULATE_PACKETS has already been defined
  #endif
  #define _EIGEN_ACCUMULATE_PACKETS(A0,A13,A2) \
    pstore(&res[j], \
      padd(pload<ResPacket>(&res[j]), \
        padd( \
          padd(pcj.pmul(EIGEN_CAT(ploa , A0)<LhsPacket>(&lhs0[j]),    ptmp0), \
                  pcj.pmul(EIGEN_CAT(ploa , A13)<LhsPacket>(&lhs1[j]),   ptmp1)), \
          padd(pcj.pmul(EIGEN_CAT(ploa , A2)<LhsPacket>(&lhs2[j]),    ptmp2), \
                  pcj.pmul(EIGEN_CAT(ploa , A13)<LhsPacket>(&lhs3[j]),   ptmp3)) )))

  conj_helper<LhsScalar,RhsScalar,ConjugateLhs,ConjugateRhs> cj;
  conj_helper<LhsPacket,RhsPacket,ConjugateLhs,ConjugateRhs> pcj;
  if(ConjugateRhs)
    alpha = numext::conj(alpha);

  enum { AllAligned = 0, EvenAligned, FirstAligned, NoneAligned };
  const Index columnsAtOnce = 4;
  const Index peels = 2;
  const Index LhsPacketAlignedMask = LhsPacketSize-1;
  const Index ResPacketAlignedMask = ResPacketSize-1;
  const Index size = rows;
  
  
  
  Index alignedStart = internal::first_aligned(res,size);
  Index alignedSize = ResPacketSize>1 ? alignedStart + ((size-alignedStart) & ~ResPacketAlignedMask) : 0;
  const Index peeledSize = alignedSize - RhsPacketSize*peels - RhsPacketSize + 1;

  const Index alignmentStep = LhsPacketSize>1 ? (LhsPacketSize - lhsStride % LhsPacketSize) & LhsPacketAlignedMask : 0;
  Index alignmentPattern = alignmentStep==0 ? AllAligned
                       : alignmentStep==(LhsPacketSize/2) ? EvenAligned
                       : FirstAligned;

  
  const Index lhsAlignmentOffset = internal::first_aligned(lhs,size);

  
  Index skipColumns = 0;
  
  if( (size_t(lhs)%sizeof(LhsScalar)) || (size_t(res)%sizeof(ResScalar)) )
  {
    alignedSize = 0;
    alignedStart = 0;
  }
  else if (LhsPacketSize>1)
  {
    eigen_internal_assert(size_t(lhs+lhsAlignmentOffset)%sizeof(LhsPacket)==0 || size<LhsPacketSize);

    while (skipColumns<LhsPacketSize &&
          alignedStart != ((lhsAlignmentOffset + alignmentStep*skipColumns)%LhsPacketSize))
      ++skipColumns;
    if (skipColumns==LhsPacketSize)
    {
      
      alignmentPattern = NoneAligned;
      skipColumns = 0;
    }
    else
    {
      skipColumns = (std::min)(skipColumns,cols);
      
    }

    eigen_internal_assert(  (alignmentPattern==NoneAligned)
                      || (skipColumns + columnsAtOnce >= cols)
                      || LhsPacketSize > size
                      || (size_t(lhs+alignedStart+lhsStride*skipColumns)%sizeof(LhsPacket))==0);
  }
  else if(Vectorizable)
  {
    alignedStart = 0;
    alignedSize = size;
    alignmentPattern = AllAligned;
  }

  Index offset1 = (FirstAligned && alignmentStep==1?3:1);
  Index offset3 = (FirstAligned && alignmentStep==1?1:3);

  Index columnBound = ((cols-skipColumns)/columnsAtOnce)*columnsAtOnce + skipColumns;
  for (Index i=skipColumns; i<columnBound; i+=columnsAtOnce)
  {
    RhsPacket ptmp0 = pset1<RhsPacket>(alpha*rhs[i*rhsIncr]),
              ptmp1 = pset1<RhsPacket>(alpha*rhs[(i+offset1)*rhsIncr]),
              ptmp2 = pset1<RhsPacket>(alpha*rhs[(i+2)*rhsIncr]),
              ptmp3 = pset1<RhsPacket>(alpha*rhs[(i+offset3)*rhsIncr]);

    
    const LhsScalar *lhs0 = lhs + i*lhsStride,     *lhs1 = lhs + (i+offset1)*lhsStride,
                    *lhs2 = lhs + (i+2)*lhsStride, *lhs3 = lhs + (i+offset3)*lhsStride;

    if (Vectorizable)
    {
      
      
      for (Index j=0; j<alignedStart; ++j)
      {
        res[j] = cj.pmadd(lhs0[j], pfirst(ptmp0), res[j]);
        res[j] = cj.pmadd(lhs1[j], pfirst(ptmp1), res[j]);
        res[j] = cj.pmadd(lhs2[j], pfirst(ptmp2), res[j]);
        res[j] = cj.pmadd(lhs3[j], pfirst(ptmp3), res[j]);
      }

      if (alignedSize>alignedStart)
      {
        switch(alignmentPattern)
        {
          case AllAligned:
            for (Index j = alignedStart; j<alignedSize; j+=ResPacketSize)
              _EIGEN_ACCUMULATE_PACKETS(d,d,d);
            break;
          case EvenAligned:
            for (Index j = alignedStart; j<alignedSize; j+=ResPacketSize)
              _EIGEN_ACCUMULATE_PACKETS(d,du,d);
            break;
          case FirstAligned:
          {
            Index j = alignedStart;
            if(peels>1)
            {
              LhsPacket A00, A01, A02, A03, A10, A11, A12, A13;
              ResPacket T0, T1;

              A01 = pload<LhsPacket>(&lhs1[alignedStart-1]);
              A02 = pload<LhsPacket>(&lhs2[alignedStart-2]);
              A03 = pload<LhsPacket>(&lhs3[alignedStart-3]);

              for (; j<peeledSize; j+=peels*ResPacketSize)
              {
                A11 = pload<LhsPacket>(&lhs1[j-1+LhsPacketSize]);  palign<1>(A01,A11);
                A12 = pload<LhsPacket>(&lhs2[j-2+LhsPacketSize]);  palign<2>(A02,A12);
                A13 = pload<LhsPacket>(&lhs3[j-3+LhsPacketSize]);  palign<3>(A03,A13);

                A00 = pload<LhsPacket>(&lhs0[j]);
                A10 = pload<LhsPacket>(&lhs0[j+LhsPacketSize]);
                T0  = pcj.pmadd(A00, ptmp0, pload<ResPacket>(&res[j]));
                T1  = pcj.pmadd(A10, ptmp0, pload<ResPacket>(&res[j+ResPacketSize]));

                T0  = pcj.pmadd(A01, ptmp1, T0);
                A01 = pload<LhsPacket>(&lhs1[j-1+2*LhsPacketSize]);  palign<1>(A11,A01);
                T0  = pcj.pmadd(A02, ptmp2, T0);
                A02 = pload<LhsPacket>(&lhs2[j-2+2*LhsPacketSize]);  palign<2>(A12,A02);
                T0  = pcj.pmadd(A03, ptmp3, T0);
                pstore(&res[j],T0);
                A03 = pload<LhsPacket>(&lhs3[j-3+2*LhsPacketSize]);  palign<3>(A13,A03);
                T1  = pcj.pmadd(A11, ptmp1, T1);
                T1  = pcj.pmadd(A12, ptmp2, T1);
                T1  = pcj.pmadd(A13, ptmp3, T1);
                pstore(&res[j+ResPacketSize],T1);
              }
            }
            for (; j<alignedSize; j+=ResPacketSize)
              _EIGEN_ACCUMULATE_PACKETS(d,du,du);
            break;
          }
          default:
            for (Index j = alignedStart; j<alignedSize; j+=ResPacketSize)
              _EIGEN_ACCUMULATE_PACKETS(du,du,du);
            break;
        }
      }
    } 

    
    for (Index j=alignedSize; j<size; ++j)
    {
      res[j] = cj.pmadd(lhs0[j], pfirst(ptmp0), res[j]);
      res[j] = cj.pmadd(lhs1[j], pfirst(ptmp1), res[j]);
      res[j] = cj.pmadd(lhs2[j], pfirst(ptmp2), res[j]);
      res[j] = cj.pmadd(lhs3[j], pfirst(ptmp3), res[j]);
    }
  }

  
  Index end = cols;
  Index start = columnBound;
  do
  {
    for (Index k=start; k<end; ++k)
    {
      RhsPacket ptmp0 = pset1<RhsPacket>(alpha*rhs[k*rhsIncr]);
      const LhsScalar* lhs0 = lhs + k*lhsStride;

      if (Vectorizable)
      {
        
        
        for (Index j=0; j<alignedStart; ++j)
          res[j] += cj.pmul(lhs0[j], pfirst(ptmp0));
        
        if ((size_t(lhs0+alignedStart)%sizeof(LhsPacket))==0)
          for (Index i = alignedStart;i<alignedSize;i+=ResPacketSize)
            pstore(&res[i], pcj.pmadd(pload<LhsPacket>(&lhs0[i]), ptmp0, pload<ResPacket>(&res[i])));
        else
          for (Index i = alignedStart;i<alignedSize;i+=ResPacketSize)
            pstore(&res[i], pcj.pmadd(ploadu<LhsPacket>(&lhs0[i]), ptmp0, pload<ResPacket>(&res[i])));
      }

      
      for (Index i=alignedSize; i<size; ++i)
        res[i] += cj.pmul(lhs0[i], pfirst(ptmp0));
    }
    if (skipColumns)
    {
      start = 0;
      end = skipColumns;
      skipColumns = 0;
    }
    else
      break;
  } while(Vectorizable);
  #undef _EIGEN_ACCUMULATE_PACKETS
}

template<typename Index, typename LhsScalar, bool ConjugateLhs, typename RhsScalar, bool ConjugateRhs, int Version>
struct general_matrix_vector_product<Index,LhsScalar,RowMajor,ConjugateLhs,RhsScalar,ConjugateRhs,Version>
{
typedef typename scalar_product_traits<LhsScalar, RhsScalar>::ReturnType ResScalar;

enum {
  Vectorizable = packet_traits<LhsScalar>::Vectorizable && packet_traits<RhsScalar>::Vectorizable
              && int(packet_traits<LhsScalar>::size)==int(packet_traits<RhsScalar>::size),
  LhsPacketSize = Vectorizable ? packet_traits<LhsScalar>::size : 1,
  RhsPacketSize = Vectorizable ? packet_traits<RhsScalar>::size : 1,
  ResPacketSize = Vectorizable ? packet_traits<ResScalar>::size : 1
};

typedef typename packet_traits<LhsScalar>::type  _LhsPacket;
typedef typename packet_traits<RhsScalar>::type  _RhsPacket;
typedef typename packet_traits<ResScalar>::type  _ResPacket;

typedef typename conditional<Vectorizable,_LhsPacket,LhsScalar>::type LhsPacket;
typedef typename conditional<Vectorizable,_RhsPacket,RhsScalar>::type RhsPacket;
typedef typename conditional<Vectorizable,_ResPacket,ResScalar>::type ResPacket;
  
EIGEN_DONT_INLINE static void run(
  Index rows, Index cols,
  const LhsScalar* lhs, Index lhsStride,
  const RhsScalar* rhs, Index rhsIncr,
  ResScalar* res, Index resIncr,
  ResScalar alpha);
};

template<typename Index, typename LhsScalar, bool ConjugateLhs, typename RhsScalar, bool ConjugateRhs, int Version>
EIGEN_DONT_INLINE void general_matrix_vector_product<Index,LhsScalar,RowMajor,ConjugateLhs,RhsScalar,ConjugateRhs,Version>::run(
  Index rows, Index cols,
  const LhsScalar* lhs, Index lhsStride,
  const RhsScalar* rhs, Index rhsIncr,
  ResScalar* res, Index resIncr,
  ResScalar alpha)
{
  EIGEN_UNUSED_VARIABLE(rhsIncr);
  eigen_internal_assert(rhsIncr==1);
  #ifdef _EIGEN_ACCUMULATE_PACKETS
  #error _EIGEN_ACCUMULATE_PACKETS has already been defined
  #endif

  #define _EIGEN_ACCUMULATE_PACKETS(A0,A13,A2) {\
    RhsPacket b = pload<RhsPacket>(&rhs[j]); \
    ptmp0 = pcj.pmadd(EIGEN_CAT(ploa,A0) <LhsPacket>(&lhs0[j]), b, ptmp0); \
    ptmp1 = pcj.pmadd(EIGEN_CAT(ploa,A13)<LhsPacket>(&lhs1[j]), b, ptmp1); \
    ptmp2 = pcj.pmadd(EIGEN_CAT(ploa,A2) <LhsPacket>(&lhs2[j]), b, ptmp2); \
    ptmp3 = pcj.pmadd(EIGEN_CAT(ploa,A13)<LhsPacket>(&lhs3[j]), b, ptmp3); }

  conj_helper<LhsScalar,RhsScalar,ConjugateLhs,ConjugateRhs> cj;
  conj_helper<LhsPacket,RhsPacket,ConjugateLhs,ConjugateRhs> pcj;

  enum { AllAligned=0, EvenAligned=1, FirstAligned=2, NoneAligned=3 };
  const Index rowsAtOnce = 4;
  const Index peels = 2;
  const Index RhsPacketAlignedMask = RhsPacketSize-1;
  const Index LhsPacketAlignedMask = LhsPacketSize-1;
  const Index depth = cols;

  
  
  
  Index alignedStart = internal::first_aligned(rhs, depth);
  Index alignedSize = RhsPacketSize>1 ? alignedStart + ((depth-alignedStart) & ~RhsPacketAlignedMask) : 0;
  const Index peeledSize = alignedSize - RhsPacketSize*peels - RhsPacketSize + 1;

  const Index alignmentStep = LhsPacketSize>1 ? (LhsPacketSize - lhsStride % LhsPacketSize) & LhsPacketAlignedMask : 0;
  Index alignmentPattern = alignmentStep==0 ? AllAligned
                         : alignmentStep==(LhsPacketSize/2) ? EvenAligned
                         : FirstAligned;

  
  const Index lhsAlignmentOffset = internal::first_aligned(lhs,depth);

  
  Index skipRows = 0;
  
  if( (sizeof(LhsScalar)!=sizeof(RhsScalar)) || (size_t(lhs)%sizeof(LhsScalar)) || (size_t(rhs)%sizeof(RhsScalar)) )
  {
    alignedSize = 0;
    alignedStart = 0;
  }
  else if (LhsPacketSize>1)
  {
    eigen_internal_assert(size_t(lhs+lhsAlignmentOffset)%sizeof(LhsPacket)==0  || depth<LhsPacketSize);

    while (skipRows<LhsPacketSize &&
           alignedStart != ((lhsAlignmentOffset + alignmentStep*skipRows)%LhsPacketSize))
      ++skipRows;
    if (skipRows==LhsPacketSize)
    {
      
      alignmentPattern = NoneAligned;
      skipRows = 0;
    }
    else
    {
      skipRows = (std::min)(skipRows,Index(rows));
      
    }
    eigen_internal_assert(  alignmentPattern==NoneAligned
                      || LhsPacketSize==1
                      || (skipRows + rowsAtOnce >= rows)
                      || LhsPacketSize > depth
                      || (size_t(lhs+alignedStart+lhsStride*skipRows)%sizeof(LhsPacket))==0);
  }
  else if(Vectorizable)
  {
    alignedStart = 0;
    alignedSize = depth;
    alignmentPattern = AllAligned;
  }

  Index offset1 = (FirstAligned && alignmentStep==1?3:1);
  Index offset3 = (FirstAligned && alignmentStep==1?1:3);

  Index rowBound = ((rows-skipRows)/rowsAtOnce)*rowsAtOnce + skipRows;
  for (Index i=skipRows; i<rowBound; i+=rowsAtOnce)
  {
    EIGEN_ALIGN16 ResScalar tmp0 = ResScalar(0);
    ResScalar tmp1 = ResScalar(0), tmp2 = ResScalar(0), tmp3 = ResScalar(0);

    
    const LhsScalar *lhs0 = lhs + i*lhsStride,     *lhs1 = lhs + (i+offset1)*lhsStride,
                    *lhs2 = lhs + (i+2)*lhsStride, *lhs3 = lhs + (i+offset3)*lhsStride;

    if (Vectorizable)
    {
      
      ResPacket ptmp0 = pset1<ResPacket>(ResScalar(0)), ptmp1 = pset1<ResPacket>(ResScalar(0)),
                ptmp2 = pset1<ResPacket>(ResScalar(0)), ptmp3 = pset1<ResPacket>(ResScalar(0));

      
      
      for (Index j=0; j<alignedStart; ++j)
      {
        RhsScalar b = rhs[j];
        tmp0 += cj.pmul(lhs0[j],b); tmp1 += cj.pmul(lhs1[j],b);
        tmp2 += cj.pmul(lhs2[j],b); tmp3 += cj.pmul(lhs3[j],b);
      }

      if (alignedSize>alignedStart)
      {
        switch(alignmentPattern)
        {
          case AllAligned:
            for (Index j = alignedStart; j<alignedSize; j+=RhsPacketSize)
              _EIGEN_ACCUMULATE_PACKETS(d,d,d);
            break;
          case EvenAligned:
            for (Index j = alignedStart; j<alignedSize; j+=RhsPacketSize)
              _EIGEN_ACCUMULATE_PACKETS(d,du,d);
            break;
          case FirstAligned:
          {
            Index j = alignedStart;
            if (peels>1)
            {
              LhsPacket A01, A02, A03, A11, A12, A13;
              A01 = pload<LhsPacket>(&lhs1[alignedStart-1]);
              A02 = pload<LhsPacket>(&lhs2[alignedStart-2]);
              A03 = pload<LhsPacket>(&lhs3[alignedStart-3]);

              for (; j<peeledSize; j+=peels*RhsPacketSize)
              {
                RhsPacket b = pload<RhsPacket>(&rhs[j]);
                A11 = pload<LhsPacket>(&lhs1[j-1+LhsPacketSize]);  palign<1>(A01,A11);
                A12 = pload<LhsPacket>(&lhs2[j-2+LhsPacketSize]);  palign<2>(A02,A12);
                A13 = pload<LhsPacket>(&lhs3[j-3+LhsPacketSize]);  palign<3>(A03,A13);

                ptmp0 = pcj.pmadd(pload<LhsPacket>(&lhs0[j]), b, ptmp0);
                ptmp1 = pcj.pmadd(A01, b, ptmp1);
                A01 = pload<LhsPacket>(&lhs1[j-1+2*LhsPacketSize]);  palign<1>(A11,A01);
                ptmp2 = pcj.pmadd(A02, b, ptmp2);
                A02 = pload<LhsPacket>(&lhs2[j-2+2*LhsPacketSize]);  palign<2>(A12,A02);
                ptmp3 = pcj.pmadd(A03, b, ptmp3);
                A03 = pload<LhsPacket>(&lhs3[j-3+2*LhsPacketSize]);  palign<3>(A13,A03);

                b = pload<RhsPacket>(&rhs[j+RhsPacketSize]);
                ptmp0 = pcj.pmadd(pload<LhsPacket>(&lhs0[j+LhsPacketSize]), b, ptmp0);
                ptmp1 = pcj.pmadd(A11, b, ptmp1);
                ptmp2 = pcj.pmadd(A12, b, ptmp2);
                ptmp3 = pcj.pmadd(A13, b, ptmp3);
              }
            }
            for (; j<alignedSize; j+=RhsPacketSize)
              _EIGEN_ACCUMULATE_PACKETS(d,du,du);
            break;
          }
          default:
            for (Index j = alignedStart; j<alignedSize; j+=RhsPacketSize)
              _EIGEN_ACCUMULATE_PACKETS(du,du,du);
            break;
        }
        tmp0 += predux(ptmp0);
        tmp1 += predux(ptmp1);
        tmp2 += predux(ptmp2);
        tmp3 += predux(ptmp3);
      }
    } 

    
    
    for (Index j=alignedSize; j<depth; ++j)
    {
      RhsScalar b = rhs[j];
      tmp0 += cj.pmul(lhs0[j],b); tmp1 += cj.pmul(lhs1[j],b);
      tmp2 += cj.pmul(lhs2[j],b); tmp3 += cj.pmul(lhs3[j],b);
    }
    res[i*resIncr]            += alpha*tmp0;
    res[(i+offset1)*resIncr]  += alpha*tmp1;
    res[(i+2)*resIncr]        += alpha*tmp2;
    res[(i+offset3)*resIncr]  += alpha*tmp3;
  }

  
  Index end = rows;
  Index start = rowBound;
  do
  {
    for (Index i=start; i<end; ++i)
    {
      EIGEN_ALIGN16 ResScalar tmp0 = ResScalar(0);
      ResPacket ptmp0 = pset1<ResPacket>(tmp0);
      const LhsScalar* lhs0 = lhs + i*lhsStride;
      
      
      for (Index j=0; j<alignedStart; ++j)
        tmp0 += cj.pmul(lhs0[j], rhs[j]);

      if (alignedSize>alignedStart)
      {
        
        if ((size_t(lhs0+alignedStart)%sizeof(LhsPacket))==0)
          for (Index j = alignedStart;j<alignedSize;j+=RhsPacketSize)
            ptmp0 = pcj.pmadd(pload<LhsPacket>(&lhs0[j]), pload<RhsPacket>(&rhs[j]), ptmp0);
        else
          for (Index j = alignedStart;j<alignedSize;j+=RhsPacketSize)
            ptmp0 = pcj.pmadd(ploadu<LhsPacket>(&lhs0[j]), pload<RhsPacket>(&rhs[j]), ptmp0);
        tmp0 += predux(ptmp0);
      }

      
      
      for (Index j=alignedSize; j<depth; ++j)
        tmp0 += cj.pmul(lhs0[j], rhs[j]);
      res[i*resIncr] += alpha*tmp0;
    }
    if (skipRows)
    {
      start = 0;
      end = skipRows;
      skipRows = 0;
    }
    else
      break;
  } while(Vectorizable);

  #undef _EIGEN_ACCUMULATE_PACKETS
}

} 

} 

#endif 
