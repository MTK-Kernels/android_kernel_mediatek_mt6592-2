// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2008-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2009 Kenneth Riddile <kfriddile@yahoo.com>
// Copyright (C) 2010 Hauke Heibel <hauke.heibel@gmail.com>
// Copyright (C) 2010 Thomas Capricelli <orzel@freehackers.org>
// Public License v. 2.0. If a copy of the MPL was not distributed



#ifndef EIGEN_MEMORY_H
#define EIGEN_MEMORY_H

#ifndef EIGEN_MALLOC_ALREADY_ALIGNED


#if defined(__GLIBC__) && ((__GLIBC__>=2 && __GLIBC_MINOR__ >= 8) || __GLIBC__>2) \
 && defined(__LP64__) && ! defined( __SANITIZE_ADDRESS__ )
  #define EIGEN_GLIBC_MALLOC_ALREADY_ALIGNED 1
#else
  #define EIGEN_GLIBC_MALLOC_ALREADY_ALIGNED 0
#endif

#if defined(__FreeBSD__) && !defined(__arm__) && !defined(__mips__)
  #define EIGEN_FREEBSD_MALLOC_ALREADY_ALIGNED 1
#else
  #define EIGEN_FREEBSD_MALLOC_ALREADY_ALIGNED 0
#endif

#if defined(__APPLE__) \
 || defined(_WIN64) \
 || EIGEN_GLIBC_MALLOC_ALREADY_ALIGNED \
 || EIGEN_FREEBSD_MALLOC_ALREADY_ALIGNED
  #define EIGEN_MALLOC_ALREADY_ALIGNED 1
#else
  #define EIGEN_MALLOC_ALREADY_ALIGNED 0
#endif

#endif

#if defined(__unix__) || defined(__unix)
  #include <unistd.h>
  #if ((defined __QNXNTO__) || (defined _GNU_SOURCE) || (defined __PGI) || ((defined _XOPEN_SOURCE) && (_XOPEN_SOURCE >= 600))) && (defined _POSIX_ADVISORY_INFO) && (_POSIX_ADVISORY_INFO > 0)
    #define EIGEN_HAS_POSIX_MEMALIGN 1
  #endif
#endif

#ifndef EIGEN_HAS_POSIX_MEMALIGN
  #define EIGEN_HAS_POSIX_MEMALIGN 0
#endif

#if defined(EIGEN_VECTORIZE_SSE) && !defined(EIGEN_ANDROID_POSIX_MEMALIGN_WR)
  #define EIGEN_HAS_MM_MALLOC 1
#else
  #define EIGEN_HAS_MM_MALLOC 0
#endif

namespace Eigen {

namespace internal {

inline void throw_std_bad_alloc()
{
  #ifdef EIGEN_EXCEPTIONS
    throw std::bad_alloc();
  #else
    std::size_t huge = -1;
    new int[huge];
  #endif
}



inline void* handmade_aligned_malloc(std::size_t size)
{
  void *original = std::malloc(size+16);
  if (original == 0) return 0;
  void *aligned = reinterpret_cast<void*>((reinterpret_cast<std::size_t>(original) & ~(std::size_t(15))) + 16);
  *(reinterpret_cast<void**>(aligned) - 1) = original;
  return aligned;
}

inline void handmade_aligned_free(void *ptr)
{
  if (ptr) std::free(*(reinterpret_cast<void**>(ptr) - 1));
}

inline void* handmade_aligned_realloc(void* ptr, std::size_t size, std::size_t = 0)
{
  if (ptr == 0) return handmade_aligned_malloc(size);
  void *original = *(reinterpret_cast<void**>(ptr) - 1);
  std::ptrdiff_t previous_offset = static_cast<char *>(ptr)-static_cast<char *>(original);
  original = std::realloc(original,size+16);
  if (original == 0) return 0;
  void *aligned = reinterpret_cast<void*>((reinterpret_cast<std::size_t>(original) & ~(std::size_t(15))) + 16);
  void *previous_aligned = static_cast<char *>(original)+previous_offset;
  if(aligned!=previous_aligned)
    std::memmove(aligned, previous_aligned, size);
  
  *(reinterpret_cast<void**>(aligned) - 1) = original;
  return aligned;
}


void* aligned_malloc(std::size_t size);
void  aligned_free(void *ptr);

inline void* generic_aligned_realloc(void* ptr, size_t size, size_t old_size)
{
  if (ptr==0)
    return aligned_malloc(size);

  if (size==0)
  {
    aligned_free(ptr);
    return 0;
  }

  void* newptr = aligned_malloc(size);
  if (newptr == 0)
  {
    #ifdef EIGEN_HAS_ERRNO
    errno = ENOMEM; 
    #endif
    return 0;
  }

  if (ptr != 0)
  {
    std::memcpy(newptr, ptr, (std::min)(size,old_size));
    aligned_free(ptr);
  }

  return newptr;
}


#ifdef EIGEN_NO_MALLOC
inline void check_that_malloc_is_allowed()
{
  eigen_assert(false && "heap allocation is forbidden (EIGEN_NO_MALLOC is defined)");
}
#elif defined EIGEN_RUNTIME_NO_MALLOC
inline bool is_malloc_allowed_impl(bool update, bool new_value = false)
{
  static bool value = true;
  if (update == 1)
    value = new_value;
  return value;
}
inline bool is_malloc_allowed() { return is_malloc_allowed_impl(false); }
inline bool set_is_malloc_allowed(bool new_value) { return is_malloc_allowed_impl(true, new_value); }
inline void check_that_malloc_is_allowed()
{
  eigen_assert(is_malloc_allowed() && "heap allocation is forbidden (EIGEN_RUNTIME_NO_MALLOC is defined and g_is_malloc_allowed is false)");
}
#else 
inline void check_that_malloc_is_allowed()
{}
#endif

inline void* aligned_malloc(size_t size)
{
  check_that_malloc_is_allowed();

  void *result;
  #if !EIGEN_ALIGN
    result = std::malloc(size);
  #elif EIGEN_MALLOC_ALREADY_ALIGNED
    result = std::malloc(size);
  #elif EIGEN_HAS_POSIX_MEMALIGN
    if(posix_memalign(&result, 16, size)) result = 0;
  #elif EIGEN_HAS_MM_MALLOC
    result = _mm_malloc(size, 16);
  #elif defined(_MSC_VER) && (!defined(_WIN32_WCE))
    result = _aligned_malloc(size, 16);
  #else
    result = handmade_aligned_malloc(size);
  #endif

  if(!result && size)
    throw_std_bad_alloc();

  return result;
}

inline void aligned_free(void *ptr)
{
  #if !EIGEN_ALIGN
    std::free(ptr);
  #elif EIGEN_MALLOC_ALREADY_ALIGNED
    std::free(ptr);
  #elif EIGEN_HAS_POSIX_MEMALIGN
    std::free(ptr);
  #elif EIGEN_HAS_MM_MALLOC
    _mm_free(ptr);
  #elif defined(_MSC_VER) && (!defined(_WIN32_WCE))
    _aligned_free(ptr);
  #else
    handmade_aligned_free(ptr);
  #endif
}

inline void* aligned_realloc(void *ptr, size_t new_size, size_t old_size)
{
  EIGEN_UNUSED_VARIABLE(old_size);

  void *result;
#if !EIGEN_ALIGN
  result = std::realloc(ptr,new_size);
#elif EIGEN_MALLOC_ALREADY_ALIGNED
  result = std::realloc(ptr,new_size);
#elif EIGEN_HAS_POSIX_MEMALIGN
  result = generic_aligned_realloc(ptr,new_size,old_size);
#elif EIGEN_HAS_MM_MALLOC
  
  
  
  #if defined(_MSC_VER) && (!defined(_WIN32_WCE)) && defined(_mm_free)
    result = _aligned_realloc(ptr,new_size,16);
  #else
    result = generic_aligned_realloc(ptr,new_size,old_size);
  #endif
#elif defined(_MSC_VER) && (!defined(_WIN32_WCE))
  result = _aligned_realloc(ptr,new_size,16);
#else
  result = handmade_aligned_realloc(ptr,new_size,old_size);
#endif

  if (!result && new_size)
    throw_std_bad_alloc();

  return result;
}


template<bool Align> inline void* conditional_aligned_malloc(size_t size)
{
  return aligned_malloc(size);
}

template<> inline void* conditional_aligned_malloc<false>(size_t size)
{
  check_that_malloc_is_allowed();

  void *result = std::malloc(size);
  if(!result && size)
    throw_std_bad_alloc();
  return result;
}

template<bool Align> inline void conditional_aligned_free(void *ptr)
{
  aligned_free(ptr);
}

template<> inline void conditional_aligned_free<false>(void *ptr)
{
  std::free(ptr);
}

template<bool Align> inline void* conditional_aligned_realloc(void* ptr, size_t new_size, size_t old_size)
{
  return aligned_realloc(ptr, new_size, old_size);
}

template<> inline void* conditional_aligned_realloc<false>(void* ptr, size_t new_size, size_t)
{
  return std::realloc(ptr, new_size);
}


template<typename T> inline T* construct_elements_of_array(T *ptr, size_t size)
{
  for (size_t i=0; i < size; ++i) ::new (ptr + i) T;
  return ptr;
}

template<typename T> inline void destruct_elements_of_array(T *ptr, size_t size)
{
  
  if(ptr)
    while(size) ptr[--size].~T();
}


template<typename T>
EIGEN_ALWAYS_INLINE void check_size_for_overflow(size_t size)
{
  if(size > size_t(-1) / sizeof(T))
    throw_std_bad_alloc();
}

template<typename T> inline T* aligned_new(size_t size)
{
  check_size_for_overflow<T>(size);
  T *result = reinterpret_cast<T*>(aligned_malloc(sizeof(T)*size));
  return construct_elements_of_array(result, size);
}

template<typename T, bool Align> inline T* conditional_aligned_new(size_t size)
{
  check_size_for_overflow<T>(size);
  T *result = reinterpret_cast<T*>(conditional_aligned_malloc<Align>(sizeof(T)*size));
  return construct_elements_of_array(result, size);
}

template<typename T> inline void aligned_delete(T *ptr, size_t size)
{
  destruct_elements_of_array<T>(ptr, size);
  aligned_free(ptr);
}

template<typename T, bool Align> inline void conditional_aligned_delete(T *ptr, size_t size)
{
  destruct_elements_of_array<T>(ptr, size);
  conditional_aligned_free<Align>(ptr);
}

template<typename T, bool Align> inline T* conditional_aligned_realloc_new(T* pts, size_t new_size, size_t old_size)
{
  check_size_for_overflow<T>(new_size);
  check_size_for_overflow<T>(old_size);
  if(new_size < old_size)
    destruct_elements_of_array(pts+new_size, old_size-new_size);
  T *result = reinterpret_cast<T*>(conditional_aligned_realloc<Align>(reinterpret_cast<void*>(pts), sizeof(T)*new_size, sizeof(T)*old_size));
  if(new_size > old_size)
    construct_elements_of_array(result+old_size, new_size-old_size);
  return result;
}


template<typename T, bool Align> inline T* conditional_aligned_new_auto(size_t size)
{
  if(size==0)
    return 0; 
  check_size_for_overflow<T>(size);
  T *result = reinterpret_cast<T*>(conditional_aligned_malloc<Align>(sizeof(T)*size));
  if(NumTraits<T>::RequireInitialization)
    construct_elements_of_array(result, size);
  return result;
}

template<typename T, bool Align> inline T* conditional_aligned_realloc_new_auto(T* pts, size_t new_size, size_t old_size)
{
  check_size_for_overflow<T>(new_size);
  check_size_for_overflow<T>(old_size);
  if(NumTraits<T>::RequireInitialization && (new_size < old_size))
    destruct_elements_of_array(pts+new_size, old_size-new_size);
  T *result = reinterpret_cast<T*>(conditional_aligned_realloc<Align>(reinterpret_cast<void*>(pts), sizeof(T)*new_size, sizeof(T)*old_size));
  if(NumTraits<T>::RequireInitialization && (new_size > old_size))
    construct_elements_of_array(result+old_size, new_size-old_size);
  return result;
}

template<typename T, bool Align> inline void conditional_aligned_delete_auto(T *ptr, size_t size)
{
  if(NumTraits<T>::RequireInitialization)
    destruct_elements_of_array<T>(ptr, size);
  conditional_aligned_free<Align>(ptr);
}


template<typename Scalar, typename Index>
static inline Index first_aligned(const Scalar* array, Index size)
{
  static const Index PacketSize = packet_traits<Scalar>::size;
  static const Index PacketAlignedMask = PacketSize-1;

  if(PacketSize==1)
  {
    
    
    return 0;
  }
  else if(size_t(array) & (sizeof(Scalar)-1))
  {
    
    
    return size;
  }
  else
  {
    return std::min<Index>( (PacketSize - (Index((size_t(array)/sizeof(Scalar))) & PacketAlignedMask))
                           & PacketAlignedMask, size);
  }
}

 
template<typename Index> 
inline static Index first_multiple(Index size, Index base)
{
  return ((size+base-1)/base)*base;
}

template<typename T, bool UseMemcpy> struct smart_copy_helper;

template<typename T> void smart_copy(const T* start, const T* end, T* target)
{
  smart_copy_helper<T,!NumTraits<T>::RequireInitialization>::run(start, end, target);
}

template<typename T> struct smart_copy_helper<T,true> {
  static inline void run(const T* start, const T* end, T* target)
  { memcpy(target, start, std::ptrdiff_t(end)-std::ptrdiff_t(start)); }
};

template<typename T> struct smart_copy_helper<T,false> {
  static inline void run(const T* start, const T* end, T* target)
  { std::copy(start, end, target); }
};



#ifndef EIGEN_ALLOCA
  #if (defined __linux__)
    #define EIGEN_ALLOCA alloca
  #elif defined(_MSC_VER)
    #define EIGEN_ALLOCA _alloca
  #endif
#endif

template<typename T> class aligned_stack_memory_handler
{
  public:
    aligned_stack_memory_handler(T* ptr, size_t size, bool dealloc)
      : m_ptr(ptr), m_size(size), m_deallocate(dealloc)
    {
      if(NumTraits<T>::RequireInitialization && m_ptr)
        Eigen::internal::construct_elements_of_array(m_ptr, size);
    }
    ~aligned_stack_memory_handler()
    {
      if(NumTraits<T>::RequireInitialization && m_ptr)
        Eigen::internal::destruct_elements_of_array<T>(m_ptr, m_size);
      if(m_deallocate)
        Eigen::internal::aligned_free(m_ptr);
    }
  protected:
    T* m_ptr;
    size_t m_size;
    bool m_deallocate;
};

} 

#ifdef EIGEN_ALLOCA

  #if defined(__arm__) || defined(_WIN32)
    #define EIGEN_ALIGNED_ALLOCA(SIZE) reinterpret_cast<void*>((reinterpret_cast<size_t>(EIGEN_ALLOCA(SIZE+16)) & ~(size_t(15))) + 16)
  #else
    #define EIGEN_ALIGNED_ALLOCA EIGEN_ALLOCA
  #endif

  #define ei_declare_aligned_stack_constructed_variable(TYPE,NAME,SIZE,BUFFER) \
    Eigen::internal::check_size_for_overflow<TYPE>(SIZE); \
    TYPE* NAME = (BUFFER)!=0 ? (BUFFER) \
               : reinterpret_cast<TYPE*>( \
                      (sizeof(TYPE)*SIZE<=EIGEN_STACK_ALLOCATION_LIMIT) ? EIGEN_ALIGNED_ALLOCA(sizeof(TYPE)*SIZE) \
                    : Eigen::internal::aligned_malloc(sizeof(TYPE)*SIZE) );  \
    Eigen::internal::aligned_stack_memory_handler<TYPE> EIGEN_CAT(NAME,_stack_memory_destructor)((BUFFER)==0 ? NAME : 0,SIZE,sizeof(TYPE)*SIZE>EIGEN_STACK_ALLOCATION_LIMIT)

#else

  #define ei_declare_aligned_stack_constructed_variable(TYPE,NAME,SIZE,BUFFER) \
    Eigen::internal::check_size_for_overflow<TYPE>(SIZE); \
    TYPE* NAME = (BUFFER)!=0 ? BUFFER : reinterpret_cast<TYPE*>(Eigen::internal::aligned_malloc(sizeof(TYPE)*SIZE));    \
    Eigen::internal::aligned_stack_memory_handler<TYPE> EIGEN_CAT(NAME,_stack_memory_destructor)((BUFFER)==0 ? NAME : 0,SIZE,true)
    
#endif



#if EIGEN_ALIGN
  #ifdef EIGEN_EXCEPTIONS
    #define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_NOTHROW(NeedsToAlign) \
      void* operator new(size_t size, const std::nothrow_t&) throw() { \
        try { return Eigen::internal::conditional_aligned_malloc<NeedsToAlign>(size); } \
        catch (...) { return 0; } \
      }
  #else
    #define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_NOTHROW(NeedsToAlign) \
      void* operator new(size_t size, const std::nothrow_t&) throw() { \
        return Eigen::internal::conditional_aligned_malloc<NeedsToAlign>(size); \
      }
  #endif

  #define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(NeedsToAlign) \
      void *operator new(size_t size) { \
        return Eigen::internal::conditional_aligned_malloc<NeedsToAlign>(size); \
      } \
      void *operator new[](size_t size) { \
        return Eigen::internal::conditional_aligned_malloc<NeedsToAlign>(size); \
      } \
      void operator delete(void * ptr) throw() { Eigen::internal::conditional_aligned_free<NeedsToAlign>(ptr); } \
      void operator delete[](void * ptr) throw() { Eigen::internal::conditional_aligned_free<NeedsToAlign>(ptr); } \
       \
       \
       \
      static void *operator new(size_t size, void *ptr) { return ::operator new(size,ptr); } \
      static void *operator new[](size_t size, void* ptr) { return ::operator new[](size,ptr); } \
      void operator delete(void * memory, void *ptr) throw() { return ::operator delete(memory,ptr); } \
      void operator delete[](void * memory, void *ptr) throw() { return ::operator delete[](memory,ptr); } \
       \
      EIGEN_MAKE_ALIGNED_OPERATOR_NEW_NOTHROW(NeedsToAlign) \
      void operator delete(void *ptr, const std::nothrow_t&) throw() { \
        Eigen::internal::conditional_aligned_free<NeedsToAlign>(ptr); \
      } \
      typedef void eigen_aligned_operator_new_marker_type;
#else
  #define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(NeedsToAlign)
#endif

#define EIGEN_MAKE_ALIGNED_OPERATOR_NEW EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(true)
#define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(Scalar,Size) \
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(bool(((Size)!=Eigen::Dynamic) && ((sizeof(Scalar)*(Size))%16==0)))


template<class T>
class aligned_allocator
{
public:
    typedef size_t    size_type;
    typedef std::ptrdiff_t difference_type;
    typedef T*        pointer;
    typedef const T*  const_pointer;
    typedef T&        reference;
    typedef const T&  const_reference;
    typedef T         value_type;

    template<class U>
    struct rebind
    {
        typedef aligned_allocator<U> other;
    };

    pointer address( reference value ) const
    {
        return &value;
    }

    const_pointer address( const_reference value ) const
    {
        return &value;
    }

    aligned_allocator()
    {
    }

    aligned_allocator( const aligned_allocator& )
    {
    }

    template<class U>
    aligned_allocator( const aligned_allocator<U>& )
    {
    }

    ~aligned_allocator()
    {
    }

    size_type max_size() const
    {
        return (std::numeric_limits<size_type>::max)();
    }

    pointer allocate( size_type num, const void* hint = 0 )
    {
        EIGEN_UNUSED_VARIABLE(hint);
        internal::check_size_for_overflow<T>(num);
        return static_cast<pointer>( internal::aligned_malloc( num * sizeof(T) ) );
    }

    void construct( pointer p, const T& value )
    {
        ::new( p ) T( value );
    }

    void destroy( pointer p )
    {
        p->~T();
    }

    void deallocate( pointer p, size_type  )
    {
        internal::aligned_free( p );
    }

    bool operator!=(const aligned_allocator<T>& ) const
    { return false; }

    bool operator==(const aligned_allocator<T>& ) const
    { return true; }
};


#if !defined(EIGEN_NO_CPUID)
#  if defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )
#    if defined(__PIC__) && defined(__i386__)
       
#      define EIGEN_CPUID(abcd,func,id) \
         __asm__ __volatile__ ("xchgl %%ebx, %k1;cpuid; xchgl %%ebx,%k1": "=a" (abcd[0]), "=&r" (abcd[1]), "=c" (abcd[2]), "=d" (abcd[3]) : "a" (func), "c" (id));
#    elif defined(__PIC__) && defined(__x86_64__)
       
       
#      define EIGEN_CPUID(abcd,func,id) \
        __asm__ __volatile__ ("xchg{q}\t{%%}rbx, %q1; cpuid; xchg{q}\t{%%}rbx, %q1": "=a" (abcd[0]), "=&r" (abcd[1]), "=c" (abcd[2]), "=d" (abcd[3]) : "0" (func), "2" (id));
#    else
       
#      define EIGEN_CPUID(abcd,func,id) \
         __asm__ __volatile__ ("cpuid": "=a" (abcd[0]), "=b" (abcd[1]), "=c" (abcd[2]), "=d" (abcd[3]) : "0" (func), "2" (id) );
#    endif
#  elif defined(_MSC_VER)
#    if (_MSC_VER > 1500) && ( defined(_M_IX86) || defined(_M_X64) )
#      define EIGEN_CPUID(abcd,func,id) __cpuidex((int*)abcd,func,id)
#    endif
#  endif
#endif

namespace internal {

#ifdef EIGEN_CPUID

inline bool cpuid_is_vendor(int abcd[4], const int vendor[3])
{
  return abcd[1]==vendor[0] && abcd[3]==vendor[1] && abcd[2]==vendor[2];
}

inline void queryCacheSizes_intel_direct(int& l1, int& l2, int& l3)
{
  int abcd[4];
  l1 = l2 = l3 = 0;
  int cache_id = 0;
  int cache_type = 0;
  do {
    abcd[0] = abcd[1] = abcd[2] = abcd[3] = 0;
    EIGEN_CPUID(abcd,0x4,cache_id);
    cache_type  = (abcd[0] & 0x0F) >> 0;
    if(cache_type==1||cache_type==3) 
    {
      int cache_level = (abcd[0] & 0xE0) >> 5;  
      int ways        = (abcd[1] & 0xFFC00000) >> 22; 
      int partitions  = (abcd[1] & 0x003FF000) >> 12; 
      int line_size   = (abcd[1] & 0x00000FFF) >>  0; 
      int sets        = (abcd[2]);                    

      int cache_size = (ways+1) * (partitions+1) * (line_size+1) * (sets+1);

      switch(cache_level)
      {
        case 1: l1 = cache_size; break;
        case 2: l2 = cache_size; break;
        case 3: l3 = cache_size; break;
        default: break;
      }
    }
    cache_id++;
  } while(cache_type>0 && cache_id<16);
}

inline void queryCacheSizes_intel_codes(int& l1, int& l2, int& l3)
{
  int abcd[4];
  abcd[0] = abcd[1] = abcd[2] = abcd[3] = 0;
  l1 = l2 = l3 = 0;
  EIGEN_CPUID(abcd,0x00000002,0);
  unsigned char * bytes = reinterpret_cast<unsigned char *>(abcd)+2;
  bool check_for_p2_core2 = false;
  for(int i=0; i<14; ++i)
  {
    switch(bytes[i])
    {
      case 0x0A: l1 = 8; break;   
      case 0x0C: l1 = 16; break;  
      case 0x0E: l1 = 24; break;  
      case 0x10: l1 = 16; break;  
      case 0x15: l1 = 16; break;  
      case 0x2C: l1 = 32; break;  
      case 0x30: l1 = 32; break;  
      case 0x60: l1 = 16; break;  
      case 0x66: l1 = 8; break;   
      case 0x67: l1 = 16; break;  
      case 0x68: l1 = 32; break;  
      case 0x1A: l2 = 96; break;   
      case 0x22: l3 = 512; break;   
      case 0x23: l3 = 1024; break;   
      case 0x25: l3 = 2048; break;   
      case 0x29: l3 = 4096; break;   
      case 0x39: l2 = 128; break;   
      case 0x3A: l2 = 192; break;   
      case 0x3B: l2 = 128; break;   
      case 0x3C: l2 = 256; break;   
      case 0x3D: l2 = 384; break;   
      case 0x3E: l2 = 512; break;   
      case 0x40: l2 = 0; break;   
      case 0x41: l2 = 128; break;   
      case 0x42: l2 = 256; break;   
      case 0x43: l2 = 512; break;   
      case 0x44: l2 = 1024; break;   
      case 0x45: l2 = 2048; break;   
      case 0x46: l3 = 4096; break;   
      case 0x47: l3 = 8192; break;   
      case 0x48: l2 = 3072; break;   
      case 0x49: if(l2!=0) l3 = 4096; else {check_for_p2_core2=true; l3 = l2 = 4096;} break;
      case 0x4A: l3 = 6144; break;   
      case 0x4B: l3 = 8192; break;   
      case 0x4C: l3 = 12288; break;   
      case 0x4D: l3 = 16384; break;   
      case 0x4E: l2 = 6144; break;   
      case 0x78: l2 = 1024; break;   
      case 0x79: l2 = 128; break;   
      case 0x7A: l2 = 256; break;   
      case 0x7B: l2 = 512; break;   
      case 0x7C: l2 = 1024; break;   
      case 0x7D: l2 = 2048; break;   
      case 0x7E: l2 = 256; break;   
      case 0x7F: l2 = 512; break;   
      case 0x80: l2 = 512; break;   
      case 0x81: l2 = 128; break;   
      case 0x82: l2 = 256; break;   
      case 0x83: l2 = 512; break;   
      case 0x84: l2 = 1024; break;   
      case 0x85: l2 = 2048; break;   
      case 0x86: l2 = 512; break;   
      case 0x87: l2 = 1024; break;   
      case 0x88: l3 = 2048; break;   
      case 0x89: l3 = 4096; break;   
      case 0x8A: l3 = 8192; break;   
      case 0x8D: l3 = 3072; break;   

      default: break;
    }
  }
  if(check_for_p2_core2 && l2 == l3)
    l3 = 0;
  l1 *= 1024;
  l2 *= 1024;
  l3 *= 1024;
}

inline void queryCacheSizes_intel(int& l1, int& l2, int& l3, int max_std_funcs)
{
  if(max_std_funcs>=4)
    queryCacheSizes_intel_direct(l1,l2,l3);
  else
    queryCacheSizes_intel_codes(l1,l2,l3);
}

inline void queryCacheSizes_amd(int& l1, int& l2, int& l3)
{
  int abcd[4];
  abcd[0] = abcd[1] = abcd[2] = abcd[3] = 0;
  EIGEN_CPUID(abcd,0x80000005,0);
  l1 = (abcd[2] >> 24) * 1024; 
  abcd[0] = abcd[1] = abcd[2] = abcd[3] = 0;
  EIGEN_CPUID(abcd,0x80000006,0);
  l2 = (abcd[2] >> 16) * 1024; 
  l3 = ((abcd[3] & 0xFFFC000) >> 18) * 512 * 1024; 
}
#endif

inline void queryCacheSizes(int& l1, int& l2, int& l3)
{
  #ifdef EIGEN_CPUID
  int abcd[4];
  const int GenuineIntel[] = {0x756e6547, 0x49656e69, 0x6c65746e};
  const int AuthenticAMD[] = {0x68747541, 0x69746e65, 0x444d4163};
  const int AMDisbetter_[] = {0x69444d41, 0x74656273, 0x21726574}; 

  
  EIGEN_CPUID(abcd,0x0,0);
  int max_std_funcs = abcd[1];
  if(cpuid_is_vendor(abcd,GenuineIntel))
    queryCacheSizes_intel(l1,l2,l3,max_std_funcs);
  else if(cpuid_is_vendor(abcd,AuthenticAMD) || cpuid_is_vendor(abcd,AMDisbetter_))
    queryCacheSizes_amd(l1,l2,l3);
  else
    
    queryCacheSizes_intel(l1,l2,l3,max_std_funcs);

  
  #else
  l1 = l2 = l3 = -1;
  #endif
}

inline int queryL1CacheSize()
{
  int l1(-1), l2, l3;
  queryCacheSizes(l1,l2,l3);
  return l1;
}

inline int queryTopLevelCacheSize()
{
  int l1, l2(-1), l3(-1);
  queryCacheSizes(l1,l2,l3);
  return (std::max)(l2,l3);
}

} 

} 

#endif 
