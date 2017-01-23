#ifndef _LIBIP6TC_H
#define _LIBIP6TC_H

#include <libiptc/ipt_kernel_headers.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

#ifndef IP6T_MIN_ALIGN
#define IP6T_MIN_ALIGN (__alignof__(struct ip6t_entry))
#endif
#define IP6T_ALIGN(s) (((s) + (IP6T_MIN_ALIGN-1)) & ~(IP6T_MIN_ALIGN-1))

typedef char ip6t_chainlabel[32];

#define IP6TC_LABEL_ACCEPT "ACCEPT"
#define IP6TC_LABEL_DROP "DROP"
#define IP6TC_LABEL_QUEUE   "QUEUE"
#define IP6TC_LABEL_RETURN "RETURN"

typedef struct ip6tc_handle *ip6tc_handle_t;

int ip6tc_is_chain(const char *chain, const ip6tc_handle_t handle);

ip6tc_handle_t ip6tc_init(const char *tablename);

void ip6tc_free(ip6tc_handle_t *h);

const char *ip6tc_first_chain(ip6tc_handle_t *handle);
const char *ip6tc_next_chain(ip6tc_handle_t *handle);

const struct ip6t_entry *ip6tc_first_rule(const char *chain,
					  ip6tc_handle_t *handle);

const struct ip6t_entry *ip6tc_next_rule(const struct ip6t_entry *prev,
					 ip6tc_handle_t *handle);

const char *ip6tc_get_target(const struct ip6t_entry *e,
			     ip6tc_handle_t *handle);

int ip6tc_builtin(const char *chain, const ip6tc_handle_t handle);

const char *ip6tc_get_policy(const char *chain,
			     struct ip6t_counters *counters,
			     ip6tc_handle_t *handle);


int ip6tc_insert_entry(const ip6t_chainlabel chain,
		       const struct ip6t_entry *e,
		       unsigned int rulenum,
		       ip6tc_handle_t *handle);

int ip6tc_replace_entry(const ip6t_chainlabel chain,
			const struct ip6t_entry *e,
			unsigned int rulenum,
			ip6tc_handle_t *handle);

int ip6tc_append_entry(const ip6t_chainlabel chain,
		       const struct ip6t_entry *e,
		       ip6tc_handle_t *handle);

int ip6tc_delete_entry(const ip6t_chainlabel chain,
		       const struct ip6t_entry *origfw,
		       unsigned char *matchmask,
		       ip6tc_handle_t *handle);

int ip6tc_delete_num_entry(const ip6t_chainlabel chain,
			   unsigned int rulenum,
			   ip6tc_handle_t *handle);

const char *ip6tc_check_packet(const ip6t_chainlabel chain,
			       struct ip6t_entry *,
			       ip6tc_handle_t *handle);

int ip6tc_flush_entries(const ip6t_chainlabel chain,
			ip6tc_handle_t *handle);

int ip6tc_zero_entries(const ip6t_chainlabel chain,
		       ip6tc_handle_t *handle);

int ip6tc_create_chain(const ip6t_chainlabel chain,
		       ip6tc_handle_t *handle);

int ip6tc_delete_chain(const ip6t_chainlabel chain,
		       ip6tc_handle_t *handle);

int ip6tc_rename_chain(const ip6t_chainlabel oldname,
		       const ip6t_chainlabel newname,
		       ip6tc_handle_t *handle);

int ip6tc_set_policy(const ip6t_chainlabel chain,
		     const ip6t_chainlabel policy,
		     struct ip6t_counters *counters,
		     ip6tc_handle_t *handle);

int ip6tc_get_references(unsigned int *ref, const ip6t_chainlabel chain,
			 ip6tc_handle_t *handle);

struct ip6t_counters *ip6tc_read_counter(const ip6t_chainlabel chain,
					unsigned int rulenum,
					ip6tc_handle_t *handle);

int ip6tc_zero_counter(const ip6t_chainlabel chain,
		       unsigned int rulenum,
		       ip6tc_handle_t *handle);

int ip6tc_set_counter(const ip6t_chainlabel chain,
		      unsigned int rulenum,
		      struct ip6t_counters *counters,
		      ip6tc_handle_t *handle);

int ip6tc_commit(ip6tc_handle_t *handle);

int ip6tc_get_raw_socket();

const char *ip6tc_strerror(int err);

int ipv6_prefix_length(const struct in6_addr *a);

#endif 
