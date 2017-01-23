#define CRCPOLY_LE 0xedb88320
#define CRCPOLY_BE 0x04c11db7

#ifndef CRC_LE_BITS
# define CRC_LE_BITS 8
#endif
#ifndef CRC_BE_BITS
# define CRC_BE_BITS 8
#endif

#if CRC_LE_BITS > 8 || CRC_LE_BITS < 1 || CRC_LE_BITS & CRC_LE_BITS-1
# error CRC_LE_BITS must be a power of 2 between 1 and 8
#endif

#if CRC_BE_BITS > 8 || CRC_BE_BITS < 1 || CRC_BE_BITS & CRC_BE_BITS-1
# error CRC_BE_BITS must be a power of 2 between 1 and 8
#endif

#define ___constant_swab32(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
		(((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
		(((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
		(((__u32)(x) & (__u32)0xff000000UL) >> 24) ))


#ifdef WORDS_BIGENDIAN
#define __constant_cpu_to_le32(x) ___constant_swab32((x))
#define __constant_cpu_to_be32(x) (x)
#define __be32_to_cpu(x) (x)
#define __cpu_to_be32(x) (x)
#define __cpu_to_le32(x) (ext2fs_swab32((x)))
#define __le32_to_cpu(x) (ext2fs_swab32((x)))
#else
#define __constant_cpu_to_le32(x) (x)
#define __constant_cpu_to_be32(x) ___constant_swab32((x))
#define __be32_to_cpu(x) (ext2fs_swab32((x)))
#define __cpu_to_be32(x) (ext2fs_swab32((x)))
#define __cpu_to_le32(x) (x)
#define __le32_to_cpu(x) (x)
#endif

#if (__GNUC__ >= 3)
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#else
#define likely(x)	(x)
#define unlikely(x)	(x)
#endif
