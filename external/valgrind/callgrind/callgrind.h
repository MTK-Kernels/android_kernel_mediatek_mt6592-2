
/*
   ----------------------------------------------------------------

   Notice that the following BSD-style license applies to this one
   file (callgrind.h) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.

   ----------------------------------------------------------------

   This file is part of callgrind, a valgrind tool for cache simulation
   and call tree tracing.

   Copyright (C) 2003-2013 Josef Weidendorfer.  All rights reserved.

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
   (callgrind.h) only.  The entire rest of Valgrind is licensed under
   the terms of the GNU General Public License, version 2.  See the
   COPYING file in the source distribution for details.

   ----------------------------------------------------------------
*/

#ifndef __CALLGRIND_H
#define __CALLGRIND_H

#include "valgrind.h"


typedef
   enum {
      VG_USERREQ__DUMP_STATS = VG_USERREQ_TOOL_BASE('C','T'),
      VG_USERREQ__ZERO_STATS,
      VG_USERREQ__TOGGLE_COLLECT,
      VG_USERREQ__DUMP_STATS_AT,
      VG_USERREQ__START_INSTRUMENTATION,
      VG_USERREQ__STOP_INSTRUMENTATION
   } Vg_CallgrindClientRequest;

#define CALLGRIND_DUMP_STATS                                    \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DUMP_STATS,       \
                                  0, 0, 0, 0, 0)

/* Dump current state of cost centers, and zero them afterwards.
   The argument is appended to a string stating the reason which triggered
   the dump. This string is written as a description field into the
   profile data dump. */
#define CALLGRIND_DUMP_STATS_AT(pos_str)                        \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DUMP_STATS_AT,    \
                                  pos_str, 0, 0, 0, 0)

#define CALLGRIND_ZERO_STATS                                    \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__ZERO_STATS,       \
                                  0, 0, 0, 0, 0)

#define CALLGRIND_TOGGLE_COLLECT                                \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TOGGLE_COLLECT,   \
                                  0, 0, 0, 0, 0)

#define CALLGRIND_START_INSTRUMENTATION                              \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__START_INSTRUMENTATION, \
                                  0, 0, 0, 0, 0)

#define CALLGRIND_STOP_INSTRUMENTATION                               \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__STOP_INSTRUMENTATION,  \
                                  0, 0, 0, 0, 0)

#endif 
