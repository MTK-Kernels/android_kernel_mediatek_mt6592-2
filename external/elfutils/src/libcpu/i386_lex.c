
#line 3 "i386_lex.c"

#define  YY_INT_ALIGNED short int


#define yy_create_buffer i386__create_buffer
#define yy_delete_buffer i386__delete_buffer
#define yy_flex_debug i386__flex_debug
#define yy_init_buffer i386__init_buffer
#define yy_flush_buffer i386__flush_buffer
#define yy_load_buffer_state i386__load_buffer_state
#define yy_switch_to_buffer i386__switch_to_buffer
#define yyin i386_in
#define yyleng i386_leng
#define yylex i386_lex
#define yylineno i386_lineno
#define yyout i386_out
#define yyrestart i386_restart
#define yytext i386_text
#define yywrap i386_wrap
#define yyalloc i386_alloc
#define yyrealloc i386_realloc
#define yyfree i386_free

#define FLEX_SCANNER
#define YY_FLEX_MAJOR_VERSION 2
#define YY_FLEX_MINOR_VERSION 5
#define YY_FLEX_SUBMINOR_VERSION 37
#if YY_FLEX_SUBMINOR_VERSION > 0
#define FLEX_BETA
#endif


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>



#ifndef FLEXINT_H
#define FLEXINT_H


#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include <inttypes.h>
typedef int8_t flex_int8_t;
typedef uint8_t flex_uint8_t;
typedef int16_t flex_int16_t;
typedef uint16_t flex_uint16_t;
typedef int32_t flex_int32_t;
typedef uint32_t flex_uint32_t;
#else
typedef signed char flex_int8_t;
typedef short int flex_int16_t;
typedef int flex_int32_t;
typedef unsigned char flex_uint8_t; 
typedef unsigned short int flex_uint16_t;
typedef unsigned int flex_uint32_t;

#ifndef INT8_MIN
#define INT8_MIN               (-128)
#endif
#ifndef INT16_MIN
#define INT16_MIN              (-32767-1)
#endif
#ifndef INT32_MIN
#define INT32_MIN              (-2147483647-1)
#endif
#ifndef INT8_MAX
#define INT8_MAX               (127)
#endif
#ifndef INT16_MAX
#define INT16_MAX              (32767)
#endif
#ifndef INT32_MAX
#define INT32_MAX              (2147483647)
#endif
#ifndef UINT8_MAX
#define UINT8_MAX              (255U)
#endif
#ifndef UINT16_MAX
#define UINT16_MAX             (65535U)
#endif
#ifndef UINT32_MAX
#define UINT32_MAX             (4294967295U)
#endif

#endif 

#endif 

#ifdef __cplusplus

#define YY_USE_CONST

#else	

#if defined (__STDC__)

#define YY_USE_CONST

#endif	
#endif	

#ifdef YY_USE_CONST
#define yyconst const
#else
#define yyconst
#endif

#define YY_NULL 0

#define YY_SC_TO_UI(c) ((unsigned int) (unsigned char) c)

#define BEGIN (yy_start) = 1 + 2 *

#define YY_START (((yy_start) - 1) / 2)
#define YYSTATE YY_START

#define YY_STATE_EOF(state) (YY_END_OF_BUFFER + state + 1)

#define YY_NEW_FILE i386_restart(i386_in  )

#define YY_END_OF_BUFFER_CHAR 0

#ifndef YY_BUF_SIZE
#define YY_BUF_SIZE 16384
#endif

#define YY_STATE_BUF_SIZE   ((YY_BUF_SIZE + 2) * sizeof(yy_state_type))

#ifndef YY_TYPEDEF_YY_BUFFER_STATE
#define YY_TYPEDEF_YY_BUFFER_STATE
typedef struct yy_buffer_state *YY_BUFFER_STATE;
#endif

#ifndef YY_TYPEDEF_YY_SIZE_T
#define YY_TYPEDEF_YY_SIZE_T
typedef size_t yy_size_t;
#endif

extern yy_size_t i386_leng;

extern FILE *i386_in, *i386_out;

#define EOB_ACT_CONTINUE_SCAN 0
#define EOB_ACT_END_OF_FILE 1
#define EOB_ACT_LAST_MATCH 2

    #define  YY_LESS_LINENO(n) \
            do { \
                int yyl;\
                for ( yyl = n; yyl < i386_leng; ++yyl )\
                    if ( i386_text[yyl] == '\n' )\
                        --i386_lineno;\
            }while(0)
    
#define yyless(n) \
	do \
		{ \
		 \
        int yyless_macro_arg = (n); \
        YY_LESS_LINENO(yyless_macro_arg);\
		*yy_cp = (yy_hold_char); \
		YY_RESTORE_YY_MORE_OFFSET \
		(yy_c_buf_p) = yy_cp = yy_bp + yyless_macro_arg - YY_MORE_ADJ; \
		YY_DO_BEFORE_ACTION;  \
		} \
	while ( 0 )

#define unput(c) yyunput( c, (yytext_ptr)  )

#ifndef YY_STRUCT_YY_BUFFER_STATE
#define YY_STRUCT_YY_BUFFER_STATE
struct yy_buffer_state
	{
	FILE *yy_input_file;

	char *yy_ch_buf;		
	char *yy_buf_pos;		

	yy_size_t yy_buf_size;

	yy_size_t yy_n_chars;

	int yy_is_our_buffer;

	int yy_is_interactive;

	int yy_at_bol;

    int yy_bs_lineno; 
    int yy_bs_column; 
    
	int yy_fill_buffer;

	int yy_buffer_status;

#define YY_BUFFER_NEW 0
#define YY_BUFFER_NORMAL 1
#define YY_BUFFER_EOF_PENDING 2

	};
#endif 

static size_t yy_buffer_stack_top = 0; 
static size_t yy_buffer_stack_max = 0; 
static YY_BUFFER_STATE * yy_buffer_stack = 0; 

#define YY_CURRENT_BUFFER ( (yy_buffer_stack) \
                          ? (yy_buffer_stack)[(yy_buffer_stack_top)] \
                          : NULL)

#define YY_CURRENT_BUFFER_LVALUE (yy_buffer_stack)[(yy_buffer_stack_top)]

static char yy_hold_char;
static yy_size_t yy_n_chars;		
yy_size_t i386_leng;

static char *yy_c_buf_p = (char *) 0;
static int yy_init = 0;		
static int yy_start = 0;	

static int yy_did_buffer_switch_on_eof;

void i386_restart (FILE *input_file  );
void i386__switch_to_buffer (YY_BUFFER_STATE new_buffer  );
YY_BUFFER_STATE i386__create_buffer (FILE *file,int size  );
void i386__delete_buffer (YY_BUFFER_STATE b  );
void i386__flush_buffer (YY_BUFFER_STATE b  );
void i386_push_buffer_state (YY_BUFFER_STATE new_buffer  );
void i386_pop_buffer_state (void );

static void i386_ensure_buffer_stack (void );
static void i386__load_buffer_state (void );
static void i386__init_buffer (YY_BUFFER_STATE b,FILE *file  );

#define YY_FLUSH_BUFFER i386__flush_buffer(YY_CURRENT_BUFFER )

YY_BUFFER_STATE i386__scan_buffer (char *base,yy_size_t size  );
YY_BUFFER_STATE i386__scan_string (yyconst char *yy_str  );
YY_BUFFER_STATE i386__scan_bytes (yyconst char *bytes,yy_size_t len  );

void *i386_alloc (yy_size_t  );
void *i386_realloc (void *,yy_size_t  );
void i386_free (void *  );

#define yy_new_buffer i386__create_buffer

#define yy_set_interactive(is_interactive) \
	{ \
	if ( ! YY_CURRENT_BUFFER ){ \
        i386_ensure_buffer_stack (); \
		YY_CURRENT_BUFFER_LVALUE =    \
            i386__create_buffer(i386_in,YY_BUF_SIZE ); \
	} \
	YY_CURRENT_BUFFER_LVALUE->yy_is_interactive = is_interactive; \
	}

#define yy_set_bol(at_bol) \
	{ \
	if ( ! YY_CURRENT_BUFFER ){\
        i386_ensure_buffer_stack (); \
		YY_CURRENT_BUFFER_LVALUE =    \
            i386__create_buffer(i386_in,YY_BUF_SIZE ); \
	} \
	YY_CURRENT_BUFFER_LVALUE->yy_at_bol = at_bol; \
	}

#define YY_AT_BOL() (YY_CURRENT_BUFFER_LVALUE->yy_at_bol)


#define i386_wrap() 1
#define YY_SKIP_YYWRAP

typedef unsigned char YY_CHAR;

FILE *i386_in = (FILE *) 0, *i386_out = (FILE *) 0;

typedef int yy_state_type;

extern int i386_lineno;

int i386_lineno = 1;

extern char *i386_text;
#define yytext_ptr i386_text

static yy_state_type yy_get_previous_state (void );
static yy_state_type yy_try_NUL_trans (yy_state_type current_state  );
static int yy_get_next_buffer (void );
static void yy_fatal_error (yyconst char msg[]  );

#define YY_DO_BEFORE_ACTION \
	(yytext_ptr) = yy_bp; \
	i386_leng = (size_t) (yy_cp - yy_bp); \
	(yy_hold_char) = *yy_cp; \
	*yy_cp = '\0'; \
	(yy_c_buf_p) = yy_cp;

#define YY_NUM_RULES 21
#define YY_END_OF_BUFFER 22
struct yy_trans_info
	{
	flex_int32_t yy_verify;
	flex_int32_t yy_nxt;
	};
static yyconst flex_int16_t yy_accept[62] =
    {   0,
        0,    0,    0,    0,   22,   20,   17,   15,   20,    5,
       20,   14,   16,   19,   18,   15,   12,    7,    8,   13,
       11,   11,   19,   14,   16,   17,    6,    0,    0,    0,
        5,    0,    9,   18,   11,   11,    0,    0,    0,    0,
       11,    0,    0,    0,    0,   11,    1,    0,    0,    0,
       11,    0,    0,    0,   11,    2,    3,    0,   10,    4,
        0
    } ;

static yyconst flex_int32_t yy_ec[256] =
    {   0,
        1,    1,    1,    1,    1,    1,    1,    1,    2,    3,
        2,    2,    2,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    2,    1,    1,    4,    1,    5,    1,    1,    1,
        1,    1,    1,    6,    1,    1,    7,    8,    9,   10,
       10,   10,   10,   10,   10,   10,   10,   11,    1,    1,
        1,    1,    1,    1,   12,   13,   13,   14,   13,   13,
       13,   13,   15,   13,   13,   16,   13,   17,   13,   13,
       13,   13,   13,   13,   13,   18,   13,   13,   13,   13,
        1,    1,    1,    1,   13,    1,   19,   13,   13,   13,

       20,   21,   13,   13,   22,   13,   23,   13,   24,   25,
       26,   27,   13,   28,   29,   13,   30,   13,   13,   31,
       32,   13,   33,    1,   34,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,

        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1
    } ;

static yyconst flex_int32_t yy_meta[35] =
    {   0,
        1,    1,    1,    1,    1,    1,    2,    2,    2,    2,
        3,    2,    2,    2,    2,    2,    2,    2,    2,    2,
        2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
        2,    2,    1,    3
    } ;

static yyconst flex_int16_t yy_base[65] =
    {   0,
        0,   32,   65,    3,  113,  114,    9,   11,   19,    7,
       78,   16,  114,  114,   18,   20,  114,  114,  114,  114,
        0,   94,   76,   23,  114,   25,  114,   90,   80,    0,
       41,   73,  114,   36,    0,   88,   76,   44,   42,   37,
       49,   37,   38,   37,   31,   40,  114,   33,   32,   28,
       37,   16,   14,   12,   17,  114,  114,    5,    0,  114,
      114,   99,  101,    2
    } ;

static yyconst flex_int16_t yy_def[65] =
    {   0,
       62,   62,   61,    3,   61,   61,   61,   61,   61,   61,
       63,   61,   61,   61,   61,   61,   61,   61,   61,   61,
       64,   64,   63,   61,   61,   61,   61,   61,   61,   61,
       61,   63,   61,   61,   64,   64,   61,   61,   61,   61,
       64,   61,   61,   61,   61,   64,   61,   61,   61,   61,
       64,   61,   61,   61,   64,   61,   61,   61,   64,   61,
        0,   61,   61,   61
    } ;

static yyconst flex_int16_t yy_nxt[149] =
    {   0,
       61,    7,    8,   35,    9,   24,   25,   10,   10,   10,
       26,   26,   26,   26,   31,   31,   31,   26,   26,   34,
       34,   34,   34,   27,   34,   34,   26,   26,   60,   39,
       59,   40,   11,    7,   12,   13,    9,   34,   34,   10,
       10,   10,   28,   58,   57,   29,   56,   30,   31,   31,
       31,   55,   54,   53,   52,   51,   50,   49,   48,   47,
       46,   45,   44,   43,   11,   14,   15,   16,   14,   14,
       17,   14,   18,   19,   14,   20,   21,   21,   21,   22,
       21,   21,   21,   21,   21,   21,   21,   21,   21,   21,
       21,   21,   21,   21,   21,   21,   21,   23,   14,    6,

        6,    6,   32,   32,   42,   41,   33,   38,   37,   33,
       36,   33,   61,    5,   61,   61,   61,   61,   61,   61,
       61,   61,   61,   61,   61,   61,   61,   61,   61,   61,
       61,   61,   61,   61,   61,   61,   61,   61,   61,   61,
       61,   61,   61,   61,   61,   61,   61,   61
    } ;

static yyconst flex_int16_t yy_chk[149] =
    {   0,
        0,    1,    1,   64,    1,    4,    4,    1,    1,    1,
        7,    7,    8,    8,   10,   10,   10,   12,   12,   15,
       15,   16,   16,    9,   24,   24,   26,   26,   58,   30,
       55,   30,    1,    2,    2,    2,    2,   34,   34,    2,
        2,    2,    9,   54,   53,    9,   52,    9,   31,   31,
       31,   51,   50,   49,   48,   46,   45,   44,   43,   42,
       41,   40,   39,   38,    2,    3,    3,    3,    3,    3,
        3,    3,    3,    3,    3,    3,    3,    3,    3,    3,
        3,    3,    3,    3,    3,    3,    3,    3,    3,    3,
        3,    3,    3,    3,    3,    3,    3,    3,    3,   62,

       62,   62,   63,   63,   37,   36,   32,   29,   28,   23,
       22,   11,    5,   61,   61,   61,   61,   61,   61,   61,
       61,   61,   61,   61,   61,   61,   61,   61,   61,   61,
       61,   61,   61,   61,   61,   61,   61,   61,   61,   61,
       61,   61,   61,   61,   61,   61,   61,   61
    } ;

static yyconst flex_int32_t yy_rule_can_match_eol[22] =
    {   0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 
    0, 0,     };

static yy_state_type yy_last_accepting_state;
static char *yy_last_accepting_cpos;

extern int i386__flex_debug;
int i386__flex_debug = 0;

#define REJECT reject_used_but_not_detected
#define yymore() yymore_used_but_not_detected
#define YY_MORE_ADJ 0
#define YY_RESTORE_YY_MORE_OFFSET
char *i386_text;
#line 1 "/home/mark/src/elfutils/libcpu/i386_lex.l"
#line 2 "/home/mark/src/elfutils/libcpu/i386_lex.l"
/* Copyright (C) 2004, 2005, 2007, 2008 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2004.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <ctype.h>
#include <error.h>
#include <libintl.h>

#include <system.h>
#include "i386_parse.h"


static void eat_to_eol (void);
static void invalid_char (int ch);

#line 585 "i386_lex.c"

#define INITIAL 0
#define MAIN 1

#ifndef YY_NO_UNISTD_H
#include <unistd.h>
#endif

#ifndef YY_EXTRA_TYPE
#define YY_EXTRA_TYPE void *
#endif

static int yy_init_globals (void );


int i386_lex_destroy (void );

int i386_get_debug (void );

void i386_set_debug (int debug_flag  );

YY_EXTRA_TYPE i386_get_extra (void );

void i386_set_extra (YY_EXTRA_TYPE user_defined  );

FILE *i386_get_in (void );

void i386_set_in  (FILE * in_str  );

FILE *i386_get_out (void );

void i386_set_out  (FILE * out_str  );

yy_size_t i386_get_leng (void );

char *i386_get_text (void );

int i386_get_lineno (void );

void i386_set_lineno (int line_number  );


#ifndef YY_SKIP_YYWRAP
#ifdef __cplusplus
extern "C" int i386_wrap (void );
#else
extern int i386_wrap (void );
#endif
#endif

    static void yyunput (int c,char *buf_ptr  );
    
#ifndef yytext_ptr
static void yy_flex_strncpy (char *,yyconst char *,int );
#endif

#ifdef YY_NEED_STRLEN
static int yy_flex_strlen (yyconst char * );
#endif

#ifndef YY_NO_INPUT

#ifdef __cplusplus
static int yyinput (void );
#else
static int input (void );
#endif

#endif

#ifndef YY_READ_BUF_SIZE
#define YY_READ_BUF_SIZE 8192
#endif

#ifndef ECHO
#define ECHO do { if (fwrite( i386_text, i386_leng, 1, i386_out )) {} } while (0)
#endif

#ifndef YY_INPUT
#define YY_INPUT(buf,result,max_size) \
	if ( YY_CURRENT_BUFFER_LVALUE->yy_is_interactive ) \
		{ \
		int c = '*'; \
		size_t n; \
		for ( n = 0; n < max_size && \
			     (c = getc( i386_in )) != EOF && c != '\n'; ++n ) \
			buf[n] = (char) c; \
		if ( c == '\n' ) \
			buf[n++] = (char) c; \
		if ( c == EOF && ferror( i386_in ) ) \
			YY_FATAL_ERROR( "input in flex scanner failed" ); \
		result = n; \
		} \
	else \
		{ \
		errno=0; \
		while ( (result = fread(buf, 1, max_size, i386_in))==0 && ferror(i386_in)) \
			{ \
			if( errno != EINTR) \
				{ \
				YY_FATAL_ERROR( "input in flex scanner failed" ); \
				break; \
				} \
			errno=0; \
			clearerr(i386_in); \
			} \
		}\
\

#endif

#ifndef yyterminate
#define yyterminate() return YY_NULL
#endif

#ifndef YY_START_STACK_INCR
#define YY_START_STACK_INCR 25
#endif

#ifndef YY_FATAL_ERROR
#define YY_FATAL_ERROR(msg) yy_fatal_error( msg )
#endif


#ifndef YY_DECL
#define YY_DECL_IS_OURS 1

extern int i386_lex (void);

#define YY_DECL int i386_lex (void)
#endif 

#ifndef YY_USER_ACTION
#define YY_USER_ACTION
#endif

#ifndef YY_BREAK
#define YY_BREAK break;
#endif

#define YY_RULE_SETUP \
	if ( i386_leng > 0 ) \
		YY_CURRENT_BUFFER_LVALUE->yy_at_bol = \
				(i386_text[i386_leng - 1] == '\n'); \
	YY_USER_ACTION

YY_DECL
{
	register yy_state_type yy_current_state;
	register char *yy_cp, *yy_bp;
	register int yy_act;
    
#line 57 "/home/mark/src/elfutils/libcpu/i386_lex.l"


#line 774 "i386_lex.c"

	if ( !(yy_init) )
		{
		(yy_init) = 1;

#ifdef YY_USER_INIT
		YY_USER_INIT;
#endif

		if ( ! (yy_start) )
			(yy_start) = 1;	

		if ( ! i386_in )
			i386_in = stdin;

		if ( ! i386_out )
			i386_out = stdout;

		if ( ! YY_CURRENT_BUFFER ) {
			i386_ensure_buffer_stack ();
			YY_CURRENT_BUFFER_LVALUE =
				i386__create_buffer(i386_in,YY_BUF_SIZE );
		}

		i386__load_buffer_state( );
		}

	while ( 1 )		
		{
		yy_cp = (yy_c_buf_p);

		
		*yy_cp = (yy_hold_char);

		yy_bp = yy_cp;

		yy_current_state = (yy_start);
		yy_current_state += YY_AT_BOL();
yy_match:
		do
			{
			register YY_CHAR yy_c = yy_ec[YY_SC_TO_UI(*yy_cp)];
			if ( yy_accept[yy_current_state] )
				{
				(yy_last_accepting_state) = yy_current_state;
				(yy_last_accepting_cpos) = yy_cp;
				}
			while ( yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state )
				{
				yy_current_state = (int) yy_def[yy_current_state];
				if ( yy_current_state >= 62 )
					yy_c = yy_meta[(unsigned int) yy_c];
				}
			yy_current_state = yy_nxt[yy_base[yy_current_state] + (unsigned int) yy_c];
			++yy_cp;
			}
		while ( yy_current_state != 61 );
		yy_cp = (yy_last_accepting_cpos);
		yy_current_state = (yy_last_accepting_state);

yy_find_action:
		yy_act = yy_accept[yy_current_state];

		YY_DO_BEFORE_ACTION;

		if ( yy_act != YY_END_OF_BUFFER && yy_rule_can_match_eol[yy_act] )
			{
			yy_size_t yyl;
			for ( yyl = 0; yyl < i386_leng; ++yyl )
				if ( i386_text[yyl] == '\n' )
					   
    i386_lineno++;
;
			}

do_action:	

		switch ( yy_act )
	{ 
			case 0: 
			
			*yy_cp = (yy_hold_char);
			yy_cp = (yy_last_accepting_cpos);
			yy_current_state = (yy_last_accepting_state);
			goto yy_find_action;

case 1:
YY_RULE_SETUP
#line 59 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ return kMASK; }
	YY_BREAK
case 2:
YY_RULE_SETUP
#line 61 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ return kPREFIX; }
	YY_BREAK
case 3:
YY_RULE_SETUP
#line 62 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ return kSUFFIX; }
	YY_BREAK
case 4:
YY_RULE_SETUP
#line 64 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ return kSYNONYM; }
	YY_BREAK
case 5:
YY_RULE_SETUP
#line 66 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ i386_lval.num = strtoul (i386_text, NULL, 10);
				  return kNUMBER; }
	YY_BREAK
case 6:
YY_RULE_SETUP
#line 69 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ BEGIN (MAIN); return kPERCPERC; }
	YY_BREAK
case 7:
YY_RULE_SETUP
#line 72 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ return '0'; }
	YY_BREAK
case 8:
YY_RULE_SETUP
#line 73 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ return '1'; }
	YY_BREAK
case 9:
YY_RULE_SETUP
#line 75 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ i386_lval.str = xstrndup (i386_text + 1,
							    i386_leng - 2);
				  return kBITFIELD; }
	YY_BREAK
case 10:
YY_RULE_SETUP
#line 79 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ i386_lval.str = (void *) -1l;
				  return kID; }
	YY_BREAK
case 11:
YY_RULE_SETUP
#line 82 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ i386_lval.str = xstrndup (i386_text, i386_leng);
				  return kID; }
	YY_BREAK
case 12:
YY_RULE_SETUP
#line 85 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ return ','; }
	YY_BREAK
case 13:
YY_RULE_SETUP
#line 87 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ return ':'; }
	YY_BREAK
case 14:
YY_RULE_SETUP
#line 89 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{  }
	YY_BREAK
case 15:
YY_RULE_SETUP
#line 91 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ return '\n'; }
	YY_BREAK
case 16:
YY_RULE_SETUP
#line 93 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ eat_to_eol (); }
	YY_BREAK
case 17:
YY_RULE_SETUP
#line 95 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{  }
	YY_BREAK
case 18:
YY_RULE_SETUP
#line 97 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ return kSPACE; }
	YY_BREAK
case 19:
YY_RULE_SETUP
#line 99 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ i386_lval.ch = *i386_text; return kCHAR; }
	YY_BREAK
case 20:
YY_RULE_SETUP
#line 101 "/home/mark/src/elfutils/libcpu/i386_lex.l"
{ invalid_char (*i386_text); }
	YY_BREAK
case 21:
YY_RULE_SETUP
#line 104 "/home/mark/src/elfutils/libcpu/i386_lex.l"
ECHO;
	YY_BREAK
#line 978 "i386_lex.c"
case YY_STATE_EOF(INITIAL):
case YY_STATE_EOF(MAIN):
	yyterminate();

	case YY_END_OF_BUFFER:
		{
		
		int yy_amount_of_matched_text = (int) (yy_cp - (yytext_ptr)) - 1;

		
		*yy_cp = (yy_hold_char);
		YY_RESTORE_YY_MORE_OFFSET

		if ( YY_CURRENT_BUFFER_LVALUE->yy_buffer_status == YY_BUFFER_NEW )
			{
			(yy_n_chars) = YY_CURRENT_BUFFER_LVALUE->yy_n_chars;
			YY_CURRENT_BUFFER_LVALUE->yy_input_file = i386_in;
			YY_CURRENT_BUFFER_LVALUE->yy_buffer_status = YY_BUFFER_NORMAL;
			}

		if ( (yy_c_buf_p) <= &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)] )
			{ 
			yy_state_type yy_next_state;

			(yy_c_buf_p) = (yytext_ptr) + yy_amount_of_matched_text;

			yy_current_state = yy_get_previous_state(  );


			yy_next_state = yy_try_NUL_trans( yy_current_state );

			yy_bp = (yytext_ptr) + YY_MORE_ADJ;

			if ( yy_next_state )
				{
				
				yy_cp = ++(yy_c_buf_p);
				yy_current_state = yy_next_state;
				goto yy_match;
				}

			else
				{
				yy_cp = (yy_last_accepting_cpos);
				yy_current_state = (yy_last_accepting_state);
				goto yy_find_action;
				}
			}

		else switch ( yy_get_next_buffer(  ) )
			{
			case EOB_ACT_END_OF_FILE:
				{
				(yy_did_buffer_switch_on_eof) = 0;

				if ( i386_wrap( ) )
					{
					(yy_c_buf_p) = (yytext_ptr) + YY_MORE_ADJ;

					yy_act = YY_STATE_EOF(YY_START);
					goto do_action;
					}

				else
					{
					if ( ! (yy_did_buffer_switch_on_eof) )
						YY_NEW_FILE;
					}
				break;
				}

			case EOB_ACT_CONTINUE_SCAN:
				(yy_c_buf_p) =
					(yytext_ptr) + yy_amount_of_matched_text;

				yy_current_state = yy_get_previous_state(  );

				yy_cp = (yy_c_buf_p);
				yy_bp = (yytext_ptr) + YY_MORE_ADJ;
				goto yy_match;

			case EOB_ACT_LAST_MATCH:
				(yy_c_buf_p) =
				&YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)];

				yy_current_state = yy_get_previous_state(  );

				yy_cp = (yy_c_buf_p);
				yy_bp = (yytext_ptr) + YY_MORE_ADJ;
				goto yy_find_action;
			}
		break;
		}

	default:
		YY_FATAL_ERROR(
			"fatal flex scanner internal error--no action found" );
	} 
		} 
} 

static int yy_get_next_buffer (void)
{
    	register char *dest = YY_CURRENT_BUFFER_LVALUE->yy_ch_buf;
	register char *source = (yytext_ptr);
	register int number_to_move, i;
	int ret_val;

	if ( (yy_c_buf_p) > &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars) + 1] )
		YY_FATAL_ERROR(
		"fatal flex scanner internal error--end of buffer missed" );

	if ( YY_CURRENT_BUFFER_LVALUE->yy_fill_buffer == 0 )
		{ 
		if ( (yy_c_buf_p) - (yytext_ptr) - YY_MORE_ADJ == 1 )
			{
			return EOB_ACT_END_OF_FILE;
			}

		else
			{
			return EOB_ACT_LAST_MATCH;
			}
		}

	

	
	number_to_move = (int) ((yy_c_buf_p) - (yytext_ptr)) - 1;

	for ( i = 0; i < number_to_move; ++i )
		*(dest++) = *(source++);

	if ( YY_CURRENT_BUFFER_LVALUE->yy_buffer_status == YY_BUFFER_EOF_PENDING )
		YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars) = 0;

	else
		{
			yy_size_t num_to_read =
			YY_CURRENT_BUFFER_LVALUE->yy_buf_size - number_to_move - 1;

		while ( num_to_read <= 0 )
			{ 

			
			YY_BUFFER_STATE b = YY_CURRENT_BUFFER_LVALUE;

			int yy_c_buf_p_offset =
				(int) ((yy_c_buf_p) - b->yy_ch_buf);

			if ( b->yy_is_our_buffer )
				{
				yy_size_t new_size = b->yy_buf_size * 2;

				if ( new_size <= 0 )
					b->yy_buf_size += b->yy_buf_size / 8;
				else
					b->yy_buf_size *= 2;

				b->yy_ch_buf = (char *)
					
					i386_realloc((void *) b->yy_ch_buf,b->yy_buf_size + 2  );
				}
			else
				
				b->yy_ch_buf = 0;

			if ( ! b->yy_ch_buf )
				YY_FATAL_ERROR(
				"fatal error - scanner input buffer overflow" );

			(yy_c_buf_p) = &b->yy_ch_buf[yy_c_buf_p_offset];

			num_to_read = YY_CURRENT_BUFFER_LVALUE->yy_buf_size -
						number_to_move - 1;

			}

		if ( num_to_read > YY_READ_BUF_SIZE )
			num_to_read = YY_READ_BUF_SIZE;

		
		YY_INPUT( (&YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[number_to_move]),
			(yy_n_chars), num_to_read );

		YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
		}

	if ( (yy_n_chars) == 0 )
		{
		if ( number_to_move == YY_MORE_ADJ )
			{
			ret_val = EOB_ACT_END_OF_FILE;
			i386_restart(i386_in  );
			}

		else
			{
			ret_val = EOB_ACT_LAST_MATCH;
			YY_CURRENT_BUFFER_LVALUE->yy_buffer_status =
				YY_BUFFER_EOF_PENDING;
			}
		}

	else
		ret_val = EOB_ACT_CONTINUE_SCAN;

	if ((yy_size_t) ((yy_n_chars) + number_to_move) > YY_CURRENT_BUFFER_LVALUE->yy_buf_size) {
		
		yy_size_t new_size = (yy_n_chars) + number_to_move + ((yy_n_chars) >> 1);
		YY_CURRENT_BUFFER_LVALUE->yy_ch_buf = (char *) i386_realloc((void *) YY_CURRENT_BUFFER_LVALUE->yy_ch_buf,new_size  );
		if ( ! YY_CURRENT_BUFFER_LVALUE->yy_ch_buf )
			YY_FATAL_ERROR( "out of dynamic memory in yy_get_next_buffer()" );
	}

	(yy_n_chars) += number_to_move;
	YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)] = YY_END_OF_BUFFER_CHAR;
	YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars) + 1] = YY_END_OF_BUFFER_CHAR;

	(yytext_ptr) = &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[0];

	return ret_val;
}


    static yy_state_type yy_get_previous_state (void)
{
	register yy_state_type yy_current_state;
	register char *yy_cp;
    
	yy_current_state = (yy_start);
	yy_current_state += YY_AT_BOL();

	for ( yy_cp = (yytext_ptr) + YY_MORE_ADJ; yy_cp < (yy_c_buf_p); ++yy_cp )
		{
		register YY_CHAR yy_c = (*yy_cp ? yy_ec[YY_SC_TO_UI(*yy_cp)] : 1);
		if ( yy_accept[yy_current_state] )
			{
			(yy_last_accepting_state) = yy_current_state;
			(yy_last_accepting_cpos) = yy_cp;
			}
		while ( yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state )
			{
			yy_current_state = (int) yy_def[yy_current_state];
			if ( yy_current_state >= 62 )
				yy_c = yy_meta[(unsigned int) yy_c];
			}
		yy_current_state = yy_nxt[yy_base[yy_current_state] + (unsigned int) yy_c];
		}

	return yy_current_state;
}

    static yy_state_type yy_try_NUL_trans  (yy_state_type yy_current_state )
{
	register int yy_is_jam;
    	register char *yy_cp = (yy_c_buf_p);

	register YY_CHAR yy_c = 1;
	if ( yy_accept[yy_current_state] )
		{
		(yy_last_accepting_state) = yy_current_state;
		(yy_last_accepting_cpos) = yy_cp;
		}
	while ( yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state )
		{
		yy_current_state = (int) yy_def[yy_current_state];
		if ( yy_current_state >= 62 )
			yy_c = yy_meta[(unsigned int) yy_c];
		}
	yy_current_state = yy_nxt[yy_base[yy_current_state] + (unsigned int) yy_c];
	yy_is_jam = (yy_current_state == 61);

		return yy_is_jam ? 0 : yy_current_state;
}

    static void yyunput (int c, register char * yy_bp )
{
	register char *yy_cp;
    
    yy_cp = (yy_c_buf_p);

	
	*yy_cp = (yy_hold_char);

	if ( yy_cp < YY_CURRENT_BUFFER_LVALUE->yy_ch_buf + 2 )
		{ 
		
		register yy_size_t number_to_move = (yy_n_chars) + 2;
		register char *dest = &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[
					YY_CURRENT_BUFFER_LVALUE->yy_buf_size + 2];
		register char *source =
				&YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[number_to_move];

		while ( source > YY_CURRENT_BUFFER_LVALUE->yy_ch_buf )
			*--dest = *--source;

		yy_cp += (int) (dest - source);
		yy_bp += (int) (dest - source);
		YY_CURRENT_BUFFER_LVALUE->yy_n_chars =
			(yy_n_chars) = YY_CURRENT_BUFFER_LVALUE->yy_buf_size;

		if ( yy_cp < YY_CURRENT_BUFFER_LVALUE->yy_ch_buf + 2 )
			YY_FATAL_ERROR( "flex scanner push-back overflow" );
		}

	*--yy_cp = (char) c;

    if ( c == '\n' ){
        --i386_lineno;
    }

	(yytext_ptr) = yy_bp;
	(yy_hold_char) = *yy_cp;
	(yy_c_buf_p) = yy_cp;
}

#ifndef YY_NO_INPUT
#ifdef __cplusplus
    static int yyinput (void)
#else
    static int input  (void)
#endif

{
	int c;
    
	*(yy_c_buf_p) = (yy_hold_char);

	if ( *(yy_c_buf_p) == YY_END_OF_BUFFER_CHAR )
		{
		if ( (yy_c_buf_p) < &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)] )
			
			*(yy_c_buf_p) = '\0';

		else
			{ 
			yy_size_t offset = (yy_c_buf_p) - (yytext_ptr);
			++(yy_c_buf_p);

			switch ( yy_get_next_buffer(  ) )
				{
				case EOB_ACT_LAST_MATCH:

					
					i386_restart(i386_in );

					

				case EOB_ACT_END_OF_FILE:
					{
					if ( i386_wrap( ) )
						return EOF;

					if ( ! (yy_did_buffer_switch_on_eof) )
						YY_NEW_FILE;
#ifdef __cplusplus
					return yyinput();
#else
					return input();
#endif
					}

				case EOB_ACT_CONTINUE_SCAN:
					(yy_c_buf_p) = (yytext_ptr) + offset;
					break;
				}
			}
		}

	c = *(unsigned char *) (yy_c_buf_p);	
	*(yy_c_buf_p) = '\0';	
	(yy_hold_char) = *++(yy_c_buf_p);

	YY_CURRENT_BUFFER_LVALUE->yy_at_bol = (c == '\n');
	if ( YY_CURRENT_BUFFER_LVALUE->yy_at_bol )
		   
    i386_lineno++;
;

	return c;
}
#endif	

    void i386_restart  (FILE * input_file )
{
    
	if ( ! YY_CURRENT_BUFFER ){
        i386_ensure_buffer_stack ();
		YY_CURRENT_BUFFER_LVALUE =
            i386__create_buffer(i386_in,YY_BUF_SIZE );
	}

	i386__init_buffer(YY_CURRENT_BUFFER,input_file );
	i386__load_buffer_state( );
}

    void i386__switch_to_buffer  (YY_BUFFER_STATE  new_buffer )
{
    
	i386_ensure_buffer_stack ();
	if ( YY_CURRENT_BUFFER == new_buffer )
		return;

	if ( YY_CURRENT_BUFFER )
		{
		
		*(yy_c_buf_p) = (yy_hold_char);
		YY_CURRENT_BUFFER_LVALUE->yy_buf_pos = (yy_c_buf_p);
		YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
		}

	YY_CURRENT_BUFFER_LVALUE = new_buffer;
	i386__load_buffer_state( );

	(yy_did_buffer_switch_on_eof) = 1;
}

static void i386__load_buffer_state  (void)
{
    	(yy_n_chars) = YY_CURRENT_BUFFER_LVALUE->yy_n_chars;
	(yytext_ptr) = (yy_c_buf_p) = YY_CURRENT_BUFFER_LVALUE->yy_buf_pos;
	i386_in = YY_CURRENT_BUFFER_LVALUE->yy_input_file;
	(yy_hold_char) = *(yy_c_buf_p);
}

    YY_BUFFER_STATE i386__create_buffer  (FILE * file, int  size )
{
	YY_BUFFER_STATE b;
    
	b = (YY_BUFFER_STATE) i386_alloc(sizeof( struct yy_buffer_state )  );
	if ( ! b )
		YY_FATAL_ERROR( "out of dynamic memory in i386__create_buffer()" );

	b->yy_buf_size = size;

	b->yy_ch_buf = (char *) i386_alloc(b->yy_buf_size + 2  );
	if ( ! b->yy_ch_buf )
		YY_FATAL_ERROR( "out of dynamic memory in i386__create_buffer()" );

	b->yy_is_our_buffer = 1;

	i386__init_buffer(b,file );

	return b;
}

    void i386__delete_buffer (YY_BUFFER_STATE  b )
{
    
	if ( ! b )
		return;

	if ( b == YY_CURRENT_BUFFER ) 
		YY_CURRENT_BUFFER_LVALUE = (YY_BUFFER_STATE) 0;

	if ( b->yy_is_our_buffer )
		i386_free((void *) b->yy_ch_buf  );

	i386_free((void *) b  );
}

    static void i386__init_buffer  (YY_BUFFER_STATE  b, FILE * file )

{
	int oerrno = errno;
    
	i386__flush_buffer(b );

	b->yy_input_file = file;
	b->yy_fill_buffer = 1;

    if (b != YY_CURRENT_BUFFER){
        b->yy_bs_lineno = 1;
        b->yy_bs_column = 0;
    }

        b->yy_is_interactive = 0;
    
	errno = oerrno;
}

    void i386__flush_buffer (YY_BUFFER_STATE  b )
{
    	if ( ! b )
		return;

	b->yy_n_chars = 0;

	b->yy_ch_buf[0] = YY_END_OF_BUFFER_CHAR;
	b->yy_ch_buf[1] = YY_END_OF_BUFFER_CHAR;

	b->yy_buf_pos = &b->yy_ch_buf[0];

	b->yy_at_bol = 1;
	b->yy_buffer_status = YY_BUFFER_NEW;

	if ( b == YY_CURRENT_BUFFER )
		i386__load_buffer_state( );
}

void i386_push_buffer_state (YY_BUFFER_STATE new_buffer )
{
    	if (new_buffer == NULL)
		return;

	i386_ensure_buffer_stack();

	
	if ( YY_CURRENT_BUFFER )
		{
		
		*(yy_c_buf_p) = (yy_hold_char);
		YY_CURRENT_BUFFER_LVALUE->yy_buf_pos = (yy_c_buf_p);
		YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
		}

	
	if (YY_CURRENT_BUFFER)
		(yy_buffer_stack_top)++;
	YY_CURRENT_BUFFER_LVALUE = new_buffer;

	
	i386__load_buffer_state( );
	(yy_did_buffer_switch_on_eof) = 1;
}

void i386_pop_buffer_state (void)
{
    	if (!YY_CURRENT_BUFFER)
		return;

	i386__delete_buffer(YY_CURRENT_BUFFER );
	YY_CURRENT_BUFFER_LVALUE = NULL;
	if ((yy_buffer_stack_top) > 0)
		--(yy_buffer_stack_top);

	if (YY_CURRENT_BUFFER) {
		i386__load_buffer_state( );
		(yy_did_buffer_switch_on_eof) = 1;
	}
}

static void i386_ensure_buffer_stack (void)
{
	yy_size_t num_to_alloc;
    
	if (!(yy_buffer_stack)) {

		num_to_alloc = 1;
		(yy_buffer_stack) = (struct yy_buffer_state**)i386_alloc
								(num_to_alloc * sizeof(struct yy_buffer_state*)
								);
		if ( ! (yy_buffer_stack) )
			YY_FATAL_ERROR( "out of dynamic memory in i386_ensure_buffer_stack()" );
								  
		memset((yy_buffer_stack), 0, num_to_alloc * sizeof(struct yy_buffer_state*));
				
		(yy_buffer_stack_max) = num_to_alloc;
		(yy_buffer_stack_top) = 0;
		return;
	}

	if ((yy_buffer_stack_top) >= ((yy_buffer_stack_max)) - 1){

		
		int grow_size = 8 ;

		num_to_alloc = (yy_buffer_stack_max) + grow_size;
		(yy_buffer_stack) = (struct yy_buffer_state**)i386_realloc
								((yy_buffer_stack),
								num_to_alloc * sizeof(struct yy_buffer_state*)
								);
		if ( ! (yy_buffer_stack) )
			YY_FATAL_ERROR( "out of dynamic memory in i386_ensure_buffer_stack()" );

		
		memset((yy_buffer_stack) + (yy_buffer_stack_max), 0, grow_size * sizeof(struct yy_buffer_state*));
		(yy_buffer_stack_max) = num_to_alloc;
	}
}

YY_BUFFER_STATE i386__scan_buffer  (char * base, yy_size_t  size )
{
	YY_BUFFER_STATE b;
    
	if ( size < 2 ||
	     base[size-2] != YY_END_OF_BUFFER_CHAR ||
	     base[size-1] != YY_END_OF_BUFFER_CHAR )
		
		return 0;

	b = (YY_BUFFER_STATE) i386_alloc(sizeof( struct yy_buffer_state )  );
	if ( ! b )
		YY_FATAL_ERROR( "out of dynamic memory in i386__scan_buffer()" );

	b->yy_buf_size = size - 2;	
	b->yy_buf_pos = b->yy_ch_buf = base;
	b->yy_is_our_buffer = 0;
	b->yy_input_file = 0;
	b->yy_n_chars = b->yy_buf_size;
	b->yy_is_interactive = 0;
	b->yy_at_bol = 1;
	b->yy_fill_buffer = 0;
	b->yy_buffer_status = YY_BUFFER_NEW;

	i386__switch_to_buffer(b  );

	return b;
}

YY_BUFFER_STATE i386__scan_string (yyconst char * yystr )
{
    
	return i386__scan_bytes(yystr,strlen(yystr) );
}

YY_BUFFER_STATE i386__scan_bytes  (yyconst char * yybytes, yy_size_t  _yybytes_len )
{
	YY_BUFFER_STATE b;
	char *buf;
	yy_size_t n;
	yy_size_t i;
    
	
	n = _yybytes_len + 2;
	buf = (char *) i386_alloc(n  );
	if ( ! buf )
		YY_FATAL_ERROR( "out of dynamic memory in i386__scan_bytes()" );

	for ( i = 0; i < _yybytes_len; ++i )
		buf[i] = yybytes[i];

	buf[_yybytes_len] = buf[_yybytes_len+1] = YY_END_OF_BUFFER_CHAR;

	b = i386__scan_buffer(buf,n );
	if ( ! b )
		YY_FATAL_ERROR( "bad buffer in i386__scan_bytes()" );

	b->yy_is_our_buffer = 1;

	return b;
}

#ifndef YY_EXIT_FAILURE
#define YY_EXIT_FAILURE 2
#endif

static void yy_fatal_error (yyconst char* msg )
{
    	(void) fprintf( stderr, "%s\n", msg );
	exit( YY_EXIT_FAILURE );
}


#undef yyless
#define yyless(n) \
	do \
		{ \
		 \
        int yyless_macro_arg = (n); \
        YY_LESS_LINENO(yyless_macro_arg);\
		i386_text[i386_leng] = (yy_hold_char); \
		(yy_c_buf_p) = i386_text + yyless_macro_arg; \
		(yy_hold_char) = *(yy_c_buf_p); \
		*(yy_c_buf_p) = '\0'; \
		i386_leng = yyless_macro_arg; \
		} \
	while ( 0 )


int i386_get_lineno  (void)
{
        
    return i386_lineno;
}

FILE *i386_get_in  (void)
{
        return i386_in;
}

FILE *i386_get_out  (void)
{
        return i386_out;
}

yy_size_t i386_get_leng  (void)
{
        return i386_leng;
}


char *i386_get_text  (void)
{
        return i386_text;
}

void i386_set_lineno (int  line_number )
{
    
    i386_lineno = line_number;
}

void i386_set_in (FILE *  in_str )
{
        i386_in = in_str ;
}

void i386_set_out (FILE *  out_str )
{
        i386_out = out_str ;
}

int i386_get_debug  (void)
{
        return i386__flex_debug;
}

void i386_set_debug (int  bdebug )
{
        i386__flex_debug = bdebug ;
}

static int yy_init_globals (void)
{

    
    i386_lineno =  1;
    
    (yy_buffer_stack) = 0;
    (yy_buffer_stack_top) = 0;
    (yy_buffer_stack_max) = 0;
    (yy_c_buf_p) = (char *) 0;
    (yy_init) = 0;
    (yy_start) = 0;

#ifdef YY_STDINIT
    i386_in = stdin;
    i386_out = stdout;
#else
    i386_in = (FILE *) 0;
    i386_out = (FILE *) 0;
#endif

    return 0;
}

int i386_lex_destroy  (void)
{
    
    
	while(YY_CURRENT_BUFFER){
		i386__delete_buffer(YY_CURRENT_BUFFER  );
		YY_CURRENT_BUFFER_LVALUE = NULL;
		i386_pop_buffer_state();
	}

	
	i386_free((yy_buffer_stack) );
	(yy_buffer_stack) = NULL;

    yy_init_globals( );

    return 0;
}


#ifndef yytext_ptr
static void yy_flex_strncpy (char* s1, yyconst char * s2, int n )
{
	register int i;
	for ( i = 0; i < n; ++i )
		s1[i] = s2[i];
}
#endif

#ifdef YY_NEED_STRLEN
static int yy_flex_strlen (yyconst char * s )
{
	register int n;
	for ( n = 0; s[n]; ++n )
		;

	return n;
}
#endif

void *i386_alloc (yy_size_t  size )
{
	return (void *) malloc( size );
}

void *i386_realloc  (void * ptr, yy_size_t  size )
{
	return (void *) realloc( (char *) ptr, size );
}

void i386_free (void * ptr )
{
	free( (char *) ptr );	
}

#define YYTABLES_NAME "yytables"

#line 104 "/home/mark/src/elfutils/libcpu/i386_lex.l"



static void
eat_to_eol (void)
{
  while (1)
    {
      int c = input ();

      if (c == EOF || c == '\n')
	break;
    }
}

static void
invalid_char (int ch)
{
  error (0, 0, (isascii (ch)
		? gettext ("invalid character '%c' at line %d; ignored")
		: gettext ("invalid character '\\%o' at line %d; ignored")),
	 ch, i386_lineno);
}


