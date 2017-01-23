// Copyright (C) 2010-2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_LAPACK_COMMON_H
#define EIGEN_LAPACK_COMMON_H

#include "../blas/common.h"

#define EIGEN_LAPACK_FUNC(FUNC,ARGLIST)               \
  extern "C" { int EIGEN_BLAS_FUNC(FUNC) ARGLIST; }   \
  int EIGEN_BLAS_FUNC(FUNC) ARGLIST

typedef Eigen::Map<Eigen::Transpositions<Eigen::Dynamic,Eigen::Dynamic,int> > PivotsType;



#endif 
