#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <i18n.h>
#include <langinfo.h>
#include <libintl.h>

static int my_streq(const char *va, const char *vb) {
	const char *a = va;
	const char *b = vb;
	int i;
	if ((NULL == a) || (NULL == b)) {
		return 0;
	}
	for (i=0; a[i] != '\0' && b[i] != '\0'; i++) {
		if (a[i] != b[i]) {
			return 0;
		}
	}
	return 1;
}

#if defined(CODESET_UTF_8)
#define MY_CODESET "UTF-8"
#elif defined(CODESET_LATIN1)
#define MY_CODESET "iso-8859-1"
#elif defined(CODESET_DEFAULT)
#define MY_CODESET "default"
#define bind_textdomain_codeset(domain,codeset) \
	MY_CODESET
#else
#error Define one of the CODESET_* macros!
#endif

typedef struct {
	char *locale;
	char *orig;
	char *latin1;
	char *utf8;
} TestCase;

const TestCase testcases[] = {
	{ "de_DE", 
	  N_("High saturation"), 
	  
	  "Hohe S\344ttigung", 
	  
	  "Hohe S\303\244ttigung" },
	{ "fr_FR",
	  N_("Not defined"),
	  
	  "Non d\233fini",
	  
	  "Non d\303\251fini"
	},
	{ "es_ES",
	  N_("High saturation"),
	  
	  "Alta saturaci\363n",
	  
	  "Alta saturaci\303\263n"
	},
	{ NULL, NULL, NULL, NULL }
};


static int check(const int i)
{
	const char *oldtextdomain = textdomain(NULL);
	const char *newtextdomain = textdomain(GETTEXT_PACKAGE);
		
	const char *newcodeset = MY_CODESET;
	const char *oldcodeset = bind_textdomain_codeset(GETTEXT_PACKAGE, NULL);
	const char *realcodeset = bind_textdomain_codeset(GETTEXT_PACKAGE, MY_CODESET);
		
	const char *orig   = testcases[i].orig;
	const char *transl = gettext(testcases[i].orig);
	const char *latin1 = testcases[i].latin1;
	const char *utf8   = testcases[i].utf8;
			
	printf( 
		"Old textdomain:  %s\n"
		"New textdomain:  %s\n",
		oldtextdomain,
		newtextdomain
		);
	
	if (NULL != oldcodeset) {
		printf(
		       "Old codeset:     \"%s\" (locale default)\n",
		       nl_langinfo(CODESET)
		       );
	} else {
		printf(
		       "Old codeset:     \"%s\"\n",
		       oldcodeset
		       );
	}
	
	printf(
	       "Wanted codeset:  %s\n"
	       "Real codeset:    %s\n",
	       newcodeset,
	       realcodeset
	       );

	printf(
	       "Original:   %s\n"
	       "Translated: %s\n"
	       "iso-8859-1: %s\n"
	       "utf-8:      %s\n",
	       orig,
	       transl,
	       latin1,
	       utf8
	       );

#if defined(CODESET_UTF_8)
	return (my_streq(transl, utf8));
#elif defined(CODESET_LATIN_1)
	return (my_streq(transl, latin1));
#else
	
	return (my_streq(orig, orig));
#endif
}


static int checks()
{
	int i;
		
	const char *localeenv = getenv("LOCALEDIR");
	const char *localedir = (localeenv!=NULL)?localeenv:LOCALEDIR;
	const char *msgcatdir = bindtextdomain(GETTEXT_PACKAGE, localedir);
	
	
	const char *oldlocale = setlocale(LC_ALL, NULL);
	const char *newlocale = setlocale(LC_ALL, "");

	if (localeenv != NULL) {
		printf("Msg catalog dir: %s (from environment variable LOCALEDIR\n",
		       msgcatdir);
	} else {
		printf("Msg catalog dir: %s\n", msgcatdir);
	}

	if (newlocale == NULL) {
		printf("Locale not available: \"%s\"\n", newlocale);
		printf("Aborting without error.\n");
		return 1;
	}


	printf( 
		"Old locale:      %s\n"
		"New locale:      %s\n",
		oldlocale,
		newlocale
		);

	for (i=0; testcases[i].locale != NULL; i++) {
		const int localelen = strlen(testcases[i].locale);
		if (strncmp(newlocale, testcases[i].locale, localelen) == 0) {
			return check(i);
		}
	}

	printf("No test case found for locale: %s\n", newlocale);
	return 1;
}


int main(int argc, char *argv[])
{
	if (argc > 1) {
		if ((argc == 2) && (strcmp("--list", argv[1]) == 0)) {
			int i;
			for (i=0; testcases[i].locale != NULL; i++) {
				printf("%s\n", testcases[i].locale);
			}
			return 0;
		} else {
			int i;
			fprintf(stderr, "Illegal command line. Aborting.\n");
			fprintf(stderr, "argc: %03d\n", argc);
			for (i=0; i<argc; i++) {
				fprintf(stderr, "%03d \"%s\"\n", i, argv[i]);
			}
			return 1;
		}
	} else {
		int ret = checks();
		printf("Test result: %s\n", (ret)?"success":"failure");
		return (ret)?0:1;
	}
	return -1;
}

