/* Tester for string functions.
   Copyright (C) 1995-2000, 2001, 2003 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#if !defined DO_STRING_INLINES
#undef __USE_STRING_INLINES
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>

#ifndef HAVE_GNU_LD
#define _sys_nerr	sys_nerr
#define _sys_errlist	sys_errlist
#endif

#define	STREQ(a, b)	(strcmp((a), (b)) == 0)

const char *it = "<UNSET>";	
size_t errors = 0;

static void
check (int thing, int number)
{
  if (!thing)
    {
      printf("%s flunked test %d\n", it, number);
      ++errors;
    }
}

static void
equal (const char *a, const char *b, int number)
{
  check(a != NULL && b != NULL && STREQ (a, b), number);
}

char one[50];
char two[50];
char *cp;

static void
test_strcmp (void)
{
  it = "strcmp";
  check (strcmp ("", "") == 0, 1);		
  check (strcmp ("a", "a") == 0, 2);		
  check (strcmp ("abc", "abc") == 0, 3);	
  check (strcmp ("abc", "abcd") < 0, 4);	
  check (strcmp ("abcd", "abc") > 0, 5);
  check (strcmp ("abcd", "abce") < 0, 6);	
  check (strcmp ("abce", "abcd") > 0, 7);
  check (strcmp ("a\203", "a") > 0, 8);		
  check (strcmp ("a\203", "a\003") > 0, 9);

  {
    char buf1[0x40], buf2[0x40];
    int i, j;
    for (i=0; i < 0x10; i++)
      for (j = 0; j < 0x10; j++)
	{
	  int k;
	  for (k = 0; k < 0x3f; k++)
	    {
	      buf1[k] = '0' ^ (k & 4);
	      buf2[k] = '4' ^ (k & 4);
	    }
	  buf1[i] = buf1[0x3f] = 0;
	  buf2[j] = buf2[0x3f] = 0;
	  for (k = 0; k < 0xf; k++)
	    {
	      int cnum = 0x10+0x10*k+0x100*j+0x1000*i;
	      check (strcmp (buf1+i,buf2+j) == 0, cnum);
	      buf1[i+k] = 'A' + i + k;
	      buf1[i+k+1] = 0;
	      check (strcmp (buf1+i,buf2+j) > 0, cnum+1);
	      check (strcmp (buf2+j,buf1+i) < 0, cnum+2);
	      buf2[j+k] = 'B' + i + k;
	      buf2[j+k+1] = 0;
	      check (strcmp (buf1+i,buf2+j) < 0, cnum+3);
	      check (strcmp (buf2+j,buf1+i) > 0, cnum+4);
	      buf2[j+k] = 'A' + i + k;
	      buf1[i] = 'A' + i + 0x80;
	      check (strcmp (buf1+i,buf2+j) > 0, cnum+5);
	      check (strcmp (buf2+j,buf1+i) < 0, cnum+6);
	      buf1[i] = 'A' + i;
	    }
	}
  }
}

#define SIMPLE_COPY(fn, n, str, ntest) \
  do {									      \
    int __n;								      \
    char *cp;								      \
    for (__n = 0; __n < (int) sizeof (one); ++__n)			      \
      one[__n] = 'Z';							      \
    fn (one, str);							      \
    for (cp = one, __n = 0; __n < n; ++__n, ++cp)			      \
      check (*cp == '0' + (n % 10), ntest);				      \
    check (*cp == '\0', ntest);						      \
  } while (0)

static void
test_strcpy (void)
{
  int i;
  it = "strcpy";
  check (strcpy (one, "abcd") == one, 1); 
  equal (one, "abcd", 2);		

  (void) strcpy (one, "x");
  equal (one, "x", 3);			
  equal (one+2, "cd", 4);		

  (void) strcpy (two, "hi there");
  (void) strcpy (one, two);
  equal (one, "hi there", 5);		
  equal (two, "hi there", 6);		

  (void) strcpy (one, "");
  equal (one, "", 7);			

  for (i = 0; i < 16; i++)
    {
      (void) strcpy (one + i, "hi there");	
      equal (one + i, "hi there", 8 + (i * 2));
      (void) strcpy (two, one + i);		
      equal (two, "hi there", 9 + (i * 2));
    }

  SIMPLE_COPY(strcpy, 0, "", 41);
  SIMPLE_COPY(strcpy, 1, "1", 42);
  SIMPLE_COPY(strcpy, 2, "22", 43);
  SIMPLE_COPY(strcpy, 3, "333", 44);
  SIMPLE_COPY(strcpy, 4, "4444", 45);
  SIMPLE_COPY(strcpy, 5, "55555", 46);
  SIMPLE_COPY(strcpy, 6, "666666", 47);
  SIMPLE_COPY(strcpy, 7, "7777777", 48);
  SIMPLE_COPY(strcpy, 8, "88888888", 49);
  SIMPLE_COPY(strcpy, 9, "999999999", 50);
  SIMPLE_COPY(strcpy, 10, "0000000000", 51);
  SIMPLE_COPY(strcpy, 11, "11111111111", 52);
  SIMPLE_COPY(strcpy, 12, "222222222222", 53);
  SIMPLE_COPY(strcpy, 13, "3333333333333", 54);
  SIMPLE_COPY(strcpy, 14, "44444444444444", 55);
  SIMPLE_COPY(strcpy, 15, "555555555555555", 56);
  SIMPLE_COPY(strcpy, 16, "6666666666666666", 57);

  
  { const void *src = "frobozz";
    void *dst = one;
    check (strcpy (dst, src) == dst, 1);
    equal (dst, "frobozz", 2);
  }
}

static void
test_stpcpy (void)
{
  it = "stpcpy";
  check ((stpcpy (one, "a") - one) == 1, 1);
  equal (one, "a", 2);

  check ((stpcpy (one, "ab") - one) == 2, 3);
  equal (one, "ab", 4);

  check ((stpcpy (one, "abc") - one) == 3, 5);
  equal (one, "abc", 6);

  check ((stpcpy (one, "abcd") - one) == 4, 7);
  equal (one, "abcd", 8);

  check ((stpcpy (one, "abcde") - one) == 5, 9);
  equal (one, "abcde", 10);

  check ((stpcpy (one, "abcdef") - one) == 6, 11);
  equal (one, "abcdef", 12);

  check ((stpcpy (one, "abcdefg") - one) == 7, 13);
  equal (one, "abcdefg", 14);

  check ((stpcpy (one, "abcdefgh") - one) == 8, 15);
  equal (one, "abcdefgh", 16);

  check ((stpcpy (one, "abcdefghi") - one) == 9, 17);
  equal (one, "abcdefghi", 18);

  check ((stpcpy (one, "x") - one) == 1, 19);
  equal (one, "x", 20);			
  equal (one+2, "cdefghi", 21);		

  check ((stpcpy (one, "xx") - one) == 2, 22);
  equal (one, "xx", 23);		
  equal (one+3, "defghi", 24);		

  check ((stpcpy (one, "xxx") - one) == 3, 25);
  equal (one, "xxx", 26);		
  equal (one+4, "efghi", 27);		

  check ((stpcpy (one, "xxxx") - one) == 4, 28);
  equal (one, "xxxx", 29);		
  equal (one+5, "fghi", 30);		

  check ((stpcpy (one, "xxxxx") - one) == 5, 31);
  equal (one, "xxxxx", 32);		
  equal (one+6, "ghi", 33);		

  check ((stpcpy (one, "xxxxxx") - one) == 6, 34);
  equal (one, "xxxxxx", 35);		
  equal (one+7, "hi", 36);		

  check ((stpcpy (one, "xxxxxxx") - one) == 7, 37);
  equal (one, "xxxxxxx", 38);		
  equal (one+8, "i", 39);		

  check ((stpcpy (stpcpy (stpcpy (one, "a"), "b"), "c") - one) == 3, 40);
  equal (one, "abc", 41);
  equal (one + 4, "xxx", 42);

  SIMPLE_COPY(stpcpy, 0, "", 43);
  SIMPLE_COPY(stpcpy, 1, "1", 44);
  SIMPLE_COPY(stpcpy, 2, "22", 45);
  SIMPLE_COPY(stpcpy, 3, "333", 46);
  SIMPLE_COPY(stpcpy, 4, "4444", 47);
  SIMPLE_COPY(stpcpy, 5, "55555", 48);
  SIMPLE_COPY(stpcpy, 6, "666666", 49);
  SIMPLE_COPY(stpcpy, 7, "7777777", 50);
  SIMPLE_COPY(stpcpy, 8, "88888888", 51);
  SIMPLE_COPY(stpcpy, 9, "999999999", 52);
  SIMPLE_COPY(stpcpy, 10, "0000000000", 53);
  SIMPLE_COPY(stpcpy, 11, "11111111111", 54);
  SIMPLE_COPY(stpcpy, 12, "222222222222", 55);
  SIMPLE_COPY(stpcpy, 13, "3333333333333", 56);
  SIMPLE_COPY(stpcpy, 14, "44444444444444", 57);
  SIMPLE_COPY(stpcpy, 15, "555555555555555", 58);
  SIMPLE_COPY(stpcpy, 16, "6666666666666666", 59);
}

#if !defined(__APPLE__)
static void
test_stpncpy (void)
{
  it = "stpncpy";
  memset (one, 'x', sizeof (one));
  check (stpncpy (one, "abc", 2) == one + 2, 1);
  check (stpncpy (one, "abc", 3) == one + 3, 2);
  check (stpncpy (one, "abc", 4) == one + 3, 3);
  check (one[3] == '\0' && one[4] == 'x', 4);
  check (stpncpy (one, "abcd", 5) == one + 4, 5);
  check (one[4] == '\0' && one[5] == 'x', 6);
  check (stpncpy (one, "abcd", 6) == one + 4, 7);
  check (one[4] == '\0' && one[5] == '\0' && one[6] == 'x', 8);
}
#endif

static void
test_strcat (void)
{
  it = "strcat";
  (void) strcpy (one, "ijk");
  check (strcat (one, "lmn") == one, 1); 
  equal (one, "ijklmn", 2);		

  (void) strcpy (one, "x");
  (void) strcat (one, "yz");
  equal (one, "xyz", 3);			
  equal (one+4, "mn", 4);			

  (void) strcpy (one, "gh");
  (void) strcpy (two, "ef");
  (void) strcat (one, two);
  equal (one, "ghef", 5);			
  equal (two, "ef", 6);			

  (void) strcpy (one, "");
  (void) strcat (one, "");
  equal (one, "", 7);			
  (void) strcpy (one, "ab");
  (void) strcat (one, "");
  equal (one, "ab", 8);
  (void) strcpy (one, "");
  (void) strcat (one, "cd");
  equal (one, "cd", 9);
}

static void
test_strncat (void)
{
  it = "strncat";
  (void) strcpy (one, "ijk");
  check (strncat (one, "lmn", 99) == one, 1);	
  equal (one, "ijklmn", 2);		

  (void) strcpy (one, "x");
  (void) strncat (one, "yz", 99);
  equal (one, "xyz", 3);		
  equal (one+4, "mn", 4);		

  (void) strcpy (one, "gh");
  (void) strcpy (two, "ef");
  (void) strncat (one, two, 99);
  equal (one, "ghef", 5);			
  equal (two, "ef", 6);			

  (void) strcpy (one, "");
  (void) strncat (one, "", 99);
  equal (one, "", 7);			
  (void) strcpy (one, "ab");
  (void) strncat (one, "", 99);
  equal (one, "ab", 8);
  (void) strcpy (one, "");
  (void) strncat (one, "cd", 99);
  equal (one, "cd", 9);

  (void) strcpy (one, "ab");
  (void) strncat (one, "cdef", 2);
  equal (one, "abcd", 10);			

  (void) strncat (one, "gh", 0);
  equal (one, "abcd", 11);			

  (void) strncat (one, "gh", 2);
  equal (one, "abcdgh", 12);		
}

static void
test_strncmp (void)
{
  
  it = "strncmp";
  check (strncmp ("", "", 99) == 0, 1);	
  check (strncmp ("a", "a", 99) == 0, 2);	
  check (strncmp ("abc", "abc", 99) == 0, 3);	
  check (strncmp ("abc", "abcd", 99) < 0, 4);	
  check (strncmp ("abcd", "abc", 99) > 0, 5);
  check (strncmp ("abcd", "abce", 99) < 0, 6);	
  check (strncmp ("abce", "abcd", 99) > 0, 7);
  check (strncmp ("a\203", "a", 2) > 0, 8);	
  check (strncmp ("a\203", "a\003", 2) > 0, 9);
  check (strncmp ("abce", "abcd", 3) == 0, 10);	
  check (strncmp ("abce", "abc", 3) == 0, 11);	
  check (strncmp ("abcd", "abce", 4) < 0, 12);	
  check (strncmp ("abc", "def", 0) == 0, 13);	
}

static void
test_strncpy (void)
{
  
  it = "strncpy";
  check (strncpy (one, "abc", 4) == one, 1);	
  equal (one, "abc", 2);			

  (void) strcpy (one, "abcdefgh");
  (void) strncpy (one, "xyz", 2);
  equal (one, "xycdefgh", 3);			

  (void) strcpy (one, "abcdefgh");
  (void) strncpy (one, "xyz", 3);		
  equal (one, "xyzdefgh", 4);

  (void) strcpy (one, "abcdefgh");
  (void) strncpy (one, "xyz", 4);		
  equal (one, "xyz", 5);
  equal (one+4, "efgh", 6);			

  (void) strcpy (one, "abcdefgh");
  (void) strncpy (one, "xyz", 5);		
  equal (one, "xyz", 7);
  equal (one+4, "", 8);
  equal (one+5, "fgh", 9);

  (void) strcpy (one, "abc");
  (void) strncpy (one, "xyz", 0);		
  equal (one, "abc", 10);

  (void) strncpy (one, "", 2);		
  equal (one, "", 11);
  equal (one+1, "", 12);
  equal (one+2, "c", 13);

  (void) strcpy (one, "hi there");
  (void) strncpy (two, one, 9);
  equal (two, "hi there", 14);		
  equal (one, "hi there", 15);		
}

static void
test_strlen (void)
{
  it = "strlen";
  check (strlen ("") == 0, 1);		
  check (strlen ("a") == 1, 2);		
  check (strlen ("abcd") == 4, 3);	
  {
    char buf[4096];
    int i;
    char *p;
    for (i=0; i < 0x100; i++)
      {
	p = (char *) ((unsigned long int)(buf + 0xff) & ~0xff) + i;
	strcpy (p, "OK");
	strcpy (p+3, "BAD/WRONG");
	check (strlen (p) == 2, 4+i);
      }
   }
}

static void
test_strchr (void)
{
  it = "strchr";
  check (strchr ("abcd", 'z') == NULL, 1);	
  (void) strcpy (one, "abcd");
  check (strchr (one, 'c') == one+2, 2);	
  check (strchr (one, 'd') == one+3, 3);	
  check (strchr (one, 'a') == one, 4);		
  check (strchr (one, '\0') == one+4, 5);	
  (void) strcpy (one, "ababa");
  check (strchr (one, 'b') == one+1, 6);	
  (void) strcpy (one, "");
  check (strchr (one, 'b') == NULL, 7);		
  check (strchr (one, '\0') == one, 8);		
  {
    char buf[4096];
    int i;
    char *p;
    for (i=0; i < 0x100; i++)
      {
	p = (char *) ((unsigned long int) (buf + 0xff) & ~0xff) + i;
	strcpy (p, "OK");
	strcpy (p+3, "BAD/WRONG");
	check (strchr (p, '/') == NULL, 9+i);
      }
   }
}

#if !defined(__APPLE__)
static void
test_strchrnul (void)
{
  const char *os;
  it = "strchrnul";
  cp = strchrnul ((os = "abcd"), 'z');
  check (*cp == '\0', 1);			
  check (cp == os + 4, 2);
  (void) strcpy (one, "abcd");
  check (strchrnul (one, 'c') == one+2, 3);	
  check (strchrnul (one, 'd') == one+3, 4);	
  check (strchrnul (one, 'a') == one, 5);	
  check (strchrnul (one, '\0') == one+4, 6);	
  (void) strcpy (one, "ababa");
  check (strchrnul (one, 'b') == one+1, 7);	
  (void) strcpy (one, "");
  check (strchrnul (one, 'b') == one, 8);	
  check (strchrnul (one, '\0') == one, 9);	
  {
    char buf[4096];
    int i;
    char *p;
    for (i=0; i < 0x100; i++)
      {
	p = (char *) ((unsigned long int) (buf + 0xff) & ~0xff) + i;
	strcpy (p, "OK");
	strcpy (p+3, "BAD/WRONG");
	cp = strchrnul (p, '/');
	check (*cp == '\0', 9+2*i);
	check (cp == p+2, 10+2*i);
      }
   }
}
#endif

#if !defined(__APPLE__)
static void
test_rawmemchr (void)
{
  it = "rawmemchr";
  (void) strcpy (one, "abcd");
  check (rawmemchr (one, 'c') == one+2, 1);	
  check (rawmemchr (one, 'd') == one+3, 2);	
  check (rawmemchr (one, 'a') == one, 3);		
  check (rawmemchr (one, '\0') == one+4, 4);	
  (void) strcpy (one, "ababa");
  check (rawmemchr (one, 'b') == one+1, 5);	
  (void) strcpy (one, "");
  check (rawmemchr (one, '\0') == one, 6);	
  {
    char buf[4096];
    int i;
    char *p;
    for (i=0; i < 0x100; i++)
      {
	p = (char *) ((unsigned long int) (buf + 0xff) & ~0xff) + i;
	strcpy (p, "OK");
	strcpy (p+3, "BAD/WRONG");
	check (rawmemchr (p, 'R') == p+8, 6+i);
      }
   }
}
#endif

static void
test_index (void)
{
  it = "index";
  check (index ("abcd", 'z') == NULL, 1);	
  (void) strcpy (one, "abcd");
  check (index (one, 'c') == one+2, 2);	
  check (index (one, 'd') == one+3, 3);	
  check (index (one, 'a') == one, 4);	
  check (index (one, '\0') == one+4, 5);	
  (void) strcpy (one, "ababa");
  check (index (one, 'b') == one+1, 6);	
  (void) strcpy (one, "");
  check (index (one, 'b') == NULL, 7);	
  check (index (one, '\0') == one, 8);	
}

static void
test_strrchr (void)
{
  it = "strrchr";
  check (strrchr ("abcd", 'z') == NULL, 1);	
  (void) strcpy (one, "abcd");
  check (strrchr (one, 'c') == one+2, 2);	
  check (strrchr (one, 'd') == one+3, 3);	
  check (strrchr (one, 'a') == one, 4);		
  check (strrchr (one, '\0') == one+4, 5);	
  (void) strcpy (one, "ababa");
  check (strrchr (one, 'b') == one+3, 6);	
  (void) strcpy (one, "");
  check (strrchr (one, 'b') == NULL, 7);	
  check (strrchr (one, '\0') == one, 8);	
  {
    char buf[4096];
    int i;
    char *p;
    for (i=0; i < 0x100; i++)
      {
	p = (char *) ((unsigned long int) (buf + 0xff) & ~0xff) + i;
	strcpy (p, "OK");
	strcpy (p+3, "BAD/WRONG");
	check (strrchr (p, '/') == NULL, 9+i);
      }
   }
}

#if !defined(__APPLE__)
static void
test_memrchr (void)
{
  size_t l;
  it = "memrchr";
  check (memrchr ("abcd", 'z', 5) == NULL, 1);	
  (void) strcpy (one, "abcd");
  l = strlen (one) + 1;
  check (memrchr (one, 'c', l) == one+2, 2);	
  check (memrchr (one, 'd', l) == one+3, 3);	
  check (memrchr (one, 'a', l) == one, 4);		
  check (memrchr (one, '\0', l) == one+4, 5);	
  (void) strcpy (one, "ababa");
  l = strlen (one) + 1;
  check (memrchr (one, 'b', l) == one+3, 6);	
  (void) strcpy (one, "");
  l = strlen (one) + 1;
  check (memrchr (one, 'b', l) == NULL, 7);	
  check (memrchr (one, '\0', l) == one, 8);	

  {
    char buf[128 + sizeof(long)];
    long align, len, i, pos;

    for (align = 0; align < (long) sizeof(long); ++align) {
      for (len = 0; len < (long) (sizeof(buf) - align); ++len) {
	for (i = 0; i < len; ++i)
	  buf[align + i] = 'x';		

	for (pos = len - 1; pos >= 0; --pos) {
#if 0
	  printf("align %d, len %d, pos %d\n", align, len, pos);
#endif
	  check(memrchr(buf + align, 'x', len) == buf + align + pos, 9);
	  check(memrchr(buf + align + pos + 1, 'x', len - (pos + 1)) == NULL,
		10);
	  buf[align + pos] = '-';
	}
      }
    }
  }
}
#endif

static void
test_rindex (void)
{
  it = "rindex";
  check (rindex ("abcd", 'z') == NULL, 1);	
  (void) strcpy (one, "abcd");
  check (rindex (one, 'c') == one+2, 2);	
  check (rindex (one, 'd') == one+3, 3);	
  check (rindex (one, 'a') == one, 4);	
  check (rindex (one, '\0') == one+4, 5);	
  (void) strcpy (one, "ababa");
  check (rindex (one, 'b') == one+3, 6);	
  (void) strcpy (one, "");
  check (rindex (one, 'b') == NULL, 7);	
  check (rindex (one, '\0') == one, 8);	
}

static void
test_strpbrk (void)
{
  it = "strpbrk";
  check(strpbrk("abcd", "z") == NULL, 1);	
  (void) strcpy(one, "abcd");
  check(strpbrk(one, "c") == one+2, 2);	
  check(strpbrk(one, "d") == one+3, 3);	
  check(strpbrk(one, "a") == one, 4);	
  check(strpbrk(one, "") == NULL, 5);	
  check(strpbrk(one, "cb") == one+1, 6);	
  (void) strcpy(one, "abcabdea");
  check(strpbrk(one, "b") == one+1, 7);	
  check(strpbrk(one, "cb") == one+1, 8);	
  check(strpbrk(one, "db") == one+1, 9);	
  (void) strcpy(one, "");
  check(strpbrk(one, "bc") == NULL, 10);	
  (void) strcpy(one, "");
  check(strpbrk(one, "bcd") == NULL, 11);	
  (void) strcpy(one, "");
  check(strpbrk(one, "bcde") == NULL, 12);	
  check(strpbrk(one, "") == NULL, 13);	
  (void) strcpy(one, "abcabdea");
  check(strpbrk(one, "befg") == one+1, 14);	
  check(strpbrk(one, "cbr") == one+1, 15);	
  check(strpbrk(one, "db") == one+1, 16);	
  check(strpbrk(one, "efgh") == one+6, 17);	
}

static void
test_strstr (void)
{
  it = "strstr";
  check(strstr("abcd", "z") == NULL, 1);	
  check(strstr("abcd", "abx") == NULL, 2);	
  (void) strcpy(one, "abcd");
  check(strstr(one, "c") == one+2, 3);	
  check(strstr(one, "bc") == one+1, 4);	
  check(strstr(one, "d") == one+3, 5);	
  check(strstr(one, "cd") == one+2, 6);	
  check(strstr(one, "abc") == one, 7);	
  check(strstr(one, "abcd") == one, 8);	
  check(strstr(one, "abcde") == NULL, 9);	
  check(strstr(one, "de") == NULL, 10);	
  check(strstr(one, "") == one, 11);	
  (void) strcpy(one, "ababa");
  check(strstr(one, "ba") == one+1, 12);	
  (void) strcpy(one, "");
  check(strstr(one, "b") == NULL, 13);	
  check(strstr(one, "") == one, 14);	
  (void) strcpy(one, "bcbca");
  check(strstr(one, "bca") == one+2, 15);	
  (void) strcpy(one, "bbbcabbca");
  check(strstr(one, "bbca") == one+1, 16);	
}

static void
test_strspn (void)
{
  it = "strspn";
  check(strspn("abcba", "abc") == 5, 1);	
  check(strspn("abcba", "ab") == 2, 2);	
  check(strspn("abc", "qx") == 0, 3);	
  check(strspn("", "ab") == 0, 4);	
  check(strspn("abc", "") == 0, 5);	
}

static void
test_strcspn (void)
{
  it = "strcspn";
  check(strcspn("abcba", "qx") == 5, 1);	
  check(strcspn("abcba", "cx") == 2, 2);	
  check(strcspn("abc", "abc") == 0, 3);	
  check(strcspn("", "ab") == 0, 4);	
  check(strcspn("abc", "") == 3, 5);	
}

static void
test_strtok (void)
{
  it = "strtok";
  (void) strcpy(one, "first, second, third");
  equal(strtok(one, ", "), "first", 1);	
  equal(one, "first", 2);
  equal(strtok((char *)NULL, ", "), "second", 3);
  equal(strtok((char *)NULL, ", "), "third", 4);
  check(strtok((char *)NULL, ", ") == NULL, 5);
  (void) strcpy(one, ", first, ");
  equal(strtok(one, ", "), "first", 6);	
  check(strtok((char *)NULL, ", ") == NULL, 7);
  (void) strcpy(one, "1a, 1b; 2a, 2b");
  equal(strtok(one, ", "), "1a", 8);	
  equal(strtok((char *)NULL, "; "), "1b", 9);
  equal(strtok((char *)NULL, ", "), "2a", 10);
  (void) strcpy(two, "x-y");
  equal(strtok(two, "-"), "x", 11);	
  equal(strtok((char *)NULL, "-"), "y", 12);
  check(strtok((char *)NULL, "-") == NULL, 13);
  (void) strcpy(one, "a,b, c,, ,d");
  equal(strtok(one, ", "), "a", 14);	
  equal(strtok((char *)NULL, ", "), "b", 15);
  equal(strtok((char *)NULL, " ,"), "c", 16);	
  equal(strtok((char *)NULL, " ,"), "d", 17);
  check(strtok((char *)NULL, ", ") == NULL, 18);
  check(strtok((char *)NULL, ", ") == NULL, 19);	
  (void) strcpy(one, ", ");
  check(strtok(one, ", ") == NULL, 20);	
  (void) strcpy(one, "");
  check(strtok(one, ", ") == NULL, 21);	
  (void) strcpy(one, "abc");
  equal(strtok(one, ", "), "abc", 22);	
  check(strtok((char *)NULL, ", ") == NULL, 23);
  (void) strcpy(one, "abc");
  equal(strtok(one, ""), "abc", 24);	
  check(strtok((char *)NULL, "") == NULL, 25);
  (void) strcpy(one, "abcdefgh");
  (void) strcpy(one, "a,b,c");
  equal(strtok(one, ","), "a", 26);	
  equal(strtok((char *)NULL, ","), "b", 27);
  equal(strtok((char *)NULL, ","), "c", 28);
  check(strtok((char *)NULL, ",") == NULL, 29);
  equal(one+6, "gh", 30);			
  equal(one, "a", 31);			
  equal(one+2, "b", 32);
  equal(one+4, "c", 33);
}

static void
test_strtok_r (void)
{
  it = "strtok_r";
  (void) strcpy(one, "first, second, third");
  cp = NULL;	
  equal(strtok_r(one, ", ", &cp), "first", 1);	
  equal(one, "first", 2);
  equal(strtok_r((char *)NULL, ", ", &cp), "second", 3);
  equal(strtok_r((char *)NULL, ", ", &cp), "third", 4);
  check(strtok_r((char *)NULL, ", ", &cp) == NULL, 5);
  (void) strcpy(one, ", first, ");
  cp = NULL;
  equal(strtok_r(one, ", ", &cp), "first", 6);	
  check(strtok_r((char *)NULL, ", ", &cp) == NULL, 7);
  (void) strcpy(one, "1a, 1b; 2a, 2b");
  cp = NULL;
  equal(strtok_r(one, ", ", &cp), "1a", 8);	
  equal(strtok_r((char *)NULL, "; ", &cp), "1b", 9);
  equal(strtok_r((char *)NULL, ", ", &cp), "2a", 10);
  (void) strcpy(two, "x-y");
  cp = NULL;
  equal(strtok_r(two, "-", &cp), "x", 11);	
  equal(strtok_r((char *)NULL, "-", &cp), "y", 12);
  check(strtok_r((char *)NULL, "-", &cp) == NULL, 13);
  (void) strcpy(one, "a,b, c,, ,d");
  cp = NULL;
  equal(strtok_r(one, ", ", &cp), "a", 14);	
  equal(strtok_r((char *)NULL, ", ", &cp), "b", 15);
  equal(strtok_r((char *)NULL, " ,", &cp), "c", 16);	
  equal(strtok_r((char *)NULL, " ,", &cp), "d", 17);
  check(strtok_r((char *)NULL, ", ", &cp) == NULL, 18);
  check(strtok_r((char *)NULL, ", ", &cp) == NULL, 19);	
  (void) strcpy(one, ", ");
  cp = NULL;
  check(strtok_r(one, ", ", &cp) == NULL, 20);	
  (void) strcpy(one, "");
  cp = NULL;
  check(strtok_r(one, ", ", &cp) == NULL, 21);	
  check(strtok_r((char *)NULL, ", ", &cp) == NULL, 22);	
  (void) strcpy(one, "abc");
  cp = NULL;
  equal(strtok_r(one, ", ", &cp), "abc", 23);	
  check(strtok_r((char *)NULL, ", ", &cp) == NULL, 24);
  (void) strcpy(one, "abc");
  cp = NULL;
  equal(strtok_r(one, "", &cp), "abc", 25);	
  check(strtok_r((char *)NULL, "", &cp) == NULL, 26);
  (void) strcpy(one, "abcdefgh");
  (void) strcpy(one, "a,b,c");
  cp = NULL;
  equal(strtok_r(one, ",", &cp), "a", 27);	
  equal(strtok_r((char *)NULL, ",", &cp), "b", 28);
  equal(strtok_r((char *)NULL, ",", &cp), "c", 29);
  check(strtok_r((char *)NULL, ",", &cp) == NULL, 30);
  equal(one+6, "gh", 31);			
  equal(one, "a", 32);			
  equal(one+2, "b", 33);
  equal(one+4, "c", 34);
}

static void
test_strsep (void)
{
  char *ptr;
  it = "strsep";
  cp = strcpy(one, "first, second, third");
  equal(strsep(&cp, ", "), "first", 1);	
  equal(one, "first", 2);
  equal(strsep(&cp, ", "), "", 3);
  equal(strsep(&cp, ", "), "second", 4);
  equal(strsep(&cp, ", "), "", 5);
  equal(strsep(&cp, ", "), "third", 6);
  check(strsep(&cp, ", ") == NULL, 7);
  cp = strcpy(one, ", first, ");
  equal(strsep(&cp, ", "), "", 8);
  equal(strsep(&cp, ", "), "", 9);
  equal(strsep(&cp, ", "), "first", 10);	
  equal(strsep(&cp, ", "), "", 11);
  equal(strsep(&cp, ", "), "", 12);
  check(strsep(&cp, ", ") == NULL, 13);
  cp = strcpy(one, "1a, 1b; 2a, 2b");
  equal(strsep(&cp, ", "), "1a", 14);	
  equal(strsep(&cp, ", "), "", 15);
  equal(strsep(&cp, "; "), "1b", 16);
  equal(strsep(&cp, ", "), "", 17);
  equal(strsep(&cp, ", "), "2a", 18);
  cp = strcpy(two, "x-y");
  equal(strsep(&cp, "-"), "x", 19);	
  equal(strsep(&cp, "-"), "y", 20);
  check(strsep(&cp, "-") == NULL, 21);
  cp = strcpy(one, "a,b, c,, ,d ");
  equal(strsep(&cp, ", "), "a", 22);	
  equal(strsep(&cp, ", "), "b", 23);
  equal(strsep(&cp, " ,"), "", 24);
  equal(strsep(&cp, " ,"), "c", 25);	
  equal(strsep(&cp, " ,"), "", 26);
  equal(strsep(&cp, " ,"), "", 27);
  equal(strsep(&cp, " ,"), "", 28);
  equal(strsep(&cp, " ,"), "d", 29);
  equal(strsep(&cp, " ,"), "", 30);
  check(strsep(&cp, ", ") == NULL, 31);
  check(strsep(&cp, ", ") == NULL, 32);	
  cp = strcpy(one, ", ");
  equal(strsep(&cp, ", "), "", 33);
  equal(strsep(&cp, ", "), "", 34);
  equal(strsep(&cp, ", "), "", 35);
  check(strsep(&cp, ", ") == NULL, 36);	
  cp = strcpy(one, "");
  equal(strsep(&cp, ", "), "", 37);
  check(strsep(&cp, ", ") == NULL, 38);	
  cp = strcpy(one, "abc");
  equal(strsep(&cp, ", "), "abc", 39);	
  check(strsep(&cp, ", ") == NULL, 40);
  cp = strcpy(one, "abc");
  equal(strsep(&cp, ""), "abc", 41);	
  check(strsep(&cp, "") == NULL, 42);
  (void) strcpy(one, "abcdefgh");
  cp = strcpy(one, "a,b,c");
  equal(strsep(&cp, ","), "a", 43);	
  equal(strsep(&cp, ","), "b", 44);
  equal(strsep(&cp, ","), "c", 45);
  check(strsep(&cp, ",") == NULL, 46);
  equal(one+6, "gh", 47);		
  equal(one, "a", 48);			
  equal(one+2, "b", 49);
  equal(one+4, "c", 50);

  {
#   if !defined(__APPLE__)
    char text[] = "This,is,a,test";
    char *list = strdupa (text);
    equal (strsep (&list, ","), "This", 51);
    equal (strsep (&list, ","), "is", 52);
    equal (strsep (&list, ","), "a", 53);
    equal (strsep (&list, ","), "test", 54);
    check (strsep (&list, ",") == NULL, 55);
#   endif
  }

  cp = strcpy(one, "a,b, c,, ,d,");
  equal(strsep(&cp, ","), "a", 56);	
  equal(strsep(&cp, ","), "b", 57);
  equal(strsep(&cp, ","), " c", 58);	
  equal(strsep(&cp, ","), "", 59);
  equal(strsep(&cp, ","), " ", 60);
  equal(strsep(&cp, ","), "d", 61);
  equal(strsep(&cp, ","), "", 62);
  check(strsep(&cp, ",") == NULL, 63);
  check(strsep(&cp, ",") == NULL, 64);	

  cp = strcpy(one, "a,b, c,, ,d,");
  equal(strsep(&cp, "xy,"), "a", 65);	
  equal(strsep(&cp, "x,y"), "b", 66);
  equal(strsep(&cp, ",xy"), " c", 67);	
  equal(strsep(&cp, "xy,"), "", 68);
  equal(strsep(&cp, "x,y"), " ", 69);
  equal(strsep(&cp, ",xy"), "d", 70);
  equal(strsep(&cp, "xy,"), "", 71);
  check(strsep(&cp, "x,y") == NULL, 72);
  check(strsep(&cp, ",xy") == NULL, 73);	

  cp = strcpy(one, "ABC");
  one[4] = ':';
  equal(strsep(&cp, "C"), "AB", 74);	
  ptr = strsep(&cp, ":");
  equal(ptr, "", 75);
  check(ptr == one + 3, 76);
  check(cp == NULL, 77);

  cp = strcpy(one, "ABC");
  one[4] = ':';
  equal(strsep(&cp, "CD"), "AB", 78);	
  ptr = strsep(&cp, ":.");
  equal(ptr, "", 79);
  check(ptr == one + 3, 80);

  cp = strcpy(one, "ABC");		
  equal(strsep(&cp, ","), "ABC", 81);
  check(cp == NULL, 82);

  *one = '\0';				
  cp = one;
  ptr = strsep(&cp, ",");
  equal(ptr, "", 83);
  check(ptr == one, 84);
  check(cp == NULL, 85);

  *one = '\0';				
  cp = one;
  ptr = strsep(&cp, "");
  equal(ptr, "", 86);
  check(ptr == one , 87);
  check(cp == NULL, 88);
}

static void
test_memcmp (void)
{
  it = "memcmp";
  check(memcmp("a", "a", 1) == 0, 1);		
  check(memcmp("abc", "abc", 3) == 0, 2);	
  check(memcmp("abcd", "abce", 4) < 0, 3);	
  check(memcmp("abce", "abcd", 4) > 0, 4);
  check(memcmp("alph", "beta", 4) < 0, 5);
  check(memcmp("a\203", "a\003", 2) > 0, 6);
  check(memcmp("abce", "abcd", 3) == 0, 7);	
  check(memcmp("abc", "def", 0) == 0, 8);	
}

static void
test_memchr (void)
{
  it = "memchr";
  check(memchr("abcd", 'z', 4) == NULL, 1);	
  (void) strcpy(one, "abcd");
  check(memchr(one, 'c', 4) == one+2, 2);	
  check(memchr(one, ~0xff|'c', 4) == one+2, 2);	
  check(memchr(one, 'd', 4) == one+3, 3);	
  check(memchr(one, 'a', 4) == one, 4);	
  check(memchr(one, '\0', 5) == one+4, 5);	
  (void) strcpy(one, "ababa");
  check(memchr(one, 'b', 5) == one+1, 6);	
  check(memchr(one, 'b', 0) == NULL, 7);	
  check(memchr(one, 'a', 1) == one, 8);	
  (void) strcpy(one, "a\203b");
  check(memchr(one, 0203, 3) == one+1, 9);	

  {
    char buf[128 + sizeof(long)];
    long align, len, i, pos;

    for (align = 0; align < (long) sizeof(long); ++align) {
      for (len = 0; len < (long) (sizeof(buf) - align); ++len) {
	for (i = 0; i < len; ++i) {
	  buf[align + i] = 'x';		
	}
	for (pos = 0; pos < len; ++pos) {
#if 0
	  printf("align %d, len %d, pos %d\n", align, len, pos);
#endif
	  check(memchr(buf + align, 'x', len) == buf + align + pos, 10);
	  check(memchr(buf + align, 'x', pos) == NULL, 11);
	  buf[align + pos] = '-';
	}
      }
    }
  }
}

static void
test_memcpy (void)
{
  int i;
  it = "memcpy";
  check(memcpy(one, "abc", 4) == one, 1);	
  equal(one, "abc", 2);			

  (void) strcpy(one, "abcdefgh");
  (void) memcpy(one+1, "xyz", 2);
  equal(one, "axydefgh", 3);		

  (void) strcpy(one, "abc");
  (void) memcpy(one, "xyz", 0);
  equal(one, "abc", 4);			

  (void) strcpy(one, "hi there");
  (void) strcpy(two, "foo");
  (void) memcpy(two, one, 9);
  equal(two, "hi there", 5);		
  equal(one, "hi there", 6);		

  for (i = 0; i < 16; i++)
    {
      const char *x = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
      strcpy (one, x);
      check (memcpy (one + i, "hi there", 9) == one + i,
	     7 + (i * 6));		
      check (memcmp (one, x, i) == 0, 8 + (i * 6));  
      equal (one + i, "hi there", 9 + (i * 6));
      check (one[i + 9] == 'x', 10 + (i * 6));       
      check (memcpy (two, one + i, 9) == two,
	     11 + (i * 6));		
      equal (two, "hi there", 12 + (i * 6));
    }
}

#if !defined(__APPLE__)
static void
test_mempcpy (void)
{
  int i;
  it = "mempcpy";
  check(mempcpy(one, "abc", 4) == one + 4, 1);	
  equal(one, "abc", 2);			

  (void) strcpy(one, "abcdefgh");
  (void) mempcpy(one+1, "xyz", 2);
  equal(one, "axydefgh", 3);		

  (void) strcpy(one, "abc");
  (void) mempcpy(one, "xyz", 0);
  equal(one, "abc", 4);			

  (void) strcpy(one, "hi there");
  (void) strcpy(two, "foo");
  (void) mempcpy(two, one, 9);
  equal(two, "hi there", 5);		
  equal(one, "hi there", 6);		

  for (i = 0; i < 16; i++)
    {
      const char *x = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
      strcpy (one, x);
      check (mempcpy (one + i, "hi there", 9) == one + i + 9,
	     7 + (i * 6));		
      check (memcmp (one, x, i) == 0, 8 + (i * 6));  
      equal (one + i, "hi there", 9 + (i * 6));
      check (one[i + 9] == 'x', 10 + (i * 6));       
      check (mempcpy (two, one + i, 9) == two + 9,
	     11 + (i * 6));		
      equal (two, "hi there", 12 + (i * 6));
    }
}
#endif

static void
test_memmove (void)
{
  it = "memmove";
  check(memmove(one, "abc", 4) == one, 1);	
  equal(one, "abc", 2);			

  (void) strcpy(one, "abcdefgh");
  (void) memmove(one+1, "xyz", 2);
  equal(one, "axydefgh", 3);		

  (void) strcpy(one, "abc");
  (void) memmove(one, "xyz", 0);
  equal(one, "abc", 4);			

  (void) strcpy(one, "hi there");
  (void) strcpy(two, "foo");
  (void) memmove(two, one, 9);
  equal(two, "hi there", 5);		
  equal(one, "hi there", 6);		

  (void) strcpy(one, "abcdefgh");
  (void) memmove(one+1, one, 9);
  equal(one, "aabcdefgh", 7);		

  (void) strcpy(one, "abcdefgh");
  (void) memmove(one+1, one+2, 7);
  equal(one, "acdefgh", 8);		

  (void) strcpy(one, "abcdefgh");
  (void) memmove(one, one, 9);
  equal(one, "abcdefgh", 9);		
}

static void
test_memccpy (void)
{
  it = "memccpy";
  check(memccpy(one, "abc", 'q', 4) == NULL, 1);	
  equal(one, "abc", 2);			

  (void) strcpy(one, "abcdefgh");
  (void) memccpy(one+1, "xyz", 'q', 2);
  equal(one, "axydefgh", 3);		

  (void) strcpy(one, "abc");
  (void) memccpy(one, "xyz", 'q', 0);
  equal(one, "abc", 4);			

  (void) strcpy(one, "hi there");
  (void) strcpy(two, "foo");
  (void) memccpy(two, one, 'q', 9);
  equal(two, "hi there", 5);		
  equal(one, "hi there", 6);		

  (void) strcpy(one, "abcdefgh");
  (void) strcpy(two, "horsefeathers");
  check(memccpy(two, one, 'f', 9) == two+6, 7);	
  equal(one, "abcdefgh", 8);		
  equal(two, "abcdefeathers", 9);		

  (void) strcpy(one, "abcd");
  (void) strcpy(two, "bumblebee");
  check(memccpy(two, one, 'a', 4) == two+1, 10);	
  equal(two, "aumblebee", 11);
  check(memccpy(two, one, 'd', 4) == two+4, 12);	
  equal(two, "abcdlebee", 13);
  (void) strcpy(one, "xyz");
  check(memccpy(two, one, 'x', 1) == two+1, 14);	
  equal(two, "xbcdlebee", 15);
}

static void
test_memset (void)
{
  int i;

  it = "memset";
  (void) strcpy(one, "abcdefgh");
  check(memset(one+1, 'x', 3) == one+1, 1);	
  equal(one, "axxxefgh", 2);		

  (void) memset(one+2, 'y', 0);
  equal(one, "axxxefgh", 3);		

  (void) memset(one+5, 0, 1);
  equal(one, "axxxe", 4);			
  equal(one+6, "gh", 5);			

  (void) memset(one+2, 010045, 1);
  equal(one, "ax\045xe", 6);		

  
  memset (one, 0x101, sizeof (one));
  for (i = 0; i < (int) sizeof (one); ++i)
    check (one[i] == '\01', 7);

  {
    char data[512];
    int j;
    int k;
    int c;

    for (i = 0; i < 512; i++)
      data[i] = 'x';
    for (c = 0; c <= 'y'; c += 'y')  
      for (j = 0; j < 256; j++)
	for (i = 0; i < 256; i++)
	  {
	    memset (data + i, c, j);
	    for (k = 0; k < i; k++)
	      if (data[k] != 'x')
		goto fail;
	    for (k = i; k < i+j; k++)
	      {
		if (data[k] != c)
		  goto fail;
		data[k] = 'x';
	      }
	    for (k = i+j; k < 512; k++)
	      if (data[k] != 'x')
		goto fail;
	    continue;

	  fail:
	    check (0, 8 + i + j * 256 + (c != 0) * 256 * 256);
	  }
  }
}

static void
test_bcopy (void)
{
  it = "bcopy";
  (void) bcopy("abc", one, 4);
  equal(one, "abc", 1);			

  (void) strcpy(one, "abcdefgh");
  (void) bcopy("xyz", one+1, 2);
  equal(one, "axydefgh", 2);		

  (void) strcpy(one, "abc");
  (void) bcopy("xyz", one, 0);
  equal(one, "abc", 3);			

  (void) strcpy(one, "hi there");
  (void) strcpy(two, "foo");
  (void) bcopy(one, two, 9);
  equal(two, "hi there", 4);		
  equal(one, "hi there", 5);		
}

static void
test_bzero (void)
{
  it = "bzero";
  (void) strcpy(one, "abcdef");
  bzero(one+2, 2);
  equal(one, "ab", 1);			
  equal(one+3, "", 2);
  equal(one+4, "ef", 3);

  (void) strcpy(one, "abcdef");
  bzero(one+2, 0);
  equal(one, "abcdef", 4);		
}

#if !defined(__APPLE__)
static void
test_strndup (void)
{
  char *p, *q;
  it = "strndup";
  p = strndup("abcdef", 12);
  check(p != NULL, 1);
  if (p != NULL)
    {
      equal(p, "abcdef", 2);
      q = strndup(p + 1, 2);
      check(q != NULL, 3);
      if (q != NULL)
	equal(q, "bc", 4);
      free (q);
    }
  free (p);
  p = strndup("abc def", 3);
  check(p != NULL, 5);
  if (p != NULL)
    equal(p, "abc", 6);
  free (p);
}
#endif

static void
test_bcmp (void)
{
  it = "bcmp";
  check(bcmp("a", "a", 1) == 0, 1);	
  check(bcmp("abc", "abc", 3) == 0, 2);	
  check(bcmp("abcd", "abce", 4) != 0, 3);	
  check(bcmp("abce", "abcd", 4) != 0, 4);
  check(bcmp("alph", "beta", 4) != 0, 5);
  check(bcmp("abce", "abcd", 3) == 0, 6);	
  check(bcmp("abc", "def", 0) == 0, 8);	
}

static void
test_strerror (void)
{
  it = "strerror";
  check(strerror(EDOM) != 0, 1);
  check(strerror(ERANGE) != 0, 2);
  check(strerror(ENOENT) != 0, 3);
}

static void
test_strcasecmp (void)
{
  it = "strcasecmp";
  
  check(strcasecmp("a", "a") == 0, 1);
  check(strcasecmp("a", "A") == 0, 2);
  check(strcasecmp("A", "a") == 0, 3);
  check(strcasecmp("a", "b") < 0, 4);
  check(strcasecmp("c", "b") > 0, 5);
  check(strcasecmp("abc", "AbC") == 0, 6);
  check(strcasecmp("0123456789", "0123456789") == 0, 7);
  check(strcasecmp("", "0123456789") < 0, 8);
  check(strcasecmp("AbC", "") > 0, 9);
  check(strcasecmp("AbC", "A") > 0, 10);
  check(strcasecmp("AbC", "Ab") > 0, 11);
  check(strcasecmp("AbC", "ab") > 0, 12);
}

static void
test_strncasecmp (void)
{
  it = "strncasecmp";
  
  check(strncasecmp("a", "a", 5) == 0, 1);
  check(strncasecmp("a", "A", 5) == 0, 2);
  check(strncasecmp("A", "a", 5) == 0, 3);
  check(strncasecmp("a", "b", 5) < 0, 4);
  check(strncasecmp("c", "b", 5) > 0, 5);
  check(strncasecmp("abc", "AbC", 5) == 0, 6);
  check(strncasecmp("0123456789", "0123456789", 10) == 0, 7);
  check(strncasecmp("", "0123456789", 10) < 0, 8);
  check(strncasecmp("AbC", "", 5) > 0, 9);
  check(strncasecmp("AbC", "A", 5) > 0, 10);
  check(strncasecmp("AbC", "Ab", 5) > 0, 11);
  check(strncasecmp("AbC", "ab", 5) > 0, 12);
  check(strncasecmp("0123456789", "AbC", 0) == 0, 13);
  check(strncasecmp("AbC", "abc", 1) == 0, 14);
  check(strncasecmp("AbC", "abc", 2) == 0, 15);
  check(strncasecmp("AbC", "abc", 3) == 0, 16);
  check(strncasecmp("AbC", "abcd", 3) == 0, 17);
  check(strncasecmp("AbC", "abcd", 4) < 0, 18);
  check(strncasecmp("ADC", "abcd", 1) == 0, 19);
  check(strncasecmp("ADC", "abcd", 2) > 0, 20);
}

static void
test_strcasestr (void)
{
  it = "strcasestr";
  check(strcasestr("abCd", "z") == NULL, 1);	
  check(strcasestr("AbcD", "abX") == NULL, 2);	
  (void) strcpy(one, "abCd");
  check(strcasestr(one, "c") == one+2, 3);	
  check(strcasestr(one, "Bc") == one+1, 4);	
  check(strcasestr(one, "d") == one+3, 5);	
  check(strcasestr(one, "Cd") == one+2, 6);	
  check(strcasestr(one, "aBc") == one, 7);	
  check(strcasestr(one, "aBcd") == one, 8);	
  check(strcasestr(one, "AbcDe") == NULL, 9);	
  check(strcasestr(one, "dE") == NULL, 10);	
  check(strcasestr(one, "") == one, 11);	
  (void) strcpy(one, "abAba");
  check(strcasestr(one, "Ba") == one+1, 12);	
  (void) strcpy(one, "");
  check(strcasestr(one, "b") == NULL, 13);	
  check(strcasestr(one, "") == one, 14);	
  (void) strcpy(one, "BcbCa");
  check(strcasestr(one, "bCa") == one+2, 15);	
  (void) strcpy(one, "bbBcaBbcA");
  check(strcasestr(one, "bbCa") == one+1, 16);	
}

int
main (void)
{
  int status;

  
  test_strcmp ();

  
  test_strcpy ();

  
  test_stpcpy ();

#if !defined(__APPLE__)
  
  test_stpncpy ();
#endif

  
  test_strcat ();

  
  test_strncat ();

  
  test_strncmp ();

  
  test_strncpy ();

  
  test_strlen ();

  
  test_strchr ();

# if !defined(__APPLE__)
  
  test_strchrnul ();
# endif

# if !defined(__APPLE__)
  
  test_rawmemchr ();
# endif

  
  test_index ();

  
  test_strrchr ();

# if !defined(__APPLE__)
  
  test_memrchr ();
# endif

  
  test_rindex ();

  
  test_strpbrk ();

  
  test_strstr ();

  
  test_strspn ();

  
  test_strcspn ();

  
  test_strtok ();

  
  test_strtok_r ();

  
  test_strsep ();

  
  test_memcmp ();

  
  test_memchr ();

  
  test_memcpy ();

  
  test_memmove ();

# if !defined(__APPLE__)
  
  test_mempcpy ();
# endif

  
  test_memccpy ();

  
  test_memset ();

  
  test_bcopy ();

  
  test_bzero ();

  
  test_bcmp ();

#if !defined(__APPLE__)
  
  test_strndup ();
#endif

  
  test_strerror ();

  
  test_strcasecmp ();

  
  test_strncasecmp ();

  test_strcasestr ();

  if (errors == 0)
    {
      status = EXIT_SUCCESS;
      
    }
  else
    {
      status = EXIT_FAILURE;
      printf("%d errors.\n", (int)errors);
    }

  return status;
}
