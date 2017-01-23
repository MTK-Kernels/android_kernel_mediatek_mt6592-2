#ifndef _LIBIPTC_H
#define _LIBIPTC_H

#include <linux/types.h>
#include <libiptc/ipt_kernel_headers.h>
#ifdef __cplusplus
#	include <climits>
#else
#	include <limits.h> 
#endif
#include <linux/netfilter_ipv4/ip_tables.h>
#include <libiptc/xtcshared.h>

#ifdef __cplusplus
extern "C" {
#endif

#define iptc_handle xtc_handle
#define ipt_chainlabel xt_chainlabel

#define IPTC_LABEL_ACCEPT  "ACCEPT"
#define IPTC_LABEL_DROP    "DROP"
#define IPTC_LABEL_QUEUE   "QUEUE"
#define IPTC_LABEL_RETURN  "RETURN"

int iptc_is_chain(const char *chain, struct xtc_handle *const handle);

struct xtc_handle *iptc_init(const char *tablename);

void iptc_free(struct xtc_handle *h);

const char *iptc_first_chain(struct xtc_handle *handle);
const char *iptc_next_chain(struct xtc_handle *handle);

const struct ipt_entry *iptc_first_rule(const char *chain,
					struct xtc_handle *handle);

const struct ipt_entry *iptc_next_rule(const struct ipt_entry *prev,
				       struct xtc_handle *handle);

const char *iptc_get_target(const struct ipt_entry *e,
			    struct xtc_handle *handle);

int iptc_builtin(const char *chain, struct xtc_handle *const handle);

const char *iptc_get_policy(const char *chain,
			    struct xt_counters *counter,
			    struct xtc_handle *handle);


int iptc_insert_entry(const xt_chainlabel chain,
		      const struct ipt_entry *e,
		      unsigned int rulenum,
		      struct xtc_handle *handle);

int iptc_replace_entry(const xt_chainlabel chain,
		       const struct ipt_entry *e,
		       unsigned int rulenum,
		       struct xtc_handle *handle);

int iptc_append_entry(const xt_chainlabel chain,
		      const struct ipt_entry *e,
		      struct xtc_handle *handle);

int iptc_check_entry(const xt_chainlabel chain,
		      const struct ipt_entry *origfw,
		      unsigned char *matchmask,
		      struct xtc_handle *handle);

int iptc_delete_entry(const xt_chainlabel chain,
		      const struct ipt_entry *origfw,
		      unsigned char *matchmask,
		      struct xtc_handle *handle);

int iptc_delete_num_entry(const xt_chainlabel chain,
			  unsigned int rulenum,
			  struct xtc_handle *handle);

const char *iptc_check_packet(const xt_chainlabel chain,
			      struct ipt_entry *entry,
			      struct xtc_handle *handle);

int iptc_flush_entries(const xt_chainlabel chain,
		       struct xtc_handle *handle);

int iptc_zero_entries(const xt_chainlabel chain,
		      struct xtc_handle *handle);

int iptc_create_chain(const xt_chainlabel chain,
		      struct xtc_handle *handle);

int iptc_delete_chain(const xt_chainlabel chain,
		      struct xtc_handle *handle);

int iptc_rename_chain(const xt_chainlabel oldname,
		      const xt_chainlabel newname,
		      struct xtc_handle *handle);

int iptc_set_policy(const xt_chainlabel chain,
		    const xt_chainlabel policy,
		    struct xt_counters *counters,
		    struct xtc_handle *handle);

int iptc_get_references(unsigned int *ref,
			const xt_chainlabel chain,
			struct xtc_handle *handle);

struct xt_counters *iptc_read_counter(const xt_chainlabel chain,
				       unsigned int rulenum,
				       struct xtc_handle *handle);

int iptc_zero_counter(const xt_chainlabel chain,
		      unsigned int rulenum,
		      struct xtc_handle *handle);

int iptc_set_counter(const xt_chainlabel chain,
		     unsigned int rulenum,
		     struct xt_counters *counters,
		     struct xtc_handle *handle);

int iptc_commit(struct xtc_handle *handle);

int iptc_get_raw_socket(void);

const char *iptc_strerror(int err);

extern void dump_entries(struct xtc_handle *const);

extern const struct xtc_ops iptc_ops;

#ifdef __cplusplus
}
#endif


#endif 
