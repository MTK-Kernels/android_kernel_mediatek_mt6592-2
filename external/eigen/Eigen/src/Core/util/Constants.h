// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2007-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_CONSTANTS_H
#define EIGEN_CONSTANTS_H

namespace Eigen {

const int Dynamic = -1;

const int DynamicIndex = 0xffffff;

const int Infinity = -1;


const unsigned int RowMajorBit = 0x1;

const unsigned int EvalBeforeNestingBit = 0x2;

const unsigned int EvalBeforeAssigningBit = 0x4;

const unsigned int PacketAccessBit = 0x8;

#ifdef EIGEN_VECTORIZE
const unsigned int ActualPacketAccessBit = PacketAccessBit;
#else
const unsigned int ActualPacketAccessBit = 0x0;
#endif

const unsigned int LinearAccessBit = 0x10;

const unsigned int LvalueBit = 0x20;

const unsigned int DirectAccessBit = 0x40;

const unsigned int AlignedBit = 0x80;

const unsigned int NestByRefBit = 0x100;

const unsigned int HereditaryBits = RowMajorBit
                                  | EvalBeforeNestingBit
                                  | EvalBeforeAssigningBit;


enum {
  
  Lower=0x1,                      
  
  Upper=0x2,                      
  
  UnitDiag=0x4, 
  
  ZeroDiag=0x8,
  
  UnitLower=UnitDiag|Lower, 
  
  UnitUpper=UnitDiag|Upper,
  
  StrictlyLower=ZeroDiag|Lower, 
  
  StrictlyUpper=ZeroDiag|Upper,
  
  SelfAdjoint=0x10,
  
  Symmetric=0x20
};

enum { 
  
  Unaligned=0, 
  
  Aligned=1 
};

enum CornerType { TopLeft, TopRight, BottomLeft, BottomRight };

enum DirectionType { 
  Vertical, 
  Horizontal, 
  BothDirections 
};

enum {
  
  DefaultTraversal,
  
  LinearTraversal,
  InnerVectorizedTraversal,
  LinearVectorizedTraversal,
  SliceVectorizedTraversal,
  
  InvalidTraversal,
  
  AllAtOnceTraversal
};

enum {
  
  NoUnrolling,
  
  InnerUnrolling,
  CompleteUnrolling
};

enum {
  Specialized,
  BuiltIn
};

enum {
  
  ColMajor = 0,
  
  RowMajor = 0x1,  
  
  AutoAlign = 0,
   
  DontAlign = 0x2
};

enum {
  
  OnTheLeft = 1,  
  
  OnTheRight = 2  
};

/* the following used to be written as:
 *
 *   struct NoChange_t {};
 *   namespace {
 *     EIGEN_UNUSED NoChange_t NoChange;
 *   }
 *
 * on the ground that it feels dangerous to disambiguate overloaded functions on enum/integer types.  
 * However, this leads to "variable declared but never referenced" warnings on Intel Composer XE,
 * and we do not know how to get rid of them (bug 450).
 */

enum NoChange_t   { NoChange };
enum Sequential_t { Sequential };
enum Default_t    { Default };

enum {
  IsDense         = 0,
  IsSparse
};

enum AccessorLevels {
  
  ReadOnlyAccessors, 
  
  WriteAccessors, 
  
  DirectAccessors, 
  
  DirectWriteAccessors
};

enum DecompositionOptions {
  
  Pivoting            = 0x01, 
  
  NoPivoting          = 0x02, 
  
  ComputeFullU        = 0x04,
  
  ComputeThinU        = 0x08,
  
  ComputeFullV        = 0x10,
  
  ComputeThinV        = 0x20,
  EigenvaluesOnly     = 0x40,
  ComputeEigenvectors = 0x80,
  
  EigVecMask = EigenvaluesOnly | ComputeEigenvectors,
  Ax_lBx              = 0x100,
  ABx_lx              = 0x200,
  BAx_lx              = 0x400,
  
  GenEigMask = Ax_lBx | ABx_lx | BAx_lx
};

enum QRPreconditioners {
  
  NoQRPreconditioner,
  
  HouseholderQRPreconditioner,
  
  ColPivHouseholderQRPreconditioner,
  
  FullPivHouseholderQRPreconditioner
};

#ifdef Success
#error The preprocessor symbol 'Success' is defined, possibly by the X11 header file X.h
#endif

enum ComputationInfo {
  
  Success = 0,        
  
  NumericalIssue = 1, 
  
  NoConvergence = 2,
  InvalidInput = 3
};

enum TransformTraits {
  
  Isometry      = 0x1,
  Affine        = 0x2,
  
  AffineCompact = 0x10 | Affine,
  
  Projective    = 0x20
};

namespace Architecture
{
  enum Type {
    Generic = 0x0,
    SSE = 0x1,
    AltiVec = 0x2,
#if defined EIGEN_VECTORIZE_SSE
    Target = SSE
#elif defined EIGEN_VECTORIZE_ALTIVEC
    Target = AltiVec
#else
    Target = Generic
#endif
  };
}

enum { CoeffBasedProductMode, LazyCoeffBasedProductMode, OuterProduct, InnerProduct, GemvProduct, GemmProduct };

enum Action {GetAction, SetAction};

struct Dense {};

struct MatrixXpr {};

struct ArrayXpr {};

} 

#endif 
