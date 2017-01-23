
/*
   ----------------------------------------------------------------

   Notice that the following BSD-style license applies to this one
   file (memcheck.h) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.

   ----------------------------------------------------------------

   This file is part of MemCheck, a heavyweight Valgrind tool for
   detecting memory errors.

   Copyright (C) 2000-2013 Julian Seward.  All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. The origin of this software must not be misrepresented; you must 
      not claim that you wrote the original software.  If you use this 
      software in a product, an acknowledgment in the product 
      documentation would be appreciated but is not required.

   3. Altered source versions must be plainly marked as such, and must
      not be misrepresented as being the original software.

   4. The name of the author may not be used to endorse or promote 
      products derived from this software without specific prior written 
      permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   ----------------------------------------------------------------

   Notice that the above BSD-style license applies to this one file
   (memcheck.h) only.  The entire rest of Valgrind is licensed under
   the terms of the GNU General Public License, version 2.  See the
   COPYING file in the source distribution for details.

   ---------------------------------------------------------------- 
*/


#ifndef __MEMCHECK_H
#define __MEMCHECK_H



#include "valgrind.h"

typedef
   enum { 
      VG_USERREQ__MAKE_MEM_NOACCESS = VG_USERREQ_TOOL_BASE('M','C'),
      VG_USERREQ__MAKE_MEM_UNDEFINED,
      VG_USERREQ__MAKE_MEM_DEFINED,
      VG_USERREQ__DISCARD,
      VG_USERREQ__CHECK_MEM_IS_ADDRESSABLE,
      VG_USERREQ__CHECK_MEM_IS_DEFINED,
      VG_USERREQ__DO_LEAK_CHECK,
      VG_USERREQ__COUNT_LEAKS,

      VG_USERREQ__GET_VBITS,
      VG_USERREQ__SET_VBITS,

      VG_USERREQ__CREATE_BLOCK,

      VG_USERREQ__MAKE_MEM_DEFINED_IF_ADDRESSABLE,

      
      VG_USERREQ__COUNT_LEAK_BLOCKS,

      VG_USERREQ__ENABLE_ADDR_ERROR_REPORTING_IN_RANGE,
      VG_USERREQ__DISABLE_ADDR_ERROR_REPORTING_IN_RANGE,

      
      _VG_USERREQ__MEMCHECK_RECORD_OVERLAP_ERROR 
         = VG_USERREQ_TOOL_BASE('M','C') + 256
   } Vg_MemCheckClientRequest;




#define VALGRIND_MAKE_MEM_NOACCESS(_qzz_addr,_qzz_len)           \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 ,      \
                            VG_USERREQ__MAKE_MEM_NOACCESS,       \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)
      
#define VALGRIND_MAKE_MEM_UNDEFINED(_qzz_addr,_qzz_len)          \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 ,      \
                            VG_USERREQ__MAKE_MEM_UNDEFINED,      \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

#define VALGRIND_MAKE_MEM_DEFINED(_qzz_addr,_qzz_len)            \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 ,      \
                            VG_USERREQ__MAKE_MEM_DEFINED,        \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

#define VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(_qzz_addr,_qzz_len)     \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 ,              \
                            VG_USERREQ__MAKE_MEM_DEFINED_IF_ADDRESSABLE, \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

#define VALGRIND_CREATE_BLOCK(_qzz_addr,_qzz_len, _qzz_desc)	   \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 ,        \
                            VG_USERREQ__CREATE_BLOCK,              \
                            (_qzz_addr), (_qzz_len), (_qzz_desc),  \
                            0, 0)

#define VALGRIND_DISCARD(_qzz_blkindex)                          \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 ,      \
                            VG_USERREQ__DISCARD,                 \
                            0, (_qzz_blkindex), 0, 0, 0)



#define VALGRIND_CHECK_MEM_IS_ADDRESSABLE(_qzz_addr,_qzz_len)      \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                             \
                            VG_USERREQ__CHECK_MEM_IS_ADDRESSABLE,  \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

#define VALGRIND_CHECK_MEM_IS_DEFINED(_qzz_addr,_qzz_len)        \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                           \
                            VG_USERREQ__CHECK_MEM_IS_DEFINED,    \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

#define VALGRIND_CHECK_VALUE_IS_DEFINED(__lvalue)                \
   VALGRIND_CHECK_MEM_IS_DEFINED(                                \
      (volatile unsigned char *)&(__lvalue),                     \
                      (unsigned long)(sizeof (__lvalue)))


#define VALGRIND_DO_LEAK_CHECK                                   \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DO_LEAK_CHECK,   \
                                    0, 0, 0, 0, 0)

#define VALGRIND_DO_ADDED_LEAK_CHECK                            \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DO_LEAK_CHECK,  \
                                    0, 1, 0, 0, 0)

#define VALGRIND_DO_CHANGED_LEAK_CHECK                          \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DO_LEAK_CHECK,  \
                                    0, 2, 0, 0, 0)

#define VALGRIND_DO_QUICK_LEAK_CHECK                             \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DO_LEAK_CHECK,   \
                                    1, 0, 0, 0, 0)

#define VALGRIND_COUNT_LEAKS(leaked, dubious, reachable, suppressed)     \
                                                        \
   {                                                                     \
    unsigned long _qzz_leaked    = 0, _qzz_dubious    = 0;               \
    unsigned long _qzz_reachable = 0, _qzz_suppressed = 0;               \
    VALGRIND_DO_CLIENT_REQUEST_STMT(                                     \
                               VG_USERREQ__COUNT_LEAKS,                  \
                               &_qzz_leaked, &_qzz_dubious,              \
                               &_qzz_reachable, &_qzz_suppressed, 0);    \
    leaked     = _qzz_leaked;                                            \
    dubious    = _qzz_dubious;                                           \
    reachable  = _qzz_reachable;                                         \
    suppressed = _qzz_suppressed;                                        \
   }

#define VALGRIND_COUNT_LEAK_BLOCKS(leaked, dubious, reachable, suppressed) \
                                                        \
   {                                                                     \
    unsigned long _qzz_leaked    = 0, _qzz_dubious    = 0;               \
    unsigned long _qzz_reachable = 0, _qzz_suppressed = 0;               \
    VALGRIND_DO_CLIENT_REQUEST_STMT(                                     \
                               VG_USERREQ__COUNT_LEAK_BLOCKS,            \
                               &_qzz_leaked, &_qzz_dubious,              \
                               &_qzz_reachable, &_qzz_suppressed, 0);    \
    leaked     = _qzz_leaked;                                            \
    dubious    = _qzz_dubious;                                           \
    reachable  = _qzz_reachable;                                         \
    suppressed = _qzz_suppressed;                                        \
   }


#define VALGRIND_GET_VBITS(zza,zzvbits,zznbytes)                \
    (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                \
                                    VG_USERREQ__GET_VBITS,      \
                                    (const char*)(zza),         \
                                    (char*)(zzvbits),           \
                                    (zznbytes), 0, 0)

#define VALGRIND_SET_VBITS(zza,zzvbits,zznbytes)                \
    (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                \
                                    VG_USERREQ__SET_VBITS,      \
                                    (const char*)(zza),         \
                                    (const char*)(zzvbits),     \
                                    (zznbytes), 0, 0 )

#define VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE(_qzz_addr,_qzz_len) \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 ,    \
       VG_USERREQ__DISABLE_ADDR_ERROR_REPORTING_IN_RANGE,      \
       (_qzz_addr), (_qzz_len), 0, 0, 0)

#define VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(_qzz_addr,_qzz_len) \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 ,    \
       VG_USERREQ__ENABLE_ADDR_ERROR_REPORTING_IN_RANGE,       \
       (_qzz_addr), (_qzz_len), 0, 0, 0)

#endif

