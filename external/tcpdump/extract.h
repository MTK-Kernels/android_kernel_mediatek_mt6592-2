/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) $Header: /tcpdump/master/tcpdump/extract.h,v 1.25 2006-01-30 16:20:07 hannes Exp $ (LBL)
 */

#ifdef LBL_ALIGN
#if defined(__GNUC__) && defined(HAVE___ATTRIBUTE__) && \
    (defined(__alpha) || defined(__alpha__) || \
     defined(__mips) || defined(__mips__))

typedef struct {
	u_int16_t	val;
} __attribute__((packed)) unaligned_u_int16_t;

typedef struct {
	u_int32_t	val;
} __attribute__((packed)) unaligned_u_int32_t;

static inline u_int16_t
EXTRACT_16BITS(const void *p)
{
	return ((u_int16_t)ntohs(((const unaligned_u_int16_t *)(p))->val));
}

static inline u_int32_t
EXTRACT_32BITS(const void *p)
{
	return ((u_int32_t)ntohl(((const unaligned_u_int32_t *)(p))->val));
}

static inline u_int64_t
EXTRACT_64BITS(const void *p)
{
	return ((u_int64_t)(((u_int64_t)ntohl(((const unaligned_u_int32_t *)(p) + 0)->val)) << 32 | \
		((u_int64_t)ntohl(((const unaligned_u_int32_t *)(p) + 1)->val)) << 0));
}

#else 
#define EXTRACT_16BITS(p) \
	((u_int16_t)((u_int16_t)*((const u_int8_t *)(p) + 0) << 8 | \
		     (u_int16_t)*((const u_int8_t *)(p) + 1)))
#define EXTRACT_32BITS(p) \
	((u_int32_t)((u_int32_t)*((const u_int8_t *)(p) + 0) << 24 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 1) << 16 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 2) << 8 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 3)))
#define EXTRACT_64BITS(p) \
	((u_int64_t)((u_int64_t)*((const u_int8_t *)(p) + 0) << 56 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 1) << 48 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 2) << 40 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 3) << 32 | \
	             (u_int64_t)*((const u_int8_t *)(p) + 4) << 24 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 5) << 16 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 6) << 8 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 7)))
#endif 
#else 
static inline u_int16_t
EXTRACT_16BITS(const void *p)
{
	return ((u_int16_t)ntohs(*(const u_int16_t *)(p)));
}

static inline u_int32_t
EXTRACT_32BITS(const void *p)
{
	return ((u_int32_t)ntohl(*(const u_int32_t *)(p)));
}

static inline u_int64_t
EXTRACT_64BITS(const void *p)
{
	return ((u_int64_t)(((u_int64_t)ntohl(*((const u_int32_t *)(p) + 0))) << 32 | \
		((u_int64_t)ntohl(*((const u_int32_t *)(p) + 1))) << 0));

}

#endif 

#define EXTRACT_24BITS(p) \
	((u_int32_t)((u_int32_t)*((const u_int8_t *)(p) + 0) << 16 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 1) << 8 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 2)))

#define EXTRACT_LE_8BITS(p) (*(p))
#define EXTRACT_LE_16BITS(p) \
	((u_int16_t)((u_int16_t)*((const u_int8_t *)(p) + 1) << 8 | \
		     (u_int16_t)*((const u_int8_t *)(p) + 0)))
#define EXTRACT_LE_32BITS(p) \
	((u_int32_t)((u_int32_t)*((const u_int8_t *)(p) + 3) << 24 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 2) << 16 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 1) << 8 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 0)))
#define EXTRACT_LE_24BITS(p) \
	((u_int32_t)((u_int32_t)*((const u_int8_t *)(p) + 2) << 16 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 1) << 8 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 0)))
#define EXTRACT_LE_64BITS(p) \
	((u_int64_t)((u_int64_t)*((const u_int8_t *)(p) + 7) << 56 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 6) << 48 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 5) << 40 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 4) << 32 | \
	             (u_int64_t)*((const u_int8_t *)(p) + 3) << 24 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 2) << 16 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 1) << 8 | \
		     (u_int64_t)*((const u_int8_t *)(p) + 0)))
