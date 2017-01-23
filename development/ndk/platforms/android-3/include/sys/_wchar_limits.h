/*
 * Copyright (C) 2013 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _SYS__WCHAR_LIMITS_H
#define _SYS__WCHAR_LIMITS_H

#include <android/api-level.h>

 
#if !defined(WCHAR_MIN)

#  if defined(_WCHAR_IS_8BIT) && defined(__arm__) && __ANDROID_API__ < 9
#    if defined(__cplusplus) && !defined(__STDC_LIMIT_MACROS)
#      define WCHAR_MIN  0
#      define WCHAR_MAX  255
#    else
#      define WCHAR_MIN   (-2147483647 - 1)
#      define WCHAR_MAX   (2147483647)
#    endif
#  elif defined(_WCHAR_IS_ALWAYS_SIGNED)
#    define WCHAR_MIN   (-2147483647 - 1)
#    define WCHAR_MAX   (2147483647)
#  else
#    define WCHAR_MAX __WCHAR_MAX__

  
#    if defined(__WCHAR_UNSIGNED__)
#      define WCHAR_MIN L'\0'
#    else
#      define WCHAR_MIN (-(WCHAR_MAX) - 1)
#    endif
#  endif 

#endif 

#endif  
