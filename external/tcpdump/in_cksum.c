
/*
 * Copyright (c) 1988, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)in_cksum.c	8.1 (Berkeley) 6/10/93
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include "interface.h"


#define ADDCARRY(x)  {if ((x) > 65535) (x) -= 65535;}
#define REDUCE {l_util.l = sum; sum = l_util.s[0] + l_util.s[1]; ADDCARRY(sum);}

u_int16_t
in_cksum(const struct cksum_vec *vec, int veclen)
{
	register const u_int16_t *w;
	register int sum = 0;
	register int mlen = 0;
	int byte_swapped = 0;

	union {
		u_int8_t	c[2];
		u_int16_t	s;
	} s_util;
	union {
		u_int16_t	s[2];
		u_int32_t	l;
	} l_util;

	for (; veclen != 0; vec++, veclen--) {
		if (vec->len == 0)
			continue;
		w = (const u_int16_t *)(void *)vec->ptr;
		if (mlen == -1) {
			s_util.c[1] = *(const u_int8_t *)w;
			sum += s_util.s;
			w = (const u_int16_t *)(void *)((const u_int8_t *)w + 1);
			mlen = vec->len - 1;
		} else
			mlen = vec->len;
		if ((1 & (unsigned long) w) && (mlen > 0)) {
			REDUCE;
			sum <<= 8;
			s_util.c[0] = *(const u_int8_t *)w;
			w = (const u_int16_t *)(void *)((const u_int8_t *)w + 1);
			mlen--;
			byte_swapped = 1;
		}
		while ((mlen -= 32) >= 0) {
			sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3];
			sum += w[4]; sum += w[5]; sum += w[6]; sum += w[7];
			sum += w[8]; sum += w[9]; sum += w[10]; sum += w[11];
			sum += w[12]; sum += w[13]; sum += w[14]; sum += w[15];
			w += 16;
		}
		mlen += 32;
		while ((mlen -= 8) >= 0) {
			sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3];
			w += 4;
		}
		mlen += 8;
		if (mlen == 0 && byte_swapped == 0)
			continue;
		REDUCE;
		while ((mlen -= 2) >= 0) {
			sum += *w++;
		}
		if (byte_swapped) {
			REDUCE;
			sum <<= 8;
			byte_swapped = 0;
			if (mlen == -1) {
				s_util.c[1] = *(const u_int8_t *)w;
				sum += s_util.s;
				mlen = 0;
			} else
				mlen = -1;
		} else if (mlen == -1)
			s_util.c[0] = *(const u_int8_t *)w;
	}
	if (mlen == -1) {
		s_util.c[1] = 0;
		sum += s_util.s;
	}
	REDUCE;
	return (~sum & 0xffff);
}

u_int16_t
in_cksum_shouldbe(u_int16_t sum, u_int16_t computed_sum)
{
	u_int32_t shouldbe;

	shouldbe = sum;
	shouldbe += ntohs(computed_sum);
	shouldbe = (shouldbe & 0xFFFF) + (shouldbe >> 16);
	shouldbe = (shouldbe & 0xFFFF) + (shouldbe >> 16);
	return shouldbe;
}