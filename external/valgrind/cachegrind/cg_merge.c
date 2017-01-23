

/*
  This file is part of Cachegrind, a Valgrind tool for cache
  profiling programs.

  Copyright (C) 2002-2013 Nicholas Nethercote
     njn@valgrind.org

  AVL tree code derived from
  ANSI C Library for maintainance of AVL Balanced Trees
  (C) 2000 Daniel Nagy, Budapest University of Technology and Economics
  Released under GNU General Public License (GPL) version 2

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307, USA.

  The GNU General Public License is contained in the file COPYING.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

typedef  signed long   Word;
typedef  unsigned long UWord;
typedef  unsigned char Bool;
#define True ((Bool)1)
#define False ((Bool)0)
typedef  signed int    Int;
typedef  unsigned int  UInt;
typedef  unsigned long long int ULong;
typedef  signed char   Char;
typedef  size_t        SizeT;



typedef  struct _WordFM  WordFM; 

void initFM ( WordFM* t, 
              void*   (*alloc_nofail)( SizeT ),
              void    (*dealloc)(void*),
              Word    (*kCmp)(Word,Word) );

WordFM* newFM( void* (*alloc_nofail)( SizeT ),
               void  (*dealloc)(void*),
               Word  (*kCmp)(Word,Word) );

void deleteFM ( WordFM*, void(*kFin)(Word), void(*vFin)(Word) );

void addToFM ( WordFM* fm, Word k, Word v );

Bool delFromFM ( WordFM* fm, Word* oldV, Word key );

Bool lookupFM ( WordFM* fm, Word* valP, Word key );

Word sizeFM ( WordFM* fm );

void initIterFM ( WordFM* fm );

Bool nextIterFM ( WordFM* fm, Word* pKey, Word* pVal );

void doneIterFM ( WordFM* fm );

WordFM* dopyFM ( WordFM* fm, Word(*dopyK)(Word), Word(*dopyV)(Word) );



static const char* argv0 = "cg_merge";

typedef
   struct {
      FILE* fp;
      UInt  lno;
      char* filename;
   }
   SOURCE;

static void printSrcLoc ( SOURCE* s )
{
   fprintf(stderr, "%s: near %s line %u\n", argv0, s->filename, s->lno-1);
}

__attribute__((noreturn))
static void mallocFail ( SOURCE* s, const char* who )
{
   fprintf(stderr, "%s: out of memory in %s\n", argv0, who );
   printSrcLoc( s );
   exit(2);
}

__attribute__((noreturn))
static void parseError ( SOURCE* s, const char* msg )
{
   fprintf(stderr, "%s: parse error: %s\n", argv0, msg );
   printSrcLoc( s );
   exit(1);
}

__attribute__((noreturn))
static void barf ( SOURCE* s, const char* msg )
{
   fprintf(stderr, "%s: %s\n", argv0, msg );
   printSrcLoc( s );
   exit(1);
}

// The line is allocated dynamically but will be overwritten with
static const char *readline ( SOURCE* s )
{
   static char  *line = NULL;
   static size_t linesiz = 0;

   int ch, i = 0;

   while (1) {
      ch = getc(s->fp);
      if (ch != EOF) {
          if (i + 1 >= linesiz) {
             linesiz += 500;
             line = realloc(line, linesiz * sizeof *line);
             if (line == NULL)
                mallocFail(s, "readline:");
          }
          line[i++] = ch;
          line[i] = 0;
          if (ch == '\n') {
             line[i-1] = 0;
             s->lno++;
             break;
          }
      } else {
         if (ferror(s->fp)) {
            perror(argv0);
            barf(s, "I/O error while reading input file");
         } else {
            
            break;
         }
      }
   }
   return i == 0 ? NULL : line;
}

static Bool streqn ( const char* s1, const char* s2, size_t n )
{
   return 0 == strncmp(s1, s2, n);
}

static Bool streq ( const char* s1, const char* s2 )
{
   return 0 == strcmp(s1, s2 );
}



typedef
   struct {
      char* fi_name;
      char* fn_name;
   }
   FileFn;

typedef
   struct {
      Int n_counts;
      ULong* counts;
   }
   Counts;

typedef
   struct {
      
      char** desc_lines;

      
      char* cmd_line;

      
      char* events_line;
      Int   n_events;

      
      char* summary_line;

      WordFM* outerMap;

      
      
      Counts* summary;
   }
   CacheProfFile;

static FileFn* new_FileFn ( char* file_name, char* fn_name )
{
   FileFn* ffn = malloc(sizeof(FileFn));
   if (ffn == NULL)
      return NULL;
   ffn->fi_name = file_name;
   ffn->fn_name = fn_name;
   return ffn;
}

static void ddel_FileFn ( FileFn* ffn )
{
   if (ffn->fi_name)
      free(ffn->fi_name);
   if (ffn->fn_name)
      free(ffn->fn_name);
   memset(ffn, 0, sizeof(FileFn));
   free(ffn);
}

static FileFn* dopy_FileFn ( FileFn* ff )
{
   char *fi2, *fn2;
   fi2 = strdup(ff->fi_name);
   if (fi2 == NULL) return NULL;
   fn2 = strdup(ff->fn_name);
   if (fn2 == NULL) {
      free(fi2);
      return NULL;
   }
   return new_FileFn( fi2, fn2 );
}

static Counts* new_Counts ( Int n_counts, ULong* counts )
{
   Int i;
   Counts* cts = malloc(sizeof(Counts));
   if (cts == NULL)
      return NULL;

   assert(n_counts >= 0);
   cts->counts = malloc(n_counts * sizeof(ULong));
   if (cts->counts == NULL) {
      free(cts);
      return NULL;
   }

   cts->n_counts = n_counts;
   for (i = 0; i < n_counts; i++)
      cts->counts[i] = counts[i];

   return cts;
}

static Counts* new_Counts_Zeroed ( Int n_counts )
{
   Int i;
   Counts* cts = malloc(sizeof(Counts));
   if (cts == NULL)
      return NULL;

   assert(n_counts >= 0);
   cts->counts = malloc(n_counts * sizeof(ULong));
   if (cts->counts == NULL) {
      free(cts);
      return NULL;
   }

   cts->n_counts = n_counts;
   for (i = 0; i < n_counts; i++)
      cts->counts[i] = 0;

   return cts;
}

static void sdel_Counts ( Counts* cts )
{
   memset(cts, 0, sizeof(Counts));
   free(cts);
}

static void ddel_Counts ( Counts* cts )
{
   if (cts->counts)
      free(cts->counts);
   memset(cts, 0, sizeof(Counts));
   free(cts);
}

static Counts* dopy_Counts ( Counts* cts )
{
   return new_Counts( cts->n_counts, cts->counts );
}

static
CacheProfFile* new_CacheProfFile ( char**  desc_lines,
                                   char*   cmd_line,
                                   char*   events_line,
                                   Int     n_events,
                                   char*   summary_line,
                                   WordFM* outerMap,
                                   Counts* summary )
{
   CacheProfFile* cpf = malloc(sizeof(CacheProfFile));
   if (cpf == NULL)
      return NULL;
   cpf->desc_lines   = desc_lines;
   cpf->cmd_line     = cmd_line;
   cpf->events_line  = events_line;
   cpf->n_events     = n_events;
   cpf->summary_line = summary_line;
   cpf->outerMap     = outerMap;
   cpf->summary      = summary;
   return cpf;
}

static WordFM* dopy_InnerMap ( WordFM* innerMap )
{
   return dopyFM ( innerMap, NULL,
                             (Word(*)(Word))dopy_Counts );
}

static void ddel_InnerMap ( WordFM* innerMap )
{
   deleteFM( innerMap, NULL, (void(*)(Word))ddel_Counts );
}

static void ddel_CacheProfFile ( CacheProfFile* cpf )
{
   char** p;
   if (cpf->desc_lines) {
      for (p = cpf->desc_lines; *p; p++)
         free(*p);
      free(cpf->desc_lines);
   }
   if (cpf->cmd_line)
      free(cpf->cmd_line);
   if (cpf->events_line)
      free(cpf->events_line);
   if (cpf->summary_line)
      free(cpf->summary_line);
   if (cpf->outerMap)
      deleteFM( cpf->outerMap, (void(*)(Word))ddel_FileFn, 
                               (void(*)(Word))ddel_InnerMap );
   if (cpf->summary)
      ddel_Counts(cpf->summary);

   memset(cpf, 0, sizeof(CacheProfFile));
   free(cpf);
}

static void showCounts ( FILE* f, Counts* c )
{
   Int i;
   for (i = 0; i < c->n_counts; i++) {
      fprintf(f, "%lld ", c->counts[i]);
   }
}

static void show_CacheProfFile ( FILE* f, CacheProfFile* cpf )
{
   Int     i;
   char**  d;
   FileFn* topKey;
   WordFM* topVal;
   UWord   subKey;
   Counts* subVal;  

   for (d = cpf->desc_lines; *d; d++)
      fprintf(f, "%s\n", *d);
   fprintf(f, "%s\n", cpf->cmd_line);
   fprintf(f, "%s\n", cpf->events_line);

   initIterFM( cpf->outerMap );
   while (nextIterFM( cpf->outerMap, (Word*)(&topKey), (Word*)(&topVal) )) {
      fprintf(f, "fl=%s\nfn=%s\n", 
                 topKey->fi_name, topKey->fn_name );
      initIterFM( topVal );
      while (nextIterFM( topVal, (Word*)(&subKey), (Word*)(&subVal) )) {
         fprintf(f, "%ld   ", subKey );
         showCounts( f, subVal );
         fprintf(f, "\n");
      }
      doneIterFM( topVal );
   }
   doneIterFM( cpf->outerMap );

   
   fprintf(f, "summary:");
   for (i = 0; i < cpf->summary->n_counts; i++)
      fprintf(f, " %lld", cpf->summary->counts[i]);
   fprintf(f, "\n");
}


static Word cmp_FileFn ( Word s1, Word s2 )
{
   FileFn* ff1 = (FileFn*)s1;
   FileFn* ff2 = (FileFn*)s2;
   Word r = strcmp(ff1->fi_name, ff2->fi_name);
   if (r == 0)
      r = strcmp(ff1->fn_name, ff2->fn_name);
   return r;
}

static Word cmp_unboxed_UWord ( Word s1, Word s2 )
{
   UWord u1 = (UWord)s1;
   UWord u2 = (UWord)s2;
   if (u1 < u2) return -1;
   if (u1 > u2) return 1;
   return 0;
}


static Bool parse_ULong ( ULong* res, const char** pptr)
{
   ULong u64;
   const char* ptr = *pptr;
   while (isspace(*ptr)) ptr++;
   if (!isdigit(*ptr)) {
      *pptr = ptr;
      return False; 
   }
   u64 = 0;
   while (isdigit(*ptr)) {
      u64 = (u64 * 10) + (ULong)(*ptr - '0');
      ptr++;
   }
   *res = u64;
   *pptr = ptr;
   return True;
}

static 
Counts* splitUpCountsLine ( SOURCE* s, UWord* lnno, const char* str )
{
   Bool    ok;
   Counts* counts;
   ULong   *tmpC = NULL;
   UInt     n_tmpC = 0, tmpCsize = 0;
   while (1) {
      if (n_tmpC >= tmpCsize) {
         tmpCsize += 50;
         tmpC = realloc(tmpC, tmpCsize * sizeof *tmpC);
         if (tmpC == NULL)
            mallocFail(s, "splitUpCountsLine:");
      }
      ok = parse_ULong( &tmpC[n_tmpC], &str );
      if (!ok)
         break;
      n_tmpC++;
   }
   if (*str != 0)
      parseError(s, "garbage in counts line");
   if (lnno ? (n_tmpC < 2) : (n_tmpC < 1))
      parseError(s, "too few counts in count line");

   if (lnno) {
      *lnno = (UWord)tmpC[0];
      counts = new_Counts( n_tmpC-1, &tmpC[1] );
   } else {
      counts = new_Counts( n_tmpC, &tmpC[0] );
   }
   free(tmpC);

   return counts;
}

static void addCounts ( SOURCE* s, Counts* counts1, Counts* counts2 )
{
   Int i;
   if (counts1->n_counts != counts2->n_counts)
      parseError(s, "addCounts: inconsistent number of counts");
   for (i = 0; i < counts1->n_counts; i++)
      counts1->counts[i] += counts2->counts[i];
}

static Bool addCountsToMap ( SOURCE* s,
                             WordFM* counts_map, 
                             UWord lnno, Counts* newCounts )
{
   Counts* oldCounts;
   
   
   if (lookupFM( counts_map, (Word*)(&oldCounts), (Word)lnno )) {
      
      addCounts( s, oldCounts, newCounts );
      return True;
   } else {
      
      addToFM( counts_map, (Word)lnno, (Word)newCounts );
      return False;
   }
}

static
void handle_counts ( SOURCE* s,
                     CacheProfFile* cpf, 
                     const char* fi, const char* fn, const char* newCountsStr )
{
   WordFM* countsMap;
   Bool    freeNewCounts;
   UWord   lnno;
   Counts* newCounts;
   FileFn* topKey; 

   if (0)  printf("%s %s %s\n", fi, fn, newCountsStr );

   
   newCounts = splitUpCountsLine( s, &lnno, newCountsStr );

   
   if (newCounts->n_counts != cpf->n_events)
      goto oom;

   
   topKey = malloc(sizeof(FileFn));
   if (topKey) {
      topKey->fi_name = strdup(fi);
      topKey->fn_name = strdup(fn);
   }
   if (! (topKey && topKey->fi_name && topKey->fn_name))
      mallocFail(s, "handle_counts:");

   
   if (lookupFM( cpf->outerMap, (Word*)(&countsMap), (Word)topKey )) {
      
      freeNewCounts = addCountsToMap( s, countsMap, lnno, newCounts );
      ddel_FileFn(topKey);
   } else {
      
      countsMap = newFM( malloc, free, cmp_unboxed_UWord );
      if (!countsMap)
         goto oom;
      addToFM( cpf->outerMap, (Word)topKey, (Word)countsMap );
      freeNewCounts = addCountsToMap( s, countsMap, lnno, newCounts );
   }

   
   addCounts( s, cpf->summary, newCounts );

   
   if (freeNewCounts)
      ddel_Counts(newCounts);

   return;

  oom:
   parseError(s, "# counts doesn't match # events");
}


static CacheProfFile* parse_CacheProfFile ( SOURCE* s )
{
   Int            i;
   char**         tmp_desclines = NULL;
   unsigned       tmp_desclines_size = 0;
   char*          p;
   int            n_tmp_desclines = 0;
   CacheProfFile* cpf;
   Counts*        summaryRead; 
   char*          curr_fn = strdup("???");
   char*          curr_fl = strdup("???");
   const char*    line;

   cpf = new_CacheProfFile( NULL, NULL, NULL, 0, NULL, NULL, NULL );
   if (cpf == NULL)
      mallocFail(s, "parse_CacheProfFile(1)");

   
   while (1) {
      line = readline(s);
      if (!line) 
         break;
      if (!streqn(line, "desc: ", 6))
         break;
      if (n_tmp_desclines >= tmp_desclines_size) {
         tmp_desclines_size += 100;
         tmp_desclines = realloc(tmp_desclines,
                                 tmp_desclines_size * sizeof *tmp_desclines);
         if (tmp_desclines == NULL)
            mallocFail(s, "parse_CacheProfFile(1)");
      }
      tmp_desclines[n_tmp_desclines++] = strdup(line);
   }

   if (n_tmp_desclines == 0)
      parseError(s, "parse_CacheProfFile: no DESC lines present");

   cpf->desc_lines = malloc( (1+n_tmp_desclines) * sizeof(char*) );
   if (cpf->desc_lines == NULL)
      mallocFail(s, "parse_CacheProfFile(2)");

   cpf->desc_lines[n_tmp_desclines] = NULL;
   for (i = 0; i < n_tmp_desclines; i++)
      cpf->desc_lines[i] = tmp_desclines[i];

   
   if (!streqn(line, "cmd: ", 5))
      parseError(s, "parse_CacheProfFile: no CMD line present");

   cpf->cmd_line = strdup(line);
   if (cpf->cmd_line == NULL)
      mallocFail(s, "parse_CacheProfFile(3)");

   
   line = readline(s);
   if (!line)
      parseError(s, "parse_CacheProfFile: eof before EVENTS line");
   if (!streqn(line, "events: ", 8))
      parseError(s, "parse_CacheProfFile: no EVENTS line present");

   
   
   cpf->events_line = strdup(line);
   if (cpf->events_line == NULL)
      mallocFail(s, "parse_CacheProfFile(3)");

   cpf->n_events = 0;
   assert(cpf->events_line[6] == ':');
   for (p = &cpf->events_line[6]; *p; p++) {
      if (p[0] == ' ' && isalpha(p[1]))
         cpf->n_events++;
   }

   
   cpf->summary = new_Counts_Zeroed( cpf->n_events );
   if (cpf->summary == NULL)
      mallocFail(s, "parse_CacheProfFile(4)");

   
   cpf->outerMap = newFM ( malloc, free, cmp_FileFn );
   if (cpf->outerMap == NULL)
      mallocFail(s, "parse_CacheProfFile(5)");

   
   while (1) {
      line = readline(s);
      if (!line)
         parseError(s, "parse_CacheProfFile: eof before SUMMARY line");

      if (isdigit(line[0])) {
         handle_counts(s, cpf, curr_fl, curr_fn, line);
         continue;
      }
      else
      if (streqn(line, "fn=", 3)) {
         free(curr_fn);
         curr_fn = strdup(line+3);
         continue;
      }
      else
      if (streqn(line, "fl=", 3)) {
         free(curr_fl);
         curr_fl = strdup(line+3);
         continue;
      }
      else
      if (streqn(line, "summary: ", 9)) {
         break;
      }
      else
         parseError(s, "parse_CacheProfFile: unexpected line in main data");
   }

   
   if (!streqn(line, "summary: ", 9))
      parseError(s, "parse_CacheProfFile: missing SUMMARY line");

   cpf->summary_line = strdup(line);
   if (cpf->summary_line == NULL)
      mallocFail(s, "parse_CacheProfFile(6)");

   
   line = readline(s);
   if (line)
      parseError(s, "parse_CacheProfFile: "
                    "extraneous content after SUMMARY line");

   
   summaryRead = splitUpCountsLine( s, NULL, &cpf->summary_line[8] );
   if (summaryRead == NULL)
      mallocFail(s, "parse_CacheProfFile(7)");
   if (summaryRead->n_counts != cpf->n_events)
      parseError(s, "parse_CacheProfFile: wrong # counts in SUMMARY line");
   for (i = 0; i < summaryRead->n_counts; i++) {
      if (summaryRead->counts[i] != cpf->summary->counts[i]) {
         parseError(s, "parse_CacheProfFile: "
                       "computed vs stated SUMMARY counts mismatch");
      }
   }
   free(summaryRead->counts);
   sdel_Counts(summaryRead);

   
   
   free(cpf->summary_line);
   cpf->summary_line = NULL;

   free(tmp_desclines);
   free(curr_fn);
   free(curr_fl);

   
   return cpf;
}


static void merge_CacheProfInfo ( SOURCE* s,
                                  CacheProfFile* dst,
                                  CacheProfFile* src )
{
   FileFn* soKey;
   WordFM* soVal;
   WordFM* doVal;
   UWord   siKey;
   Counts* siVal;
   Counts* diVal;

   if (!streq( dst->events_line, src->events_line ))
     barf(s, "\"events:\" line of most recent file does "
             "not match those previously processed");

   initIterFM( src->outerMap );

   
   while (nextIterFM( src->outerMap, (Word*)&soKey, (Word*)&soVal )) {

      
      if (! lookupFM( dst->outerMap, (Word*)&doVal, (Word)soKey )) {

         
         FileFn* c_soKey = dopy_FileFn(soKey);
         WordFM* c_soVal = dopy_InnerMap(soVal);
         if ((!c_soKey) || (!c_soVal)) goto oom;
         addToFM( dst->outerMap, (Word)c_soKey, (Word)c_soVal );

      } else {

         
         initIterFM( soVal );

         
         while (nextIterFM( soVal, (Word*)&siKey, (Word*)&siVal )) {

            
            if (! lookupFM( doVal, (Word*)&diVal, siKey )) {

               
               Counts* c_siVal = dopy_Counts( siVal );
               if (!c_siVal) goto oom;
               addToFM( doVal, siKey, (Word)c_siVal );

            } else {

               
               addCounts( s, diVal, siVal );

            }
         }

      }

   }

   
   addCounts(s, dst->summary, src->summary );

   return;

  oom:
   mallocFail(s, "merge_CacheProfInfo");
}

static void usage ( void )
{
   fprintf(stderr, "%s: Merges multiple cachegrind output files into one\n", 
                   argv0);
   fprintf(stderr, "%s: usage: %s [-o outfile] [files-to-merge]\n", 
                   argv0, argv0);
   exit(1);
}

int main ( int argc, char** argv )
{
   Int            i;
   SOURCE         src;
   CacheProfFile  *cpf, *cpfTmp;

   FILE*          outfile = NULL;
   char*          outfilename = NULL;
   Int            outfileix = 0;

   if (argv[0])
      argv0 = argv[0];

   if (argc < 2)
      usage();

   for (i = 1; i < argc; i++) {
      if (streq(argv[i], "-h") || streq(argv[i], "--help"))
         usage();
   }

   
   for (i = 1; i < argc; i++) {
      if (streq(argv[i], "-o")) {
         if (i+1 < argc) {
            outfilename = argv[i+1];
            outfileix   = i;
            break;
         } else {
            usage();
         }
      }
   }

   cpf = NULL;

   for (i = 1; i < argc; i++) {

      if (i == outfileix) {
         
         i += 1;
         continue;
      }

      fprintf(stderr, "%s: parsing %s\n", argv0, argv[i]);
      src.lno      = 1;
      src.filename = argv[i];
      src.fp       = fopen(src.filename, "r");
      if (!src.fp) {
         perror(argv0);
         barf(&src, "Cannot open input file");
      }
      assert(src.fp);
      cpfTmp = parse_CacheProfFile( &src );
      fclose(src.fp);

      
      if (cpf == NULL) {
         
         cpf = cpfTmp;
      } else {
         
         fprintf(stderr, "%s: merging %s\n", argv0, argv[i]);
         merge_CacheProfInfo( &src, cpf, cpfTmp );
         ddel_CacheProfFile( cpfTmp );
      }

   }

   

   if (cpf) {

      fprintf(stderr, "%s: writing %s\n", 
                       argv0, outfilename ? outfilename : "(stdout)" );

      
      if (outfilename) {
         outfile = fopen(outfilename, "w");
         if (!outfile) {
            fprintf(stderr, "%s: can't create output file %s\n", 
                            argv0, outfilename);
            perror(argv0);
            exit(1);
         }
      } else {
         outfile = stdout;
      }

      show_CacheProfFile( outfile, cpf );
      if (ferror(outfile)) {
         fprintf(stderr, "%s: error writing output file %s\n", 
                         argv0, outfilename ? outfilename : "(stdout)" );
         perror(argv0);
         if (outfile != stdout)
            fclose(outfile);
         exit(1);
      }

      fflush(outfile);
      if (outfile != stdout)
         fclose( outfile );

      ddel_CacheProfFile( cpf );
   }

   return 0;
}




typedef
   struct _AvlNode {
      Word key;
      Word val;
      struct _AvlNode* left;
      struct _AvlNode* right;
      Char balance;
   }
   AvlNode;

typedef 
   struct {
      Word w;
      Bool b;
   }
   MaybeWord;

#define WFM_STKMAX    32    

struct _WordFM {
   AvlNode* root;
   void*    (*alloc_nofail)( SizeT );
   void     (*dealloc)(void*);
   Word     (*kCmp)(Word,Word);
   AvlNode* nodeStack[WFM_STKMAX]; 
   Int      numStack[WFM_STKMAX];  
   Int      stackTop;              
}; 

static Bool avl_removeroot_wrk(AvlNode** t, Word(*kCmp)(Word,Word));

static void avl_swl ( AvlNode** root )
{
   AvlNode* a = *root;
   AvlNode* b = a->right;
   *root    = b;
   a->right = b->left;
   b->left  = a;
}

static void avl_swr ( AvlNode** root )
{
   AvlNode* a = *root;
   AvlNode* b = a->left;
   *root    = b;
   a->left  = b->right;
   b->right = a;
}

static void avl_nasty ( AvlNode* root )
{
   switch (root->balance) {
      case -1: 
         root->left->balance  = 0;
         root->right->balance = 1;
         break;
      case 1:
         root->left->balance  = -1;
         root->right->balance = 0;
         break;
      case 0:
         root->left->balance  = 0;
         root->right->balance = 0;
         break;
      default:
         assert(0);
   }
   root->balance=0;
}

static Word size_avl_nonNull ( AvlNode* nd )
{
   return 1 + (nd->left  ? size_avl_nonNull(nd->left)  : 0)
            + (nd->right ? size_avl_nonNull(nd->right) : 0);
}

static 
Bool avl_insert_wrk ( AvlNode**         rootp, 
                      MaybeWord* oldV,
                      AvlNode*          a, 
                      Word              (*kCmp)(Word,Word) )
{
   Word cmpres;

   
   a->left    = 0;
   a->right   = 0;
   a->balance = 0;
   oldV->b    = False;

   
   if (!(*rootp)) {
      (*rootp) = a;
      return True;
   }
 
   cmpres = kCmp( (*rootp)->key, a->key );

   if (cmpres > 0) {
      
      if ((*rootp)->left) {
         AvlNode* left_subtree = (*rootp)->left;
         if (avl_insert_wrk(&left_subtree, oldV, a, kCmp)) {
            switch ((*rootp)->balance--) {
               case  1: return False;
               case  0: return True;
               case -1: break;
               default: assert(0);
            }
            if ((*rootp)->left->balance < 0) {
               avl_swr( rootp );
               (*rootp)->balance = 0;
               (*rootp)->right->balance = 0;
            } else {
               avl_swl( &((*rootp)->left) );
               avl_swr( rootp );
               avl_nasty( *rootp );
            }
         } else {
            (*rootp)->left = left_subtree;
         }
         return False;
      } else {
         (*rootp)->left = a;
         if ((*rootp)->balance--) 
            return False;
         return True;
      }
      assert(0);
   }
   else 
   if (cmpres < 0) {
      
      if ((*rootp)->right) {
         AvlNode* right_subtree = (*rootp)->right;
         if (avl_insert_wrk(&right_subtree, oldV, a, kCmp)) {
            switch((*rootp)->balance++){
               case -1: return False;
               case  0: return True;
               case  1: break;
               default: assert(0);
            }
            if ((*rootp)->right->balance > 0) {
               avl_swl( rootp );
               (*rootp)->balance = 0;
               (*rootp)->left->balance = 0;
            } else {
               avl_swr( &((*rootp)->right) );
               avl_swl( rootp );
               avl_nasty( *rootp );
            }
         } else {
            (*rootp)->right = right_subtree;
         }
         return False;
      } else {
         (*rootp)->right = a;
         if ((*rootp)->balance++) 
            return False;
         return True;
      }
      assert(0);
   }
   else {
      oldV->b = True;
      oldV->w = (*rootp)->val;
      (*rootp)->val = a->val;
      return False;
   }
}

static
Bool avl_remove_wrk ( AvlNode** rootp, 
                      AvlNode*  a, 
                      Word(*kCmp)(Word,Word) )
{
   Bool ch;
   Word cmpres = kCmp( (*rootp)->key, a->key );

   if (cmpres > 0){
      
      AvlNode* left_subtree = (*rootp)->left;
      assert(left_subtree);
      ch = avl_remove_wrk(&left_subtree, a, kCmp);
      (*rootp)->left=left_subtree;
      if (ch) {
         switch ((*rootp)->balance++) {
            case -1: return True;
            case  0: return False;
            case  1: break;
            default: assert(0);
         }
         switch ((*rootp)->right->balance) {
            case 0:
               avl_swl( rootp );
               (*rootp)->balance = -1;
               (*rootp)->left->balance = 1;
               return False;
            case 1: 
               avl_swl( rootp );
               (*rootp)->balance = 0;
               (*rootp)->left->balance = 0;
               return -1;
            case -1:
               break;
            default:
               assert(0);
         }
         avl_swr( &((*rootp)->right) );
         avl_swl( rootp );
         avl_nasty( *rootp );
         return True;
      }
   }
   else
   if (cmpres < 0) {
      
      AvlNode* right_subtree = (*rootp)->right;
      assert(right_subtree);
      ch = avl_remove_wrk(&right_subtree, a, kCmp);
      (*rootp)->right = right_subtree;
      if (ch) {
         switch ((*rootp)->balance--) {
            case  1: return True;
            case  0: return False;
            case -1: break;
            default: assert(0);
         }
         switch ((*rootp)->left->balance) {
            case 0:
               avl_swr( rootp );
               (*rootp)->balance = 1;
               (*rootp)->right->balance = -1;
               return False;
            case -1:
               avl_swr( rootp );
               (*rootp)->balance = 0;
               (*rootp)->right->balance = 0;
               return True;
            case 1:
               break;
            default:
               assert(0);
         }
         avl_swl( &((*rootp)->left) );
         avl_swr( rootp );
         avl_nasty( *rootp );
         return True;
      }
   }
   else {
      assert(cmpres == 0);
      assert((*rootp)==a);
      return avl_removeroot_wrk(rootp, kCmp);
   }
   return 0;
}

static 
Bool avl_removeroot_wrk ( AvlNode** rootp, 
                          Word(*kCmp)(Word,Word) )
{
   Bool     ch;
   AvlNode* a;
   if (!(*rootp)->left) {
      if (!(*rootp)->right) {
         (*rootp) = 0;
         return True;
      }
      (*rootp) = (*rootp)->right;
      return True;
   }
   if (!(*rootp)->right) {
      (*rootp) = (*rootp)->left;
      return True;
   }
   if ((*rootp)->balance < 0) {
      
      a = (*rootp)->left;
      while (a->right) a = a->right;
   } else {
      
      a = (*rootp)->right;
      while (a->left) a = a->left;
   }
   ch = avl_remove_wrk(rootp, a, kCmp);
   a->left    = (*rootp)->left;
   a->right   = (*rootp)->right;
   a->balance = (*rootp)->balance;
   (*rootp)   = a;
   if(a->balance == 0) return ch;
   return False;
}

static 
AvlNode* avl_find_node ( AvlNode* t, Word k, Word(*kCmp)(Word,Word) )
{
   Word cmpres;
   while (True) {
      if (t == NULL) return NULL;
      cmpres = kCmp(t->key, k);
      if (cmpres > 0) t = t->left;  else
      if (cmpres < 0) t = t->right; else
      return t;
   }
}

static void stackClear(WordFM* fm)
{
   Int i;
   assert(fm);
   for (i = 0; i < WFM_STKMAX; i++) {
      fm->nodeStack[i] = NULL;
      fm->numStack[i]  = 0;
   }
   fm->stackTop = 0;
}

static inline void stackPush(WordFM* fm, AvlNode* n, Int i)
{
   assert(fm->stackTop < WFM_STKMAX);
   assert(1 <= i && i <= 3);
   fm->nodeStack[fm->stackTop] = n;
   fm-> numStack[fm->stackTop] = i;
   fm->stackTop++;
}

static inline Bool stackPop(WordFM* fm, AvlNode** n, Int* i)
{
   assert(fm->stackTop <= WFM_STKMAX);

   if (fm->stackTop > 0) {
      fm->stackTop--;
      *n = fm->nodeStack[fm->stackTop];
      *i = fm-> numStack[fm->stackTop];
      assert(1 <= *i && *i <= 3);
      fm->nodeStack[fm->stackTop] = NULL;
      fm-> numStack[fm->stackTop] = 0;
      return True;
   } else {
      return False;
   }
}

static 
AvlNode* avl_dopy ( AvlNode* nd, 
                    Word(*dopyK)(Word), 
                    Word(*dopyV)(Word),
                    void*(alloc_nofail)(SizeT) )
{
   AvlNode* nyu;
   if (! nd)
      return NULL;
   nyu = alloc_nofail(sizeof(AvlNode));
   assert(nyu);
   
   nyu->left = nd->left;
   nyu->right = nd->right;
   nyu->balance = nd->balance;

   
   if (dopyK) {
      nyu->key = dopyK( nd->key );
      if (nd->key != 0 && nyu->key == 0)
         return NULL; 
   } else {
      
      nyu->key = nd->key;
   }

   
   if (dopyV) {
      nyu->val = dopyV( nd->val );
      if (nd->val != 0 && nyu->val == 0)
         return NULL; 
   } else {
      
      nyu->val = nd->val;
   }

   
   if (nyu->left) {
      nyu->left = avl_dopy( nyu->left, dopyK, dopyV, alloc_nofail );
      if (! nyu->left)
         return NULL;
   }
   if (nyu->right) {
      nyu->right = avl_dopy( nyu->right, dopyK, dopyV, alloc_nofail );
      if (! nyu->right)
         return NULL;
   }

   return nyu;
}


void initFM ( WordFM* fm,
              void*   (*alloc_nofail)( SizeT ),
              void    (*dealloc)(void*),
              Word    (*kCmp)(Word,Word) )
{
   fm->root         = 0;
   fm->kCmp         = kCmp;
   fm->alloc_nofail = alloc_nofail;
   fm->dealloc      = dealloc;
   fm->stackTop     = 0;
}

WordFM* newFM( void* (*alloc_nofail)( SizeT ),
               void  (*dealloc)(void*),
               Word  (*kCmp)(Word,Word) )
{
   WordFM* fm = alloc_nofail(sizeof(WordFM));
   assert(fm);
   initFM(fm, alloc_nofail, dealloc, kCmp);
   return fm;
}

static void avl_free ( AvlNode* nd, 
                       void(*kFin)(Word),
                       void(*vFin)(Word),
                       void(*dealloc)(void*) )
{
   if (!nd)
      return;
   if (nd->left)
      avl_free(nd->left, kFin, vFin, dealloc);
   if (nd->right)
      avl_free(nd->right, kFin, vFin, dealloc);
   if (kFin)
      kFin( nd->key );
   if (vFin)
      vFin( nd->val );
   memset(nd, 0, sizeof(AvlNode));
   dealloc(nd);
}

void deleteFM ( WordFM* fm, void(*kFin)(Word), void(*vFin)(Word) )
{
   void(*dealloc)(void*) = fm->dealloc;
   avl_free( fm->root, kFin, vFin, dealloc );
   memset(fm, 0, sizeof(WordFM) );
   dealloc(fm);
}

void addToFM ( WordFM* fm, Word k, Word v )
{
   MaybeWord oldV;
   AvlNode* node;
   node = fm->alloc_nofail( sizeof(struct _AvlNode) );
   node->key = k;
   node->val = v;
   oldV.b = False;
   oldV.w = 0;
   avl_insert_wrk( &fm->root, &oldV, node, fm->kCmp );
   
   
   if (oldV.b)
      free(node);
}

Bool delFromFM ( WordFM* fm, Word* oldV, Word key )
{
   AvlNode* node = avl_find_node( fm->root, key, fm->kCmp );
   if (node) {
      avl_remove_wrk( &fm->root, node, fm->kCmp );
      if (oldV)
         *oldV = node->val;
      fm->dealloc(node);
      return True;
   } else {
      return False;
   }
}

Bool lookupFM ( WordFM* fm, Word* valP, Word key )
{
   AvlNode* node = avl_find_node( fm->root, key, fm->kCmp );
   if (node) {
      if (valP)
         *valP = node->val;
      return True;
   } else {
      return False;
   }
}

Word sizeFM ( WordFM* fm )
{
   
   return fm->root ? size_avl_nonNull( fm->root ) : 0;
}

void initIterFM ( WordFM* fm )
{
   assert(fm);
   stackClear(fm);
   if (fm->root)
      stackPush(fm, fm->root, 1);
}

Bool nextIterFM ( WordFM* fm, Word* pKey, Word* pVal )
{
   Int i = 0;
   AvlNode* n = NULL;
   
   assert(fm);

   
   
   
   
   while (stackPop(fm, &n, &i)) {
      switch (i) {
      case 1: 
         stackPush(fm, n, 2);
         if (n->left)  stackPush(fm, n->left, 1);
         break;
      case 2: 
         stackPush(fm, n, 3);
         if (pKey) *pKey = n->key;
         if (pVal) *pVal = n->val;
         return True;
      case 3:
         if (n->right) stackPush(fm, n->right, 1);
         break;
      default:
         assert(0);
      }
   }

   
   return False;
}

void doneIterFM ( WordFM* fm )
{
}

WordFM* dopyFM ( WordFM* fm, Word(*dopyK)(Word), Word(*dopyV)(Word) )
{
   WordFM* nyu; 

   
   assert(fm->stackTop == 0);

   nyu = fm->alloc_nofail( sizeof(WordFM) );
   assert(nyu);

   *nyu = *fm;

   fm->stackTop = 0;
   memset(fm->nodeStack, 0, sizeof(fm->nodeStack));
   memset(fm->numStack, 0,  sizeof(fm->numStack));

   if (nyu->root) {
      nyu->root = avl_dopy( nyu->root, dopyK, dopyV, fm->alloc_nofail );
      if (! nyu->root)
         return NULL;
   }

   return nyu;
}


