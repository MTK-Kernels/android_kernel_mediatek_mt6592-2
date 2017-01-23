// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_GENERIC_PACKET_MATH_H
#define EIGEN_GENERIC_PACKET_MATH_H

namespace Eigen {

namespace internal {


#ifndef EIGEN_DEBUG_ALIGNED_LOAD
#define EIGEN_DEBUG_ALIGNED_LOAD
#endif

#ifndef EIGEN_DEBUG_UNALIGNED_LOAD
#define EIGEN_DEBUG_UNALIGNED_LOAD
#endif

#ifndef EIGEN_DEBUG_ALIGNED_STORE
#define EIGEN_DEBUG_ALIGNED_STORE
#endif

#ifndef EIGEN_DEBUG_UNALIGNED_STORE
#define EIGEN_DEBUG_UNALIGNED_STORE
#endif

struct default_packet_traits
{
  enum {
    HasAdd    = 1,
    HasSub    = 1,
    HasMul    = 1,
    HasNegate = 1,
    HasAbs    = 1,
    HasAbs2   = 1,
    HasMin    = 1,
    HasMax    = 1,
    HasConj   = 1,
    HasSetLinear = 1,

    HasDiv    = 0,
    HasSqrt   = 0,
    HasExp    = 0,
    HasLog    = 0,
    HasPow    = 0,

    HasSin    = 0,
    HasCos    = 0,
    HasTan    = 0,
    HasASin   = 0,
    HasACos   = 0,
    HasATan   = 0
  };
};

template<typename T> struct packet_traits : default_packet_traits
{
  typedef T type;
  enum {
    Vectorizable = 0,
    size = 1,
    AlignedOnScalar = 0
  };
  enum {
    HasAdd    = 0,
    HasSub    = 0,
    HasMul    = 0,
    HasNegate = 0,
    HasAbs    = 0,
    HasAbs2   = 0,
    HasMin    = 0,
    HasMax    = 0,
    HasConj   = 0,
    HasSetLinear = 0
  };
};

template<typename Packet> inline Packet
padd(const Packet& a,
        const Packet& b) { return a+b; }

template<typename Packet> inline Packet
psub(const Packet& a,
        const Packet& b) { return a-b; }

template<typename Packet> inline Packet
pnegate(const Packet& a) { return -a; }

template<typename Packet> inline Packet
pconj(const Packet& a) { return numext::conj(a); }

template<typename Packet> inline Packet
pmul(const Packet& a,
        const Packet& b) { return a*b; }

template<typename Packet> inline Packet
pdiv(const Packet& a,
        const Packet& b) { return a/b; }

template<typename Packet> inline Packet
pmin(const Packet& a,
        const Packet& b) { using std::min; return (min)(a, b); }

template<typename Packet> inline Packet
pmax(const Packet& a,
        const Packet& b) { using std::max; return (max)(a, b); }

template<typename Packet> inline Packet
pabs(const Packet& a) { using std::abs; return abs(a); }

template<typename Packet> inline Packet
pand(const Packet& a, const Packet& b) { return a & b; }

template<typename Packet> inline Packet
por(const Packet& a, const Packet& b) { return a | b; }

template<typename Packet> inline Packet
pxor(const Packet& a, const Packet& b) { return a ^ b; }

template<typename Packet> inline Packet
pandnot(const Packet& a, const Packet& b) { return a & (!b); }

template<typename Packet> inline Packet
pload(const typename unpacket_traits<Packet>::type* from) { return *from; }

template<typename Packet> inline Packet
ploadu(const typename unpacket_traits<Packet>::type* from) { return *from; }

template<typename Packet> inline Packet
ploaddup(const typename unpacket_traits<Packet>::type* from) { return *from; }

template<typename Packet> inline Packet
pset1(const typename unpacket_traits<Packet>::type& a) { return a; }

template<typename Scalar> inline typename packet_traits<Scalar>::type
plset(const Scalar& a) { return a; }

template<typename Scalar, typename Packet> inline void pstore(Scalar* to, const Packet& from)
{ (*to) = from; }

template<typename Scalar, typename Packet> inline void pstoreu(Scalar* to, const Packet& from)
{ (*to) = from; }

template<typename Scalar> inline void prefetch(const Scalar* addr)
{
#if !defined(_MSC_VER)
__builtin_prefetch(addr);
#endif
}

template<typename Packet> inline typename unpacket_traits<Packet>::type pfirst(const Packet& a)
{ return a; }

template<typename Packet> inline Packet
preduxp(const Packet* vecs) { return vecs[0]; }

template<typename Packet> inline typename unpacket_traits<Packet>::type predux(const Packet& a)
{ return a; }

template<typename Packet> inline typename unpacket_traits<Packet>::type predux_mul(const Packet& a)
{ return a; }

template<typename Packet> inline typename unpacket_traits<Packet>::type predux_min(const Packet& a)
{ return a; }

template<typename Packet> inline typename unpacket_traits<Packet>::type predux_max(const Packet& a)
{ return a; }

template<typename Packet> inline Packet preverse(const Packet& a)
{ return a; }


template<typename Packet> inline Packet pcplxflip(const Packet& a)
{
  
  return Packet(imag(a),real(a));
}


template<typename Packet> EIGEN_DECLARE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS
Packet psin(const Packet& a) { using std::sin; return sin(a); }

template<typename Packet> EIGEN_DECLARE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS
Packet pcos(const Packet& a) { using std::cos; return cos(a); }

template<typename Packet> EIGEN_DECLARE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS
Packet ptan(const Packet& a) { using std::tan; return tan(a); }

template<typename Packet> EIGEN_DECLARE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS
Packet pasin(const Packet& a) { using std::asin; return asin(a); }

template<typename Packet> EIGEN_DECLARE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS
Packet pacos(const Packet& a) { using std::acos; return acos(a); }

template<typename Packet> EIGEN_DECLARE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS
Packet pexp(const Packet& a) { using std::exp; return exp(a); }

template<typename Packet> EIGEN_DECLARE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS
Packet plog(const Packet& a) { using std::log; return log(a); }

template<typename Packet> EIGEN_DECLARE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS
Packet psqrt(const Packet& a) { using std::sqrt; return sqrt(a); }

/***************************************************************************
* The following functions might not have to be overwritten for vectorized types
***************************************************************************/

template<typename Packet>
inline void pstore1(typename unpacket_traits<Packet>::type* to, const typename unpacket_traits<Packet>::type& a)
{
  pstore(to, pset1<Packet>(a));
}

template<typename Packet> inline Packet
pmadd(const Packet&  a,
         const Packet&  b,
         const Packet&  c)
{ return padd(pmul(a, b),c); }

template<typename Packet, int LoadMode>
inline Packet ploadt(const typename unpacket_traits<Packet>::type* from)
{
  if(LoadMode == Aligned)
    return pload<Packet>(from);
  else
    return ploadu<Packet>(from);
}

template<typename Scalar, typename Packet, int LoadMode>
inline void pstoret(Scalar* to, const Packet& from)
{
  if(LoadMode == Aligned)
    pstore(to, from);
  else
    pstoreu(to, from);
}

template<int Offset,typename PacketType>
struct palign_impl
{
  
  static inline void run(PacketType&, const PacketType&) {}
};

template<int Offset,typename PacketType>
inline void palign(PacketType& first, const PacketType& second)
{
  palign_impl<Offset,PacketType>::run(first,second);
}


template<> inline std::complex<float> pmul(const std::complex<float>& a, const std::complex<float>& b)
{ return std::complex<float>(real(a)*real(b) - imag(a)*imag(b), imag(a)*real(b) + real(a)*imag(b)); }

template<> inline std::complex<double> pmul(const std::complex<double>& a, const std::complex<double>& b)
{ return std::complex<double>(real(a)*real(b) - imag(a)*imag(b), imag(a)*real(b) + real(a)*imag(b)); }

} 

} 

#endif 

