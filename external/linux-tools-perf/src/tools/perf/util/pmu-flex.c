
#line 3 "<stdout>"

#define  YY_INT_ALIGNED short int


#define yy_create_buffer perf_pmu__create_buffer
#define yy_delete_buffer perf_pmu__delete_buffer
#define yy_flex_debug perf_pmu__flex_debug
#define yy_init_buffer perf_pmu__init_buffer
#define yy_flush_buffer perf_pmu__flush_buffer
#define yy_load_buffer_state perf_pmu__load_buffer_state
#define yy_switch_to_buffer perf_pmu__switch_to_buffer
#define yyin perf_pmu_in
#define yyleng perf_pmu_leng
#define yylex perf_pmu_lex
#define yylineno perf_pmu_lineno
#define yyout perf_pmu_out
#define yyrestart perf_pmu_restart
#define yytext perf_pmu_text
#define yywrap perf_pmu_wrap
#define yyalloc perf_pmu_alloc
#define yyrealloc perf_pmu_realloc
#define yyfree perf_pmu_free

#define FLEX_SCANNER
#define YY_FLEX_MAJOR_VERSION 2
#define YY_FLEX_MINOR_VERSION 5
#define YY_FLEX_SUBMINOR_VERSION 35
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

#define YY_NEW_FILE perf_pmu_restart(perf_pmu_in  )

#define YY_END_OF_BUFFER_CHAR 0

#ifndef YY_BUF_SIZE
#ifdef __ia64__
#define YY_BUF_SIZE 32768
#else
#define YY_BUF_SIZE 16384
#endif 
#endif

#define YY_STATE_BUF_SIZE   ((YY_BUF_SIZE + 2) * sizeof(yy_state_type))

#ifndef YY_TYPEDEF_YY_BUFFER_STATE
#define YY_TYPEDEF_YY_BUFFER_STATE
typedef struct yy_buffer_state *YY_BUFFER_STATE;
#endif

extern int perf_pmu_leng;

extern FILE *perf_pmu_in, *perf_pmu_out;

#define EOB_ACT_CONTINUE_SCAN 0
#define EOB_ACT_END_OF_FILE 1
#define EOB_ACT_LAST_MATCH 2

    #define YY_LESS_LINENO(n)
    
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

#ifndef YY_TYPEDEF_YY_SIZE_T
#define YY_TYPEDEF_YY_SIZE_T
typedef size_t yy_size_t;
#endif

#ifndef YY_STRUCT_YY_BUFFER_STATE
#define YY_STRUCT_YY_BUFFER_STATE
struct yy_buffer_state
	{
	FILE *yy_input_file;

	char *yy_ch_buf;		
	char *yy_buf_pos;		

	yy_size_t yy_buf_size;

	int yy_n_chars;

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
static int yy_n_chars;		
int perf_pmu_leng;

static char *yy_c_buf_p = (char *) 0;
static int yy_init = 0;		
static int yy_start = 0;	

static int yy_did_buffer_switch_on_eof;

void perf_pmu_restart (FILE *input_file  );
void perf_pmu__switch_to_buffer (YY_BUFFER_STATE new_buffer  );
YY_BUFFER_STATE perf_pmu__create_buffer (FILE *file,int size  );
void perf_pmu__delete_buffer (YY_BUFFER_STATE b  );
void perf_pmu__flush_buffer (YY_BUFFER_STATE b  );
void perf_pmu_push_buffer_state (YY_BUFFER_STATE new_buffer  );
void perf_pmu_pop_buffer_state (void );

static void perf_pmu_ensure_buffer_stack (void );
static void perf_pmu__load_buffer_state (void );
static void perf_pmu__init_buffer (YY_BUFFER_STATE b,FILE *file  );

#define YY_FLUSH_BUFFER perf_pmu__flush_buffer(YY_CURRENT_BUFFER )

YY_BUFFER_STATE perf_pmu__scan_buffer (char *base,yy_size_t size  );
YY_BUFFER_STATE perf_pmu__scan_string (yyconst char *yy_str  );
YY_BUFFER_STATE perf_pmu__scan_bytes (yyconst char *bytes,int len  );

void *perf_pmu_alloc (yy_size_t  );
void *perf_pmu_realloc (void *,yy_size_t  );
void perf_pmu_free (void *  );

#define yy_new_buffer perf_pmu__create_buffer

#define yy_set_interactive(is_interactive) \
	{ \
	if ( ! YY_CURRENT_BUFFER ){ \
        perf_pmu_ensure_buffer_stack (); \
		YY_CURRENT_BUFFER_LVALUE =    \
            perf_pmu__create_buffer(perf_pmu_in,YY_BUF_SIZE ); \
	} \
	YY_CURRENT_BUFFER_LVALUE->yy_is_interactive = is_interactive; \
	}

#define yy_set_bol(at_bol) \
	{ \
	if ( ! YY_CURRENT_BUFFER ){\
        perf_pmu_ensure_buffer_stack (); \
		YY_CURRENT_BUFFER_LVALUE =    \
            perf_pmu__create_buffer(perf_pmu_in,YY_BUF_SIZE ); \
	} \
	YY_CURRENT_BUFFER_LVALUE->yy_at_bol = at_bol; \
	}

#define YY_AT_BOL() (YY_CURRENT_BUFFER_LVALUE->yy_at_bol)


typedef unsigned char YY_CHAR;

FILE *perf_pmu_in = (FILE *) 0, *perf_pmu_out = (FILE *) 0;

typedef int yy_state_type;

extern int perf_pmu_lineno;

int perf_pmu_lineno = 1;

extern char *perf_pmu_text;
#define yytext_ptr perf_pmu_text

static yy_state_type yy_get_previous_state (void );
static yy_state_type yy_try_NUL_trans (yy_state_type current_state  );
static int yy_get_next_buffer (void );
static void yy_fatal_error (yyconst char msg[]  );

#define YY_DO_BEFORE_ACTION \
	(yytext_ptr) = yy_bp; \
	perf_pmu_leng = (size_t) (yy_cp - yy_bp); \
	(yy_hold_char) = *yy_cp; \
	*yy_cp = '\0'; \
	(yy_c_buf_p) = yy_cp;

#define YY_NUM_RULES 10
#define YY_END_OF_BUFFER 11
struct yy_trans_info
	{
	flex_int32_t yy_verify;
	flex_int32_t yy_nxt;
	};
static yyconst flex_int16_t yy_accept[20] =
    {   0,
        0,    0,   11,    8,    9,    7,    5,    1,    6,    8,
        1,    0,    0,    0,    0,    2,    3,    4,    0
    } ;

static yyconst flex_int32_t yy_ec[256] =
    {   0,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    2,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    3,    4,    1,    1,    5,    6,    7,
        5,    5,    5,    5,    5,    5,    5,    8,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    9,    1,

        1,   10,   11,    1,   12,    1,    1,    1,    1,   13,
       14,    1,    1,    1,    1,    1,    1,    1,    1,    1,
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
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1
    } ;

static yyconst flex_int32_t yy_meta[15] =
    {   0,
        1,    1,    1,    1,    2,    2,    2,    1,    1,    1,
        1,    1,    1,    1
    } ;

static yyconst flex_int16_t yy_base[21] =
    {   0,
        0,    0,   23,   24,   24,   24,   24,    0,   24,    8,
        0,    8,   10,    7,    7,    9,   24,   24,   24,   15
    } ;

static yyconst flex_int16_t yy_def[21] =
    {   0,
       19,    1,   19,   19,   19,   19,   19,   20,   19,   19,
       20,   19,   19,   19,   19,   19,   19,   19,    0,   19
    } ;

static yyconst flex_int16_t yy_nxt[39] =
    {   0,
        4,    5,    6,    7,    8,    8,    8,    9,   10,    4,
        4,    4,    4,    4,   17,   18,   11,   16,   15,   14,
       13,   12,   19,    3,   19,   19,   19,   19,   19,   19,
       19,   19,   19,   19,   19,   19,   19,   19
    } ;

static yyconst flex_int16_t yy_chk[39] =
    {   0,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,   16,   16,   20,   15,   14,   13,
       12,   10,    3,   19,   19,   19,   19,   19,   19,   19,
       19,   19,   19,   19,   19,   19,   19,   19
    } ;

static yy_state_type yy_last_accepting_state;
static char *yy_last_accepting_cpos;

extern int perf_pmu__flex_debug;
int perf_pmu__flex_debug = 0;

#define REJECT reject_used_but_not_detected
#define yymore() yymore_used_but_not_detected
#define YY_MORE_ADJ 0
#define YY_RESTORE_YY_MORE_OFFSET
char *perf_pmu_text;
#line 1 "util/pmu.l"
#line 4 "util/pmu.l"
#include <stdlib.h>
#include <linux/bitops.h>
#include "pmu.h"
#include "pmu-bison.h"

static int value(int base)
{
	long num;

	errno = 0;
	num = strtoul(perf_pmu_text, NULL, base);
	if (errno)
		return PP_ERROR;

	perf_pmu_lval.num = num;
	return PP_VALUE;
}

#line 506 "<stdout>"

#define INITIAL 0

#ifndef YY_NO_UNISTD_H
#include <unistd.h>
#endif

#ifndef YY_EXTRA_TYPE
#define YY_EXTRA_TYPE void *
#endif

static int yy_init_globals (void );


int perf_pmu_lex_destroy (void );

int perf_pmu_get_debug (void );

void perf_pmu_set_debug (int debug_flag  );

YY_EXTRA_TYPE perf_pmu_get_extra (void );

void perf_pmu_set_extra (YY_EXTRA_TYPE user_defined  );

FILE *perf_pmu_get_in (void );

void perf_pmu_set_in  (FILE * in_str  );

FILE *perf_pmu_get_out (void );

void perf_pmu_set_out  (FILE * out_str  );

int perf_pmu_get_leng (void );

char *perf_pmu_get_text (void );

int perf_pmu_get_lineno (void );

void perf_pmu_set_lineno (int line_number  );


#ifndef YY_SKIP_YYWRAP
#ifdef __cplusplus
extern "C" int perf_pmu_wrap (void );
#else
extern int perf_pmu_wrap (void );
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
#ifdef __ia64__
#define YY_READ_BUF_SIZE 16384
#else
#define YY_READ_BUF_SIZE 8192
#endif 
#endif

#ifndef ECHO
#define ECHO do { if (fwrite( perf_pmu_text, perf_pmu_leng, 1, perf_pmu_out )) {} } while (0)
#endif

#ifndef YY_INPUT
#define YY_INPUT(buf,result,max_size) \
	if ( YY_CURRENT_BUFFER_LVALUE->yy_is_interactive ) \
		{ \
		int c = '*'; \
		size_t n; \
		for ( n = 0; n < max_size && \
			     (c = getc( perf_pmu_in )) != EOF && c != '\n'; ++n ) \
			buf[n] = (char) c; \
		if ( c == '\n' ) \
			buf[n++] = (char) c; \
		if ( c == EOF && ferror( perf_pmu_in ) ) \
			YY_FATAL_ERROR( "input in flex scanner failed" ); \
		result = n; \
		} \
	else \
		{ \
		errno=0; \
		while ( (result = fread(buf, 1, max_size, perf_pmu_in))==0 && ferror(perf_pmu_in)) \
			{ \
			if( errno != EINTR) \
				{ \
				YY_FATAL_ERROR( "input in flex scanner failed" ); \
				break; \
				} \
			errno=0; \
			clearerr(perf_pmu_in); \
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

extern int perf_pmu_lex (void);

#define YY_DECL int perf_pmu_lex (void)
#endif 

#ifndef YY_USER_ACTION
#define YY_USER_ACTION
#endif

#ifndef YY_BREAK
#define YY_BREAK break;
#endif

#define YY_RULE_SETUP \
	YY_USER_ACTION

YY_DECL
{
	register yy_state_type yy_current_state;
	register char *yy_cp, *yy_bp;
	register int yy_act;
    
#line 26 "util/pmu.l"


#line 696 "<stdout>"

	if ( !(yy_init) )
		{
		(yy_init) = 1;

#ifdef YY_USER_INIT
		YY_USER_INIT;
#endif

		if ( ! (yy_start) )
			(yy_start) = 1;	

		if ( ! perf_pmu_in )
			perf_pmu_in = stdin;

		if ( ! perf_pmu_out )
			perf_pmu_out = stdout;

		if ( ! YY_CURRENT_BUFFER ) {
			perf_pmu_ensure_buffer_stack ();
			YY_CURRENT_BUFFER_LVALUE =
				perf_pmu__create_buffer(perf_pmu_in,YY_BUF_SIZE );
		}

		perf_pmu__load_buffer_state( );
		}

	while ( 1 )		
		{
		yy_cp = (yy_c_buf_p);

		
		*yy_cp = (yy_hold_char);

		yy_bp = yy_cp;

		yy_current_state = (yy_start);
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
				if ( yy_current_state >= 20 )
					yy_c = yy_meta[(unsigned int) yy_c];
				}
			yy_current_state = yy_nxt[yy_base[yy_current_state] + (unsigned int) yy_c];
			++yy_cp;
			}
		while ( yy_base[yy_current_state] != 24 );

yy_find_action:
		yy_act = yy_accept[yy_current_state];
		if ( yy_act == 0 )
			{ 
			yy_cp = (yy_last_accepting_cpos);
			yy_current_state = (yy_last_accepting_state);
			yy_act = yy_accept[yy_current_state];
			}

		YY_DO_BEFORE_ACTION;

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
#line 28 "util/pmu.l"
{ return value(10); }
	YY_BREAK
case 2:
YY_RULE_SETUP
#line 29 "util/pmu.l"
{ return PP_CONFIG; }
	YY_BREAK
case 3:
YY_RULE_SETUP
#line 30 "util/pmu.l"
{ return PP_CONFIG1; }
	YY_BREAK
case 4:
YY_RULE_SETUP
#line 31 "util/pmu.l"
{ return PP_CONFIG2; }
	YY_BREAK
case 5:
YY_RULE_SETUP
#line 32 "util/pmu.l"
{ return '-'; }
	YY_BREAK
case 6:
YY_RULE_SETUP
#line 33 "util/pmu.l"
{ return ':'; }
	YY_BREAK
case 7:
YY_RULE_SETUP
#line 34 "util/pmu.l"
{ return ','; }
	YY_BREAK
case 8:
YY_RULE_SETUP
#line 35 "util/pmu.l"
{ ; }
	YY_BREAK
case 9:
YY_RULE_SETUP
#line 36 "util/pmu.l"
{ ; }
	YY_BREAK
case 10:
YY_RULE_SETUP
#line 38 "util/pmu.l"
ECHO;
	YY_BREAK
#line 830 "<stdout>"
case YY_STATE_EOF(INITIAL):
	yyterminate();

	case YY_END_OF_BUFFER:
		{
		
		int yy_amount_of_matched_text = (int) (yy_cp - (yytext_ptr)) - 1;

		
		*yy_cp = (yy_hold_char);
		YY_RESTORE_YY_MORE_OFFSET

		if ( YY_CURRENT_BUFFER_LVALUE->yy_buffer_status == YY_BUFFER_NEW )
			{
			(yy_n_chars) = YY_CURRENT_BUFFER_LVALUE->yy_n_chars;
			YY_CURRENT_BUFFER_LVALUE->yy_input_file = perf_pmu_in;
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
				yy_cp = (yy_c_buf_p);
				goto yy_find_action;
				}
			}

		else switch ( yy_get_next_buffer(  ) )
			{
			case EOB_ACT_END_OF_FILE:
				{
				(yy_did_buffer_switch_on_eof) = 0;

				if ( perf_pmu_wrap( ) )
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
			int num_to_read =
			YY_CURRENT_BUFFER_LVALUE->yy_buf_size - number_to_move - 1;

		while ( num_to_read <= 0 )
			{ 

			
			YY_BUFFER_STATE b = YY_CURRENT_BUFFER;

			int yy_c_buf_p_offset =
				(int) ((yy_c_buf_p) - b->yy_ch_buf);

			if ( b->yy_is_our_buffer )
				{
				int new_size = b->yy_buf_size * 2;

				if ( new_size <= 0 )
					b->yy_buf_size += b->yy_buf_size / 8;
				else
					b->yy_buf_size *= 2;

				b->yy_ch_buf = (char *)
					
					perf_pmu_realloc((void *) b->yy_ch_buf,b->yy_buf_size + 2  );
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
			(yy_n_chars), (size_t) num_to_read );

		YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
		}

	if ( (yy_n_chars) == 0 )
		{
		if ( number_to_move == YY_MORE_ADJ )
			{
			ret_val = EOB_ACT_END_OF_FILE;
			perf_pmu_restart(perf_pmu_in  );
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
		YY_CURRENT_BUFFER_LVALUE->yy_ch_buf = (char *) perf_pmu_realloc((void *) YY_CURRENT_BUFFER_LVALUE->yy_ch_buf,new_size  );
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
			if ( yy_current_state >= 20 )
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
		if ( yy_current_state >= 20 )
			yy_c = yy_meta[(unsigned int) yy_c];
		}
	yy_current_state = yy_nxt[yy_base[yy_current_state] + (unsigned int) yy_c];
	yy_is_jam = (yy_current_state == 19);

	return yy_is_jam ? 0 : yy_current_state;
}

    static void yyunput (int c, register char * yy_bp )
{
	register char *yy_cp;
    
    yy_cp = (yy_c_buf_p);

	
	*yy_cp = (yy_hold_char);

	if ( yy_cp < YY_CURRENT_BUFFER_LVALUE->yy_ch_buf + 2 )
		{ 
		
		register int number_to_move = (yy_n_chars) + 2;
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
			int offset = (yy_c_buf_p) - (yytext_ptr);
			++(yy_c_buf_p);

			switch ( yy_get_next_buffer(  ) )
				{
				case EOB_ACT_LAST_MATCH:

					
					perf_pmu_restart(perf_pmu_in );

					

				case EOB_ACT_END_OF_FILE:
					{
					if ( perf_pmu_wrap( ) )
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

	return c;
}
#endif	

    void perf_pmu_restart  (FILE * input_file )
{
    
	if ( ! YY_CURRENT_BUFFER ){
        perf_pmu_ensure_buffer_stack ();
		YY_CURRENT_BUFFER_LVALUE =
            perf_pmu__create_buffer(perf_pmu_in,YY_BUF_SIZE );
	}

	perf_pmu__init_buffer(YY_CURRENT_BUFFER,input_file );
	perf_pmu__load_buffer_state( );
}

    void perf_pmu__switch_to_buffer  (YY_BUFFER_STATE  new_buffer )
{
    
	perf_pmu_ensure_buffer_stack ();
	if ( YY_CURRENT_BUFFER == new_buffer )
		return;

	if ( YY_CURRENT_BUFFER )
		{
		
		*(yy_c_buf_p) = (yy_hold_char);
		YY_CURRENT_BUFFER_LVALUE->yy_buf_pos = (yy_c_buf_p);
		YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
		}

	YY_CURRENT_BUFFER_LVALUE = new_buffer;
	perf_pmu__load_buffer_state( );

	(yy_did_buffer_switch_on_eof) = 1;
}

static void perf_pmu__load_buffer_state  (void)
{
    	(yy_n_chars) = YY_CURRENT_BUFFER_LVALUE->yy_n_chars;
	(yytext_ptr) = (yy_c_buf_p) = YY_CURRENT_BUFFER_LVALUE->yy_buf_pos;
	perf_pmu_in = YY_CURRENT_BUFFER_LVALUE->yy_input_file;
	(yy_hold_char) = *(yy_c_buf_p);
}

    YY_BUFFER_STATE perf_pmu__create_buffer  (FILE * file, int  size )
{
	YY_BUFFER_STATE b;
    
	b = (YY_BUFFER_STATE) perf_pmu_alloc(sizeof( struct yy_buffer_state )  );
	if ( ! b )
		YY_FATAL_ERROR( "out of dynamic memory in perf_pmu__create_buffer()" );

	b->yy_buf_size = size;

	b->yy_ch_buf = (char *) perf_pmu_alloc(b->yy_buf_size + 2  );
	if ( ! b->yy_ch_buf )
		YY_FATAL_ERROR( "out of dynamic memory in perf_pmu__create_buffer()" );

	b->yy_is_our_buffer = 1;

	perf_pmu__init_buffer(b,file );

	return b;
}

    void perf_pmu__delete_buffer (YY_BUFFER_STATE  b )
{
    
	if ( ! b )
		return;

	if ( b == YY_CURRENT_BUFFER ) 
		YY_CURRENT_BUFFER_LVALUE = (YY_BUFFER_STATE) 0;

	if ( b->yy_is_our_buffer )
		perf_pmu_free((void *) b->yy_ch_buf  );

	perf_pmu_free((void *) b  );
}

#ifndef __cplusplus
extern int isatty (int );
#endif 
    
    static void perf_pmu__init_buffer  (YY_BUFFER_STATE  b, FILE * file )

{
	int oerrno = errno;
    
	perf_pmu__flush_buffer(b );

	b->yy_input_file = file;
	b->yy_fill_buffer = 1;

    if (b != YY_CURRENT_BUFFER){
        b->yy_bs_lineno = 1;
        b->yy_bs_column = 0;
    }

        b->yy_is_interactive = file ? (isatty( fileno(file) ) > 0) : 0;
    
	errno = oerrno;
}

    void perf_pmu__flush_buffer (YY_BUFFER_STATE  b )
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
		perf_pmu__load_buffer_state( );
}

void perf_pmu_push_buffer_state (YY_BUFFER_STATE new_buffer )
{
    	if (new_buffer == NULL)
		return;

	perf_pmu_ensure_buffer_stack();

	
	if ( YY_CURRENT_BUFFER )
		{
		
		*(yy_c_buf_p) = (yy_hold_char);
		YY_CURRENT_BUFFER_LVALUE->yy_buf_pos = (yy_c_buf_p);
		YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
		}

	
	if (YY_CURRENT_BUFFER)
		(yy_buffer_stack_top)++;
	YY_CURRENT_BUFFER_LVALUE = new_buffer;

	
	perf_pmu__load_buffer_state( );
	(yy_did_buffer_switch_on_eof) = 1;
}

void perf_pmu_pop_buffer_state (void)
{
    	if (!YY_CURRENT_BUFFER)
		return;

	perf_pmu__delete_buffer(YY_CURRENT_BUFFER );
	YY_CURRENT_BUFFER_LVALUE = NULL;
	if ((yy_buffer_stack_top) > 0)
		--(yy_buffer_stack_top);

	if (YY_CURRENT_BUFFER) {
		perf_pmu__load_buffer_state( );
		(yy_did_buffer_switch_on_eof) = 1;
	}
}

static void perf_pmu_ensure_buffer_stack (void)
{
	int num_to_alloc;
    
	if (!(yy_buffer_stack)) {

		num_to_alloc = 1;
		(yy_buffer_stack) = (struct yy_buffer_state**)perf_pmu_alloc
								(num_to_alloc * sizeof(struct yy_buffer_state*)
								);
		if ( ! (yy_buffer_stack) )
			YY_FATAL_ERROR( "out of dynamic memory in perf_pmu_ensure_buffer_stack()" );
								  
		memset((yy_buffer_stack), 0, num_to_alloc * sizeof(struct yy_buffer_state*));
				
		(yy_buffer_stack_max) = num_to_alloc;
		(yy_buffer_stack_top) = 0;
		return;
	}

	if ((yy_buffer_stack_top) >= ((yy_buffer_stack_max)) - 1){

		
		int grow_size = 8 ;

		num_to_alloc = (yy_buffer_stack_max) + grow_size;
		(yy_buffer_stack) = (struct yy_buffer_state**)perf_pmu_realloc
								((yy_buffer_stack),
								num_to_alloc * sizeof(struct yy_buffer_state*)
								);
		if ( ! (yy_buffer_stack) )
			YY_FATAL_ERROR( "out of dynamic memory in perf_pmu_ensure_buffer_stack()" );

		
		memset((yy_buffer_stack) + (yy_buffer_stack_max), 0, grow_size * sizeof(struct yy_buffer_state*));
		(yy_buffer_stack_max) = num_to_alloc;
	}
}

YY_BUFFER_STATE perf_pmu__scan_buffer  (char * base, yy_size_t  size )
{
	YY_BUFFER_STATE b;
    
	if ( size < 2 ||
	     base[size-2] != YY_END_OF_BUFFER_CHAR ||
	     base[size-1] != YY_END_OF_BUFFER_CHAR )
		
		return 0;

	b = (YY_BUFFER_STATE) perf_pmu_alloc(sizeof( struct yy_buffer_state )  );
	if ( ! b )
		YY_FATAL_ERROR( "out of dynamic memory in perf_pmu__scan_buffer()" );

	b->yy_buf_size = size - 2;	
	b->yy_buf_pos = b->yy_ch_buf = base;
	b->yy_is_our_buffer = 0;
	b->yy_input_file = 0;
	b->yy_n_chars = b->yy_buf_size;
	b->yy_is_interactive = 0;
	b->yy_at_bol = 1;
	b->yy_fill_buffer = 0;
	b->yy_buffer_status = YY_BUFFER_NEW;

	perf_pmu__switch_to_buffer(b  );

	return b;
}

YY_BUFFER_STATE perf_pmu__scan_string (yyconst char * yystr )
{
    
	return perf_pmu__scan_bytes(yystr,strlen(yystr) );
}

YY_BUFFER_STATE perf_pmu__scan_bytes  (yyconst char * yybytes, int  _yybytes_len )
{
	YY_BUFFER_STATE b;
	char *buf;
	yy_size_t n;
	int i;
    
	
	n = _yybytes_len + 2;
	buf = (char *) perf_pmu_alloc(n  );
	if ( ! buf )
		YY_FATAL_ERROR( "out of dynamic memory in perf_pmu__scan_bytes()" );

	for ( i = 0; i < _yybytes_len; ++i )
		buf[i] = yybytes[i];

	buf[_yybytes_len] = buf[_yybytes_len+1] = YY_END_OF_BUFFER_CHAR;

	b = perf_pmu__scan_buffer(buf,n );
	if ( ! b )
		YY_FATAL_ERROR( "bad buffer in perf_pmu__scan_bytes()" );

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
		perf_pmu_text[perf_pmu_leng] = (yy_hold_char); \
		(yy_c_buf_p) = perf_pmu_text + yyless_macro_arg; \
		(yy_hold_char) = *(yy_c_buf_p); \
		*(yy_c_buf_p) = '\0'; \
		perf_pmu_leng = yyless_macro_arg; \
		} \
	while ( 0 )


int perf_pmu_get_lineno  (void)
{
        
    return perf_pmu_lineno;
}

FILE *perf_pmu_get_in  (void)
{
        return perf_pmu_in;
}

FILE *perf_pmu_get_out  (void)
{
        return perf_pmu_out;
}

int perf_pmu_get_leng  (void)
{
        return perf_pmu_leng;
}


char *perf_pmu_get_text  (void)
{
        return perf_pmu_text;
}

void perf_pmu_set_lineno (int  line_number )
{
    
    perf_pmu_lineno = line_number;
}

void perf_pmu_set_in (FILE *  in_str )
{
        perf_pmu_in = in_str ;
}

void perf_pmu_set_out (FILE *  out_str )
{
        perf_pmu_out = out_str ;
}

int perf_pmu_get_debug  (void)
{
        return perf_pmu__flex_debug;
}

void perf_pmu_set_debug (int  bdebug )
{
        perf_pmu__flex_debug = bdebug ;
}

static int yy_init_globals (void)
{

    (yy_buffer_stack) = 0;
    (yy_buffer_stack_top) = 0;
    (yy_buffer_stack_max) = 0;
    (yy_c_buf_p) = (char *) 0;
    (yy_init) = 0;
    (yy_start) = 0;

#ifdef YY_STDINIT
    perf_pmu_in = stdin;
    perf_pmu_out = stdout;
#else
    perf_pmu_in = (FILE *) 0;
    perf_pmu_out = (FILE *) 0;
#endif

    return 0;
}

int perf_pmu_lex_destroy  (void)
{
    
    
	while(YY_CURRENT_BUFFER){
		perf_pmu__delete_buffer(YY_CURRENT_BUFFER  );
		YY_CURRENT_BUFFER_LVALUE = NULL;
		perf_pmu_pop_buffer_state();
	}

	
	perf_pmu_free((yy_buffer_stack) );
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

void *perf_pmu_alloc (yy_size_t  size )
{
	return (void *) malloc( size );
}

void *perf_pmu_realloc  (void * ptr, yy_size_t  size )
{
	return (void *) realloc( (char *) ptr, size );
}

void perf_pmu_free (void * ptr )
{
	free( (char *) ptr );	
}

#define YYTABLES_NAME "yytables"

#line 38 "util/pmu.l"



int perf_pmu_wrap(void)
{
	return 1;
}

