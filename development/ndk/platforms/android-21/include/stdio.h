
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	@(#)stdio.h	5.17 (Berkeley) 6/3/91
 */

#ifndef	_STDIO_H_
#define	_STDIO_H_


#include <sys/cdefs.h>
#include <sys/types.h>

#include <stdarg.h>
#include <stddef.h>

#define __need_NULL
#include <stddef.h>

#define	_FSTDIO			

typedef off_t fpos_t;		


#if defined(__LP64__)
struct __sbuf {
  unsigned char* _base;
  size_t _size;
};
#else
struct __sbuf {
	unsigned char *_base;
	int	_size;
};
#endif

typedef	struct __sFILE {
	unsigned char *_p;	
	int	_r;		
	int	_w;		
#if defined(__LP64__)
	int	_flags;		
	int	_file;		
#else
	short	_flags;		
	short	_file;		
#endif
	struct	__sbuf _bf;	
	int	_lbfsize;	

	
	void	*_cookie;	
	int	(*_close)(void *);
	int	(*_read)(void *, char *, int);
	fpos_t	(*_seek)(void *, fpos_t, int);
	int	(*_write)(void *, const char *, int);

	
	struct	__sbuf _ext;
	
	unsigned char *_up;	
	int	_ur;		

	
	unsigned char _ubuf[3];	
	unsigned char _nbuf[1];	

	
	struct	__sbuf _lb;	

	
	int	_blksize;	
	fpos_t	_offset;	
} FILE;

__BEGIN_DECLS
extern FILE __sF[];
__END_DECLS

#define	__SLBF	0x0001		
#define	__SNBF	0x0002		
#define	__SRD	0x0004		
#define	__SWR	0x0008		
	
#define	__SRW	0x0010		
#define	__SEOF	0x0020		
#define	__SERR	0x0040		
#define	__SMBF	0x0080		
#define	__SAPP	0x0100		
#define	__SSTR	0x0200		
#define	__SOPT	0x0400		
#define	__SNPT	0x0800		
#define	__SOFF	0x1000		
#define	__SMOD	0x2000		
#define	__SALC	0x4000		
#define	__SIGN	0x8000		

#define	_IOFBF	0		
#define	_IOLBF	1		
#define	_IONBF	2		

#define	BUFSIZ	1024		
#define	EOF	(-1)


#define	FOPEN_MAX	20	
#define	FILENAME_MAX	1024	

#if __BSD_VISIBLE || __XPG_VISIBLE
#define	P_tmpdir	"/tmp/"
#endif
#define	L_tmpnam	1024	
#define	TMP_MAX		308915776

#ifndef SEEK_SET
#define	SEEK_SET	0	
#endif
#ifndef SEEK_CUR
#define	SEEK_CUR	1	
#endif
#ifndef SEEK_END
#define	SEEK_END	2	
#endif

#define	stdin	(&__sF[0])
#define	stdout	(&__sF[1])
#define	stderr	(&__sF[2])

__BEGIN_DECLS
void	 clearerr(FILE *);
int	 fclose(FILE *);
int	 feof(FILE *);
int	 ferror(FILE *);
int	 fflush(FILE *);
int	 fgetc(FILE *);
char	*fgets(char * __restrict, int, FILE * __restrict);
FILE	*fopen(const char * __restrict , const char * __restrict);
int	 fprintf(FILE * __restrict , const char * __restrict, ...)
		__printflike(2, 3);
int	 fputc(int, FILE *);
int	 fputs(const char * __restrict, FILE * __restrict);
size_t	 fread(void * __restrict, size_t, size_t, FILE * __restrict);
FILE	*freopen(const char * __restrict, const char * __restrict,
	    FILE * __restrict);
int	 fscanf(FILE * __restrict, const char * __restrict, ...)
		__scanflike(2, 3);
int	 fseek(FILE *, long, int);
long	 ftell(FILE *);
size_t	 fwrite(const void * __restrict, size_t, size_t, FILE * __restrict);
int	 getc(FILE *);
int	 getchar(void);
ssize_t	 getdelim(char ** __restrict, size_t * __restrict, int,
	    FILE * __restrict);
ssize_t	 getline(char ** __restrict, size_t * __restrict, FILE * __restrict);

void	 perror(const char *);
int	 printf(const char * __restrict, ...)
		__printflike(1, 2);
int	 putc(int, FILE *);
int	 putchar(int);
int	 puts(const char *);
int	 remove(const char *);
void	 rewind(FILE *);
int	 scanf(const char * __restrict, ...)
		__scanflike(1, 2);
void	 setbuf(FILE * __restrict, char * __restrict);
int	 setvbuf(FILE * __restrict, char * __restrict, int, size_t);
int	 sscanf(const char * __restrict, const char * __restrict, ...)
		__scanflike(2, 3);
FILE	*tmpfile(void);
int	 ungetc(int, FILE *);
int	 vfprintf(FILE * __restrict, const char * __restrict, __va_list)
		__printflike(2, 0);
int	 vprintf(const char * __restrict, __va_list)
		__printflike(1, 0);

int dprintf(int, const char * __restrict, ...) __printflike(2, 3);
int vdprintf(int, const char * __restrict, __va_list) __printflike(2, 0);

#ifndef __AUDIT__
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
char* gets(char*) __warnattr("gets is very unsafe; consider using fgets");
#endif
int sprintf(char* __restrict, const char* __restrict, ...)
    __printflike(2, 3); 
char* tmpnam(char*) __warnattr("tmpnam possibly used unsafely; consider using mkstemp");
int vsprintf(char* __restrict, const char* __restrict, __va_list)
    __printflike(2, 0); 
#if __XPG_VISIBLE
char* tempnam(const char*, const char*)
    __warnattr("tempnam possibly used unsafely; consider using mkstemp");
#endif
#endif

extern int rename(const char*, const char*);
extern int renameat(int, const char*, int, const char*);

int	 fgetpos(FILE * __restrict, fpos_t * __restrict);
int	 fsetpos(FILE *, const fpos_t *);

int	 fseeko(FILE *, off_t, int);
off_t	 ftello(FILE *);

#if __ISO_C_VISIBLE >= 1999 || __BSD_VISIBLE
int	 snprintf(char * __restrict, size_t, const char * __restrict, ...)
		__printflike(3, 4);
int	 vfscanf(FILE * __restrict, const char * __restrict, __va_list)
		__scanflike(2, 0);
int	 vscanf(const char *, __va_list)
		__scanflike(1, 0);
int	 vsnprintf(char * __restrict, size_t, const char * __restrict, __va_list)
		__printflike(3, 0);
int	 vsscanf(const char * __restrict, const char * __restrict, __va_list)
		__scanflike(2, 0);
#endif 

__END_DECLS


#if __BSD_VISIBLE || __POSIX_VISIBLE || __XPG_VISIBLE
#define	L_ctermid	1024	

__BEGIN_DECLS
FILE	*fdopen(int, const char *);
int	 fileno(FILE *);

#if (__POSIX_VISIBLE >= 199209)
int	 pclose(FILE *);
FILE	*popen(const char *, const char *);
#endif

#if __POSIX_VISIBLE >= 199506
void	 flockfile(FILE *);
int	 ftrylockfile(FILE *);
void	 funlockfile(FILE *);

int	 getc_unlocked(FILE *);
int	 getchar_unlocked(void);
int	 putc_unlocked(int, FILE *);
int	 putchar_unlocked(int);
#endif 

__END_DECLS

#endif 

#if __BSD_VISIBLE
__BEGIN_DECLS
int	 asprintf(char ** __restrict, const char * __restrict, ...)
		__printflike(2, 3);
char	*fgetln(FILE * __restrict, size_t * __restrict);
int	 fpurge(FILE *);
void	 setbuffer(FILE *, char *, int);
int	 setlinebuf(FILE *);
int	 vasprintf(char ** __restrict, const char * __restrict,
    __va_list)
		__printflike(2, 0);
__END_DECLS

__BEGIN_DECLS
FILE	*funopen(const void *,
		int (*)(void *, char *, int),
		int (*)(void *, const char *, int),
		fpos_t (*)(void *, fpos_t, int),
		int (*)(void *));
__END_DECLS
#define	fropen(cookie, fn) funopen(cookie, fn, 0, 0, 0)
#define	fwopen(cookie, fn) funopen(cookie, 0, fn, 0, 0)
#endif 

#if defined(__BIONIC_FORTIFY)

__BEGIN_DECLS

__BIONIC_FORTIFY_INLINE
__printflike(3, 0)
int vsnprintf(char *dest, size_t size, const char *format, __va_list ap)
{
    return __builtin___vsnprintf_chk(dest, size, 0, __bos(dest), format, ap);
}

__BIONIC_FORTIFY_INLINE
__printflike(2, 0)
int vsprintf(char *dest, const char *format, __va_list ap)
{
    return __builtin___vsprintf_chk(dest, 0, __bos(dest), format, ap);
}

#if defined(__clang__)
  #if !defined(snprintf)
    #define __wrap_snprintf(dest, size, ...) __builtin___snprintf_chk(dest, size, 0, __bos(dest), __VA_ARGS__)
    #define snprintf(...) __wrap_snprintf(__VA_ARGS__)
  #endif
#else
__BIONIC_FORTIFY_INLINE
__printflike(3, 4)
int snprintf(char *dest, size_t size, const char *format, ...)
{
    return __builtin___snprintf_chk(dest, size, 0,
        __bos(dest), format, __builtin_va_arg_pack());
}
#endif

#if defined(__clang__)
  #if !defined(sprintf)
    #define __wrap_sprintf(dest, ...) __builtin___sprintf_chk(dest, 0, __bos(dest), __VA_ARGS__)
    #define sprintf(...) __wrap_sprintf(__VA_ARGS__)
  #endif
#else
__BIONIC_FORTIFY_INLINE
__printflike(2, 3)
int sprintf(char *dest, const char *format, ...)
{
    return __builtin___sprintf_chk(dest, 0,
        __bos(dest), format, __builtin_va_arg_pack());
}
#endif

extern char* __fgets_chk(char*, int, FILE*, size_t);
extern char* __fgets_real(char*, int, FILE*) __asm__(__USER_LABEL_PREFIX__ "fgets");
__errordecl(__fgets_too_big_error, "fgets called with size bigger than buffer");
__errordecl(__fgets_too_small_error, "fgets called with size less than zero");

#if !defined(__clang__)

__BIONIC_FORTIFY_INLINE
char *fgets(char* dest, int size, FILE* stream) {
    size_t bos = __bos(dest);

    
    
    if (__builtin_constant_p(size) && (size < 0)) {
        __fgets_too_small_error();
    }

    
    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __fgets_real(dest, size, stream);
    }

    
    
    if (__builtin_constant_p(size) && (size <= (int) bos)) {
        return __fgets_real(dest, size, stream);
    }

    
    
    if (__builtin_constant_p(size) && (size > (int) bos)) {
        __fgets_too_big_error();
    }

    return __fgets_chk(dest, size, stream, bos);
}

#endif 

__END_DECLS

#endif 

#endif 
