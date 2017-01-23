#ifndef _IPTABLES_USER_H
#define _IPTABLES_USER_H

#include "iptables_common.h"
#include "libiptc/libiptc.h"

#ifndef IPT_LIB_DIR
#define IPT_LIB_DIR "/usr/local/lib/iptables"
#endif

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#ifndef IPT_SO_GET_REVISION_MATCH 
#define IPT_SO_GET_REVISION_MATCH	(IPT_BASE_CTL + 2)
#define IPT_SO_GET_REVISION_TARGET	(IPT_BASE_CTL + 3)

struct ipt_get_revision
{
	char name[IPT_FUNCTION_MAXNAMELEN-1];

	u_int8_t revision;
};
#endif 

struct iptables_rule_match
{
	struct iptables_rule_match *next;

	struct iptables_match *match;
};

struct iptables_match
{
	struct iptables_match *next;

	ipt_chainlabel name;

	
	u_int8_t revision;

	const char *version;

	
	size_t size;

	
	size_t userspacesize;

	
	void (*help)(void);

	
	void (*init)(struct ipt_entry_match *m, unsigned int *nfcache);

	int (*parse)(int c, char **argv, int invert, unsigned int *flags,
		     const struct ipt_entry *entry,
		     unsigned int *nfcache,
		     struct ipt_entry_match **match);

	
	void (*final_check)(unsigned int flags);

	
	void (*print)(const struct ipt_ip *ip,
		      const struct ipt_entry_match *match, int numeric);

	
	void (*save)(const struct ipt_ip *ip,
		     const struct ipt_entry_match *match);

	
	const struct option *extra_opts;

	
	unsigned int option_offset;
	struct ipt_entry_match *m;
	unsigned int mflags;
#ifdef NO_SHARED_LIBS
	unsigned int loaded; 
#endif
};

struct iptables_target
{
	struct iptables_target *next;

	ipt_chainlabel name;

	
	u_int8_t revision;

	const char *version;

	
	size_t size;

	
	size_t userspacesize;

	
	void (*help)(void);

	
	void (*init)(struct ipt_entry_target *t, unsigned int *nfcache);

	int (*parse)(int c, char **argv, int invert, unsigned int *flags,
		     const struct ipt_entry *entry,
		     struct ipt_entry_target **target);

	
	void (*final_check)(unsigned int flags);

	
	void (*print)(const struct ipt_ip *ip,
		      const struct ipt_entry_target *target, int numeric);

	
	void (*save)(const struct ipt_ip *ip,
		     const struct ipt_entry_target *target);

	
	struct option *extra_opts;

	
	unsigned int option_offset;
	struct ipt_entry_target *t;
	unsigned int tflags;
	unsigned int used;
#ifdef NO_SHARED_LIBS
	unsigned int loaded; 
#endif
};

extern int line;

extern void register_match(struct iptables_match *me);
extern void register_target(struct iptables_target *me);

extern struct in_addr *dotted_to_addr(const char *dotted);
extern char *addr_to_dotted(const struct in_addr *addrp);
extern char *addr_to_anyname(const struct in_addr *addr);
extern char *mask_to_dotted(const struct in_addr *mask);

extern void parse_hostnetworkmask(const char *name, struct in_addr **addrpp,
                      struct in_addr *maskp, unsigned int *naddrs);
extern u_int16_t parse_protocol(const char *s);

extern int do_command(int argc, char *argv[], char **table,
		      iptc_handle_t *handle);
extern struct iptables_match *iptables_matches;
extern struct iptables_target *iptables_targets;

enum ipt_tryload {
	DONT_LOAD,
	TRY_LOAD,
	LOAD_MUST_SUCCEED
};

extern struct iptables_target *find_target(const char *name, enum ipt_tryload);
extern struct iptables_match *find_match(const char *name, enum ipt_tryload, struct iptables_rule_match **match);

extern int delete_chain(const ipt_chainlabel chain, int verbose,
			iptc_handle_t *handle);
extern int flush_entries(const ipt_chainlabel chain, int verbose,
			iptc_handle_t *handle);
extern int for_each_chain(int (*fn)(const ipt_chainlabel, int, iptc_handle_t *),
		int verbose, int builtinstoo, iptc_handle_t *handle);
#endif 
