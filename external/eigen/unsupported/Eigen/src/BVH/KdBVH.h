// Copyright (C) 2009 Ilya Baran <ibaran@mit.edu>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef KDBVH_H_INCLUDED
#define KDBVH_H_INCLUDED

namespace Eigen { 

namespace internal {

template<typename Scalar, int Dim>
struct vector_int_pair
{
EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(Scalar, Dim)
  typedef Matrix<Scalar, Dim, 1> VectorType;

  vector_int_pair(const VectorType &v, int i) : first(v), second(i) {}

  VectorType first;
  int second;
};

template<typename ObjectList, typename VolumeList, typename BoxIter>
struct get_boxes_helper {
  void operator()(const ObjectList &objects, BoxIter boxBegin, BoxIter boxEnd, VolumeList &outBoxes)
  {
    outBoxes.insert(outBoxes.end(), boxBegin, boxEnd);
    eigen_assert(outBoxes.size() == objects.size());
  }
};

template<typename ObjectList, typename VolumeList>
struct get_boxes_helper<ObjectList, VolumeList, int> {
  void operator()(const ObjectList &objects, int, int, VolumeList &outBoxes)
  {
    outBoxes.reserve(objects.size());
    for(int i = 0; i < (int)objects.size(); ++i)
      outBoxes.push_back(bounding_box(objects[i]));
  }
};

} 


template<typename _Scalar, int _Dim, typename _Object> class KdBVH
{
public:
  enum { Dim = _Dim };
  typedef _Object Object;
  typedef std::vector<Object, aligned_allocator<Object> > ObjectList;
  typedef _Scalar Scalar;
  typedef AlignedBox<Scalar, Dim> Volume;
  typedef std::vector<Volume, aligned_allocator<Volume> > VolumeList;
  typedef int Index;
  typedef const int *VolumeIterator; 
  typedef const Object *ObjectIterator;

  KdBVH() {}

  
  template<typename Iter> KdBVH(Iter begin, Iter end) { init(begin, end, 0, 0); } 

  
  template<typename OIter, typename BIter> KdBVH(OIter begin, OIter end, BIter boxBegin, BIter boxEnd) { init(begin, end, boxBegin, boxEnd); }

  template<typename Iter> void init(Iter begin, Iter end) { init(begin, end, 0, 0); }

  template<typename OIter, typename BIter> void init(OIter begin, OIter end, BIter boxBegin, BIter boxEnd)
  {
    objects.clear();
    boxes.clear();
    children.clear();

    objects.insert(objects.end(), begin, end);
    int n = static_cast<int>(objects.size());

    if(n < 2)
      return; 

    VolumeList objBoxes;
    VIPairList objCenters;

    
    internal::get_boxes_helper<ObjectList, VolumeList, BIter>()(objects, boxBegin, boxEnd, objBoxes);

    objCenters.reserve(n);
    boxes.reserve(n - 1);
    children.reserve(2 * n - 2);

    for(int i = 0; i < n; ++i)
      objCenters.push_back(VIPair(objBoxes[i].center(), i));

    build(objCenters, 0, n, objBoxes, 0); 

    ObjectList tmp(n);
    tmp.swap(objects);
    for(int i = 0; i < n; ++i)
      objects[i] = tmp[objCenters[i].second];
  }

  
  inline Index getRootIndex() const { return (int)boxes.size() - 1; }

  EIGEN_STRONG_INLINE void getChildren(Index index, VolumeIterator &outVBegin, VolumeIterator &outVEnd,
                                       ObjectIterator &outOBegin, ObjectIterator &outOEnd) const
  { 
    if(index < 0) {
      outVBegin = outVEnd;
      if(!objects.empty())
        outOBegin = &(objects[0]);
      outOEnd = outOBegin + objects.size(); 
      return;
    }

    int numBoxes = static_cast<int>(boxes.size());

    int idx = index * 2;
    if(children[idx + 1] < numBoxes) { 
      outVBegin = &(children[idx]);
      outVEnd = outVBegin + 2;
      outOBegin = outOEnd;
    }
    else if(children[idx] >= numBoxes) { 
      outVBegin = outVEnd;
      outOBegin = &(objects[children[idx] - numBoxes]);
      outOEnd = outOBegin + 2;
    } else { 
      outVBegin = &(children[idx]);
      outVEnd = outVBegin + 1;
      outOBegin = &(objects[children[idx + 1] - numBoxes]);
      outOEnd = outOBegin + 1;
    }
  }

  
  inline const Volume &getVolume(Index index) const
  {
    return boxes[index];
  }

private:
  typedef internal::vector_int_pair<Scalar, Dim> VIPair;
  typedef std::vector<VIPair, aligned_allocator<VIPair> > VIPairList;
  typedef Matrix<Scalar, Dim, 1> VectorType;
  struct VectorComparator 
  {
    VectorComparator(int inDim) : dim(inDim) {}
    inline bool operator()(const VIPair &v1, const VIPair &v2) const { return v1.first[dim] < v2.first[dim]; }
    int dim;
  };

  
  
  
  void build(VIPairList &objCenters, int from, int to, const VolumeList &objBoxes, int dim)
  {
    eigen_assert(to - from > 1);
    if(to - from == 2) {
      boxes.push_back(objBoxes[objCenters[from].second].merged(objBoxes[objCenters[from + 1].second]));
      children.push_back(from + (int)objects.size() - 1); 
      children.push_back(from + (int)objects.size());
    }
    else if(to - from == 3) {
      int mid = from + 2;
      std::nth_element(objCenters.begin() + from, objCenters.begin() + mid,
                        objCenters.begin() + to, VectorComparator(dim)); 
      build(objCenters, from, mid, objBoxes, (dim + 1) % Dim);
      int idx1 = (int)boxes.size() - 1;
      boxes.push_back(boxes[idx1].merged(objBoxes[objCenters[mid].second]));
      children.push_back(idx1);
      children.push_back(mid + (int)objects.size() - 1);
    }
    else {
      int mid = from + (to - from) / 2;
      nth_element(objCenters.begin() + from, objCenters.begin() + mid,
                  objCenters.begin() + to, VectorComparator(dim)); 
      build(objCenters, from, mid, objBoxes, (dim + 1) % Dim);
      int idx1 = (int)boxes.size() - 1;
      build(objCenters, mid, to, objBoxes, (dim + 1) % Dim);
      int idx2 = (int)boxes.size() - 1;
      boxes.push_back(boxes[idx1].merged(boxes[idx2]));
      children.push_back(idx1);
      children.push_back(idx2);
    }
  }

  std::vector<int> children; 
  VolumeList boxes;
  ObjectList objects;
};

} 

#endif 
