/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#if !defined(_ISAKMP_OAKLEY_H_)
#define _ISAKMP_OAKLEY_H_

#define OAKLEY_ATTR_ENC_ALG                   1 
#define   OAKLEY_ATTR_ENC_ALG_DES               1
#define   OAKLEY_ATTR_ENC_ALG_IDEA              2
#define   OAKLEY_ATTR_ENC_ALG_BL                3
#define   OAKLEY_ATTR_ENC_ALG_RC5               4
#define   OAKLEY_ATTR_ENC_ALG_3DES              5
#define   OAKLEY_ATTR_ENC_ALG_CAST              6
#define OAKLEY_ATTR_HASH_ALG                  2 
#define   OAKLEY_ATTR_HASH_ALG_MD5              1
#define   OAKLEY_ATTR_HASH_ALG_SHA              2
#define   OAKLEY_ATTR_HASH_ALG_TIGER            3
#define OAKLEY_ATTR_AUTH_METHOD               3 
#define   OAKLEY_ATTR_AUTH_METHOD_PSKEY         1
#define   OAKLEY_ATTR_AUTH_METHOD_DSS           2
#define   OAKLEY_ATTR_AUTH_METHOD_RSA           3
#define   OAKLEY_ATTR_AUTH_METHOD_RSAENC        4
#define   OAKLEY_ATTR_AUTH_METHOD_RSAREV        5
#define OAKLEY_ATTR_GRP_DESC                  4 
#define   OAKLEY_ATTR_GRP_DESC_MODP768          1
#define   OAKLEY_ATTR_GRP_DESC_MODP1024         2
#define   OAKLEY_ATTR_GRP_DESC_EC2N155          3
#define   OAKLEY_ATTR_GRP_DESC_EC2N185          4
#define OAKLEY_ATTR_GRP_TYPE                  5 
#define   OAKLEY_ATTR_GRP_TYPE_MODP             1
#define   OAKLEY_ATTR_GRP_TYPE_ECP              2
#define   OAKLEY_ATTR_GRP_TYPE_EC2N             3
#define OAKLEY_ATTR_GRP_PI                    6 
#define OAKLEY_ATTR_GRP_GEN_ONE               7 
#define OAKLEY_ATTR_GRP_GEN_TWO               8 
#define OAKLEY_ATTR_GRP_CURVE_A               9 
#define OAKLEY_ATTR_GRP_CURVE_B              10 
#define OAKLEY_ATTR_SA_LTYPE                 11 
#define   OAKLEY_ATTR_SA_LTYPE_DEFAULT          1
#define   OAKLEY_ATTR_SA_LTYPE_SEC              1
#define   OAKLEY_ATTR_SA_LTYPE_KB               2
#define OAKLEY_ATTR_SA_LDUR                  12 
#define   OAKLEY_ATTR_SA_LDUR_DEFAULT           28800 
#define OAKLEY_ATTR_PRF                      13 
#define OAKLEY_ATTR_KEY_LEN                  14 
#define OAKLEY_ATTR_FIELD_SIZE               15 
#define OAKLEY_ATTR_GRP_ORDER                16 

#define OAKLEY_ID_IPV4_ADDR          0
#define OAKLEY_ID_IPV4_ADDR_SUBNET   1
#define OAKLEY_ID_IPV6_ADDR          2
#define OAKLEY_ID_IPV6_ADDR_SUBNET   3

#define ISAKMP_ETYPE_QUICK    32
#define ISAKMP_ETYPE_NEWGRP   33

#define OAKLEY_MAIN_MODE    0
#define OAKLEY_QUICK_MODE   1

#define OAKLEY_PRIME_MODP768 "\
	FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1 \
	29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD \
	EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245 \
	E485B576 625E7EC6 F44C42E9 A63A3620 FFFFFFFF FFFFFFFF"

#define OAKLEY_PRIME_MODP1024 "\
	FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1 \
	29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD \
	EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245 \
	E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED \
	EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE65381 \
	FFFFFFFF FFFFFFFF"

#define DEFAULTSECRETSIZE ( 128 / 8 ) 
#define DEFAULTNONCESIZE  ( 128 / 8 ) 

#define MAXPADLWORD 20

#if 0
struct oakley_sa {
	u_int8_t  proto_id;            
	vchar_t   *spi;                
	u_int8_t  dhgrp;               
	u_int8_t  auth_t;              
	u_int8_t  prf_t;               
	u_int8_t  hash_t;              
	u_int8_t  enc_t;               
	u_int8_t  life_t;              
	u_int32_t ldur;                
};
#endif

#endif 
