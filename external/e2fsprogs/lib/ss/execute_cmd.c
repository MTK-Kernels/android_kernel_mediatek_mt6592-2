/*
 * Copyright 1987, 1988, 1989 by Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose is hereby granted, provided that
 * the names of M.I.T. and the M.I.T. S.I.P.B. not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  M.I.T. and the
 * M.I.T. S.I.P.B. make no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifdef HAS_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#else
extern int errno;
#endif
#include "ss_internal.h"
#include <stdio.h>

static int check_request_table PROTOTYPE((ss_request_table *rqtbl, int argc,
					  char *argv[], int sci_idx));
static int really_execute_command PROTOTYPE((int sci_idx, int argc,
					     char **argv[]));


#ifdef __SABER__
static struct _ss_request_entry * get_request (tbl, idx)
    ss_request_table * tbl;
    int idx;
{
    struct _ss_request_table *tbl1 = (struct _ss_request_table *) tbl;
    struct _ss_request_entry *e = (struct _ss_request_entry *) tbl1->requests;
    return e + idx;
}
#else
#define get_request(tbl,idx)    ((tbl) -> requests + (idx))
#endif


static int check_request_table(register ss_request_table *rqtbl, int argc,
			       char *argv[], int sci_idx)
{
#ifdef __SABER__
    struct _ss_request_entry *request;
#else
    register ss_request_entry *request;
#endif
    register ss_data *info;
    register char const * const * name;
    char *string = argv[0];
    int i;

    info = ss_info(sci_idx);
    info->argc = argc;
    info->argv = argv;
    for (i = 0; (request = get_request(rqtbl, i))->command_names; i++) {
	for (name = request->command_names; *name; name++)
	    if (!strcmp(*name, string)) {
		info->current_request = request->command_names[0];
		(request->function)(argc, (const char *const *) argv,
				    sci_idx,info->info_ptr);
		info->current_request = (char *)NULL;
		return(0);
	    }
    }
    return(SS_ET_COMMAND_NOT_FOUND);
}


static int really_execute_command(int sci_idx, int argc, char **argv[])
{
    register ss_request_table **rqtbl;
    register ss_data *info;

    info = ss_info(sci_idx);

    for (rqtbl = info->rqt_tables; *rqtbl; rqtbl++) {
        if (check_request_table (*rqtbl, argc, *argv, sci_idx) == 0)
            return(0);
    }
    return(SS_ET_COMMAND_NOT_FOUND);
}


int ss_execute_command(int sci_idx, register char *argv[])
{
	register int i, argc;
	char **argp;

	argc = 0;
	for (argp = argv; *argp; argp++)
		argc++;
	argp = (char **)malloc((argc+1)*sizeof(char *));
	for (i = 0; i <= argc; i++)
		argp[i] = argv[i];
	i = really_execute_command(sci_idx, argc, &argp);
	free(argp);
	return(i);
}


int ss_execute_line(int sci_idx, char *line_ptr)
{
    char **argv;
    int argc, ret;

    
    while (line_ptr[0] == ' ' || line_ptr[0] == '\t')
        line_ptr++;

    
    if (*line_ptr == '!') {
        if (ss_info(sci_idx)->flags.escape_disabled)
            return SS_ET_ESCAPE_DISABLED;
        else {
            line_ptr++;
            return (system(line_ptr) < 0) ? errno : 0;
        }
    }

    
    argv = ss_parse(sci_idx, line_ptr, &argc);
    if (argc == 0) {
	free(argv);
        return 0;
    }

    
    ret = really_execute_command (sci_idx, argc, &argv);

    free(argv);

    return(ret);
}
