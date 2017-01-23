
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

#define __need___va_list
#include <stdarg.h>

#define __need_size_t
#include <stddef.h>

#define __need_NULL
#include <stddef.h>

#define	_FSTDIO			

typedef off_t fpos_t;		


struct __sbuf {
	unsigned char *_base;
	int	_size;
};

typedef	struct __sFILE {
	unsigned char *_p;	
	int	_r;		
	int	_w;		
	short	_flags;		
	short	_file;		
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
int	 fgetpos(FILE *, fpos_t *);
char	*fgets(char *, int, FILE *);
FILE	*fopen(const char *, const char *);
int	 fprintf(FILE *, const char *, ...);
int	 fputc(int, FILE *);
int	 fputs(const char *, FILE *);
size_t	 fread(void *, size_t, size_t, FILE *);
FILE	*freopen(const char *, const char *, FILE *);
int	 fscanf(FILE *, const char *, ...);
int	 fseek(FILE *, long, int);
int	 fseeko(FILE *, off_t, int);
int	 fsetpos(FILE *, const fpos_t *);
long	 ftell(FILE *);
off_t	 ftello(FILE *);
size_t	 fwrite(const void *, size_t, size_t, FILE *);
int	 getc(FILE *);
int	 getchar(void);
char	*gets(char *);
#if __BSD_VISIBLE && !defined(__SYS_ERRLIST)
#define __SYS_ERRLIST

extern int sys_nerr;			
extern char *sys_errlist[];
#endif
void	 perror(const char *);
int	 printf(const char *, ...);
int	 putc(int, FILE *);
int	 putchar(int);
int	 puts(const char *);
int	 remove(const char *);
int	 rename(const char *, const char *);
void	 rewind(FILE *);
int	 scanf(const char *, ...);
void	 setbuf(FILE *, char *);
int	 setvbuf(FILE *, char *, int, size_t);
int	 sprintf(char *, const char *, ...);
int	 sscanf(const char *, const char *, ...);
FILE	*tmpfile(void);
char	*tmpnam(char *);
int	 ungetc(int, FILE *);
int	 vfprintf(FILE *, const char *, __va_list);
int	 vprintf(const char *, __va_list);
int	 vsprintf(char *, const char *, __va_list);

#if __ISO_C_VISIBLE >= 1999 || __BSD_VISIBLE
int	 snprintf(char *, size_t, const char *, ...)
		__attribute__((__format__ (printf, 3, 4)))
		__attribute__((__nonnull__ (3)));
int	 vfscanf(FILE *, const char *, __va_list)
		__attribute__((__format__ (scanf, 2, 0)))
		__attribute__((__nonnull__ (2)));
int	 vscanf(const char *, __va_list)
		__attribute__((__format__ (scanf, 1, 0)))
		__attribute__((__nonnull__ (1)));
int	 vsnprintf(char *, size_t, const char *, __va_list)
		__attribute__((__format__ (printf, 3, 0)))
		__attribute__((__nonnull__ (3)));
int	 vsscanf(const char *, const char *, __va_list)
		__attribute__((__format__ (scanf, 2, 0)))
		__attribute__((__nonnull__ (2)));
#endif 

__END_DECLS


#if __BSD_VISIBLE || __POSIX_VISIBLE || __XPG_VISIBLE
#define	L_ctermid	1024	
#define L_cuserid	9	

__BEGIN_DECLS
#if 0 
char	*ctermid(char *);
char	*cuserid(char *);
#endif 
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

#if __XPG_VISIBLE
char	*tempnam(const char *, const char *);
#endif
__END_DECLS

#endif 

#if __BSD_VISIBLE
__BEGIN_DECLS
int	 asprintf(char **, const char *, ...)
		__attribute__((__format__ (printf, 2, 3)))
		__attribute__((__nonnull__ (2)));
char	*fgetln(FILE *, size_t *);
int	 fpurge(FILE *);
int	 getw(FILE *);
int	 putw(int, FILE *);
void	 setbuffer(FILE *, char *, int);
int	 setlinebuf(FILE *);
int	 vasprintf(char **, const char *, __va_list)
		__attribute__((__format__ (printf, 2, 0)))
		__attribute__((__nonnull__ (2)));
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


#define	__sfeof(p)	(((p)->_flags & __SEOF) != 0)
#define	__sferror(p)	(((p)->_flags & __SERR) != 0)
#define	__sclearerr(p)	((void)((p)->_flags &= ~(__SERR|__SEOF)))
#define	__sfileno(p)	((p)->_file)

#define	feof(p)		__sfeof(p)
#define	ferror(p)	__sferror(p)

#ifndef _POSIX_THREADS
#define	clearerr(p)	__sclearerr(p)
#endif

#if __POSIX_VISIBLE
#define	fileno(p)	__sfileno(p)
#endif

#define	getchar()	getc(stdin)
#define	putchar(x)	putc(x, stdout)
#define getchar_unlocked()	getc_unlocked(stdin)
#define putchar_unlocked(c)	putc_unlocked(c, stdout)

#ifdef _GNU_SOURCE
int fdprintf(int, const char*, ...);
int vfdprintf(int, const char*, __va_list);
#endif 

#endif 
