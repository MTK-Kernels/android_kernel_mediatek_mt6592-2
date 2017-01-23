#ifndef _UAPI_LIBC_COMPAT_H
#define _UAPI_LIBC_COMPAT_H

#if defined(__GLIBC__)

#if defined(_NETINET_IN_H)

#define __UAPI_DEF_IN6_ADDR		0
#if defined(__USE_MISC) || defined (__USE_GNU)
#define __UAPI_DEF_IN6_ADDR_ALT		0
#else
#define __UAPI_DEF_IN6_ADDR_ALT		1
#endif
#define __UAPI_DEF_SOCKADDR_IN6		0
#define __UAPI_DEF_IPV6_MREQ		0
#define __UAPI_DEF_IPPROTO_V6		0
#define __UAPI_DEF_IPV6_OPTIONS		0

#else

#define __UAPI_DEF_IN6_ADDR		1
#define __UAPI_DEF_IN6_ADDR_ALT		1
#define __UAPI_DEF_SOCKADDR_IN6		1
#define __UAPI_DEF_IPV6_MREQ		1
#define __UAPI_DEF_IPPROTO_V6		1
#define __UAPI_DEF_IPV6_OPTIONS		1

#endif 

#if defined(_SYS_XATTR_H)
#define __UAPI_DEF_XATTR		0
#else
#define __UAPI_DEF_XATTR		1
#endif

#else 

#define __UAPI_DEF_IN6_ADDR		1
#define __UAPI_DEF_IN6_ADDR_ALT		1
#define __UAPI_DEF_SOCKADDR_IN6		1
#define __UAPI_DEF_IPV6_MREQ		1
#define __UAPI_DEF_IPPROTO_V6		1
#define __UAPI_DEF_IPV6_OPTIONS		1

#define __UAPI_DEF_XATTR		1

#endif 

#endif 
