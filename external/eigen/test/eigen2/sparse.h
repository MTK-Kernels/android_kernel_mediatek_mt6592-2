// Copyright (C) 2008 Daniel Gomez Ferro <dgomezferro@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_TESTSPARSE_H

#include "main.h"

#if EIGEN_GNUC_AT_LEAST(4,0) && !defined __ICC
#include <tr1/unordered_map>
#define EIGEN_UNORDERED_MAP_SUPPORT
namespace std {
  using std::tr1::unordered_map;
}
#endif

#ifdef EIGEN_GOOGLEHASH_SUPPORT
  #include <google/sparse_hash_map>
#endif

#include <Eigen/Cholesky>
#include <Eigen/LU>
#include <Eigen/Sparse>

enum {
  ForceNonZeroDiag = 1,
  MakeLowerTriangular = 2,
  MakeUpperTriangular = 4,
  ForceRealDiag = 8
};

template<typename Scalar> void
initSparse(double density,
           Matrix<Scalar,Dynamic,Dynamic>& refMat,
           SparseMatrix<Scalar>& sparseMat,
           int flags = 0,
           std::vector<Vector2i>* zeroCoords = 0,
           std::vector<Vector2i>* nonzeroCoords = 0)
{
  sparseMat.startFill(int(refMat.rows()*refMat.cols()*density));
  for(int j=0; j<refMat.cols(); j++)
  {
    for(int i=0; i<refMat.rows(); i++)
    {
      Scalar v = (ei_random<double>(0,1) < density) ? ei_random<Scalar>() : Scalar(0);
      if ((flags&ForceNonZeroDiag) && (i==j))
      {
        v = ei_random<Scalar>()*Scalar(3.);
        v = v*v + Scalar(5.);
      }
      if ((flags & MakeLowerTriangular) && j>i)
        v = Scalar(0);
      else if ((flags & MakeUpperTriangular) && j<i)
        v = Scalar(0);
      
      if ((flags&ForceRealDiag) && (i==j))
        v = ei_real(v);
        
      if (v!=Scalar(0))
      {
        sparseMat.fill(i,j) = v;
        if (nonzeroCoords)
          nonzeroCoords->push_back(Vector2i(i,j));
      }
      else if (zeroCoords)
      {
        zeroCoords->push_back(Vector2i(i,j));
      }
      refMat(i,j) = v;
    }
  }
  sparseMat.endFill();
}

template<typename Scalar> void
initSparse(double density,
           Matrix<Scalar,Dynamic,Dynamic>& refMat,
           DynamicSparseMatrix<Scalar>& sparseMat,
           int flags = 0,
           std::vector<Vector2i>* zeroCoords = 0,
           std::vector<Vector2i>* nonzeroCoords = 0)
{
  sparseMat.startFill(int(refMat.rows()*refMat.cols()*density));
  for(int j=0; j<refMat.cols(); j++)
  {
    for(int i=0; i<refMat.rows(); i++)
    {
      Scalar v = (ei_random<double>(0,1) < density) ? ei_random<Scalar>() : Scalar(0);
      if ((flags&ForceNonZeroDiag) && (i==j))
      {
        v = ei_random<Scalar>()*Scalar(3.);
        v = v*v + Scalar(5.);
      }
      if ((flags & MakeLowerTriangular) && j>i)
        v = Scalar(0);
      else if ((flags & MakeUpperTriangular) && j<i)
        v = Scalar(0);
      
      if ((flags&ForceRealDiag) && (i==j))
        v = ei_real(v);
        
      if (v!=Scalar(0))
      {
        sparseMat.fill(i,j) = v;
        if (nonzeroCoords)
          nonzeroCoords->push_back(Vector2i(i,j));
      }
      else if (zeroCoords)
      {
        zeroCoords->push_back(Vector2i(i,j));
      }
      refMat(i,j) = v;
    }
  }
  sparseMat.endFill();
}

template<typename Scalar> void
initSparse(double density,
           Matrix<Scalar,Dynamic,1>& refVec,
           SparseVector<Scalar>& sparseVec,
           std::vector<int>* zeroCoords = 0,
           std::vector<int>* nonzeroCoords = 0)
{
  sparseVec.reserve(int(refVec.size()*density));
  sparseVec.setZero();
  for(int i=0; i<refVec.size(); i++)
  {
    Scalar v = (ei_random<double>(0,1) < density) ? ei_random<Scalar>() : Scalar(0);
    if (v!=Scalar(0))
    {
      sparseVec.fill(i) = v;
      if (nonzeroCoords)
        nonzeroCoords->push_back(i);
    }
    else if (zeroCoords)
        zeroCoords->push_back(i);
    refVec[i] = v;
  }
}

#endif 
