// Copyright (C) 2009 Ilya Baran <ibaran@mit.edu>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_BVALGORITHMS_H
#define EIGEN_BVALGORITHMS_H

namespace Eigen { 

namespace internal {

#ifndef EIGEN_PARSED_BY_DOXYGEN
template<typename BVH, typename Intersector>
bool intersect_helper(const BVH &tree, Intersector &intersector, typename BVH::Index root)
{
  typedef typename BVH::Index Index;
  typedef typename BVH::VolumeIterator VolIter;
  typedef typename BVH::ObjectIterator ObjIter;

  VolIter vBegin = VolIter(), vEnd = VolIter();
  ObjIter oBegin = ObjIter(), oEnd = ObjIter();

  std::vector<Index> todo(1, root);

  while(!todo.empty()) {
    tree.getChildren(todo.back(), vBegin, vEnd, oBegin, oEnd);
    todo.pop_back();

    for(; vBegin != vEnd; ++vBegin) 
      if(intersector.intersectVolume(tree.getVolume(*vBegin)))
        todo.push_back(*vBegin);

    for(; oBegin != oEnd; ++oBegin) 
      if(intersector.intersectObject(*oBegin))
        return true; 
  }
  return false;
}
#endif 

template<typename Volume1, typename Object1, typename Object2, typename Intersector>
struct intersector_helper1
{
  intersector_helper1(const Object2 &inStored, Intersector &in) : stored(inStored), intersector(in) {}
  bool intersectVolume(const Volume1 &vol) { return intersector.intersectVolumeObject(vol, stored); }
  bool intersectObject(const Object1 &obj) { return intersector.intersectObjectObject(obj, stored); }
  Object2 stored;
  Intersector &intersector;
private:
  intersector_helper1& operator=(const intersector_helper1&);
};

template<typename Volume2, typename Object2, typename Object1, typename Intersector>
struct intersector_helper2
{
  intersector_helper2(const Object1 &inStored, Intersector &in) : stored(inStored), intersector(in) {}
  bool intersectVolume(const Volume2 &vol) { return intersector.intersectObjectVolume(stored, vol); }
  bool intersectObject(const Object2 &obj) { return intersector.intersectObjectObject(stored, obj); }
  Object1 stored;
  Intersector &intersector;
private:
  intersector_helper2& operator=(const intersector_helper2&);
};

} 

template<typename BVH, typename Intersector>
void BVIntersect(const BVH &tree, Intersector &intersector)
{
  internal::intersect_helper(tree, intersector, tree.getRootIndex());
}

template<typename BVH1, typename BVH2, typename Intersector>
void BVIntersect(const BVH1 &tree1, const BVH2 &tree2, Intersector &intersector) 
{
  typedef typename BVH1::Index Index1;
  typedef typename BVH2::Index Index2;
  typedef internal::intersector_helper1<typename BVH1::Volume, typename BVH1::Object, typename BVH2::Object, Intersector> Helper1;
  typedef internal::intersector_helper2<typename BVH2::Volume, typename BVH2::Object, typename BVH1::Object, Intersector> Helper2;
  typedef typename BVH1::VolumeIterator VolIter1;
  typedef typename BVH1::ObjectIterator ObjIter1;
  typedef typename BVH2::VolumeIterator VolIter2;
  typedef typename BVH2::ObjectIterator ObjIter2;

  VolIter1 vBegin1 = VolIter1(), vEnd1 = VolIter1();
  ObjIter1 oBegin1 = ObjIter1(), oEnd1 = ObjIter1();
  VolIter2 vBegin2 = VolIter2(), vEnd2 = VolIter2(), vCur2 = VolIter2();
  ObjIter2 oBegin2 = ObjIter2(), oEnd2 = ObjIter2(), oCur2 = ObjIter2();

  std::vector<std::pair<Index1, Index2> > todo(1, std::make_pair(tree1.getRootIndex(), tree2.getRootIndex()));

  while(!todo.empty()) {
    tree1.getChildren(todo.back().first, vBegin1, vEnd1, oBegin1, oEnd1);
    tree2.getChildren(todo.back().second, vBegin2, vEnd2, oBegin2, oEnd2);
    todo.pop_back();

    for(; vBegin1 != vEnd1; ++vBegin1) { 
      const typename BVH1::Volume &vol1 = tree1.getVolume(*vBegin1);
      for(vCur2 = vBegin2; vCur2 != vEnd2; ++vCur2) { 
        if(intersector.intersectVolumeVolume(vol1, tree2.getVolume(*vCur2)))
          todo.push_back(std::make_pair(*vBegin1, *vCur2));
      }

      for(oCur2 = oBegin2; oCur2 != oEnd2; ++oCur2) {
        Helper1 helper(*oCur2, intersector);
        if(internal::intersect_helper(tree1, helper, *vBegin1))
          return; 
      }
    }

    for(; oBegin1 != oEnd1; ++oBegin1) { 
      for(vCur2 = vBegin2; vCur2 != vEnd2; ++vCur2) { 
        Helper2 helper(*oBegin1, intersector);
        if(internal::intersect_helper(tree2, helper, *vCur2))
          return; 
      }

      for(oCur2 = oBegin2; oCur2 != oEnd2; ++oCur2) {
        if(intersector.intersectObjectObject(*oBegin1, *oCur2))
          return; 
      }
    }
  }
}

namespace internal {

#ifndef EIGEN_PARSED_BY_DOXYGEN
template<typename BVH, typename Minimizer>
typename Minimizer::Scalar minimize_helper(const BVH &tree, Minimizer &minimizer, typename BVH::Index root, typename Minimizer::Scalar minimum)
{
  typedef typename Minimizer::Scalar Scalar;
  typedef typename BVH::Index Index;
  typedef std::pair<Scalar, Index> QueueElement; 
  typedef typename BVH::VolumeIterator VolIter;
  typedef typename BVH::ObjectIterator ObjIter;

  VolIter vBegin = VolIter(), vEnd = VolIter();
  ObjIter oBegin = ObjIter(), oEnd = ObjIter();
  std::priority_queue<QueueElement, std::vector<QueueElement>, std::greater<QueueElement> > todo; 

  todo.push(std::make_pair(Scalar(), root));

  while(!todo.empty()) {
    tree.getChildren(todo.top().second, vBegin, vEnd, oBegin, oEnd);
    todo.pop();

    for(; oBegin != oEnd; ++oBegin) 
      minimum = (std::min)(minimum, minimizer.minimumOnObject(*oBegin));

    for(; vBegin != vEnd; ++vBegin) { 
      Scalar val = minimizer.minimumOnVolume(tree.getVolume(*vBegin));
      if(val < minimum)
        todo.push(std::make_pair(val, *vBegin));
    }
  }

  return minimum;
}
#endif 


template<typename Volume1, typename Object1, typename Object2, typename Minimizer>
struct minimizer_helper1
{
  typedef typename Minimizer::Scalar Scalar;
  minimizer_helper1(const Object2 &inStored, Minimizer &m) : stored(inStored), minimizer(m) {}
  Scalar minimumOnVolume(const Volume1 &vol) { return minimizer.minimumOnVolumeObject(vol, stored); }
  Scalar minimumOnObject(const Object1 &obj) { return minimizer.minimumOnObjectObject(obj, stored); }
  Object2 stored;
  Minimizer &minimizer;
private:
  minimizer_helper1& operator=(const minimizer_helper1&);
};

template<typename Volume2, typename Object2, typename Object1, typename Minimizer>
struct minimizer_helper2
{
  typedef typename Minimizer::Scalar Scalar;
  minimizer_helper2(const Object1 &inStored, Minimizer &m) : stored(inStored), minimizer(m) {}
  Scalar minimumOnVolume(const Volume2 &vol) { return minimizer.minimumOnObjectVolume(stored, vol); }
  Scalar minimumOnObject(const Object2 &obj) { return minimizer.minimumOnObjectObject(stored, obj); }
  Object1 stored;
  Minimizer &minimizer;
private:
  minimizer_helper2& operator=(const minimizer_helper2&);
};

} 

template<typename BVH, typename Minimizer>
typename Minimizer::Scalar BVMinimize(const BVH &tree, Minimizer &minimizer)
{
  return internal::minimize_helper(tree, minimizer, tree.getRootIndex(), (std::numeric_limits<typename Minimizer::Scalar>::max)());
}

template<typename BVH1, typename BVH2, typename Minimizer>
typename Minimizer::Scalar BVMinimize(const BVH1 &tree1, const BVH2 &tree2, Minimizer &minimizer)
{
  typedef typename Minimizer::Scalar Scalar;
  typedef typename BVH1::Index Index1;
  typedef typename BVH2::Index Index2;
  typedef internal::minimizer_helper1<typename BVH1::Volume, typename BVH1::Object, typename BVH2::Object, Minimizer> Helper1;
  typedef internal::minimizer_helper2<typename BVH2::Volume, typename BVH2::Object, typename BVH1::Object, Minimizer> Helper2;
  typedef std::pair<Scalar, std::pair<Index1, Index2> > QueueElement; 
  typedef typename BVH1::VolumeIterator VolIter1;
  typedef typename BVH1::ObjectIterator ObjIter1;
  typedef typename BVH2::VolumeIterator VolIter2;
  typedef typename BVH2::ObjectIterator ObjIter2;

  VolIter1 vBegin1 = VolIter1(), vEnd1 = VolIter1();
  ObjIter1 oBegin1 = ObjIter1(), oEnd1 = ObjIter1();
  VolIter2 vBegin2 = VolIter2(), vEnd2 = VolIter2(), vCur2 = VolIter2();
  ObjIter2 oBegin2 = ObjIter2(), oEnd2 = ObjIter2(), oCur2 = ObjIter2();
  std::priority_queue<QueueElement, std::vector<QueueElement>, std::greater<QueueElement> > todo; 

  Scalar minimum = (std::numeric_limits<Scalar>::max)();
  todo.push(std::make_pair(Scalar(), std::make_pair(tree1.getRootIndex(), tree2.getRootIndex())));

  while(!todo.empty()) {
    tree1.getChildren(todo.top().second.first, vBegin1, vEnd1, oBegin1, oEnd1);
    tree2.getChildren(todo.top().second.second, vBegin2, vEnd2, oBegin2, oEnd2);
    todo.pop();

    for(; oBegin1 != oEnd1; ++oBegin1) { 
      for(oCur2 = oBegin2; oCur2 != oEnd2; ++oCur2) {
        minimum = (std::min)(minimum, minimizer.minimumOnObjectObject(*oBegin1, *oCur2));
      }

      for(vCur2 = vBegin2; vCur2 != vEnd2; ++vCur2) { 
        Helper2 helper(*oBegin1, minimizer);
        minimum = (std::min)(minimum, internal::minimize_helper(tree2, helper, *vCur2, minimum));
      }
    }

    for(; vBegin1 != vEnd1; ++vBegin1) { 
      const typename BVH1::Volume &vol1 = tree1.getVolume(*vBegin1);

      for(oCur2 = oBegin2; oCur2 != oEnd2; ++oCur2) {
        Helper1 helper(*oCur2, minimizer);
        minimum = (std::min)(minimum, internal::minimize_helper(tree1, helper, *vBegin1, minimum));
      }

      for(vCur2 = vBegin2; vCur2 != vEnd2; ++vCur2) { 
        Scalar val = minimizer.minimumOnVolumeVolume(vol1, tree2.getVolume(*vCur2));
        if(val < minimum)
          todo.push(std::make_pair(val, std::make_pair(*vBegin1, *vCur2)));
      }
    }
  }
  return minimum;
}

} 

#endif 
