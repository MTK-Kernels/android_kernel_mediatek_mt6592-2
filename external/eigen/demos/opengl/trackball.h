// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_TRACKBALL_H
#define EIGEN_TRACKBALL_H

#include <Eigen/Geometry>

class Camera;

class Trackball
{
  public:

    enum Mode {Around, Local};

    Trackball() : mpCamera(0) {}

    void start(Mode m = Around) { mMode = m; mLastPointOk = false; }

    void setCamera(Camera* pCam) { mpCamera = pCam; }

    void track(const Eigen::Vector2i& newPoint2D);

  protected:

    bool mapToSphere( const Eigen::Vector2i& p2, Eigen::Vector3f& v3);

    Camera* mpCamera;
    Eigen::Vector3f mLastPoint3D;
    Mode mMode;
    bool mLastPointOk;

};

#endif 
