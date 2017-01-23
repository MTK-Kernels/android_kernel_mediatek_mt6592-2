
/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */


#define YYBISON 1

#define YYBISON_VERSION "2.5"

#define YYSKELETON_NAME "yacc.c"

#define YYPURE 1

#define YYPUSH 0

#define YYPULL 1

#define YYLSP_NEEDED 0

#define yyparse         parse_events_parse
#define yylex           parse_events_lex
#define yyerror         parse_events_error
#define yylval          parse_events_lval
#define yychar          parse_events_char
#define yydebug         parse_events_debug
#define yynerrs         parse_events_nerrs



#line 6 "util/parse-events.y"


#define YYDEBUG 1

#include <linux/compiler.h>
#include <linux/list.h>
#include "types.h"
#include "util.h"
#include "parse-events.h"
#include "parse-events-bison.h"

extern int parse_events_lex (YYSTYPE* lvalp, void* scanner);

#define ABORT_ON(val) \
do { \
	if (val) \
		YYABORT; \
} while (0)

#define ALLOC_LIST(list) \
do { \
	list = malloc(sizeof(*list)); \
	ABORT_ON(!list);              \
	INIT_LIST_HEAD(list);         \
} while (0)

static inc_group_count(struct list_head *list,
		       struct parse_events_evlist *data)
{
	
	if (!list_is_last(list->next, list))
		data->nr_groups++;
}



#line 116 "util/parse-events-bison.c"

#ifndef YYDEBUG
# define YYDEBUG 0
#endif

#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   enum yytokentype {
     PE_START_EVENTS = 258,
     PE_START_TERMS = 259,
     PE_VALUE = 260,
     PE_VALUE_SYM_HW = 261,
     PE_VALUE_SYM_SW = 262,
     PE_RAW = 263,
     PE_TERM = 264,
     PE_EVENT_NAME = 265,
     PE_NAME = 266,
     PE_MODIFIER_EVENT = 267,
     PE_MODIFIER_BP = 268,
     PE_NAME_CACHE_TYPE = 269,
     PE_NAME_CACHE_OP_RESULT = 270,
     PE_PREFIX_MEM = 271,
     PE_PREFIX_RAW = 272,
     PE_PREFIX_GROUP = 273,
     PE_ERROR = 274
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

#line 81 "util/parse-events.y"

	char *str;
	u64 num;
	struct list_head *head;
	struct parse_events_term *term;



#line 180 "util/parse-events-bison.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE 
# define YYSTYPE_IS_DECLARED 1
#endif




#line 192 "util/parse-events-bison.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> 
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> 
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) 
#endif

#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE


# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> 
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> 
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> 
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   
#  define YYSTACK_FREE(Ptr) do { ; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM 4032 
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> 
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); 
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); 
#   endif
#  endif
# endif
#endif 


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif
#endif 

#define YYFINAL  35
#define YYLAST   87

#define YYNTOKENS  27
#define YYNNTS  24
#define YYNRULES  55
#define YYNSTATES  87

#define YYUNDEFTOK  2
#define YYMAXUTOK   274

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    20,    25,     2,    24,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    21,     2,
       2,    26,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    22,     2,    23,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19
};

#if YYDEBUG
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     6,     9,    11,    15,    19,    21,    23,
      27,    29,    34,    38,    42,    44,    46,    49,    51,    54,
      56,    58,    60,    63,    65,    68,    71,    74,    79,    81,
      83,    88,    91,    97,   101,   103,   109,   113,   117,   121,
     123,   125,   129,   131,   135,   139,   143,   145,   147,   151,
     155,   157,   159,   160,   162,   164
};

static const yytype_int8 yyrhs[] =
{
      28,     0,    -1,     3,    29,    -1,     4,    46,    -1,    30,
      -1,    30,    20,    31,    -1,    30,    20,    34,    -1,    31,
      -1,    34,    -1,    32,    21,    12,    -1,    32,    -1,    11,
      22,    33,    23,    -1,    22,    33,    23,    -1,    33,    20,
      34,    -1,    34,    -1,    35,    -1,    36,    12,    -1,    36,
      -1,    10,    37,    -1,    37,    -1,    38,    -1,    40,    -1,
      41,    49,    -1,    42,    -1,    43,    49,    -1,    44,    49,
      -1,    45,    49,    -1,    11,    24,    47,    24,    -1,     6,
      -1,     7,    -1,    39,    24,    47,    24,    -1,    39,    50,
      -1,    14,    25,    15,    25,    15,    -1,    14,    25,    15,
      -1,    14,    -1,    16,     5,    21,    13,    49,    -1,    16,
       5,    49,    -1,    11,    21,    11,    -1,     5,    21,     5,
      -1,     8,    -1,    47,    -1,    47,    20,    48,    -1,    48,
      -1,    11,    26,    11,    -1,    11,    26,     5,    -1,    11,
      26,     6,    -1,    11,    -1,     6,    -1,     9,    26,    11,
      -1,     9,    26,     5,    -1,     9,    -1,    21,    -1,    -1,
      24,    -1,    21,    -1,    -1
};

static const yytype_uint16 yyrline[] =
{
       0,    90,    90,    92,    94,   102,   111,   120,   122,   125,
     133,   136,   145,   155,   164,   166,   169,   182,   185,   192,
     194,   195,   196,   197,   198,   199,   200,   203,   215,   217,
     220,   234,   248,   258,   268,   279,   290,   302,   313,   324,
     335,   342,   352,   364,   373,   382,   391,   400,   409,   417,
     425,   433,   433,   435,   435,   435
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "PE_START_EVENTS", "PE_START_TERMS",
  "PE_VALUE", "PE_VALUE_SYM_HW", "PE_VALUE_SYM_SW", "PE_RAW", "PE_TERM",
  "PE_EVENT_NAME", "PE_NAME", "PE_MODIFIER_EVENT", "PE_MODIFIER_BP",
  "PE_NAME_CACHE_TYPE", "PE_NAME_CACHE_OP_RESULT", "PE_PREFIX_MEM",
  "PE_PREFIX_RAW", "PE_PREFIX_GROUP", "PE_ERROR", "','", "':'", "'{'",
  "'}'", "'/'", "'-'", "'='", "$accept", "start", "start_events", "groups",
  "group", "group_def", "events", "event", "event_mod", "event_name",
  "event_def", "event_pmu", "value_sym", "event_legacy_symbol",
  "event_legacy_cache", "event_legacy_mem", "event_legacy_tracepoint",
  "event_legacy_numeric", "event_legacy_raw", "start_terms",
  "event_config", "event_term", "sep_dc", "sep_slash_dc", 0
};
#endif

# ifdef YYPRINT
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
      44,    58,   123,   125,    47,    45,    61
};
# endif

static const yytype_uint8 yyr1[] =
{
       0,    27,    28,    28,    29,    30,    30,    30,    30,    31,
      31,    32,    32,    33,    33,    34,    35,    35,    36,    36,
      37,    37,    37,    37,    37,    37,    37,    38,    39,    39,
      40,    40,    41,    41,    41,    42,    42,    43,    44,    45,
      46,    47,    47,    48,    48,    48,    48,    48,    48,    48,
      48,    49,    49,    50,    50,    50
};

static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     2,     1,     3,     3,     1,     1,     3,
       1,     4,     3,     3,     1,     1,     2,     1,     2,     1,
       1,     1,     2,     1,     2,     2,     2,     4,     1,     1,
       4,     2,     5,     3,     1,     5,     3,     3,     3,     1,
       1,     3,     1,     3,     3,     3,     1,     1,     3,     3,
       1,     1,     0,     1,     1,     0
};

static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     0,     0,    28,    29,    39,     0,     0,
      34,     0,     0,     2,     4,     7,    10,     8,    15,    17,
      19,    20,    55,    21,    52,    23,    52,    52,    52,    47,
      50,    46,     3,    40,    42,     1,     0,     0,    18,     0,
       0,     0,     0,    52,     0,    14,     0,     0,    16,    54,
      53,    31,    51,    22,    24,    25,    26,     0,     0,     0,
      38,    37,     0,     0,    33,    51,    36,     0,    12,     5,
       6,     9,     0,    49,    48,    44,    45,    43,    41,    11,
      27,     0,    52,    13,    30,    32,    35
};

static const yytype_int8 yydefgoto[] =
{
      -1,     3,    13,    14,    15,    16,    44,    45,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    32,
      33,    34,    53,    51
};

#define YYPACT_NINF -38
static const yytype_int8 yypact[] =
{
      16,     0,    41,    12,    19,   -38,   -38,   -38,    30,    32,
      14,    44,    18,   -38,    31,   -38,    43,   -38,   -38,    49,
     -38,   -38,     9,   -38,    46,   -38,    46,    46,    46,   -38,
      42,    45,   -38,    50,   -38,   -38,    60,    34,   -38,    58,
      18,    41,    57,    52,    39,   -38,     0,    62,   -38,   -38,
      41,   -38,   -38,   -38,   -38,   -38,   -38,     4,    37,    41,
     -38,   -38,    40,    -3,    51,    64,   -38,    18,   -38,   -38,
     -38,   -38,     7,   -38,   -38,   -38,   -38,   -38,   -38,   -38,
     -38,    63,    46,   -38,   -38,   -38,   -38
};

static const yytype_int8 yypgoto[] =
{
     -38,   -38,   -38,   -38,    29,   -38,    47,    -1,   -38,   -38,
      71,   -38,   -38,   -38,   -38,   -38,   -38,   -38,   -38,   -38,
     -37,    21,   -25,   -38
};

#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      17,    54,    55,    56,    63,     4,     5,     6,     7,    73,
       8,     9,    35,    72,    10,    74,    11,    59,    66,     1,
       2,    80,    12,     4,     5,     6,     7,    59,     8,    37,
      49,    84,    10,    50,    11,     4,     5,     6,     7,    42,
      36,    37,    75,    76,    10,    70,    11,    29,    77,    43,
      30,    46,    31,    39,    40,    39,    41,    86,    41,    67,
      67,    48,    68,    79,    47,    60,    83,    52,    57,    61,
      59,    58,    64,    65,    71,    69,    81,    82,    85,    38,
      78,     0,     0,     0,     0,     0,     0,    62
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-38))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

static const yytype_int8 yycheck[] =
{
       1,    26,    27,    28,    41,     5,     6,     7,     8,     5,
      10,    11,     0,    50,    14,    11,    16,    20,    43,     3,
       4,    24,    22,     5,     6,     7,     8,    20,    10,    11,
      21,    24,    14,    24,    16,     5,     6,     7,     8,    25,
      21,    11,     5,     6,    14,    46,    16,     6,    11,     5,
       9,    20,    11,    21,    22,    21,    24,    82,    24,    20,
      20,    12,    23,    23,    21,     5,    67,    21,    26,    11,
      20,    26,    15,    21,    12,    46,    25,    13,    15,     8,
      59,    -1,    -1,    -1,    -1,    -1,    -1,    40
};

static const yytype_uint8 yystos[] =
{
       0,     3,     4,    28,     5,     6,     7,     8,    10,    11,
      14,    16,    22,    29,    30,    31,    32,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,     6,
       9,    11,    46,    47,    48,     0,    21,    11,    37,    21,
      22,    24,    25,     5,    33,    34,    20,    21,    12,    21,
      24,    50,    21,    49,    49,    49,    49,    26,    26,    20,
       5,    11,    33,    47,    15,    21,    49,    20,    23,    31,
      34,    12,    47,     5,    11,     5,     6,    11,    48,    23,
      24,    25,    13,    34,    24,    15,    49
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab



#define YYFAIL		goto yyerrlab
#if defined YYFAIL
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (_data, scanner, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256



#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif



#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif



#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, scanner)
#endif

#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> 
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, _data, scanner); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))



#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, void *_data, void *scanner)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, _data, scanner)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    void *_data;
    void *scanner;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (_data);
  YYUSE (scanner);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}



#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, void *_data, void *scanner)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, _data, scanner)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    void *_data;
    void *scanner;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, _data, scanner);
  YYFPRINTF (yyoutput, ")");
}


#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))



#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, void *_data, void *scanner)
#else
static void
yy_reduce_print (yyvsp, yyrule, _data, scanner)
    YYSTYPE *yyvsp;
    int yyrule;
    void *_data;
    void *scanner;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , _data, scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, _data, scanner); \
} while (YYID (0))

int yydebug;
#else 
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif 


#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif


#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  
  const char *yyformat = 0;
  
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  int yycount = 0;

  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          int yyxbegin = yyn < 0 ? -yyn : 0;
          
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif 


#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, void *_data, void *scanner)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, _data, scanner)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    void *_data;
    void *scanner;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (_data);
  YYUSE (scanner);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else 
#if defined __STDC__ || defined __cplusplus
int yyparse (void *_data, void *scanner);
#else
int yyparse ();
#endif
#endif 



#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else 
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *_data, void *scanner)
#else
int
yyparse (_data, scanner)
    void *_data;
    void *scanner;
#endif
#endif
{
int yychar;

YYSTYPE yylval;

    
    int yynerrs;

    int yystate;
    
    int yyerrstatus;


    
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  
  int yytoken;
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; 

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

 yynewstate:
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else 
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif 

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

yybackup:


  
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  

  
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyerrstatus)
    yyerrstatus--;

  
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


yyreduce:
  
  yylen = yyr2[yyn];

  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 4:

#line 95 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;

	parse_events_update_lists((yyvsp[(1) - (1)].head), &data->list);
}
    break;

  case 5:

#line 103 "util/parse-events.y"
    {
	struct list_head *list  = (yyvsp[(1) - (3)].head);
	struct list_head *group = (yyvsp[(3) - (3)].head);

	parse_events_update_lists(group, list);
	(yyval.head) = list;
}
    break;

  case 6:

#line 112 "util/parse-events.y"
    {
	struct list_head *list  = (yyvsp[(1) - (3)].head);
	struct list_head *event = (yyvsp[(3) - (3)].head);

	parse_events_update_lists(event, list);
	(yyval.head) = list;
}
    break;

  case 9:

#line 126 "util/parse-events.y"
    {
	struct list_head *list = (yyvsp[(1) - (3)].head);

	ABORT_ON(parse_events__modifier_group(list, (yyvsp[(3) - (3)].str)));
	(yyval.head) = list;
}
    break;

  case 11:

#line 137 "util/parse-events.y"
    {
	struct list_head *list = (yyvsp[(3) - (4)].head);

	inc_group_count(list, _data);
	parse_events__set_leader((yyvsp[(1) - (4)].str), list);
	(yyval.head) = list;
}
    break;

  case 12:

#line 146 "util/parse-events.y"
    {
	struct list_head *list = (yyvsp[(2) - (3)].head);

	inc_group_count(list, _data);
	parse_events__set_leader(NULL, list);
	(yyval.head) = list;
}
    break;

  case 13:

#line 156 "util/parse-events.y"
    {
	struct list_head *event = (yyvsp[(3) - (3)].head);
	struct list_head *list  = (yyvsp[(1) - (3)].head);

	parse_events_update_lists(event, list);
	(yyval.head) = list;
}
    break;

  case 16:

#line 170 "util/parse-events.y"
    {
	struct list_head *list = (yyvsp[(1) - (2)].head);

	ABORT_ON(parse_events__modifier_event(list, (yyvsp[(2) - (2)].str), false));
	(yyval.head) = list;
}
    break;

  case 18:

#line 186 "util/parse-events.y"
    {
	ABORT_ON(parse_events_name((yyvsp[(2) - (2)].head), (yyvsp[(1) - (2)].str)));
	free((yyvsp[(1) - (2)].str));
	(yyval.head) = (yyvsp[(2) - (2)].head);
}
    break;

  case 27:

#line 204 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_pmu(list, &data->idx, (yyvsp[(1) - (4)].str), (yyvsp[(3) - (4)].head)));
	parse_events__free_terms((yyvsp[(3) - (4)].head));
	(yyval.head) = list;
}
    break;

  case 30:

#line 221 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;
	int type = (yyvsp[(1) - (4)].num) >> 16;
	int config = (yyvsp[(1) - (4)].num) & 255;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_numeric(list, &data->idx,
					  type, config, (yyvsp[(3) - (4)].head)));
	parse_events__free_terms((yyvsp[(3) - (4)].head));
	(yyval.head) = list;
}
    break;

  case 31:

#line 235 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;
	int type = (yyvsp[(1) - (2)].num) >> 16;
	int config = (yyvsp[(1) - (2)].num) & 255;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_numeric(list, &data->idx,
					  type, config, NULL));
	(yyval.head) = list;
}
    break;

  case 32:

#line 249 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_cache(list, &data->idx, (yyvsp[(1) - (5)].str), (yyvsp[(3) - (5)].str), (yyvsp[(5) - (5)].str)));
	(yyval.head) = list;
}
    break;

  case 33:

#line 259 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_cache(list, &data->idx, (yyvsp[(1) - (3)].str), (yyvsp[(3) - (3)].str), NULL));
	(yyval.head) = list;
}
    break;

  case 34:

#line 269 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_cache(list, &data->idx, (yyvsp[(1) - (1)].str), NULL, NULL));
	(yyval.head) = list;
}
    break;

  case 35:

#line 280 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_breakpoint(list, &data->idx,
					     (void *)(uintptr_t) (yyvsp[(2) - (5)].num), (yyvsp[(4) - (5)].str)));
	(yyval.head) = list;
}
    break;

  case 36:

#line 291 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_breakpoint(list, &data->idx,
					     (void *)(uintptr_t) (yyvsp[(2) - (3)].num), NULL));
	(yyval.head) = list;
}
    break;

  case 37:

#line 303 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_tracepoint(list, &data->idx, (yyvsp[(1) - (3)].str), (yyvsp[(3) - (3)].str)));
	(yyval.head) = list;
}
    break;

  case 38:

#line 314 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_numeric(list, &data->idx, (u32)(yyvsp[(1) - (3)].num), (yyvsp[(3) - (3)].num), NULL));
	(yyval.head) = list;
}
    break;

  case 39:

#line 325 "util/parse-events.y"
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_numeric(list, &data->idx,
					  PERF_TYPE_RAW, (yyvsp[(1) - (1)].num), NULL));
	(yyval.head) = list;
}
    break;

  case 40:

#line 336 "util/parse-events.y"
    {
	struct parse_events_terms *data = _data;
	data->terms = (yyvsp[(1) - (1)].head);
}
    break;

  case 41:

#line 343 "util/parse-events.y"
    {
	struct list_head *head = (yyvsp[(1) - (3)].head);
	struct parse_events_term *term = (yyvsp[(3) - (3)].term);

	ABORT_ON(!head);
	list_add_tail(&term->list, head);
	(yyval.head) = (yyvsp[(1) - (3)].head);
}
    break;

  case 42:

#line 353 "util/parse-events.y"
    {
	struct list_head *head = malloc(sizeof(*head));
	struct parse_events_term *term = (yyvsp[(1) - (1)].term);

	ABORT_ON(!head);
	INIT_LIST_HEAD(head);
	list_add_tail(&term->list, head);
	(yyval.head) = head;
}
    break;

  case 43:

#line 365 "util/parse-events.y"
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__str(&term, PARSE_EVENTS__TERM_TYPE_USER,
					(yyvsp[(1) - (3)].str), (yyvsp[(3) - (3)].str)));
	(yyval.term) = term;
}
    break;

  case 44:

#line 374 "util/parse-events.y"
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__num(&term, PARSE_EVENTS__TERM_TYPE_USER,
					(yyvsp[(1) - (3)].str), (yyvsp[(3) - (3)].num)));
	(yyval.term) = term;
}
    break;

  case 45:

#line 383 "util/parse-events.y"
    {
	struct parse_events_term *term;
	int config = (yyvsp[(3) - (3)].num) & 255;

	ABORT_ON(parse_events_term__sym_hw(&term, (yyvsp[(1) - (3)].str), config));
	(yyval.term) = term;
}
    break;

  case 46:

#line 392 "util/parse-events.y"
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__num(&term, PARSE_EVENTS__TERM_TYPE_USER,
					(yyvsp[(1) - (1)].str), 1));
	(yyval.term) = term;
}
    break;

  case 47:

#line 401 "util/parse-events.y"
    {
	struct parse_events_term *term;
	int config = (yyvsp[(1) - (1)].num) & 255;

	ABORT_ON(parse_events_term__sym_hw(&term, NULL, config));
	(yyval.term) = term;
}
    break;

  case 48:

#line 410 "util/parse-events.y"
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__str(&term, (int)(yyvsp[(1) - (3)].num), NULL, (yyvsp[(3) - (3)].str)));
	(yyval.term) = term;
}
    break;

  case 49:

#line 418 "util/parse-events.y"
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__num(&term, (int)(yyvsp[(1) - (3)].num), NULL, (yyvsp[(3) - (3)].num)));
	(yyval.term) = term;
}
    break;

  case 50:

#line 426 "util/parse-events.y"
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__num(&term, (int)(yyvsp[(1) - (1)].num), NULL, 1));
	(yyval.term) = term;
}
    break;



#line 1927 "util/parse-events-bison.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


yyerrlab:
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (_data, scanner, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (_data, scanner, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {

      if (yychar <= YYEOF)
	{
	  
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, _data, scanner);
	  yychar = YYEMPTY;
	}
    }

  goto yyerrlab1;


yyerrorlab:

  if ( 0)
     goto yyerrorlab;

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


yyerrlab1:
  yyerrstatus = 3;	

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, _data, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


yyacceptlab:
  yyresult = 0;
  goto yyreturn;

yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
yyexhaustedlab:
  yyerror (_data, scanner, YY_("memory exhausted"));
  yyresult = 2;
  
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, _data, scanner);
    }
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, _data, scanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  
  return YYID (yyresult);
}



#line 437 "util/parse-events.y"


void parse_events_error(void *data __maybe_unused, void *scanner __maybe_unused,
			char const *msg __maybe_unused)
{
}

