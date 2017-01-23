// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_RANDOMSETTER_H
#define EIGEN_RANDOMSETTER_H

namespace Eigen { 

template<typename Scalar> struct StdMapTraits
{
  typedef int KeyType;
  typedef std::map<KeyType,Scalar> Type;
  enum {
    IsSorted = 1
  };

  static void setInvalidKey(Type&, const KeyType&) {}
};

#ifdef EIGEN_UNORDERED_MAP_SUPPORT
template<typename Scalar> struct StdUnorderedMapTraits
{
  typedef int KeyType;
  typedef std::unordered_map<KeyType,Scalar> Type;
  enum {
    IsSorted = 0
  };

  static void setInvalidKey(Type&, const KeyType&) {}
};
#endif 

#ifdef _DENSE_HASH_MAP_H_
template<typename Scalar> struct GoogleDenseHashMapTraits
{
  typedef int KeyType;
  typedef google::dense_hash_map<KeyType,Scalar> Type;
  enum {
    IsSorted = 0
  };

  static void setInvalidKey(Type& map, const KeyType& k)
  { map.set_empty_key(k); }
};
#endif

#ifdef _SPARSE_HASH_MAP_H_
template<typename Scalar> struct GoogleSparseHashMapTraits
{
  typedef int KeyType;
  typedef google::sparse_hash_map<KeyType,Scalar> Type;
  enum {
    IsSorted = 0
  };

  static void setInvalidKey(Type&, const KeyType&) {}
};
#endif

template<typename SparseMatrixType,
         template <typename T> class MapTraits =
#if defined _DENSE_HASH_MAP_H_
          GoogleDenseHashMapTraits
#elif defined _HASH_MAP
          GnuHashMapTraits
#else
          StdMapTraits
#endif
         ,int OuterPacketBits = 6>
class RandomSetter
{
    typedef typename SparseMatrixType::Scalar Scalar;
    typedef typename SparseMatrixType::Index Index;

    struct ScalarWrapper
    {
      ScalarWrapper() : value(0) {}
      Scalar value;
    };
    typedef typename MapTraits<ScalarWrapper>::KeyType KeyType;
    typedef typename MapTraits<ScalarWrapper>::Type HashMapType;
    static const int OuterPacketMask = (1 << OuterPacketBits) - 1;
    enum {
      SwapStorage = 1 - MapTraits<ScalarWrapper>::IsSorted,
      TargetRowMajor = (SparseMatrixType::Flags & RowMajorBit) ? 1 : 0,
      SetterRowMajor = SwapStorage ? 1-TargetRowMajor : TargetRowMajor
    };

  public:

    inline RandomSetter(SparseMatrixType& target)
      : mp_target(&target)
    {
      const Index outerSize = SwapStorage ? target.innerSize() : target.outerSize();
      const Index innerSize = SwapStorage ? target.outerSize() : target.innerSize();
      m_outerPackets = outerSize >> OuterPacketBits;
      if (outerSize&OuterPacketMask)
        m_outerPackets += 1;
      m_hashmaps = new HashMapType[m_outerPackets];
      
      Index aux = innerSize - 1;
      m_keyBitsOffset = 0;
      while (aux)
      {
        ++m_keyBitsOffset;
        aux = aux >> 1;
      }
      KeyType ik = (1<<(OuterPacketBits+m_keyBitsOffset));
      for (Index k=0; k<m_outerPackets; ++k)
        MapTraits<ScalarWrapper>::setInvalidKey(m_hashmaps[k],ik);

      
      for (Index j=0; j<mp_target->outerSize(); ++j)
        for (typename SparseMatrixType::InnerIterator it(*mp_target,j); it; ++it)
          (*this)(TargetRowMajor?j:it.index(), TargetRowMajor?it.index():j) = it.value();
    }

    
    ~RandomSetter()
    {
      KeyType keyBitsMask = (1<<m_keyBitsOffset)-1;
      if (!SwapStorage) 
      {
        mp_target->setZero();
        mp_target->makeCompressed();
        mp_target->reserve(nonZeros());
        Index prevOuter = -1;
        for (Index k=0; k<m_outerPackets; ++k)
        {
          const Index outerOffset = (1<<OuterPacketBits) * k;
          typename HashMapType::iterator end = m_hashmaps[k].end();
          for (typename HashMapType::iterator it = m_hashmaps[k].begin(); it!=end; ++it)
          {
            const Index outer = (it->first >> m_keyBitsOffset) + outerOffset;
            const Index inner = it->first & keyBitsMask;
            if (prevOuter!=outer)
            {
              for (Index j=prevOuter+1;j<=outer;++j)
                mp_target->startVec(j);
              prevOuter = outer;
            }
            mp_target->insertBackByOuterInner(outer, inner) = it->second.value;
          }
        }
        mp_target->finalize();
      }
      else
      {
        VectorXi positions(mp_target->outerSize());
        positions.setZero();
        
        for (Index k=0; k<m_outerPackets; ++k)
        {
          typename HashMapType::iterator end = m_hashmaps[k].end();
          for (typename HashMapType::iterator it = m_hashmaps[k].begin(); it!=end; ++it)
          {
            const Index outer = it->first & keyBitsMask;
            ++positions[outer];
          }
        }
        
        Index count = 0;
        for (Index j=0; j<mp_target->outerSize(); ++j)
        {
          Index tmp = positions[j];
          mp_target->outerIndexPtr()[j] = count;
          positions[j] = count;
          count += tmp;
        }
        mp_target->makeCompressed();
        mp_target->outerIndexPtr()[mp_target->outerSize()] = count;
        mp_target->resizeNonZeros(count);
        
        for (Index k=0; k<m_outerPackets; ++k)
        {
          const Index outerOffset = (1<<OuterPacketBits) * k;
          typename HashMapType::iterator end = m_hashmaps[k].end();
          for (typename HashMapType::iterator it = m_hashmaps[k].begin(); it!=end; ++it)
          {
            const Index inner = (it->first >> m_keyBitsOffset) + outerOffset;
            const Index outer = it->first & keyBitsMask;
            
            
            
            
            Index posStart = mp_target->outerIndexPtr()[outer];
            Index i = (positions[outer]++) - 1;
            while ( (i >= posStart) && (mp_target->innerIndexPtr()[i] > inner) )
            {
              mp_target->valuePtr()[i+1] = mp_target->valuePtr()[i];
              mp_target->innerIndexPtr()[i+1] = mp_target->innerIndexPtr()[i];
              --i;
            }
            mp_target->innerIndexPtr()[i+1] = inner;
            mp_target->valuePtr()[i+1] = it->second.value;
          }
        }
      }
      delete[] m_hashmaps;
    }

    
    Scalar& operator() (Index row, Index col)
    {
      const Index outer = SetterRowMajor ? row : col;
      const Index inner = SetterRowMajor ? col : row;
      const Index outerMajor = outer >> OuterPacketBits; 
      const Index outerMinor = outer & OuterPacketMask;  
      const KeyType key = (KeyType(outerMinor)<<m_keyBitsOffset) | inner;
      return m_hashmaps[outerMajor][key].value;
    }

    Index nonZeros() const
    {
      Index nz = 0;
      for (Index k=0; k<m_outerPackets; ++k)
        nz += static_cast<Index>(m_hashmaps[k].size());
      return nz;
    }


  protected:

    HashMapType* m_hashmaps;
    SparseMatrixType* mp_target;
    Index m_outerPackets;
    unsigned char m_keyBitsOffset;
};

} 

#endif 
