#ifndef _LIBIP6TC_H
#define _LIBIP6TC_H

#include <linux/types.h>
#include <libiptc/ipt_kernel_headers.h>
#ifdef __cplusplus
#	include <climits>
#else
#	include <limits.h> 
#endif
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <libiptc/xtcshared.h>

#define ip6tc_handle xtc_handle
#define ip6t_chainlabel xt_chainlabel

#define IP6TC_LABEL_ACCEPT "ACCEPT"
#define IP6TC_LABEL_DROP "DROP"
#define IP6TC_LABEL_QUEUE   "QUEUE"
#define IP6TC_LABEL_RETURN "RETURN"

int ip6tc_is_chain(const char *chain, struct xtc_handle *const handle);

struct xtc_handle *ip6tc_init(const char *tablename);

void ip6tc_free(struct xtc_handle *h);

const char *ip6tc_first_chain(struct xtc_handle *handle);
const char *ip6tc_next_chain(struct xtc_handle *handle);

const struct ip6t_entry *ip6tc_first_rule(const char *chain,
					  struct xtc_handle *handle);

const struct ip6t_entry *ip6tc_next_rule(const struct ip6t_entry *prev,
					 struct xtc_handle *handle);

const char *ip6tc_get_target(const struct ip6t_entry *e,
			     struct xtc_handle *handle);

int ip6tc_builtin(const char *chain, struct xtc_handle *const handle);

const char *ip6tc_get_policy(const char *chain,
			     struct xt_counters *counters,
			     struct xtc_handle *handle);


int ip6tc_insert_entry(const xt_chainlabel chain,
		       const struct ip6t_entry *e,
		       unsigned int rulenum,
		       struct xtc_handle *handle);

int ip6tc_replace_entry(const xt_chainlabel chain,
			const struct ip6t_entry *e,
			unsigned int rulenum,
			struct xtc_handle *handle);

int ip6tc_append_entry(const xt_chainlabel chain,
		       const struct ip6t_entry *e,
		       struct xtc_handle *handle);

int ip6tc_check_entry(const xt_chainlabel chain,
		       const struct ip6t_entry *origfw,
		       unsigned char *matchmask,
		       struct xtc_handle *handle);

int ip6tc_delete_entry(const xt_chainlabel chain,
		       const struct ip6t_entry *origfw,
		       unsigned char *matchmask,
		       struct xtc_handle *handle);

int ip6tc_delete_num_entry(const xt_chainlabel chain,
			   unsigned int rulenum,
			   struct xtc_handle *handle);

const char *ip6tc_check_packet(const xt_chainlabel chain,
			       struct ip6t_entry *,
			       struct xtc_handle *handle);

int ip6tc_flush_entries(const xt_chainlabel chain,
			struct xtc_handle *handle);

int ip6tc_zero_entries(const xt_chainlabel chain,
		       struct xtc_handle *handle);

int ip6tc_create_chain(const xt_chainlabel chain,
		       struct xtc_handle *handle);

int ip6tc_delete_chain(const xt_chainlabel chain,
		       struct xtc_handle *handle);

int ip6tc_rename_chain(const xt_chainlabel oldname,
		       const xt_chainlabel newname,
		       struct xtc_handle *handle);

int ip6tc_set_policy(const xt_chainlabel chain,
		     const xt_chainlabel policy,
		     struct xt_counters *counters,
		     struct xtc_handle *handle);

int ip6tc_get_references(unsigned int *ref, const xt_chainlabel chain,
			 struct xtc_handle *handle);

struct xt_counters *ip6tc_read_counter(const xt_chainlabel chain,
					unsigned int rulenum,
					struct xtc_handle *handle);

int ip6tc_zero_counter(const xt_chainlabel chain,
		       unsigned int rulenum,
		       struct xtc_handle *handle);

int ip6tc_set_counter(const xt_chainlabel chain,
		      unsigned int rulenum,
		      struct xt_counters *counters,
		      struct xtc_handle *handle);

int ip6tc_commit(struct xtc_handle *handle);

int ip6tc_get_raw_socket(void);

const char *ip6tc_strerror(int err);

extern void dump_entries6(struct xtc_handle *const);

extern const struct xtc_ops ip6tc_ops;

#endif 
