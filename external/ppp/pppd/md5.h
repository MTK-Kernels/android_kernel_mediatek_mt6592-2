
/*
 ***********************************************************************
 ** Copyright (C) 1990, RSA Data Security, Inc. All rights reserved.  **
 **                                                                   **
 ** License to copy and use this software is granted provided that    **
 ** it is identified as the "RSA Data Security, Inc. MD5 Message-     **
 ** Digest Algorithm" in all material mentioning or referencing this  **
 ** software or this function.                                        **
 **                                                                   **
 ** License is also granted to make and use derivative works          **
 ** provided that such works are identified as "derived from the RSA  **
 ** Data Security, Inc. MD5 Message-Digest Algorithm" in all          **
 ** material mentioning or referencing the derived work.              **
 **                                                                   **
 ** RSA Data Security, Inc. makes no representations concerning       **
 ** either the merchantability of this software or the suitability    **
 ** of this software for any particular purpose.  It is provided "as  **
 ** is" without express or implied warranty of any kind.              **
 **                                                                   **
 ** These notices must be retained in any copies of any part of this  **
 ** documentation and/or software.                                    **
 ***********************************************************************
 */

#ifndef __MD5_INCLUDE__

#ifdef _LP64
typedef unsigned int UINT4;
typedef int          INT4;
#else
typedef unsigned long UINT4;
typedef long          INT4;
#endif
#define _UINT4_T

typedef struct {
  UINT4 i[2];                   
  UINT4 buf[4];                                    
  unsigned char in[64];                              
  unsigned char digest[16];     
} MD5_CTX;

void MD5_Init (MD5_CTX *mdContext);
void MD5_Update (MD5_CTX *mdContext, unsigned char *inBuf, unsigned int inLen);
void MD5_Final (unsigned char hash[], MD5_CTX *mdContext);

#define __MD5_INCLUDE__
#endif 
