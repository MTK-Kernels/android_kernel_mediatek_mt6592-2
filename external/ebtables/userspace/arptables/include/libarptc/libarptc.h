#ifndef _LIBARPTC_H
#define _LIBARPTC_H

#include <libarptc/arpt_kernel_headers.h>
#include <linux/netfilter_arp/arp_tables.h>

#ifndef ARPT_MIN_ALIGN
#define ARPT_MIN_ALIGN (__alignof__(struct arpt_entry))
#endif

#define ARPT_ALIGN(s) (((s) + ((ARPT_MIN_ALIGN)-1)) & ~((ARPT_MIN_ALIGN)-1))

typedef char arpt_chainlabel[32];

#define ARPTC_LABEL_ACCEPT  "ACCEPT"
#define ARPTC_LABEL_DROP    "DROP"
#define ARPTC_LABEL_QUEUE   "QUEUE"
#define ARPTC_LABEL_RETURN  "RETURN"


extern int RUNTIME_NF_ARP_NUMHOOKS; 


typedef struct arptc_handle *arptc_handle_t;

int arptc_is_chain(const char *chain, const arptc_handle_t handle);

arptc_handle_t arptc_init(const char *tablename);

const char *arptc_first_chain(arptc_handle_t *handle);
const char *arptc_next_chain(arptc_handle_t *handle);

const struct arpt_entry *arptc_first_rule(const char *chain,
					arptc_handle_t *handle);

const struct arpt_entry *arptc_next_rule(const struct arpt_entry *prev,
				       arptc_handle_t *handle);

const char *arptc_get_target(const struct arpt_entry *e,
			    arptc_handle_t *handle);

int arptc_builtin(const char *chain, const arptc_handle_t handle);

const char *arptc_get_policy(const char *chain,
			    struct arpt_counters *counter,
			    arptc_handle_t *handle);


int arptc_insert_entry(const arpt_chainlabel chain,
		      const struct arpt_entry *e,
		      unsigned int rulenum,
		      arptc_handle_t *handle);

int arptc_replace_entry(const arpt_chainlabel chain,
		       const struct arpt_entry *e,
		       unsigned int rulenum,
		       arptc_handle_t *handle);

int arptc_append_entry(const arpt_chainlabel chain,
		      const struct arpt_entry *e,
		      arptc_handle_t *handle);

int arptc_delete_entry(const arpt_chainlabel chain,
		      const struct arpt_entry *origfw,
		      unsigned char *matchmask,
		      arptc_handle_t *handle);

int arptc_delete_num_entry(const arpt_chainlabel chain,
			  unsigned int rulenum,
			  arptc_handle_t *handle);

const char *arptc_check_packet(const arpt_chainlabel chain,
			      struct arpt_entry *entry,
			      arptc_handle_t *handle);

int arptc_flush_entries(const arpt_chainlabel chain,
		       arptc_handle_t *handle);

int arptc_zero_entries(const arpt_chainlabel chain,
		      arptc_handle_t *handle);

int arptc_create_chain(const arpt_chainlabel chain,
		      arptc_handle_t *handle);

int arptc_delete_chain(const arpt_chainlabel chain,
		      arptc_handle_t *handle);

int arptc_rename_chain(const arpt_chainlabel oldname,
		      const arpt_chainlabel newname,
		      arptc_handle_t *handle);

int arptc_set_policy(const arpt_chainlabel chain,
		    const arpt_chainlabel policy,
		    struct arpt_counters *counters,
		    arptc_handle_t *handle);

int arptc_get_references(unsigned int *ref,
			const arpt_chainlabel chain,
			arptc_handle_t *handle);

struct arpt_counters *arptc_read_counter(const arpt_chainlabel chain,
				       unsigned int rulenum,
				       arptc_handle_t *handle);

int arptc_zero_counter(const arpt_chainlabel chain,
		      unsigned int rulenum,
		      arptc_handle_t *handle);

int arptc_set_counter(const arpt_chainlabel chain,
		     unsigned int rulenum,
		     struct arpt_counters *counters,
		     arptc_handle_t *handle);

int arptc_commit(arptc_handle_t *handle);

int arptc_get_raw_socket();

const char *arptc_strerror(int err);



#endif 
