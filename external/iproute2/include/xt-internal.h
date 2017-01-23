#ifndef _XTABLES_INTERNAL_H
#define _XTABLES_INTERNAL_H 1

#ifndef XT_LIB_DIR
#	define XT_LIB_DIR "/lib/xtables"
#endif

struct afinfo {
	
	int family;

	
	char *libprefix;

	
	int ipproto;

	
	char *kmod;

	
	int so_rev_match;

	
	int so_rev_target;
};

enum xt_tryload {
	DONT_LOAD,
	DURING_LOAD,
	TRY_LOAD,
	LOAD_MUST_SUCCEED
};

struct xtables_rule_match {
	struct xtables_rule_match *next;
	struct xtables_match *match;
	unsigned int completed;
};

extern char *lib_dir;

extern void *fw_calloc(size_t count, size_t size);
extern void *fw_malloc(size_t size);

extern const char *modprobe_program;
extern int xtables_insmod(const char *modname, const char *modprobe, int quiet);
extern int load_xtables_ko(const char *modprobe, int quiet);

extern struct afinfo afinfo;

extern struct xtables_match *xtables_matches;
extern struct xtables_target *xtables_targets;

extern struct xtables_match *find_match(const char *name, enum xt_tryload,
					struct xtables_rule_match **match);
extern struct xtables_target *find_target(const char *name, enum xt_tryload);

extern void _init(void);

#endif 
