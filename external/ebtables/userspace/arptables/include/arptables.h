#ifndef _ARPTABLES_USER_H
#define _ARPTABLES_USER_H

#include "arptables_common.h"
#include "libarptc/libarptc.h"


struct arpt_entry_match
{
	int iets;
};


struct arptables_match
{
	struct arptables_match *next;

	arpt_chainlabel name;

	const char *version;

	
	size_t size;

	
	size_t userspacesize;

	
	void (*help)(void);

	
	void (*init)(struct arpt_entry_match *m, unsigned int *nfcache);

	int (*parse)(int c, char **argv, int invert, unsigned int *flags,
		     const struct arpt_entry *entry,
		     unsigned int *nfcache,
		     struct arpt_entry_match **match);

	
	void (*final_check)(unsigned int flags);

	
	void (*print)(const struct arpt_arp *ip,
		      const struct arpt_entry_match *match, int numeric);

	
	void (*save)(const struct arpt_arp *ip,
		     const struct arpt_entry_match *match);

	
	const struct option *extra_opts;

	
	unsigned int option_offset;
	struct arpt_entry_match *m;
	unsigned int mflags;
	unsigned int used;
	unsigned int loaded; 
};

struct arptables_target
{
	struct arptables_target *next;

	arpt_chainlabel name;

	const char *version;

	
	size_t size;

	
	size_t userspacesize;

	
	void (*help)(void);

	
	void (*init)(struct arpt_entry_target *t);

	int (*parse)(int c, char **argv, int invert, unsigned int *flags,
		     const struct arpt_entry *entry,
		     struct arpt_entry_target **target);

	
	void (*final_check)(unsigned int flags);

	
	void (*print)(const struct arpt_arp *ip,
		      const struct arpt_entry_target *target, int numeric);

	
	void (*save)(const struct arpt_arp *ip,
		     const struct arpt_entry_target *target);

	
	struct option *extra_opts;

	
	unsigned int option_offset;
	struct arpt_entry_target *t;
	unsigned int tflags;
	unsigned int used;
	unsigned int loaded; 
};

extern void register_match(struct arptables_match *me);
extern void register_target(struct arptables_target *me);

extern struct in_addr *dotted_to_addr(const char *dotted);
extern char *addr_to_dotted(const struct in_addr *addrp);
extern char *addr_to_anyname(const struct in_addr *addr);
extern char *mask_to_dotted(const struct in_addr *mask);

extern void parse_hostnetworkmask(const char *name, struct in_addr **addrpp,
                      struct in_addr *maskp, unsigned int *naddrs);
extern u_int16_t parse_protocol(const char *s);

extern int do_command(int argc, char *argv[], char **table,
		      arptc_handle_t *handle);
extern struct arptables_match *arptables_matches;
extern struct arptables_target *arptables_targets;

enum arpt_tryload {
	DONT_LOAD,
	TRY_LOAD,
	LOAD_MUST_SUCCEED
};

extern struct arptables_target *find_target(const char *name, enum arpt_tryload);
extern struct arptables_match *find_match(const char *name, enum arpt_tryload);

extern int delete_chain(const arpt_chainlabel chain, int verbose,
			arptc_handle_t *handle);
extern int flush_entries(const arpt_chainlabel chain, int verbose, 
			arptc_handle_t *handle);
extern int for_each_chain(int (*fn)(const arpt_chainlabel, int, arptc_handle_t *),
		int verbose, int builtinstoo, arptc_handle_t *handle);
struct in_addr *parse_hostnetwork(const char *name, unsigned int *naddrs);
void print_mac(const unsigned char *mac, int l);
#endif 
