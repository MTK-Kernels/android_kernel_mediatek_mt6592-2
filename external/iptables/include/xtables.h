#ifndef _XTABLES_H
#define _XTABLES_H


#include <sys/socket.h> 
#include <sys/types.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif
#ifndef IPPROTO_DCCP
#define IPPROTO_DCCP 33
#endif
#ifndef IPPROTO_MH
#	define IPPROTO_MH 135
#endif
#ifndef IPPROTO_UDPLITE
#define IPPROTO_UDPLITE	136
#endif

#include <xtables-version.h>

struct in_addr;

#define XTOPT_POINTER(stype, member) \
	.ptroff = offsetof(stype, member), \
	.size = sizeof(((stype *)NULL)->member)
#define XTOPT_TABLEEND {.name = NULL}

enum xt_option_type {
	XTTYPE_NONE,
	XTTYPE_UINT8,
	XTTYPE_UINT16,
	XTTYPE_UINT32,
	XTTYPE_UINT64,
	XTTYPE_UINT8RC,
	XTTYPE_UINT16RC,
	XTTYPE_UINT32RC,
	XTTYPE_UINT64RC,
	XTTYPE_DOUBLE,
	XTTYPE_STRING,
	XTTYPE_TOSMASK,
	XTTYPE_MARKMASK32,
	XTTYPE_SYSLOGLEVEL,
	XTTYPE_HOST,
	XTTYPE_HOSTMASK,
	XTTYPE_PROTOCOL,
	XTTYPE_PORT,
	XTTYPE_PORTRC,
	XTTYPE_PLEN,
	XTTYPE_PLENMASK,
	XTTYPE_ETHERMAC,
};

enum xt_option_flags {
	XTOPT_INVERT = 1 << 0,
	XTOPT_MAND   = 1 << 1,
	XTOPT_MULTI  = 1 << 2,
	XTOPT_PUT    = 1 << 3,
	XTOPT_NBO    = 1 << 4,
};

struct xt_option_entry {
	const char *name;
	enum xt_option_type type;
	unsigned int id, excl, also, flags;
	unsigned int ptroff;
	size_t size;
	unsigned int min, max;
};

struct xt_option_call {
	const char *arg, *ext_name;
	const struct xt_option_entry *entry;
	void *data;
	unsigned int xflags;
	bool invert;
	uint8_t nvals;
	union {
		uint8_t u8, u8_range[2], syslog_level, protocol;
		uint16_t u16, u16_range[2], port, port_range[2];
		uint32_t u32, u32_range[2];
		uint64_t u64, u64_range[2];
		double dbl;
		struct {
			union nf_inet_addr haddr, hmask;
			uint8_t hlen;
		};
		struct {
			uint8_t tos_value, tos_mask;
		};
		struct {
			uint32_t mark, mask;
		};
		uint8_t ethermac[6];
	} val;
	
	union {
		struct xt_entry_match **match;
		struct xt_entry_target **target;
	};
	void *xt_entry;
	void *udata;
};

struct xt_fcheck_call {
	const char *ext_name;
	void *data, *udata;
	unsigned int xflags;
};

struct xtables_lmap {
	char *name;
	int id;
	struct xtables_lmap *next;
};

enum xtables_ext_flags {
	XTABLES_EXT_ALIAS = 1 << 0,
};

struct xtables_match
{
	const char *version;

	struct xtables_match *next;

	const char *name;
	const char *real_name;

	
	u_int8_t revision;

	
	u_int8_t ext_flags;

	u_int16_t family;

	
	size_t size;

	
	size_t userspacesize;

	
	void (*help)(void);

	
	void (*init)(struct xt_entry_match *m);

	
	int (*parse)(int c, char **argv, int invert, unsigned int *flags,
		     const void *entry,
		     struct xt_entry_match **match);

	
	void (*final_check)(unsigned int flags);

	
	
	void (*print)(const void *ip,
		      const struct xt_entry_match *match, int numeric);

	
	
	void (*save)(const void *ip, const struct xt_entry_match *match);

	
	const char *(*alias)(const struct xt_entry_match *match);

	
	const struct option *extra_opts;

	
	void (*x6_parse)(struct xt_option_call *);
	void (*x6_fcheck)(struct xt_fcheck_call *);
	const struct xt_option_entry *x6_options;

	
	size_t udata_size;

	
	void *udata;
	unsigned int option_offset;
	struct xt_entry_match *m;
	unsigned int mflags;
	unsigned int loaded; 
};

struct xtables_target
{
	const char *version;

	struct xtables_target *next;


	const char *name;

	
	const char *real_name;

	
	u_int8_t revision;

	
	u_int8_t ext_flags;

	u_int16_t family;


	
	size_t size;

	
	size_t userspacesize;

	
	void (*help)(void);

	
	void (*init)(struct xt_entry_target *t);

	
	int (*parse)(int c, char **argv, int invert, unsigned int *flags,
		     const void *entry,
		     struct xt_entry_target **targetinfo);

	
	void (*final_check)(unsigned int flags);

	
	void (*print)(const void *ip,
		      const struct xt_entry_target *target, int numeric);

	
	void (*save)(const void *ip,
		     const struct xt_entry_target *target);

	
	const char *(*alias)(const struct xt_entry_target *target);

	
	const struct option *extra_opts;

	
	void (*x6_parse)(struct xt_option_call *);
	void (*x6_fcheck)(struct xt_fcheck_call *);
	const struct xt_option_entry *x6_options;

	size_t udata_size;

	
	void *udata;
	unsigned int option_offset;
	struct xt_entry_target *t;
	unsigned int tflags;
	unsigned int used;
	unsigned int loaded; 
};

struct xtables_rule_match {
	struct xtables_rule_match *next;
	struct xtables_match *match;
	bool completed;
};

struct xtables_pprot {
	const char *name;
	u_int8_t num;
};

enum xtables_tryload {
	XTF_DONT_LOAD,
	XTF_DURING_LOAD,
	XTF_TRY_LOAD,
	XTF_LOAD_MUST_SUCCEED,
};

enum xtables_exittype {
	OTHER_PROBLEM = 1,
	PARAMETER_PROBLEM,
	VERSION_PROBLEM,
	RESOURCE_PROBLEM,
	XTF_ONLY_ONCE,
	XTF_NO_INVERT,
	XTF_BAD_VALUE,
	XTF_ONE_ACTION,
};

struct xtables_globals
{
	unsigned int option_offset;
	const char *program_name, *program_version;
	struct option *orig_opts;
	struct option *opts;
	void (*exit_err)(enum xtables_exittype status, const char *msg, ...) __attribute__((noreturn, format(printf,2,3)));
};

#define XT_GETOPT_TABLEEND {.name = NULL, .has_arg = false}

#ifdef __cplusplus
extern "C" {
#endif

extern const char *xtables_modprobe_program;
extern struct xtables_match *xtables_matches;
extern struct xtables_target *xtables_targets;

extern void xtables_init(void);
extern void xtables_set_nfproto(uint8_t);
extern void *xtables_calloc(size_t, size_t);
extern void *xtables_malloc(size_t);
extern void *xtables_realloc(void *, size_t);

extern int xtables_insmod(const char *, const char *, bool);
extern int xtables_load_ko(const char *, bool);
extern int xtables_set_params(struct xtables_globals *xtp);
extern void xtables_free_opts(int reset_offset);
extern struct option *xtables_merge_options(struct option *origopts,
	struct option *oldopts, const struct option *newopts,
	unsigned int *option_offset);

extern int xtables_init_all(struct xtables_globals *xtp, uint8_t nfproto);
extern struct xtables_match *xtables_find_match(const char *name,
	enum xtables_tryload, struct xtables_rule_match **match);
extern struct xtables_target *xtables_find_target(const char *name,
	enum xtables_tryload);

extern void xtables_rule_matches_free(struct xtables_rule_match **matches);

extern void xtables_register_match(struct xtables_match *me);
extern void xtables_register_matches(struct xtables_match *, unsigned int);
extern void xtables_register_target(struct xtables_target *me);
extern void xtables_register_targets(struct xtables_target *, unsigned int);

extern bool xtables_strtoul(const char *, char **, uintmax_t *,
	uintmax_t, uintmax_t);
extern bool xtables_strtoui(const char *, char **, unsigned int *,
	unsigned int, unsigned int);
extern int xtables_service_to_port(const char *name, const char *proto);
extern u_int16_t xtables_parse_port(const char *port, const char *proto);
extern void
xtables_parse_interface(const char *arg, char *vianame, unsigned char *mask);

#define aligned_u64 u_int64_t __attribute__((aligned(8)))

extern struct xtables_globals *xt_params;
#define xtables_error (xt_params->exit_err)

extern void xtables_param_act(unsigned int, const char *, ...);

extern const char *xtables_ipaddr_to_numeric(const struct in_addr *);
extern const char *xtables_ipaddr_to_anyname(const struct in_addr *);
extern const char *xtables_ipmask_to_numeric(const struct in_addr *);
extern struct in_addr *xtables_numeric_to_ipaddr(const char *);
extern struct in_addr *xtables_numeric_to_ipmask(const char *);
extern int xtables_ipmask_to_cidr(const struct in_addr *);
extern void xtables_ipparse_any(const char *, struct in_addr **,
	struct in_addr *, unsigned int *);
extern void xtables_ipparse_multiple(const char *, struct in_addr **,
	struct in_addr **, unsigned int *);

extern struct in6_addr *xtables_numeric_to_ip6addr(const char *);
extern const char *xtables_ip6addr_to_numeric(const struct in6_addr *);
extern const char *xtables_ip6addr_to_anyname(const struct in6_addr *);
extern const char *xtables_ip6mask_to_numeric(const struct in6_addr *);
extern int xtables_ip6mask_to_cidr(const struct in6_addr *);
extern void xtables_ip6parse_any(const char *, struct in6_addr **,
	struct in6_addr *, unsigned int *);
extern void xtables_ip6parse_multiple(const char *, struct in6_addr **,
	struct in6_addr **, unsigned int *);

extern void xtables_save_string(const char *value);

#define FMT_NUMERIC		0x0001
#define FMT_NOCOUNTS		0x0002
#define FMT_KILOMEGAGIGA	0x0004
#define FMT_OPTIONS		0x0008
#define FMT_NOTABLE		0x0010
#define FMT_NOTARGET		0x0020
#define FMT_VIA			0x0040
#define FMT_NONEWLINE		0x0080
#define FMT_LINENUMBERS		0x0100

#define FMT_PRINT_RULE (FMT_NOCOUNTS | FMT_OPTIONS | FMT_VIA \
                        | FMT_NUMERIC | FMT_NOTABLE)
#define FMT(tab,notab) ((format) & FMT_NOTABLE ? (notab) : (tab))

extern void xtables_print_num(uint64_t number, unsigned int format);

#if defined(ALL_INCLUSIVE) || defined(NO_SHARED_LIBS)
#	ifdef _INIT
#		undef _init
#		define _init _INIT
#	endif
	extern void init_extensions(void);
	extern void init_extensions4(void);
	extern void init_extensions6(void);
#else
#	define _init __attribute__((constructor)) _INIT
#endif

extern const struct xtables_pprot xtables_chain_protos[];
extern u_int16_t xtables_parse_protocol(const char *s);

extern int kernel_version;
extern void get_kernel_version(void);
#define LINUX_VERSION(x,y,z)	(0x10000*(x) + 0x100*(y) + z)
#define LINUX_VERSION_MAJOR(x)	(((x)>>16) & 0xFF)
#define LINUX_VERSION_MINOR(x)	(((x)>> 8) & 0xFF)
#define LINUX_VERSION_PATCH(x)	( (x)      & 0xFF)

extern void xtables_option_metavalidate(const char *,
					const struct xt_option_entry *);
extern struct option *xtables_options_xfrm(struct option *, struct option *,
					   const struct xt_option_entry *,
					   unsigned int *);
extern void xtables_option_parse(struct xt_option_call *);
extern void xtables_option_tpcall(unsigned int, char **, bool,
				  struct xtables_target *, void *);
extern void xtables_option_mpcall(unsigned int, char **, bool,
				  struct xtables_match *, void *);
extern void xtables_option_tfcall(struct xtables_target *);
extern void xtables_option_mfcall(struct xtables_match *);
extern void xtables_options_fcheck(const char *, unsigned int,
				   const struct xt_option_entry *);

extern struct xtables_lmap *xtables_lmap_init(const char *);
extern void xtables_lmap_free(struct xtables_lmap *);
extern int xtables_lmap_name2id(const struct xtables_lmap *, const char *);
extern const char *xtables_lmap_id2name(const struct xtables_lmap *, int);

#ifdef XTABLES_INTERNAL


#	ifndef ARRAY_SIZE
#		define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#	endif

extern void _init(void);

#endif

#ifdef __cplusplus
} 
#endif

#endif 
