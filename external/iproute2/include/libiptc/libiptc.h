#ifndef _LIBIPTC_H
#define _LIBIPTC_H

#include <libiptc/ipt_kernel_headers.h>
#include <linux/netfilter_ipv4/ip_tables.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IPT_MIN_ALIGN
#define IPT_MIN_ALIGN (__alignof__(struct ipt_entry))
#endif

#define IPT_ALIGN(s) (((s) + ((IPT_MIN_ALIGN)-1)) & ~((IPT_MIN_ALIGN)-1))

typedef char ipt_chainlabel[32];

#define IPTC_LABEL_ACCEPT  "ACCEPT"
#define IPTC_LABEL_DROP    "DROP"
#define IPTC_LABEL_QUEUE   "QUEUE"
#define IPTC_LABEL_RETURN  "RETURN"

typedef struct iptc_handle *iptc_handle_t;

int iptc_is_chain(const char *chain, const iptc_handle_t handle);

iptc_handle_t iptc_init(const char *tablename);

void iptc_free(iptc_handle_t *h);

const char *iptc_first_chain(iptc_handle_t *handle);
const char *iptc_next_chain(iptc_handle_t *handle);

const struct ipt_entry *iptc_first_rule(const char *chain,
					iptc_handle_t *handle);

const struct ipt_entry *iptc_next_rule(const struct ipt_entry *prev,
				       iptc_handle_t *handle);

const char *iptc_get_target(const struct ipt_entry *e,
			    iptc_handle_t *handle);

int iptc_builtin(const char *chain, const iptc_handle_t handle);

const char *iptc_get_policy(const char *chain,
			    struct ipt_counters *counter,
			    iptc_handle_t *handle);


int iptc_insert_entry(const ipt_chainlabel chain,
		      const struct ipt_entry *e,
		      unsigned int rulenum,
		      iptc_handle_t *handle);

int iptc_replace_entry(const ipt_chainlabel chain,
		       const struct ipt_entry *e,
		       unsigned int rulenum,
		       iptc_handle_t *handle);

int iptc_append_entry(const ipt_chainlabel chain,
		      const struct ipt_entry *e,
		      iptc_handle_t *handle);

int iptc_delete_entry(const ipt_chainlabel chain,
		      const struct ipt_entry *origfw,
		      unsigned char *matchmask,
		      iptc_handle_t *handle);

int iptc_delete_num_entry(const ipt_chainlabel chain,
			  unsigned int rulenum,
			  iptc_handle_t *handle);

const char *iptc_check_packet(const ipt_chainlabel chain,
			      struct ipt_entry *entry,
			      iptc_handle_t *handle);

int iptc_flush_entries(const ipt_chainlabel chain,
		       iptc_handle_t *handle);

int iptc_zero_entries(const ipt_chainlabel chain,
		      iptc_handle_t *handle);

int iptc_create_chain(const ipt_chainlabel chain,
		      iptc_handle_t *handle);

int iptc_delete_chain(const ipt_chainlabel chain,
		      iptc_handle_t *handle);

int iptc_rename_chain(const ipt_chainlabel oldname,
		      const ipt_chainlabel newname,
		      iptc_handle_t *handle);

int iptc_set_policy(const ipt_chainlabel chain,
		    const ipt_chainlabel policy,
		    struct ipt_counters *counters,
		    iptc_handle_t *handle);

int iptc_get_references(unsigned int *ref,
			const ipt_chainlabel chain,
			iptc_handle_t *handle);

struct ipt_counters *iptc_read_counter(const ipt_chainlabel chain,
				       unsigned int rulenum,
				       iptc_handle_t *handle);

int iptc_zero_counter(const ipt_chainlabel chain,
		      unsigned int rulenum,
		      iptc_handle_t *handle);

int iptc_set_counter(const ipt_chainlabel chain,
		     unsigned int rulenum,
		     struct ipt_counters *counters,
		     iptc_handle_t *handle);

int iptc_commit(iptc_handle_t *handle);

int iptc_get_raw_socket(void);

const char *iptc_strerror(int err);

#ifdef __cplusplus
}
#endif


#endif 
