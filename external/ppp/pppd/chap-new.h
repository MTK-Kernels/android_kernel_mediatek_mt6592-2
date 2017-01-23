/*
 * chap-new.c - New CHAP implementation.
 *
 * Copyright (c) 2003 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 3. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define CHAP_HDRLEN	4

#define CHAP_CHALLENGE	1
#define CHAP_RESPONSE	2
#define CHAP_SUCCESS	3
#define CHAP_FAILURE	4

#define CHAP_MD5		5
#define CHAP_MICROSOFT		0x80
#define CHAP_MICROSOFT_V2	0x81

#define MAX_CHALLENGE_LEN	64
#define MAX_RESPONSE_LEN	64

#define MDTYPE_MICROSOFT_V2	0x1
#define MDTYPE_MICROSOFT	0x2
#define MDTYPE_MD5		0x4
#define MDTYPE_NONE		0

extern int chap_mdtype_all;

#define CHAP_DIGEST(mdtype) \
    ((mdtype) & MDTYPE_MD5)? CHAP_MD5: \
    ((mdtype) & MDTYPE_MICROSOFT_V2)? CHAP_MICROSOFT_V2: \
    ((mdtype) & MDTYPE_MICROSOFT)? CHAP_MICROSOFT: \
    0

#define CHAP_MDTYPE(mdtype) ((mdtype) ^ ((mdtype) - 1)) & (mdtype)

#define CHAP_MDTYPE_D(digest) \
    ((digest) == CHAP_MICROSOFT_V2)? MDTYPE_MICROSOFT_V2: \
    ((digest) == CHAP_MICROSOFT)? MDTYPE_MICROSOFT: \
    ((digest) == CHAP_MD5)? MDTYPE_MD5: \
    0

#define CHAP_CANDIGEST(mdtype, digest) \
    ((digest) == CHAP_MICROSOFT_V2)? (mdtype) & MDTYPE_MICROSOFT_V2: \
    ((digest) == CHAP_MICROSOFT)? (mdtype) & MDTYPE_MICROSOFT: \
    ((digest) == CHAP_MD5)? (mdtype) & MDTYPE_MD5: \
    0

struct chap_digest_type {
	int code;

	void (*generate_challenge)(unsigned char *challenge);
	int (*verify_response)(int id, char *name,
		unsigned char *secret, int secret_len,
		unsigned char *challenge, unsigned char *response,
		char *message, int message_space);
	void (*make_response)(unsigned char *response, int id, char *our_name,
		unsigned char *challenge, char *secret, int secret_len,
		unsigned char *priv);
	int (*check_success)(int id, unsigned char *pkt, int len);
	void (*handle_failure)(unsigned char *pkt, int len);

	struct chap_digest_type *next;
};

extern int (*chap_verify_hook)(char *name, char *ourname, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *response,
			char *message, int message_space);

extern void chap_register_digest(struct chap_digest_type *);

extern void chap_auth_peer(int unit, char *our_name, int digest_code);

extern void chap_auth_with_peer(int unit, char *our_name, int digest_code);

extern struct protent chap_protent;
