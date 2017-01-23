
#define YYBISON 1  

#define yyparse __gettextparse
#define yylex __gettextlex
#define yyerror __gettexterror
#define yylval __gettextlval
#define yychar __gettextchar
#define yydebug __gettextdebug
#define yynerrs __gettextnerrs
# define	EQUOP2	257
# define	CMPOP2	258
# define	ADDOP2	259
# define	MULOP2	260
# define	NUMBER	261

#line 1 "plural.y"

/* Expression parsing for plural form selection.
   Copyright (C) 2000-2001, 2003 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@cygnus.com>, 2000.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#if defined _AIX && !defined __GNUC__
 #pragma alloca
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include "plural-exp.h"

#ifndef _LIBC
# define __gettextparse PLURAL_PARSE
#endif

#define YYLEX_PARAM	&((struct parse_args *) arg)->cp
#define YYPARSE_PARAM	arg

#line 49 "plural.y"
#ifndef YYSTYPE
typedef union {
  unsigned long int num;
  enum operator op;
  struct expression *exp;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#line 55 "plural.y"

static int yylex (YYSTYPE *lval, const char **pexp);
static void yyerror (const char *str);


static struct expression *
new_exp (int nargs, enum operator op, struct expression * const *args)
{
  int i;
  struct expression *newp;

  
  for (i = nargs - 1; i >= 0; i--)
    if (args[i] == NULL)
      goto fail;

  
  newp = (struct expression *) malloc (sizeof (*newp));
  if (newp != NULL)
    {
      newp->nargs = nargs;
      newp->operation = op;
      for (i = nargs - 1; i >= 0; i--)
	newp->val.args[i] = args[i];
      return newp;
    }

 fail:
  for (i = nargs - 1; i >= 0; i--)
    FREE_EXPRESSION (args[i]);

  return NULL;
}

static inline struct expression *
new_exp_0 (enum operator op)
{
  return new_exp (0, op, NULL);
}

static inline struct expression *
new_exp_1 (enum operator op, struct expression *right)
{
  struct expression *args[1];

  args[0] = right;
  return new_exp (1, op, args);
}

static struct expression *
new_exp_2 (enum operator op, struct expression *left, struct expression *right)
{
  struct expression *args[2];

  args[0] = left;
  args[1] = right;
  return new_exp (2, op, args);
}

static inline struct expression *
new_exp_3 (enum operator op, struct expression *bexp,
	   struct expression *tbranch, struct expression *fbranch)
{
  struct expression *args[3];

  args[0] = bexp;
  args[1] = tbranch;
  args[2] = fbranch;
  return new_exp (3, op, args);
}

#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		27
#define	YYFLAG		-32768
#define	YYNTBASE	16

#define YYTRANSLATE(x) ((unsigned)(x) <= 261 ? yytranslate[x] : 18)

static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    10,     2,     2,     2,     2,     5,     2,
      14,    15,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    12,     2,
       2,     2,     2,     3,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      13,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     4,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     1,     6,     7,     8,
       9,    11
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     2,     8,    12,    16,    20,    24,    28,    32,
      35,    37,    39
};
static const short yyrhs[] =
{
      17,     0,    17,     3,    17,    12,    17,     0,    17,     4,
      17,     0,    17,     5,    17,     0,    17,     6,    17,     0,
      17,     7,    17,     0,    17,     8,    17,     0,    17,     9,
      17,     0,    10,    17,     0,    13,     0,    11,     0,    14,
      17,    15,     0
};

#endif

#if YYDEBUG
static const short yyrline[] =
{
       0,   150,   158,   162,   166,   170,   174,   178,   182,   186,
     190,   194,   199
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

static const char *const yytname[] =
{
  "$", "error", "$undefined.", "'?'", "'|'", "'&'", "EQUOP2", "CMPOP2",
  "ADDOP2", "MULOP2", "'!'", "NUMBER", "':'", "'n'", "'('", "')'",
  "start", "exp", 0
};
#endif

static const short yyr1[] =
{
       0,    16,    17,    17,    17,    17,    17,    17,    17,    17,
      17,    17,    17
};

static const short yyr2[] =
{
       0,     1,     5,     3,     3,     3,     3,     3,     3,     2,
       1,     1,     3
};

static const short yydefact[] =
{
       0,     0,    11,    10,     0,     1,     9,     0,     0,     0,
       0,     0,     0,     0,     0,    12,     0,     3,     4,     5,
       6,     7,     8,     0,     2,     0,     0,     0
};

static const short yydefgoto[] =
{
      25,     5
};

static const short yypact[] =
{
      -9,    -9,-32768,-32768,    -9,    34,-32768,    11,    -9,    -9,
      -9,    -9,    -9,    -9,    -9,-32768,    24,    39,    43,    16,
      26,    -3,-32768,    -9,    34,    21,    53,-32768
};

static const short yypgoto[] =
{
  -32768,    -1
};


#define	YYLAST		53


static const short yytable[] =
{
       6,     1,     2,     7,     3,     4,    14,    16,    17,    18,
      19,    20,    21,    22,     8,     9,    10,    11,    12,    13,
      14,    26,    24,    12,    13,    14,    15,     8,     9,    10,
      11,    12,    13,    14,    13,    14,    23,     8,     9,    10,
      11,    12,    13,    14,    10,    11,    12,    13,    14,    11,
      12,    13,    14,    27
};

static const short yycheck[] =
{
       1,    10,    11,     4,    13,    14,     9,     8,     9,    10,
      11,    12,    13,    14,     3,     4,     5,     6,     7,     8,
       9,     0,    23,     7,     8,     9,    15,     3,     4,     5,
       6,     7,     8,     9,     8,     9,    12,     3,     4,     5,
       6,     7,     8,     9,     5,     6,     7,     8,     9,     6,
       7,     8,     9,     0
};
#define YYPURE 1

#line 3 "/usr/local/share/bison/bison.simple"

/* Skeleton output parser for bison,

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */


/* This is the parser code that is written into each bison parser when
   the %semantic_parser declaration is not specified in the grammar.
   It was written by Richard Stallman by simplifying the hairy parser
   used when %semantic_parser is specified.  */


#if ! defined (yyoverflow) || defined (YYERROR_VERBOSE)


# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   
#  define YYSTACK_FREE(Ptr) do { ; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> 
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif 


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
# if YYLSP_NEEDED
  YYLTYPE yyls;
# endif
};

# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

# if YYLSP_NEEDED
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAX)
# else
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)
# endif

# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif


#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> 
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256



#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)       	\
   Current.last_line   = Rhs[N].last_line;	\
   Current.last_column = Rhs[N].last_column;
#endif



#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, &yylloc, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval, &yylloc)
#  endif
# else 
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval)
#  endif
# endif 
#else 
# define YYLEX			yylex ()
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
} while (0)
int yydebug;
#else 
# define YYDPRINTF(Args)
#endif 

#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif


#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

#ifdef YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/local/share/bison/bison.simple"



#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL
# else
#  define YYPARSE_PARAM_ARG YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
# endif
#else 
# define YYPARSE_PARAM_ARG
# define YYPARSE_PARAM_DECL
#endif 

#ifdef __GNUC__
# ifdef YYPARSE_PARAM
int yyparse (void *);
# else
int yyparse (void);
# endif
#endif


#define YY_DECL_NON_LSP_VARIABLES			\
				\
int yychar;						\
							\
	\
YYSTYPE yylval;						\
							\
			\
int yynerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
	\
YYLTYPE yylloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif



#if !YYPURE
YY_DECL_VARIABLES
#endif  

int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  
#if YYPURE
  YY_DECL_VARIABLES
#endif  

  register int yystate;
  register int yyn;
  int yyresult;
  
  int yyerrstatus;
  
  int yychar1 = 0;


  
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;

#if YYLSP_NEEDED
  
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
# define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  YYSIZE_T yystacksize = YYINITDEPTH;


  YYSTYPE yyval;
#if YYLSP_NEEDED
  YYLTYPE yyloc;
#endif

  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		


  yyssp = yyss;
  yyvsp = yyvs;
#if YYLSP_NEEDED
  yylsp = yyls;
#endif
  goto yysetstate;

 yynewstate:
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;

# if YYLSP_NEEDED
	YYLTYPE *yyls1 = yyls;
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
# else
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);
# endif
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else 
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      
      if (yystacksize >= YYMAXDEPTH)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (yyls);
# endif
# undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif 

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
#if YYLSP_NEEDED
      yylsp = yyls + yysize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;


yybackup:


  

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  


  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  

  if (yychar <= 0)		
    {
      yychar1 = 0;
      yychar = YYEOF;		

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

#if YYDEBUG
      if (yydebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     yychar, yytname[yychar1]);
# ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];


  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

  
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


yyreduce:
  
  yylen = yyr2[yyn];

  yyval = yyvsp[1-yylen];

#if YYLSP_NEEDED
  yyloc = yylsp[1-yylen];
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
#endif

#if YYDEBUG
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn, yyrline[yyn]);

      
      for (yyi = yyprhs[yyn]; yyrhs[yyi] > 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif

  switch (yyn) {

case 1:
#line 151 "plural.y"
{
	    if (yyvsp[0].exp == NULL)
	      YYABORT;
	    ((struct parse_args *) arg)->res = yyvsp[0].exp;
	  }
    break;
case 2:
#line 159 "plural.y"
{
	    yyval.exp = new_exp_3 (qmop, yyvsp[-4].exp, yyvsp[-2].exp, yyvsp[0].exp);
	  }
    break;
case 3:
#line 163 "plural.y"
{
	    yyval.exp = new_exp_2 (lor, yyvsp[-2].exp, yyvsp[0].exp);
	  }
    break;
case 4:
#line 167 "plural.y"
{
	    yyval.exp = new_exp_2 (land, yyvsp[-2].exp, yyvsp[0].exp);
	  }
    break;
case 5:
#line 171 "plural.y"
{
	    yyval.exp = new_exp_2 (yyvsp[-1].op, yyvsp[-2].exp, yyvsp[0].exp);
	  }
    break;
case 6:
#line 175 "plural.y"
{
	    yyval.exp = new_exp_2 (yyvsp[-1].op, yyvsp[-2].exp, yyvsp[0].exp);
	  }
    break;
case 7:
#line 179 "plural.y"
{
	    yyval.exp = new_exp_2 (yyvsp[-1].op, yyvsp[-2].exp, yyvsp[0].exp);
	  }
    break;
case 8:
#line 183 "plural.y"
{
	    yyval.exp = new_exp_2 (yyvsp[-1].op, yyvsp[-2].exp, yyvsp[0].exp);
	  }
    break;
case 9:
#line 187 "plural.y"
{
	    yyval.exp = new_exp_1 (lnot, yyvsp[0].exp);
	  }
    break;
case 10:
#line 191 "plural.y"
{
	    yyval.exp = new_exp_0 (var);
	  }
    break;
case 11:
#line 195 "plural.y"
{
	    if ((yyval.exp = new_exp_0 (num)) != NULL)
	      yyval.exp->val.num = yyvsp[0].num;
	  }
    break;
case 12:
#line 200 "plural.y"
{
	    yyval.exp = yyvsp[-1].exp;
	  }
    break;
}

#line 705 "/usr/local/share/bison/bison.simple"


  yyvsp -= yylen;
  yyssp -= yylen;
#if YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;
#if YYLSP_NEEDED
  *++yylsp = yyloc;
#endif


  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;


yyerrlab:
  
  if (!yyerrstatus)
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[YYTRANSLATE (yychar)]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[YYTRANSLATE (yychar)]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif 
	yyerror ("parse error");
    }
  goto yyerrlab1;


yyerrlab1:
  if (yyerrstatus == 3)
    {

      
      if (yychar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yychar = YYEMPTY;
    }


  yyerrstatus = 3;		

  goto yyerrhandle;


yyerrdefault:
#if 0

  
  yyn = yydefact[yystate];
  if (yyn)
    goto yydefault;
#endif


yyerrpop:
  if (yyssp == yyss)
    YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#if YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

yyerrhandle:
  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;


yyacceptlab:
  yyresult = 0;
  goto yyreturn;

yyabortlab:
  yyresult = 1;
  goto yyreturn;

yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}
#line 205 "plural.y"


void
internal_function
FREE_EXPRESSION (struct expression *exp)
{
  if (exp == NULL)
    return;

  
  switch (exp->nargs)
    {
    case 3:
      FREE_EXPRESSION (exp->val.args[2]);
      
    case 2:
      FREE_EXPRESSION (exp->val.args[1]);
      
    case 1:
      FREE_EXPRESSION (exp->val.args[0]);
      
    default:
      break;
    }

  free (exp);
}


static int
yylex (YYSTYPE *lval, const char **pexp)
{
  const char *exp = *pexp;
  int result;

  while (1)
    {
      if (exp[0] == '\0')
	{
	  *pexp = exp;
	  return YYEOF;
	}

      if (exp[0] != ' ' && exp[0] != '\t')
	break;

      ++exp;
    }

  result = *exp++;
  switch (result)
    {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      {
	unsigned long int n = result - '0';
	while (exp[0] >= '0' && exp[0] <= '9')
	  {
	    n *= 10;
	    n += exp[0] - '0';
	    ++exp;
	  }
	lval->num = n;
	result = NUMBER;
      }
      break;

    case '=':
      if (exp[0] == '=')
	{
	  ++exp;
	  lval->op = equal;
	  result = EQUOP2;
	}
      else
	result = YYERRCODE;
      break;

    case '!':
      if (exp[0] == '=')
	{
	  ++exp;
	  lval->op = not_equal;
	  result = EQUOP2;
	}
      break;

    case '&':
    case '|':
      if (exp[0] == result)
	++exp;
      else
	result = YYERRCODE;
      break;

    case '<':
      if (exp[0] == '=')
	{
	  ++exp;
	  lval->op = less_or_equal;
	}
      else
	lval->op = less_than;
      result = CMPOP2;
      break;

    case '>':
      if (exp[0] == '=')
	{
	  ++exp;
	  lval->op = greater_or_equal;
	}
      else
	lval->op = greater_than;
      result = CMPOP2;
      break;

    case '*':
      lval->op = mult;
      result = MULOP2;
      break;

    case '/':
      lval->op = divide;
      result = MULOP2;
      break;

    case '%':
      lval->op = module;
      result = MULOP2;
      break;

    case '+':
      lval->op = plus;
      result = ADDOP2;
      break;

    case '-':
      lval->op = minus;
      result = ADDOP2;
      break;

    case 'n':
    case '?':
    case ':':
    case '(':
    case ')':
      
      break;

    case ';':
    case '\n':
    case '\0':
      
      --exp;
      result = YYEOF;
      break;

    default:
      result = YYERRCODE;
#if YYDEBUG != 0
      --exp;
#endif
      break;
    }

  *pexp = exp;

  return result;
}


static void
yyerror (const char *str)
{
  
}
