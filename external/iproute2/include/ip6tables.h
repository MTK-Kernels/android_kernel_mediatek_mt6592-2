#ifndef _IP6TABLES_USER_H
#define _IP6TABLES_USER_H

#include "iptables_common.h"
#include "libiptc/libip6tc.h"

struct ip6tables_rule_match
{
	struct ip6tables_rule_match *next;

	struct ip6tables_match *match;
};

struct ip6tables_match
{
	struct ip6tables_match *next;

	ip6t_chainlabel name;

	const char *version;

	
	size_t size;

	
	size_t userspacesize;

	
	void (*help)(void);

	
	void (*init)(struct ip6t_entry_match *m, unsigned int *nfcache);

	int (*parse)(int c, char **argv, int invert, unsigned int *flags,
		     const struct ip6t_entry *entry,
		     unsigned int *nfcache,
		     struct ip6t_entry_match **match);

	
	void (*final_check)(unsigned int flags);

	
	void (*print)(const struct ip6t_ip6 *ip,
		      const struct ip6t_entry_match *match, int numeric);

	
	void (*save)(const struct ip6t_ip6 *ip,
		     const struct ip6t_entry_match *match);

	
	const struct option *extra_opts;

	
	unsigned int option_offset;
	struct ip6t_entry_match *m;
	unsigned int mflags;
#ifdef NO_SHARED_LIBS
	unsigned int loaded; 
#endif
};

struct ip6tables_target
{
	struct ip6tables_target *next;

	ip6t_chainlabel name;

	const char *version;

	
	size_t size;

	
	size_t userspacesize;

	
	void (*help)(void);

	
	void (*init)(struct ip6t_entry_target *t, unsigned int *nfcache);

	int (*parse)(int c, char **argv, int invert, unsigned int *flags,
		     const struct ip6t_entry *entry,
		     struct ip6t_entry_target **target);

	
	void (*final_check)(unsigned int flags);

	
	void (*print)(const struct ip6t_ip6 *ip,
		      const struct ip6t_entry_target *target, int numeric);

	
	void (*save)(const struct ip6t_ip6 *ip,
		     const struct ip6t_entry_target *target);

	
	struct option *extra_opts;

	
	unsigned int option_offset;
	struct ip6t_entry_target *t;
	unsigned int tflags;
	unsigned int used;
#ifdef NO_SHARED_LIBS
	unsigned int loaded; 
#endif
};

extern int line;

extern void register_match6(struct ip6tables_match *me);
extern void register_target6(struct ip6tables_target *me);

extern int do_command6(int argc, char *argv[], char **table,
		       ip6tc_handle_t *handle);
extern struct ip6tables_match *ip6tables_matches;
extern struct ip6tables_target *ip6tables_targets;

enum ip6t_tryload {
	DONT_LOAD,
	TRY_LOAD,
	LOAD_MUST_SUCCEED
};

extern struct ip6tables_target *find_target(const char *name, enum ip6t_tryload);
extern struct ip6tables_match *find_match(const char *name, enum ip6t_tryload, struct ip6tables_rule_match **match);

extern int for_each_chain(int (*fn)(const ip6t_chainlabel, int, ip6tc_handle_t *), int verbose, int builtinstoo, ip6tc_handle_t *handle);
extern int flush_entries(const ip6t_chainlabel chain, int verbose, ip6tc_handle_t *handle);
extern int delete_chain(const ip6t_chainlabel chain, int verbose, ip6tc_handle_t *handle);
extern int ip6tables_insmod(const char *modname, const char *modprobe);

#endif 
