
#ifndef __BYTEORDER_H
#define __BYTEORDER_H

#include <arpa/inet.h>

#include <machine/byte_order.h>
#define swap16(x) NXSwapShort(x)
#define swap32(x) NXSwapLong(x)
#define swap64(x) NXSwapLongLong(x)



#ifndef htobe16
# define htobe16(x) htons(x)
#endif
#ifndef htobe32
# define htobe32(x) htonl(x)
#endif
#ifndef be16toh
# define be16toh(x) ntohs(x)
#endif
#ifndef be32toh
# define be32toh(x) ntohl(x)
#endif

#define HTOBE16(x) (x) = htobe16(x)
#define HTOBE32(x) (x) = htobe32(x)
#define BE32TOH(x) (x) = be32toh(x)
#define BE16TOH(x) (x) = be16toh(x)

#ifndef htole16
# define htole16(x)      (x)
#endif
#ifndef htole32
# define htole32(x)      (x)
#endif
#ifndef htole64
# define htole64(x)      (x)
#endif
#ifndef le16toh
# define le16toh(x)      (x)
#endif
#ifndef le32toh
# define le32toh(x)      (x)
#endif
#ifndef le64toh
# define le64toh(x)      (x)
#endif

#define HTOLE16(x)      (void) (x)
#define HTOLE32(x)      (void) (x)
#define HTOLE64(x)      (void) (x)
#define LE16TOH(x)      (void) (x)
#define LE32TOH(x)      (void) (x)
#define LE64TOH(x)      (void) (x)

#ifndef htobe64
# define htobe64(x)      swap64(x)
#endif
#ifndef be64toh
# define be64toh(x)      swap64(x)
#endif

#define HTOBE64(x)      (x) = htobe64(x)
#define BE64TOH(x)      (x) = be64toh(x)

#include <stdint.h>


#ifndef be16atoh
# define be16atoh(x)     be16toh(*(uint16_t*)(x))
#endif
#ifndef be32atoh
# define be32atoh(x)     be32toh(*(uint32_t*)(x))
#endif
#ifndef be64atoh
# define be64atoh(x)     be64toh(*(uint64_t*)(x))
#endif
#ifndef le16atoh
# define le16atoh(x)     le16toh(*(uint16_t*)(x))
#endif
#ifndef le32atoh
# define le32atoh(x)     le32toh(*(uint32_t*)(x))
#endif
#ifndef le64atoh
# define le64atoh(x)     le64toh(*(uint64_t*)(x))
#endif

#ifndef htob16a
# define htobe16a(a,x)   *(uint16_t*)(a) = htobe16(x)
#endif
#ifndef htobe32a
# define htobe32a(a,x)   *(uint32_t*)(a) = htobe32(x)
#endif
#ifndef htobe64a
# define htobe64a(a,x)   *(uint64_t*)(a) = htobe64(x)
#endif
#ifndef htole16a
# define htole16a(a,x)   *(uint16_t*)(a) = htole16(x)
#endif
#ifndef htole32a
# define htole32a(a,x)   *(uint32_t*)(a) = htole32(x)
#endif
#ifndef htole64a
# define htole64a(a,x)   *(uint64_t*)(a) = htole64(x)
#endif

#endif 
