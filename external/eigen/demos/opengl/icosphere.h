// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ICOSPHERE_H
#define EIGEN_ICOSPHERE_H

#include <Eigen/Core>
#include <vector>

class IcoSphere
{
  public:
    IcoSphere(unsigned int levels=1);
    const std::vector<Eigen::Vector3f>& vertices() const { return mVertices; }
    const std::vector<int>& indices(int level) const;
    void draw(int level);
  protected:
    void _subdivide();
    std::vector<Eigen::Vector3f> mVertices;
    std::vector<std::vector<int>*> mIndices;
    std::vector<int> mListIds;
};

#endif 
