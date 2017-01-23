#ifndef parse_events_HEADER_H
#define parse_events_HEADER_H 1
#define parse_events_IN_HEADER 1

#line 6 "util/parse-events-flex.h"

#define  YY_INT_ALIGNED short int


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

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

#define yyin yyg->yyin_r
#define yyout yyg->yyout_r
#define yyextra yyg->yyextra_r
#define yyleng yyg->yyleng_r
#define yytext yyg->yytext_r
#define yylineno (YY_CURRENT_BUFFER_LVALUE->yy_bs_lineno)
#define yycolumn (YY_CURRENT_BUFFER_LVALUE->yy_bs_column)
#define yy_flex_debug yyg->yy_flex_debug_r

#ifndef YY_BUF_SIZE
#ifdef __ia64__
#define YY_BUF_SIZE 32768
#else
#define YY_BUF_SIZE 16384
#endif 
#endif

#ifndef YY_TYPEDEF_YY_BUFFER_STATE
#define YY_TYPEDEF_YY_BUFFER_STATE
typedef struct yy_buffer_state *YY_BUFFER_STATE;
#endif

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

	};
#endif 

void parse_events_restart (FILE *input_file ,yyscan_t yyscanner );
void parse_events__switch_to_buffer (YY_BUFFER_STATE new_buffer ,yyscan_t yyscanner );
YY_BUFFER_STATE parse_events__create_buffer (FILE *file,int size ,yyscan_t yyscanner );
void parse_events__delete_buffer (YY_BUFFER_STATE b ,yyscan_t yyscanner );
void parse_events__flush_buffer (YY_BUFFER_STATE b ,yyscan_t yyscanner );
void parse_events_push_buffer_state (YY_BUFFER_STATE new_buffer ,yyscan_t yyscanner );
void parse_events_pop_buffer_state (yyscan_t yyscanner );

YY_BUFFER_STATE parse_events__scan_buffer (char *base,yy_size_t size ,yyscan_t yyscanner );
YY_BUFFER_STATE parse_events__scan_string (yyconst char *yy_str ,yyscan_t yyscanner );
YY_BUFFER_STATE parse_events__scan_bytes (yyconst char *bytes,int len ,yyscan_t yyscanner );

void *parse_events_alloc (yy_size_t ,yyscan_t yyscanner );
void *parse_events_realloc (void *,yy_size_t ,yyscan_t yyscanner );
void parse_events_free (void * ,yyscan_t yyscanner );


#define yytext_ptr yytext_r

#ifdef YY_HEADER_EXPORT_START_CONDITIONS
#define INITIAL 0
#define mem 1
#define config 2
#define event 3

#endif

#ifndef YY_NO_UNISTD_H
#include <unistd.h>
#endif

#ifndef YY_EXTRA_TYPE
#define YY_EXTRA_TYPE void *
#endif

int parse_events_lex_init (yyscan_t* scanner);

int parse_events_lex_init_extra (YY_EXTRA_TYPE user_defined,yyscan_t* scanner);


int parse_events_lex_destroy (yyscan_t yyscanner );

int parse_events_get_debug (yyscan_t yyscanner );

void parse_events_set_debug (int debug_flag ,yyscan_t yyscanner );

YY_EXTRA_TYPE parse_events_get_extra (yyscan_t yyscanner );

void parse_events_set_extra (YY_EXTRA_TYPE user_defined ,yyscan_t yyscanner );

FILE *parse_events_get_in (yyscan_t yyscanner );

void parse_events_set_in  (FILE * in_str ,yyscan_t yyscanner );

FILE *parse_events_get_out (yyscan_t yyscanner );

void parse_events_set_out  (FILE * out_str ,yyscan_t yyscanner );

int parse_events_get_leng (yyscan_t yyscanner );

char *parse_events_get_text (yyscan_t yyscanner );

int parse_events_get_lineno (yyscan_t yyscanner );

void parse_events_set_lineno (int line_number ,yyscan_t yyscanner );

YYSTYPE * parse_events_get_lval (yyscan_t yyscanner );

void parse_events_set_lval (YYSTYPE * yylval_param ,yyscan_t yyscanner );


#ifndef YY_SKIP_YYWRAP
#ifdef __cplusplus
extern "C" int parse_events_wrap (yyscan_t yyscanner );
#else
extern int parse_events_wrap (yyscan_t yyscanner );
#endif
#endif

#ifndef yytext_ptr
static void yy_flex_strncpy (char *,yyconst char *,int ,yyscan_t yyscanner);
#endif

#ifdef YY_NEED_STRLEN
static int yy_flex_strlen (yyconst char * ,yyscan_t yyscanner);
#endif

#ifndef YY_NO_INPUT

#endif

#ifndef YY_READ_BUF_SIZE
#ifdef __ia64__
#define YY_READ_BUF_SIZE 16384
#else
#define YY_READ_BUF_SIZE 8192
#endif 
#endif

#ifndef YY_START_STACK_INCR
#define YY_START_STACK_INCR 25
#endif

#ifndef YY_DECL
#define YY_DECL_IS_OURS 1

extern int parse_events_lex \
               (YYSTYPE * yylval_param ,yyscan_t yyscanner);

#define YY_DECL int parse_events_lex \
               (YYSTYPE * yylval_param , yyscan_t yyscanner)
#endif 


#undef YY_NEW_FILE
#undef YY_FLUSH_BUFFER
#undef yy_set_bol
#undef yy_new_buffer
#undef yy_set_interactive
#undef YY_DO_BEFORE_ACTION

#ifdef YY_DECL_IS_OURS
#undef YY_DECL_IS_OURS
#undef YY_DECL
#endif

#line 212 "util/parse-events.l"


#line 352 "util/parse-events-flex.h"
#undef parse_events_IN_HEADER
#endif 
