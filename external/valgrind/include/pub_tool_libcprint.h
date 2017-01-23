

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Julian Seward
      jseward@acm.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef __PUB_TOOL_LIBCPRINT_H
#define __PUB_TOOL_LIBCPRINT_H

#include "pub_tool_basics.h"      



extern UInt VG_(sprintf)  ( HChar* buf, const HChar* format, ... )
                          PRINTF_CHECK(2, 3);

extern UInt VG_(vsprintf) ( HChar* buf, const HChar* format, va_list vargs )
                          PRINTF_CHECK(2, 0);

extern UInt VG_(snprintf) ( HChar* buf, Int size, 
                                       const HChar *format, ... )
                          PRINTF_CHECK(3, 4);

extern UInt VG_(vsnprintf)( HChar* buf, Int size, 
                                       const HChar *format, va_list vargs )
                          PRINTF_CHECK(3, 0);



#define OINK(nnn) VG_(message)(Vg_DebugMsg, "OINK %d\n",nnn)


typedef
   enum {                 
      Vg_FailMsg,         
      Vg_UserMsg,         
      Vg_DebugMsg,        
      Vg_ClientMsg        
   }
   VgMsgKind;

extern UInt VG_(printf)   ( const HChar *format, ... )
                          PRINTF_CHECK(1, 2);
extern UInt VG_(vprintf)  ( const HChar *format, va_list vargs )
                          PRINTF_CHECK(1, 0);

extern UInt VG_(printf_xml)  ( const HChar *format, ... )
                             PRINTF_CHECK(1, 2);

extern UInt VG_(vprintf_xml) ( const HChar *format, va_list vargs )
                             PRINTF_CHECK(1, 0);

typedef struct _VgFile VgFile;

extern VgFile *VG_(fopen)    ( const HChar *name, Int flags, Int mode );
extern void    VG_(fclose)   ( VgFile *fp );
extern UInt    VG_(fprintf)  ( VgFile *fp, const HChar *format, ... )
                               PRINTF_CHECK(2, 3);
extern UInt    VG_(vfprintf) ( VgFile *fp, const HChar *format, va_list vargs )
                               PRINTF_CHECK(2, 0);

extern UInt VG_(emit) ( const HChar* format, ... ) PRINTF_CHECK(1, 2);

extern void VG_(vcbprintf)( void(*char_sink)(HChar, void* opaque),
                            void* opaque,
                            const HChar* format, va_list vargs );

extern UInt VG_(message)( VgMsgKind kind, const HChar* format, ... )
   PRINTF_CHECK(2, 3);

extern UInt VG_(vmessage)( VgMsgKind kind, const HChar* format, va_list vargs )
   PRINTF_CHECK(2, 0);


extern UInt VG_(fmsg)( const HChar* format, ... ) PRINTF_CHECK(1, 2);

__attribute__((noreturn))
extern void VG_(fmsg_bad_option) ( const HChar* opt, const HChar* format, ... )
   PRINTF_CHECK(2, 3);

extern UInt VG_(umsg)( const HChar* format, ... ) PRINTF_CHECK(1, 2);

extern UInt VG_(dmsg)( const HChar* format, ... ) PRINTF_CHECK(1, 2);

extern void VG_(message_flush) ( void );

/* Return a textual representation of a SysRes value in a statically
   allocated buffer. The buffer will be overwritten with the next 
   invocation. */
extern const HChar *VG_(sr_as_string) ( SysRes sr );

#endif   

