/*
 * problemP.h --- Private header file for fix_problem()
 *
 * Copyright 1997 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

struct e2fsck_problem {
	problem_t	e2p_code;
	const char *	e2p_description;
	char		prompt;
	int		flags;
	problem_t	second_code;
	int		count;
	int		max_count;
};

struct latch_descr {
	int		latch_code;
	problem_t	question;
	problem_t	end_message;
	int		flags;
};

#define PR_PREEN_OK	0x000001 
#define PR_NO_OK	0x000002 
#define PR_NO_DEFAULT	0x000004 
#define PR_MSG_ONLY	0x000008 


#define PR_FATAL	0x001000 
#define PR_AFTER_CODE	0x002000 
				 
#define PR_PREEN_NOMSG	0x004000 
#define PR_NOCOLLATE	0x008000 
#define PR_NO_NOMSG	0x010000 
#define PR_PREEN_NO	0x020000 
#define PR_PREEN_NOHDR	0x040000 
#define PR_CONFIG	0x080000 
#define PR_FORCE_NO	0x100000 
