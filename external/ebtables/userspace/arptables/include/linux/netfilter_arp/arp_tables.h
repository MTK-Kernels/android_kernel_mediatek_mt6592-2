
#ifndef _ARPTABLES_H
#define _ARPTABLES_H

#ifdef __KERNEL__
#include <linux/if.h>
#include <linux/types.h>
#include <linux/in.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#endif

#include <linux/netfilter_arp.h>

#define ARPT_FUNCTION_MAXNAMELEN 30
#define ARPT_TABLE_MAXNAMELEN 32

#define ARPT_DEV_ADDR_LEN_MAX 16

struct arpt_devaddr_info {
	char addr[ARPT_DEV_ADDR_LEN_MAX];
	char mask[ARPT_DEV_ADDR_LEN_MAX];
};

struct arpt_arp {
	
	struct in_addr src, tgt;
	
	struct in_addr smsk, tmsk;

	
	u_int8_t arhln, arhln_mask;
	struct arpt_devaddr_info src_devaddr;
	struct arpt_devaddr_info tgt_devaddr;

	
	u_int16_t arpop, arpop_mask;

	
	u_int16_t arhrd, arhrd_mask;
	u_int16_t arpro, arpro_mask;


	char iniface[IFNAMSIZ], outiface[IFNAMSIZ];
	unsigned char iniface_mask[IFNAMSIZ], outiface_mask[IFNAMSIZ];

	
	u_int8_t flags;
	
	u_int16_t invflags;
};

struct arpt_entry_target
{
	union {
		struct {
			u_int16_t target_size;

			
			char name[ARPT_FUNCTION_MAXNAMELEN];
		} user;
		struct {
			u_int16_t target_size;

			
			struct arpt_target *target;
		} kernel;

		
		u_int16_t target_size;
	} u;

	unsigned char data[0];
};

struct arpt_standard_target
{
	struct arpt_entry_target target;
	int verdict;
};

struct arpt_counters
{
	u_int64_t pcnt, bcnt;			
};

#define ARPT_F_MASK		0x00	

#define ARPT_INV_VIA_IN		0x0001	
#define ARPT_INV_VIA_OUT	0x0002	
#define ARPT_INV_SRCIP		0x0004	
#define ARPT_INV_TGTIP		0x0008	
#define ARPT_INV_SRCDEVADDR	0x0010	
#define ARPT_INV_TGTDEVADDR	0x0020	
#define ARPT_INV_ARPOP		0x0040	
#define ARPT_INV_ARPHRD		0x0080	
#define ARPT_INV_ARPPRO		0x0100	
#define ARPT_INV_ARPHLN		0x0200	
#define ARPT_INV_MASK		0x03FF	

struct arpt_entry
{
	struct arpt_arp arp;

	
	u_int16_t target_offset;
	
	u_int16_t next_offset;

	
	unsigned int comefrom;

	
	struct arpt_counters counters;

	
	unsigned char elems[0];
};

#define ARPT_BASE_CTL		96	

#define ARPT_SO_SET_REPLACE		(ARPT_BASE_CTL)
#define ARPT_SO_SET_ADD_COUNTERS	(ARPT_BASE_CTL + 1)
#define ARPT_SO_SET_MAX			ARPT_SO_SET_ADD_COUNTERS

#define ARPT_SO_GET_INFO		(ARPT_BASE_CTL)
#define ARPT_SO_GET_ENTRIES		(ARPT_BASE_CTL + 1)
#define ARPT_SO_GET_MAX			ARPT_SO_GET_ENTRIES

#define ARPT_CONTINUE 0xFFFFFFFF

#define ARPT_RETURN (-NF_REPEAT - 1)

struct arpt_getinfo
{
	
	char name[ARPT_TABLE_MAXNAMELEN];

	
	
	unsigned int valid_hooks;

	
	unsigned int hook_entry[3];

	
	unsigned int underflow[3];

	
	unsigned int num_entries;

	
	unsigned int size;
};

struct arpt_replace
{
	
	char name[ARPT_TABLE_MAXNAMELEN];

	unsigned int valid_hooks;

	
	unsigned int num_entries;

	
	unsigned int size;

	
	unsigned int hook_entry[3];

	
	unsigned int underflow[3];

	
	
	unsigned int num_counters;
	
	struct arpt_counters *counters;

	
	struct arpt_entry entries[0];
};

struct arpt_counters_info
{
	
	char name[ARPT_TABLE_MAXNAMELEN];

	unsigned int num_counters;

	
	struct arpt_counters counters[0];
};

struct arpt_get_entries
{
	
	char name[ARPT_TABLE_MAXNAMELEN];

	
	unsigned int size;

	
	struct arpt_entry entrytable[0];
};

#define ARPT_STANDARD_TARGET ""
#define ARPT_ERROR_TARGET "ERROR"

static __inline__ struct arpt_entry_target *arpt_get_target(struct arpt_entry *e)
{
	return (void *)e + e->target_offset;
}

#define ARPT_ENTRY_ITERATE(entries, size, fn, args...)		\
({								\
	unsigned int __i;					\
	int __ret = 0;						\
	struct arpt_entry *__entry;				\
								\
	for (__i = 0; __i < (size); __i += __entry->next_offset) { \
		__entry = (void *)(entries) + __i;		\
								\
		__ret = fn(__entry , ## args);			\
		if (__ret != 0)					\
			break;					\
	}							\
	__ret;							\
})

#ifdef __KERNEL__

struct arpt_target
{
	struct list_head list;

	const char name[ARPT_FUNCTION_MAXNAMELEN];

	
	unsigned int (*target)(struct sk_buff **pskb,
			       unsigned int hooknum,
			       const struct net_device *in,
			       const struct net_device *out,
			       const void *targinfo,
			       void *userdata);

	
	int (*checkentry)(const char *tablename,
			  const struct arpt_entry *e,
			  void *targinfo,
			  unsigned int targinfosize,
			  unsigned int hook_mask);

	
	void (*destroy)(void *targinfo, unsigned int targinfosize);

	
	struct module *me;
};

extern int arpt_register_target(struct arpt_target *target);
extern void arpt_unregister_target(struct arpt_target *target);

struct arpt_table
{
	struct list_head list;

	
	char name[ARPT_TABLE_MAXNAMELEN];

	
	struct arpt_replace *table;

	
	unsigned int valid_hooks;

	
	rwlock_t lock;

	
	struct arpt_table_info *private;

	
	struct module *me;
};

extern int arpt_register_table(struct arpt_table *table);
extern void arpt_unregister_table(struct arpt_table *table);
extern unsigned int arpt_do_table(struct sk_buff **pskb,
				  unsigned int hook,
				  const struct net_device *in,
				  const struct net_device *out,
				  struct arpt_table *table,
				  void *userdata);

#define ARPT_ALIGN(s) (((s) + (__alignof__(struct arpt_entry)-1)) & ~(__alignof__(struct arpt_entry)-1))
#endif 
#endif 
