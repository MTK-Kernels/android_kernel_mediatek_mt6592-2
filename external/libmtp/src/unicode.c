/**
 * \file unicode.c
 *
 * This file contains general Unicode string manipulation functions.
 * It mainly consist of functions for converting between UCS-2 (used on
 * the devices) and UTF-8 (used by several applications).
 *
 * For a deeper understanding of Unicode encoding formats see the
 * Wikipedia entries for
 * <a href="http://en.wikipedia.org/wiki/UTF-16/UCS-2">UTF-16/UCS-2</a>
 * and <a href="http://en.wikipedia.org/wiki/UTF-8">UTF-8</a>.
 *
 * Copyright (C) 2005-2009 Linus Walleij <triad@df.lth.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_ICONV
#include "iconv.h"
#else
#error "libmtp unicode.c needs fixing to work without iconv()!"
#endif
#include "libmtp.h"
#include "unicode.h"
#include "util.h"
#include "ptp.h"

#define STRING_BUFFER_LENGTH 1024

int ucs2_strlen(uint16_t const * const unicstr)
{
  int length;

  
  for(length = 0; unicstr[length] != 0x0000U; length ++);
  return length;
}

char *utf16_to_utf8(LIBMTP_mtpdevice_t *device, const uint16_t *unicstr)
{
  PTPParams *params = (PTPParams *) device->params;
  char *stringp = (char *) unicstr;
  char loclstr[STRING_BUFFER_LENGTH*3+1]; 
  char *locp = loclstr;
  size_t nconv;
  size_t convlen = (ucs2_strlen(unicstr)+1) * sizeof(uint16_t); 
  size_t convmax = STRING_BUFFER_LENGTH*3;

  loclstr[0]='\0';
  
  nconv = iconv(params->cd_ucs2_to_locale, &stringp, &convlen, &locp, &convmax);
  if (nconv == (size_t) -1) {
    
    *locp = '\0';
  }
  loclstr[STRING_BUFFER_LENGTH*3] = '\0';
  
  if ((uint8_t) loclstr[0] == 0xEFU && (uint8_t) loclstr[1] == 0xBBU && (uint8_t) loclstr[2] == 0xBFU) {
    return strdup(loclstr+3);
  }
  return strdup(loclstr);
}

uint16_t *utf8_to_utf16(LIBMTP_mtpdevice_t *device, const char *localstr)
{
  PTPParams *params = (PTPParams *) device->params;
  char *stringp = (char *) localstr; 
  char unicstr[(STRING_BUFFER_LENGTH+1)*2]; 
  char *unip = unicstr;
  size_t nconv = 0;
  size_t convlen = strlen(localstr)+1; 
  size_t convmax = STRING_BUFFER_LENGTH*2;

  unicstr[0]='\0';
  unicstr[1]='\0';

  
  nconv = iconv(params->cd_locale_to_ucs2, &stringp, &convlen, &unip, &convmax);

  if (nconv == (size_t) -1) {
    
    unip[0] = '\0';
    unip[1] = '\0';
  }
  
  unicstr[STRING_BUFFER_LENGTH*2] = '\0';
  unicstr[STRING_BUFFER_LENGTH*2+1] = '\0';

  
  
  int ret_len = ucs2_strlen((uint16_t*)unicstr)*sizeof(uint16_t)+2;
  uint16_t* ret = malloc(ret_len);
  memcpy(ret,unicstr,(size_t)ret_len);
  return ret;
}

void strip_7bit_from_utf8(char *str)
{
  int i,j,k;
  i = 0;
  j = 0;
  k = strlen(str);
  while (i < k) {
    if ((uint8_t) str[i] > 0x7FU) {
      str[j] = '_';
      i++;
      
      while((uint8_t) str[i] > 0x7FU) {
	i++;
      }
    } else {
      str[j] = str[i];
      i++;
    }
    j++;
  }
  
  str[j] = '\0';
}
