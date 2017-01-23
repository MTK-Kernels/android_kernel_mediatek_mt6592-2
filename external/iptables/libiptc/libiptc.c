

/* (C) 1999 Paul ``Rusty'' Russell - Placed under the GNU GPL (See
 * COPYING for details).
 * (C) 2000-2004 by the Netfilter Core Team <coreteam@netfilter.org>
 *
 * 2003-Jun-20: Harald Welte <laforge@netfilter.org>:
 *	- Reimplementation of chain cache to use offsets instead of entries
 * 2003-Jun-23: Harald Welte <laforge@netfilter.org>:
 * 	- performance optimization, sponsored by Astaro AG (http://www.astaro.com/)
 * 	  don't rebuild the chain cache after every operation, instead fix it
 * 	  up after a ruleset change.
 * 2004-Aug-18: Harald Welte <laforge@netfilter.org>:
 * 	- further performance work: total reimplementation of libiptc.
 * 	- libiptc now has a real internal (linked-list) represntation of the
 * 	  ruleset and a parser/compiler from/to this internal representation
 * 	- again sponsored by Astaro AG (http://www.astaro.com/)
 *
 * 2008-Jan+Jul: Jesper Dangaard Brouer <hawk@comx.dk>
 * 	- performance work: speedup chain list "name" searching.
 * 	- performance work: speedup initial ruleset parsing.
 * 	- sponsored by ComX Networks A/S (http://www.comx.dk/)
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <xtables.h>
#include <libiptc/xtcshared.h>

#include "linux_list.h"


#ifdef IPTC_DEBUG2
#include <fcntl.h>
#define DEBUGP(x, args...)	fprintf(stderr, "%s: " x, __FUNCTION__, ## args)
#define DEBUGP_C(x, args...)	fprintf(stderr, x, ## args)
#else
#define DEBUGP(x, args...)
#define DEBUGP_C(x, args...)
#endif

#ifdef DEBUG
#define debug(x, args...)	fprintf(stderr, x, ## args)
#else
#define debug(x, args...)
#endif

static void *iptc_fn = NULL;

static const char *hooknames[] = {
	[HOOK_PRE_ROUTING]	= "PREROUTING",
	[HOOK_LOCAL_IN]		= "INPUT",
	[HOOK_FORWARD]		= "FORWARD",
	[HOOK_LOCAL_OUT]	= "OUTPUT",
	[HOOK_POST_ROUTING]	= "POSTROUTING",
};

#undef ipt_error_target 
struct ipt_error_target
{
	STRUCT_ENTRY_TARGET t;
	char error[TABLE_MAXNAMELEN];
};

struct chain_head;
struct rule_head;

struct counter_map
{
	enum {
		COUNTER_MAP_NOMAP,
		COUNTER_MAP_NORMAL_MAP,
		COUNTER_MAP_ZEROED,
		COUNTER_MAP_SET
	} maptype;
	unsigned int mappos;
};

enum iptcc_rule_type {
	IPTCC_R_STANDARD,		
	IPTCC_R_MODULE,			
	IPTCC_R_FALLTHROUGH,		
	IPTCC_R_JUMP,			
};

struct rule_head
{
	struct list_head list;
	struct chain_head *chain;
	struct counter_map counter_map;

	unsigned int index;		
	unsigned int offset;		

	enum iptcc_rule_type type;
	struct chain_head *jump;	

	unsigned int size;		
	STRUCT_ENTRY entry[0];
};

struct chain_head
{
	struct list_head list;
	char name[TABLE_MAXNAMELEN];
	unsigned int hooknum;		
	unsigned int references;	
	int verdict;			

	STRUCT_COUNTERS counters;	
	struct counter_map counter_map;

	unsigned int num_rules;		
	struct list_head rules;		

	unsigned int index;		
	unsigned int head_offset;	
	unsigned int foot_index;	
	unsigned int foot_offset;	
};

struct xtc_handle {
	int sockfd;
	int changed;			 

	struct list_head chains;

	struct chain_head *chain_iterator_cur;
	struct rule_head *rule_iterator_cur;

	unsigned int num_chains;         

	struct chain_head **chain_index;   
	unsigned int        chain_index_sz;

	int sorted_offsets; 

	STRUCT_GETINFO info;
	STRUCT_GET_ENTRIES *entries;
};

enum bsearch_type {
	BSEARCH_NAME,	
	BSEARCH_OFFSET,	
};

static struct chain_head *iptcc_alloc_chain_head(const char *name, int hooknum)
{
	struct chain_head *c = malloc(sizeof(*c));
	if (!c)
		return NULL;
	memset(c, 0, sizeof(*c));

	strncpy(c->name, name, TABLE_MAXNAMELEN);
	c->hooknum = hooknum;
	INIT_LIST_HEAD(&c->rules);

	return c;
}

static struct rule_head *iptcc_alloc_rule(struct chain_head *c, unsigned int size)
{
	struct rule_head *r = malloc(sizeof(*r)+size);
	if (!r)
		return NULL;
	memset(r, 0, sizeof(*r));

	r->chain = c;
	r->size = size;

	return r;
}

static inline void
set_changed(struct xtc_handle *h)
{
	h->changed = 1;
}

#ifdef IPTC_DEBUG
static void do_check(struct xtc_handle *h, unsigned int line);
#define CHECK(h) do { if (!getenv("IPTC_NO_CHECK")) do_check((h), __LINE__); } while(0)
#else
#define CHECK(h)
#endif



static inline int
iptcb_get_number(const STRUCT_ENTRY *i,
	   const STRUCT_ENTRY *seek,
	   unsigned int *pos)
{
	if (i == seek)
		return 1;
	(*pos)++;
	return 0;
}

static inline int
iptcb_get_entry_n(STRUCT_ENTRY *i,
	    unsigned int number,
	    unsigned int *pos,
	    STRUCT_ENTRY **pe)
{
	if (*pos == number) {
		*pe = i;
		return 1;
	}
	(*pos)++;
	return 0;
}

static inline STRUCT_ENTRY *
iptcb_get_entry(struct xtc_handle *h, unsigned int offset)
{
	return (STRUCT_ENTRY *)((char *)h->entries->entrytable + offset);
}

static unsigned int
iptcb_entry2index(struct xtc_handle *const h, const STRUCT_ENTRY *seek)
{
	unsigned int pos = 0;

	if (ENTRY_ITERATE(h->entries->entrytable, h->entries->size,
			  iptcb_get_number, seek, &pos) == 0) {
		fprintf(stderr, "ERROR: offset %u not an entry!\n",
			(unsigned int)((char *)seek - (char *)h->entries->entrytable));
		abort();
	}
	return pos;
}

static inline STRUCT_ENTRY *
iptcb_offset2entry(struct xtc_handle *h, unsigned int offset)
{
	return (STRUCT_ENTRY *) ((void *)h->entries->entrytable+offset);
}


static inline unsigned long
iptcb_entry2offset(struct xtc_handle *const h, const STRUCT_ENTRY *e)
{
	return (void *)e - (void *)h->entries->entrytable;
}

static inline unsigned int
iptcb_offset2index(struct xtc_handle *const h, unsigned int offset)
{
	return iptcb_entry2index(h, iptcb_offset2entry(h, offset));
}

static inline unsigned int
iptcb_ent_is_hook_entry(STRUCT_ENTRY *e, struct xtc_handle *h)
{
	unsigned int i;

	for (i = 0; i < NUMHOOKS; i++) {
		if ((h->info.valid_hooks & (1 << i))
		    && iptcb_get_entry(h, h->info.hook_entry[i]) == e)
			return i+1;
	}
	return 0;
}


#ifndef CHAIN_INDEX_BUCKET_LEN
#define CHAIN_INDEX_BUCKET_LEN 40
#endif

#ifndef CHAIN_INDEX_INSERT_MAX
#define CHAIN_INDEX_INSERT_MAX 355
#endif

static inline unsigned int iptcc_is_builtin(struct chain_head *c);

static struct list_head *
__iptcc_bsearch_chain_index(const char *name, unsigned int offset,
			    unsigned int *idx, struct xtc_handle *handle,
			    enum bsearch_type type)
{
	unsigned int pos, end;
	int res;

	struct list_head *list_pos;
	list_pos=&handle->chains;

	
	if (handle->chain_index_sz == 0) {
		debug("WARNING: handle->chain_index_sz == 0\n");
		return list_pos;
	}

	
	end = handle->chain_index_sz;
	pos = end / 2;

	debug("bsearch Find chain:%s (pos:%d end:%d) (offset:%d)\n",
	      name, pos, end, offset);

	
 loop:
	if (!handle->chain_index[pos]) {
		fprintf(stderr, "ERROR: NULL pointer chain_index[%d]\n", pos);
		return &handle->chains; 
	}

	debug("bsearch Index[%d] name:%s ",
	      pos, handle->chain_index[pos]->name);

	
	switch (type) {
	case BSEARCH_NAME:
		res = strcmp(name, handle->chain_index[pos]->name);
		break;
	case BSEARCH_OFFSET:
		debug("head_offset:[%d] foot_offset:[%d] ",
		      handle->chain_index[pos]->head_offset,
		      handle->chain_index[pos]->foot_offset);
		res = offset - handle->chain_index[pos]->head_offset;
		break;
	default:
		fprintf(stderr, "ERROR: %d not a valid bsearch type\n",
			type);
		abort();
		break;
	}
	debug("res:%d ", res);


	list_pos = &handle->chain_index[pos]->list;
	*idx = pos;

	if (res == 0) { 
		debug("[found] Direct hit pos:%d end:%d\n", pos, end);
		return list_pos;
	} else if (res < 0) { 
		end = pos;
		pos = pos / 2;

		
		if (end == 0) {
			debug("[found] Reached first array elem (end%d)\n",end);
			return list_pos;
		}
		debug("jump back to pos:%d (end:%d)\n", pos, end);
		goto loop;
	} else { 

		
		if (pos == handle->chain_index_sz-1) {
			debug("[found] Last array elem (end:%d)\n", end);
			return list_pos;
		}

		
		switch (type) {
		case BSEARCH_NAME:
			res = strcmp(name, handle->chain_index[pos+1]->name);
			break;
		case BSEARCH_OFFSET:
			res = offset - handle->chain_index[pos+1]->head_offset;
			break;
		}

		if (res < 0) {
			debug("[found] closest list (end:%d)\n", end);
			return list_pos;
		}

		pos = (pos+end)/2;
		debug("jump forward to pos:%d (end:%d)\n", pos, end);
		goto loop;
	}
}

static struct list_head *
iptcc_bsearch_chain_index(const char *name, unsigned int *idx,
			  struct xtc_handle *handle)
{
	return __iptcc_bsearch_chain_index(name, 0, idx, handle, BSEARCH_NAME);
}


static struct list_head *
iptcc_bsearch_chain_offset(unsigned int offset, unsigned int *idx,
			  struct xtc_handle *handle)
{
	struct list_head *pos;

	if (!handle->sorted_offsets)
		pos = handle->chains.next;
	else
		pos = __iptcc_bsearch_chain_index(NULL, offset, idx, handle,
						  BSEARCH_OFFSET);
	return pos;
}


#ifdef DEBUG
static struct list_head *
iptcc_linearly_search_chain_index(const char *name, struct xtc_handle *handle)
{
	unsigned int i=0;
	int res=0;

	struct list_head *list_pos;
	list_pos = &handle->chains;

	if (handle->chain_index_sz)
		list_pos = &handle->chain_index[0]->list;

	

	for (i=0; i < handle->chain_index_sz; i++) {
		if (handle->chain_index[i]) {
			res = strcmp(handle->chain_index[i]->name, name);
			if (res > 0)
				break; 
			list_pos = &handle->chain_index[i]->list;
			if (res == 0)
				break; 
		}
	}

	return list_pos;
}
#endif

static int iptcc_chain_index_alloc(struct xtc_handle *h)
{
	unsigned int list_length = CHAIN_INDEX_BUCKET_LEN;
	unsigned int array_elems;
	unsigned int array_mem;

	
	array_elems = (h->num_chains / list_length) +
                      (h->num_chains % list_length ? 1 : 0);
	array_mem   = sizeof(h->chain_index) * array_elems;

	debug("Alloc Chain index, elems:%d mem:%d bytes\n",
	      array_elems, array_mem);

	h->chain_index = malloc(array_mem);
	if (h->chain_index == NULL && array_mem > 0) {
		h->chain_index_sz = 0;
		return -ENOMEM;
	}
	memset(h->chain_index, 0, array_mem);
	h->chain_index_sz = array_elems;

	return 1;
}

static void iptcc_chain_index_free(struct xtc_handle *h)
{
	h->chain_index_sz = 0;
	free(h->chain_index);
}


#ifdef DEBUG
static void iptcc_chain_index_dump(struct xtc_handle *h)
{
	unsigned int i = 0;

	
	for (i=0; i < h->chain_index_sz; i++) {
		if (h->chain_index[i]) {
			fprintf(stderr, "Chain index[%d].name: %s\n",
				i, h->chain_index[i]->name);
		}
	}
}
#endif

static int iptcc_chain_index_build(struct xtc_handle *h)
{
	unsigned int list_length = CHAIN_INDEX_BUCKET_LEN;
	unsigned int chains = 0;
	unsigned int cindex = 0;
	struct chain_head *c;

	
	debug("Building chain index\n");

	debug("Number of user defined chains:%d bucket_sz:%d array_sz:%d\n",
		h->num_chains, list_length, h->chain_index_sz);

	if (h->chain_index_sz == 0)
		return 0;

	list_for_each_entry(c, &h->chains, list) {

		if (!iptcc_is_builtin(c)) {
			cindex=chains / list_length;

			if (cindex >= h->chain_index_sz)
				break;

			if ((chains % list_length)== 0) {
				debug("\nIndex[%d] Chains:", cindex);
				h->chain_index[cindex] = c;
			}
			chains++;
		}
		debug("%s, ", c->name);
	}
	debug("\n");

	return 1;
}

static int iptcc_chain_index_rebuild(struct xtc_handle *h)
{
	debug("REBUILD chain index array\n");
	iptcc_chain_index_free(h);
	if ((iptcc_chain_index_alloc(h)) < 0)
		return -ENOMEM;
	iptcc_chain_index_build(h);
	return 1;
}

static int iptcc_chain_index_delete_chain(struct chain_head *c, struct xtc_handle *h)
{
	struct list_head *index_ptr, *next;
	struct chain_head *c2;
	unsigned int idx, idx2;

	index_ptr = iptcc_bsearch_chain_index(c->name, &idx, h);

	debug("Del chain[%s] c->list:%p index_ptr:%p\n",
	      c->name, &c->list, index_ptr);

	
	next = c->list.next;
	list_del(&c->list);

	if (index_ptr == &c->list) { 

		c2         = list_entry(next, struct chain_head, list);
		iptcc_bsearch_chain_index(c2->name, &idx2, h);
		if (idx != idx2) {
			
			return iptcc_chain_index_rebuild(h);
		} else {
			
			debug("Update cindex[%d] with next ptr name:[%s]\n",
			      idx, c2->name);
			h->chain_index[idx]=c2;
			return 0;
		}
	}
	return 0;
}



static inline unsigned int iptcc_is_builtin(struct chain_head *c)
{
	return (c->hooknum ? 1 : 0);
}

static struct rule_head *iptcc_get_rule_num(struct chain_head *c,
					    unsigned int rulenum)
{
	struct rule_head *r;
	unsigned int num = 0;

	list_for_each_entry(r, &c->rules, list) {
		num++;
		if (num == rulenum)
			return r;
	}
	return NULL;
}

static struct rule_head *iptcc_get_rule_num_reverse(struct chain_head *c,
					    unsigned int rulenum)
{
	struct rule_head *r;
	unsigned int num = 0;

	list_for_each_entry_reverse(r, &c->rules, list) {
		num++;
		if (num == rulenum)
			return r;
	}
	return NULL;
}

static struct chain_head *
iptcc_find_chain_by_offset(struct xtc_handle *handle, unsigned int offset)
{
	struct list_head *pos;
	struct list_head *list_start_pos;
	unsigned int i;

	if (list_empty(&handle->chains))
		return NULL;

	
  	list_start_pos = iptcc_bsearch_chain_offset(offset, &i, handle);


	debug("Offset:[%u] starting search at index:[%u]\n", offset, i);
	list_for_each(pos, list_start_pos->prev) {
		struct chain_head *c = list_entry(pos, struct chain_head, list);
		debug(".");
		if (offset >= c->head_offset && offset <= c->foot_offset) {
			debug("Offset search found chain:[%s]\n", c->name);
			return c;
		}
	}

	return NULL;
}

static struct chain_head *
iptcc_find_label(const char *name, struct xtc_handle *handle)
{
	struct list_head *pos;
	struct list_head *list_start_pos;
	unsigned int i=0;
	int res;

	if (list_empty(&handle->chains))
		return NULL;

	
	list_for_each(pos, &handle->chains) {
		struct chain_head *c = list_entry(pos, struct chain_head, list);
		if (!iptcc_is_builtin(c))
			break;
		if (!strcmp(c->name, name))
			return c;
	}

	
  	
  	list_start_pos = iptcc_bsearch_chain_index(name, &i, handle);

	
	if (list_start_pos == &handle->chains) {
		list_start_pos = pos;
	}
#ifdef DEBUG
	else {
		
		struct list_head *test_pos;
		struct chain_head *test_c, *tmp_c;
		test_pos = iptcc_linearly_search_chain_index(name, handle);
		if (list_start_pos != test_pos) {
			debug("BUG in chain_index search\n");
			test_c=list_entry(test_pos,      struct chain_head,list);
			tmp_c =list_entry(list_start_pos,struct chain_head,list);
			debug("Verify search found:\n");
			debug(" Chain:%s\n", test_c->name);
			debug("BSearch found:\n");
			debug(" Chain:%s\n", tmp_c->name);
			exit(42);
		}
	}
#endif

	
	if (handle->num_chains == 0)
		return NULL;

	
	list_for_each(pos, list_start_pos->prev) {
		struct chain_head *c = list_entry(pos, struct chain_head, list);
		res = strcmp(c->name, name);
		debug("List search name:%s == %s res:%d\n", name, c->name, res);
		if (res==0)
			return c;

		
		if (res>0 && !iptcc_is_builtin(c)) { 
			debug(" Not in list, walked too far, sorted list\n");
			return NULL;
		}

		
		if (pos == &handle->chains) {
			debug("Stop, list head reached\n");
			return NULL;
		}
	}

	debug("List search NOT found name:%s\n", name);
	return NULL;
}

static void iptcc_delete_rule(struct rule_head *r)
{
	DEBUGP("deleting rule %p (offset %u)\n", r, r->offset);
	
	if (r->type == IPTCC_R_JUMP
	    && r->jump)
		r->jump->references--;

	list_del(&r->list);
	free(r);
}



static int __iptcc_p_del_policy(struct xtc_handle *h, unsigned int num)
{
	const unsigned char *data;

	if (h->chain_iterator_cur) {
		
		struct rule_head *pr = (struct rule_head *)
			h->chain_iterator_cur->rules.prev;

		
		data = GET_TARGET(pr->entry)->data;
		h->chain_iterator_cur->verdict = *(const int *)data;

		
		h->chain_iterator_cur->counter_map.maptype =
						COUNTER_MAP_ZEROED;
		h->chain_iterator_cur->counter_map.mappos = num-1;
		memcpy(&h->chain_iterator_cur->counters, &pr->entry->counters,
			sizeof(h->chain_iterator_cur->counters));

		
		h->chain_iterator_cur->foot_index = num;
		h->chain_iterator_cur->foot_offset = pr->offset;

		
		iptcc_delete_rule(pr);
		h->chain_iterator_cur->num_rules--;

		return 1;
	}
	return 0;
}

static void iptc_insert_chain(struct xtc_handle *h, struct chain_head *c)
{
	struct chain_head *tmp;
	struct list_head  *list_start_pos;
	unsigned int i=1;

	
  	list_start_pos = iptcc_bsearch_chain_index(c->name, &i, h);

	
	if (i==0 && strcmp(c->name, h->chain_index[0]->name) <= 0) {
		h->chain_index[0] = c; 
		list_start_pos = h->chains.next;
		debug("Update chain_index[0] with %s\n", c->name);
	}

	
	if (list_start_pos == &h->chains) {
		list_start_pos = h->chains.next;
	}

	
	if (!c->hooknum) {
		list_for_each_entry(tmp, list_start_pos->prev, list) {
			if (!tmp->hooknum && strcmp(c->name, tmp->name) <= 0) {
				list_add(&c->list, tmp->list.prev);
				return;
			}

			
			if (&tmp->list == &h->chains) {
				debug("Insert, list head reached add to tail\n");
				break;
			}
		}
	}

	
	list_add_tail(&c->list, &h->chains);
}

static void __iptcc_p_add_chain(struct xtc_handle *h, struct chain_head *c,
				unsigned int offset, unsigned int *num)
{
	struct list_head  *tail = h->chains.prev;
	struct chain_head *ctail;

	__iptcc_p_del_policy(h, *num);

	c->head_offset = offset;
	c->index = *num;

	if (iptcc_is_builtin(c)) 
		list_add_tail(&c->list, &h->chains);
	else {
		ctail = list_entry(tail, struct chain_head, list);

		if (strcmp(c->name, ctail->name) > 0 ||
		    iptcc_is_builtin(ctail))
			list_add_tail(&c->list, &h->chains);
		else {
			iptc_insert_chain(h, c);

			h->sorted_offsets = 0;

			debug("NOTICE: chain:[%s] was NOT sorted(ctail:%s)\n",
			      c->name, ctail->name);
		}
	}

	h->chain_iterator_cur = c;
}

static int cache_add_entry(STRUCT_ENTRY *e,
			   struct xtc_handle *h,
			   STRUCT_ENTRY **prev,
			   unsigned int *num)
{
	unsigned int builtin;
	unsigned int offset = (char *)e - (char *)h->entries->entrytable;

	DEBUGP("entering...");

	
	if (iptcb_entry2offset(h,e) + e->next_offset == h->entries->size) {
		
		DEBUGP_C("%u:%u: end of table:\n", *num, offset);

		__iptcc_p_del_policy(h, *num);

		h->chain_iterator_cur = NULL;
		goto out_inc;
	}


	if (strcmp(GET_TARGET(e)->u.user.name, ERROR_TARGET) == 0) {
		struct chain_head *c =
			iptcc_alloc_chain_head((const char *)GET_TARGET(e)->data, 0);
		DEBUGP_C("%u:%u:new userdefined chain %s: %p\n", *num, offset,
			(char *)c->name, c);
		if (!c) {
			errno = -ENOMEM;
			return -1;
		}
		h->num_chains++; 

		__iptcc_p_add_chain(h, c, offset, num);

	} else if ((builtin = iptcb_ent_is_hook_entry(e, h)) != 0) {
		struct chain_head *c =
			iptcc_alloc_chain_head((char *)hooknames[builtin-1],
						builtin);
		DEBUGP_C("%u:%u new builtin chain: %p (rules=%p)\n",
			*num, offset, c, &c->rules);
		if (!c) {
			errno = -ENOMEM;
			return -1;
		}

		c->hooknum = builtin;

		__iptcc_p_add_chain(h, c, offset, num);

		
		goto new_rule;
	} else {
		
		struct rule_head *r;
new_rule:

		if (!(r = iptcc_alloc_rule(h->chain_iterator_cur,
					   e->next_offset))) {
			errno = ENOMEM;
			return -1;
		}
		DEBUGP_C("%u:%u normal rule: %p: ", *num, offset, r);

		r->index = *num;
		r->offset = offset;
		memcpy(r->entry, e, e->next_offset);
		r->counter_map.maptype = COUNTER_MAP_NORMAL_MAP;
		r->counter_map.mappos = r->index;

		
		if (!strcmp(GET_TARGET(e)->u.user.name, STANDARD_TARGET)) {
			STRUCT_STANDARD_TARGET *t;

			t = (STRUCT_STANDARD_TARGET *)GET_TARGET(e);
			if (t->target.u.target_size
			    != ALIGN(sizeof(STRUCT_STANDARD_TARGET))) {
				errno = EINVAL;
				free(r);
				return -1;
			}

			if (t->verdict < 0) {
				DEBUGP_C("standard, verdict=%d\n", t->verdict);
				r->type = IPTCC_R_STANDARD;
			} else if (t->verdict == r->offset+e->next_offset) {
				DEBUGP_C("fallthrough\n");
				r->type = IPTCC_R_FALLTHROUGH;
			} else {
				DEBUGP_C("jump, target=%u\n", t->verdict);
				r->type = IPTCC_R_JUMP;
			}
		} else {
			DEBUGP_C("module, target=%s\n", GET_TARGET(e)->u.user.name);
			r->type = IPTCC_R_MODULE;
		}

		list_add_tail(&r->list, &h->chain_iterator_cur->rules);
		h->chain_iterator_cur->num_rules++;
	}
out_inc:
	(*num)++;
	return 0;
}


static int parse_table(struct xtc_handle *h)
{
	STRUCT_ENTRY *prev;
	unsigned int num = 0;
	struct chain_head *c;

	h->sorted_offsets = 1;

	
	ENTRY_ITERATE(h->entries->entrytable, h->entries->size,
			cache_add_entry, h, &prev, &num);

	
	if ((iptcc_chain_index_alloc(h)) < 0)
		return -ENOMEM;
	iptcc_chain_index_build(h);

	
	list_for_each_entry(c, &h->chains, list) {
		struct rule_head *r;
		list_for_each_entry(r, &c->rules, list) {
			struct chain_head *lc;
			STRUCT_STANDARD_TARGET *t;

			if (r->type != IPTCC_R_JUMP)
				continue;

			t = (STRUCT_STANDARD_TARGET *)GET_TARGET(r->entry);
			lc = iptcc_find_chain_by_offset(h, t->verdict);
			if (!lc)
				return -1;
			r->jump = lc;
			lc->references++;
		}
	}

	return 1;
}



struct iptcb_chain_start{
	STRUCT_ENTRY e;
	struct xt_error_target name;
};
#define IPTCB_CHAIN_START_SIZE	(sizeof(STRUCT_ENTRY) +			\
				 ALIGN(sizeof(struct xt_error_target)))

struct iptcb_chain_foot {
	STRUCT_ENTRY e;
	STRUCT_STANDARD_TARGET target;
};
#define IPTCB_CHAIN_FOOT_SIZE	(sizeof(STRUCT_ENTRY) +			\
				 ALIGN(sizeof(STRUCT_STANDARD_TARGET)))

struct iptcb_chain_error {
	STRUCT_ENTRY entry;
	struct xt_error_target target;
};
#define IPTCB_CHAIN_ERROR_SIZE	(sizeof(STRUCT_ENTRY) +			\
				 ALIGN(sizeof(struct xt_error_target)))



static inline int iptcc_compile_rule (struct xtc_handle *h, STRUCT_REPLACE *repl, struct rule_head *r)
{
	
	if (r->type == IPTCC_R_JUMP) {
		STRUCT_STANDARD_TARGET *t;
		t = (STRUCT_STANDARD_TARGET *)GET_TARGET(r->entry);
		
		memset(t->target.u.user.name, 0, FUNCTION_MAXNAMELEN);
		strcpy(t->target.u.user.name, STANDARD_TARGET);
		t->verdict = r->jump->head_offset + IPTCB_CHAIN_START_SIZE;
	} else if (r->type == IPTCC_R_FALLTHROUGH) {
		STRUCT_STANDARD_TARGET *t;
		t = (STRUCT_STANDARD_TARGET *)GET_TARGET(r->entry);
		t->verdict = r->offset + r->size;
	}

	
	memcpy((char *)repl->entries+r->offset, r->entry, r->size);

	return 1;
}

static int iptcc_compile_chain(struct xtc_handle *h, STRUCT_REPLACE *repl, struct chain_head *c)
{
	int ret;
	struct rule_head *r;
	struct iptcb_chain_start *head;
	struct iptcb_chain_foot *foot;

	
	if (!iptcc_is_builtin(c)) {
		
		head = (void *)repl->entries + c->head_offset;
		head->e.target_offset = sizeof(STRUCT_ENTRY);
		head->e.next_offset = IPTCB_CHAIN_START_SIZE;
		strcpy(head->name.target.u.user.name, ERROR_TARGET);
		head->name.target.u.target_size =
				ALIGN(sizeof(struct xt_error_target));
		strcpy(head->name.errorname, c->name);
	} else {
		repl->hook_entry[c->hooknum-1] = c->head_offset;
		repl->underflow[c->hooknum-1] = c->foot_offset;
	}

	
	list_for_each_entry(r, &c->rules, list) {
		ret = iptcc_compile_rule(h, repl, r);
		if (ret < 0)
			return ret;
	}

	
	foot = (void *)repl->entries + c->foot_offset;
	foot->e.target_offset = sizeof(STRUCT_ENTRY);
	foot->e.next_offset = IPTCB_CHAIN_FOOT_SIZE;
	strcpy(foot->target.target.u.user.name, STANDARD_TARGET);
	foot->target.target.u.target_size =
				ALIGN(sizeof(STRUCT_STANDARD_TARGET));
	
	if (iptcc_is_builtin(c))
		foot->target.verdict = c->verdict;
	else
		foot->target.verdict = RETURN;
	
	memcpy(&foot->e.counters, &c->counters, sizeof(STRUCT_COUNTERS));

	return 0;
}

static int iptcc_compile_chain_offsets(struct xtc_handle *h, struct chain_head *c,
				       unsigned int *offset, unsigned int *num)
{
	struct rule_head *r;

	c->head_offset = *offset;
	DEBUGP("%s: chain_head %u, offset=%u\n", c->name, *num, *offset);

	if (!iptcc_is_builtin(c))  {
		
		*offset += sizeof(STRUCT_ENTRY)
			     + ALIGN(sizeof(struct xt_error_target));
		(*num)++;
	}

	list_for_each_entry(r, &c->rules, list) {
		DEBUGP("rule %u, offset=%u, index=%u\n", *num, *offset, *num);
		r->offset = *offset;
		r->index = *num;
		*offset += r->size;
		(*num)++;
	}

	DEBUGP("%s; chain_foot %u, offset=%u, index=%u\n", c->name, *num,
		*offset, *num);
	c->foot_offset = *offset;
	c->foot_index = *num;
	*offset += sizeof(STRUCT_ENTRY)
		   + ALIGN(sizeof(STRUCT_STANDARD_TARGET));
	(*num)++;

	return 1;
}

static int iptcc_compile_table_prep(struct xtc_handle *h, unsigned int *size)
{
	struct chain_head *c;
	unsigned int offset = 0, num = 0;
	int ret = 0;

	
	list_for_each_entry(c, &h->chains, list) {
		ret = iptcc_compile_chain_offsets(h, c, &offset, &num);
		if (ret < 0)
			return ret;
	}

	
	num++;
	offset += sizeof(STRUCT_ENTRY)
		  + ALIGN(sizeof(struct xt_error_target));

	
	*size = offset;
	return num;
}

static int iptcc_compile_table(struct xtc_handle *h, STRUCT_REPLACE *repl)
{
	struct chain_head *c;
	struct iptcb_chain_error *error;

	
	list_for_each_entry(c, &h->chains, list) {
		int ret = iptcc_compile_chain(h, repl, c);
		if (ret < 0)
			return ret;
	}

	
	error = (void *)repl->entries + repl->size - IPTCB_CHAIN_ERROR_SIZE;
	error->entry.target_offset = sizeof(STRUCT_ENTRY);
	error->entry.next_offset = IPTCB_CHAIN_ERROR_SIZE;
	error->target.target.u.user.target_size =
		ALIGN(sizeof(struct xt_error_target));
	strcpy((char *)&error->target.target.u.user.name, ERROR_TARGET);
	strcpy((char *)&error->target.errorname, "ERROR");

	return 1;
}


static struct xtc_handle *
alloc_handle(const char *tablename, unsigned int size, unsigned int num_rules)
{
	struct xtc_handle *h;

	h = malloc(sizeof(*h));
	if (!h) {
		errno = ENOMEM;
		return NULL;
	}
	memset(h, 0, sizeof(*h));
	INIT_LIST_HEAD(&h->chains);
	strcpy(h->info.name, tablename);

	h->entries = malloc(sizeof(STRUCT_GET_ENTRIES) + size);
	if (!h->entries)
		goto out_free_handle;

	strcpy(h->entries->name, tablename);
	h->entries->size = size;

	return h;

out_free_handle:
	free(h);

	return NULL;
}


struct xtc_handle *
TC_INIT(const char *tablename)
{
	struct xtc_handle *h;
	STRUCT_GETINFO info;
	unsigned int tmp;
	socklen_t s;
	int sockfd;

retry:
	iptc_fn = TC_INIT;

	if (strlen(tablename) >= TABLE_MAXNAMELEN) {
		errno = EINVAL;
		return NULL;
	}

	sockfd = socket(TC_AF, SOCK_RAW, IPPROTO_RAW);
	if (sockfd < 0)
		return NULL;

	if (fcntl(sockfd, F_SETFD, FD_CLOEXEC) == -1) {
		fprintf(stderr, "Could not set close on exec: %s\n",
			strerror(errno));
		abort();
	}

	s = sizeof(info);

	strcpy(info.name, tablename);
	if (getsockopt(sockfd, TC_IPPROTO, SO_GET_INFO, &info, &s) < 0) {
		close(sockfd);
		return NULL;
	}

	DEBUGP("valid_hooks=0x%08x, num_entries=%u, size=%u\n",
		info.valid_hooks, info.num_entries, info.size);

	if ((h = alloc_handle(info.name, info.size, info.num_entries))
	    == NULL) {
		close(sockfd);
		return NULL;
	}

	
	h->sockfd = sockfd;
	h->info = info;

	h->entries->size = h->info.size;

	tmp = sizeof(STRUCT_GET_ENTRIES) + h->info.size;

	if (getsockopt(h->sockfd, TC_IPPROTO, SO_GET_ENTRIES, h->entries,
		       &tmp) < 0)
		goto error;

#ifdef IPTC_DEBUG2
	{
		int fd = open("/tmp/libiptc-so_get_entries.blob",
				O_CREAT|O_WRONLY);
		if (fd >= 0) {
			write(fd, h->entries, tmp);
			close(fd);
		}
	}
#endif

	if (parse_table(h) < 0)
		goto error;

	CHECK(h);
	return h;
error:
	TC_FREE(h);
	
	if (errno == EAGAIN)
		goto retry;
	return NULL;
}

void
TC_FREE(struct xtc_handle *h)
{
	struct chain_head *c, *tmp;

	iptc_fn = TC_FREE;
	close(h->sockfd);

	list_for_each_entry_safe(c, tmp, &h->chains, list) {
		struct rule_head *r, *rtmp;

		list_for_each_entry_safe(r, rtmp, &c->rules, list) {
			free(r);
		}

		free(c);
	}

	iptcc_chain_index_free(h);

	free(h->entries);
	free(h);
}

static inline int
print_match(const STRUCT_ENTRY_MATCH *m)
{
	printf("Match name: `%s'\n", m->u.user.name);
	return 0;
}

static int dump_entry(STRUCT_ENTRY *e, struct xtc_handle *const handle);

void
TC_DUMP_ENTRIES(struct xtc_handle *const handle)
{
	iptc_fn = TC_DUMP_ENTRIES;
	CHECK(handle);

	printf("libiptc v%s. %u bytes.\n",
	       XTABLES_VERSION, handle->entries->size);
	printf("Table `%s'\n", handle->info.name);
	printf("Hooks: pre/in/fwd/out/post = %x/%x/%x/%x/%x\n",
	       handle->info.hook_entry[HOOK_PRE_ROUTING],
	       handle->info.hook_entry[HOOK_LOCAL_IN],
	       handle->info.hook_entry[HOOK_FORWARD],
	       handle->info.hook_entry[HOOK_LOCAL_OUT],
	       handle->info.hook_entry[HOOK_POST_ROUTING]);
	printf("Underflows: pre/in/fwd/out/post = %x/%x/%x/%x/%x\n",
	       handle->info.underflow[HOOK_PRE_ROUTING],
	       handle->info.underflow[HOOK_LOCAL_IN],
	       handle->info.underflow[HOOK_FORWARD],
	       handle->info.underflow[HOOK_LOCAL_OUT],
	       handle->info.underflow[HOOK_POST_ROUTING]);

	ENTRY_ITERATE(handle->entries->entrytable, handle->entries->size,
		      dump_entry, handle);
}

int TC_IS_CHAIN(const char *chain, struct xtc_handle *const handle)
{
	iptc_fn = TC_IS_CHAIN;
	return iptcc_find_label(chain, handle) != NULL;
}

static void iptcc_chain_iterator_advance(struct xtc_handle *handle)
{
	struct chain_head *c = handle->chain_iterator_cur;

	if (c->list.next == &handle->chains)
		handle->chain_iterator_cur = NULL;
	else
		handle->chain_iterator_cur =
			list_entry(c->list.next, struct chain_head, list);
}

const char *
TC_FIRST_CHAIN(struct xtc_handle *handle)
{
	struct chain_head *c = list_entry(handle->chains.next,
					  struct chain_head, list);

	iptc_fn = TC_FIRST_CHAIN;


	if (list_empty(&handle->chains)) {
		DEBUGP(": no chains\n");
		return NULL;
	}

	handle->chain_iterator_cur = c;
	iptcc_chain_iterator_advance(handle);

	DEBUGP(": returning `%s'\n", c->name);
	return c->name;
}

const char *
TC_NEXT_CHAIN(struct xtc_handle *handle)
{
	struct chain_head *c = handle->chain_iterator_cur;

	iptc_fn = TC_NEXT_CHAIN;

	if (!c) {
		DEBUGP(": no more chains\n");
		return NULL;
	}

	iptcc_chain_iterator_advance(handle);

	DEBUGP(": returning `%s'\n", c->name);
	return c->name;
}

const STRUCT_ENTRY *
TC_FIRST_RULE(const char *chain, struct xtc_handle *handle)
{
	struct chain_head *c;
	struct rule_head *r;

	iptc_fn = TC_FIRST_RULE;

	DEBUGP("first rule(%s): ", chain);

	c = iptcc_find_label(chain, handle);
	if (!c) {
		errno = ENOENT;
		return NULL;
	}

	
	if (list_empty(&c->rules)) {
		DEBUGP_C("no rules, returning NULL\n");
		return NULL;
	}

	r = list_entry(c->rules.next, struct rule_head, list);
	handle->rule_iterator_cur = r;
	DEBUGP_C("%p\n", r);

	return r->entry;
}

const STRUCT_ENTRY *
TC_NEXT_RULE(const STRUCT_ENTRY *prev, struct xtc_handle *handle)
{
	struct rule_head *r;

	iptc_fn = TC_NEXT_RULE;
	DEBUGP("rule_iterator_cur=%p...", handle->rule_iterator_cur);

	if (handle->rule_iterator_cur == NULL) {
		DEBUGP_C("returning NULL\n");
		return NULL;
	}

	r = list_entry(handle->rule_iterator_cur->list.next,
			struct rule_head, list);

	iptc_fn = TC_NEXT_RULE;

	DEBUGP_C("next=%p, head=%p...", &r->list,
		&handle->rule_iterator_cur->chain->rules);

	if (&r->list == &handle->rule_iterator_cur->chain->rules) {
		handle->rule_iterator_cur = NULL;
		DEBUGP_C("finished, returning NULL\n");
		return NULL;
	}

	handle->rule_iterator_cur = r;

	
	DEBUGP_C("returning rule %p\n", r);
	return r->entry;
}

static const char *standard_target_map(int verdict)
{
	switch (verdict) {
		case RETURN:
			return LABEL_RETURN;
			break;
		case -NF_ACCEPT-1:
			return LABEL_ACCEPT;
			break;
		case -NF_DROP-1:
			return LABEL_DROP;
			break;
		case -NF_QUEUE-1:
			return LABEL_QUEUE;
			break;
		default:
			fprintf(stderr, "ERROR: %d not a valid target)\n",
				verdict);
			abort();
			break;
	}
	
	return NULL;
}

const char *TC_GET_TARGET(const STRUCT_ENTRY *ce,
			  struct xtc_handle *handle)
{
	STRUCT_ENTRY *e = (STRUCT_ENTRY *)ce;
	struct rule_head *r = container_of(e, struct rule_head, entry[0]);
	const unsigned char *data;

	iptc_fn = TC_GET_TARGET;

	switch(r->type) {
		int spos;
		case IPTCC_R_FALLTHROUGH:
			return "";
			break;
		case IPTCC_R_JUMP:
			DEBUGP("r=%p, jump=%p, name=`%s'\n", r, r->jump, r->jump->name);
			return r->jump->name;
			break;
		case IPTCC_R_STANDARD:
			data = GET_TARGET(e)->data;
			spos = *(const int *)data;
			DEBUGP("r=%p, spos=%d'\n", r, spos);
			return standard_target_map(spos);
			break;
		case IPTCC_R_MODULE:
			return GET_TARGET(e)->u.user.name;
			break;
	}
	return NULL;
}
int
TC_BUILTIN(const char *chain, struct xtc_handle *const handle)
{
	struct chain_head *c;

	iptc_fn = TC_BUILTIN;

	c = iptcc_find_label(chain, handle);
	if (!c) {
		errno = ENOENT;
		return 0;
	}

	return iptcc_is_builtin(c);
}

const char *
TC_GET_POLICY(const char *chain,
	      STRUCT_COUNTERS *counters,
	      struct xtc_handle *handle)
{
	struct chain_head *c;

	iptc_fn = TC_GET_POLICY;

	DEBUGP("called for chain %s\n", chain);

	c = iptcc_find_label(chain, handle);
	if (!c) {
		errno = ENOENT;
		return NULL;
	}

	if (!iptcc_is_builtin(c))
		return NULL;

	*counters = c->counters;

	return standard_target_map(c->verdict);
}

static int
iptcc_standard_map(struct rule_head *r, int verdict)
{
	STRUCT_ENTRY *e = r->entry;
	STRUCT_STANDARD_TARGET *t;

	t = (STRUCT_STANDARD_TARGET *)GET_TARGET(e);

	if (t->target.u.target_size
	    != ALIGN(sizeof(STRUCT_STANDARD_TARGET))) {
		errno = EINVAL;
		return 0;
	}
	
	memset(t->target.u.user.name, 0, FUNCTION_MAXNAMELEN);
	strcpy(t->target.u.user.name, STANDARD_TARGET);
	t->verdict = verdict;

	r->type = IPTCC_R_STANDARD;

	return 1;
}

static int
iptcc_map_target(struct xtc_handle *const handle,
	   struct rule_head *r)
{
	STRUCT_ENTRY *e = r->entry;
	STRUCT_ENTRY_TARGET *t = GET_TARGET(e);

	
	if (strcmp(t->u.user.name, "") == 0) {
		r->type = IPTCC_R_FALLTHROUGH;
		return 1;
	}
	
	else if (strcmp(t->u.user.name, LABEL_ACCEPT) == 0)
		return iptcc_standard_map(r, -NF_ACCEPT - 1);
	else if (strcmp(t->u.user.name, LABEL_DROP) == 0)
		return iptcc_standard_map(r, -NF_DROP - 1);
	else if (strcmp(t->u.user.name, LABEL_QUEUE) == 0)
		return iptcc_standard_map(r, -NF_QUEUE - 1);
	else if (strcmp(t->u.user.name, LABEL_RETURN) == 0)
		return iptcc_standard_map(r, RETURN);
	else if (TC_BUILTIN(t->u.user.name, handle)) {
		
		errno = EINVAL;
		return 0;
	} else {
		
		struct chain_head *c;
		DEBUGP("trying to find chain `%s': ", t->u.user.name);

		c = iptcc_find_label(t->u.user.name, handle);
		if (c) {
			DEBUGP_C("found!\n");
			r->type = IPTCC_R_JUMP;
			r->jump = c;
			c->references++;
			return 1;
		}
		DEBUGP_C("not found :(\n");
	}

	
	
	memset(t->u.user.name + strlen(t->u.user.name),
	       0,
	       FUNCTION_MAXNAMELEN - 1 - strlen(t->u.user.name));
	r->type = IPTCC_R_MODULE;
	set_changed(handle);
	return 1;
}

int
TC_INSERT_ENTRY(const IPT_CHAINLABEL chain,
		const STRUCT_ENTRY *e,
		unsigned int rulenum,
		struct xtc_handle *handle)
{
	struct chain_head *c;
	struct rule_head *r;
	struct list_head *prev;

	iptc_fn = TC_INSERT_ENTRY;

	if (!(c = iptcc_find_label(chain, handle))) {
		errno = ENOENT;
		return 0;
	}

	if (rulenum > c->num_rules) {
		errno = E2BIG;
		return 0;
	}

	if (rulenum == c->num_rules) {
		prev = &c->rules;
	} else if (rulenum + 1 <= c->num_rules/2) {
		r = iptcc_get_rule_num(c, rulenum + 1);
		prev = &r->list;
	} else {
		r = iptcc_get_rule_num_reverse(c, c->num_rules - rulenum);
		prev = &r->list;
	}

	if (!(r = iptcc_alloc_rule(c, e->next_offset))) {
		errno = ENOMEM;
		return 0;
	}

	memcpy(r->entry, e, e->next_offset);
	r->counter_map.maptype = COUNTER_MAP_SET;

	if (!iptcc_map_target(handle, r)) {
		free(r);
		return 0;
	}

	list_add_tail(&r->list, prev);
	c->num_rules++;

	set_changed(handle);

	return 1;
}

int
TC_REPLACE_ENTRY(const IPT_CHAINLABEL chain,
		 const STRUCT_ENTRY *e,
		 unsigned int rulenum,
		 struct xtc_handle *handle)
{
	struct chain_head *c;
	struct rule_head *r, *old;

	iptc_fn = TC_REPLACE_ENTRY;

	if (!(c = iptcc_find_label(chain, handle))) {
		errno = ENOENT;
		return 0;
	}

	if (rulenum >= c->num_rules) {
		errno = E2BIG;
		return 0;
	}

	
	if (rulenum + 1 <= c->num_rules/2) {
		old = iptcc_get_rule_num(c, rulenum + 1);
	} else {
		old = iptcc_get_rule_num_reverse(c, c->num_rules - rulenum);
	}

	if (!(r = iptcc_alloc_rule(c, e->next_offset))) {
		errno = ENOMEM;
		return 0;
	}

	memcpy(r->entry, e, e->next_offset);
	r->counter_map.maptype = COUNTER_MAP_SET;

	if (!iptcc_map_target(handle, r)) {
		free(r);
		return 0;
	}

	list_add(&r->list, &old->list);
	iptcc_delete_rule(old);

	set_changed(handle);

	return 1;
}

int
TC_APPEND_ENTRY(const IPT_CHAINLABEL chain,
		const STRUCT_ENTRY *e,
		struct xtc_handle *handle)
{
	struct chain_head *c;
	struct rule_head *r;

	iptc_fn = TC_APPEND_ENTRY;
	if (!(c = iptcc_find_label(chain, handle))) {
		DEBUGP("unable to find chain `%s'\n", chain);
		errno = ENOENT;
		return 0;
	}

	if (!(r = iptcc_alloc_rule(c, e->next_offset))) {
		DEBUGP("unable to allocate rule for chain `%s'\n", chain);
		errno = ENOMEM;
		return 0;
	}

	memcpy(r->entry, e, e->next_offset);
	r->counter_map.maptype = COUNTER_MAP_SET;

	if (!iptcc_map_target(handle, r)) {
		DEBUGP("unable to map target of rule for chain `%s'\n", chain);
		free(r);
		return 0;
	}

	list_add_tail(&r->list, &c->rules);
	c->num_rules++;

	set_changed(handle);

	return 1;
}

static inline int
match_different(const STRUCT_ENTRY_MATCH *a,
		const unsigned char *a_elems,
		const unsigned char *b_elems,
		unsigned char **maskptr)
{
	const STRUCT_ENTRY_MATCH *b;
	unsigned int i;

	
	b = (void *)b_elems + ((unsigned char *)a - a_elems);

	if (a->u.match_size != b->u.match_size)
		return 1;

	if (strcmp(a->u.user.name, b->u.user.name) != 0)
		return 1;

	*maskptr += ALIGN(sizeof(*a));

	for (i = 0; i < a->u.match_size - ALIGN(sizeof(*a)); i++)
		if (((a->data[i] ^ b->data[i]) & (*maskptr)[i]) != 0)
			return 1;
	*maskptr += i;
	return 0;
}

static inline int
target_same(struct rule_head *a, struct rule_head *b,const unsigned char *mask)
{
	unsigned int i;
	STRUCT_ENTRY_TARGET *ta, *tb;

	if (a->type != b->type)
		return 0;

	ta = GET_TARGET(a->entry);
	tb = GET_TARGET(b->entry);

	switch (a->type) {
	case IPTCC_R_FALLTHROUGH:
		return 1;
	case IPTCC_R_JUMP:
		return a->jump == b->jump;
	case IPTCC_R_STANDARD:
		return ((STRUCT_STANDARD_TARGET *)ta)->verdict
			== ((STRUCT_STANDARD_TARGET *)tb)->verdict;
	case IPTCC_R_MODULE:
		if (ta->u.target_size != tb->u.target_size)
			return 0;
		if (strcmp(ta->u.user.name, tb->u.user.name) != 0)
			return 0;

		for (i = 0; i < ta->u.target_size - sizeof(*ta); i++)
			if (((ta->data[i] ^ tb->data[i]) & mask[i]) != 0)
				return 0;
		return 1;
	default:
		fprintf(stderr, "ERROR: bad type %i\n", a->type);
		abort();
	}
}

static unsigned char *
is_same(const STRUCT_ENTRY *a,
	const STRUCT_ENTRY *b,
	unsigned char *matchmask);


static int delete_entry(const IPT_CHAINLABEL chain, const STRUCT_ENTRY *origfw,
			unsigned char *matchmask, struct xtc_handle *handle,
			bool dry_run)
{
	struct chain_head *c;
	struct rule_head *r, *i;

	iptc_fn = TC_DELETE_ENTRY;
	if (!(c = iptcc_find_label(chain, handle))) {
		errno = ENOENT;
		return 0;
	}

	
	r = iptcc_alloc_rule(c, origfw->next_offset);
	if (!r) {
		errno = ENOMEM;
		return 0;
	}

	memcpy(r->entry, origfw, origfw->next_offset);
	r->counter_map.maptype = COUNTER_MAP_NOMAP;
	if (!iptcc_map_target(handle, r)) {
		DEBUGP("unable to map target of rule for chain `%s'\n", chain);
		free(r);
		return 0;
	} else {
		if (r->type == IPTCC_R_JUMP
		    && r->jump)
			r->jump->references--;
	}

	list_for_each_entry(i, &c->rules, list) {
		unsigned char *mask;

		mask = is_same(r->entry, i->entry, matchmask);
		if (!mask)
			continue;

		if (!target_same(r, i, mask))
			continue;

		
		if (dry_run){
			free(r);
			return 1;
		}

		if (i == handle->rule_iterator_cur) {
			handle->rule_iterator_cur =
				list_entry(handle->rule_iterator_cur->list.prev,
					   struct rule_head, list);
		}

		c->num_rules--;
		iptcc_delete_rule(i);

		set_changed(handle);
		free(r);
		return 1;
	}

	free(r);
	errno = ENOENT;
	return 0;
}

int TC_CHECK_ENTRY(const IPT_CHAINLABEL chain, const STRUCT_ENTRY *origfw,
		   unsigned char *matchmask, struct xtc_handle *handle)
{
	
	return delete_entry(chain, origfw, matchmask, handle, true);
}

int TC_DELETE_ENTRY(const IPT_CHAINLABEL chain,	const STRUCT_ENTRY *origfw,
		    unsigned char *matchmask, struct xtc_handle *handle)
{
	return delete_entry(chain, origfw, matchmask, handle, false);
}

int
TC_DELETE_NUM_ENTRY(const IPT_CHAINLABEL chain,
		    unsigned int rulenum,
		    struct xtc_handle *handle)
{
	struct chain_head *c;
	struct rule_head *r;

	iptc_fn = TC_DELETE_NUM_ENTRY;

	if (!(c = iptcc_find_label(chain, handle))) {
		errno = ENOENT;
		return 0;
	}

	if (rulenum >= c->num_rules) {
		errno = E2BIG;
		return 0;
	}

	
	if (rulenum + 1 <= c->num_rules/2) {
		r = iptcc_get_rule_num(c, rulenum + 1);
	} else {
		r = iptcc_get_rule_num_reverse(c, c->num_rules - rulenum);
	}

	if (r == handle->rule_iterator_cur) {
		handle->rule_iterator_cur =
			list_entry(handle->rule_iterator_cur->list.prev,
				   struct rule_head, list);
	}

	c->num_rules--;
	iptcc_delete_rule(r);

	set_changed(handle);

	return 1;
}

int
TC_FLUSH_ENTRIES(const IPT_CHAINLABEL chain, struct xtc_handle *handle)
{
	struct chain_head *c;
	struct rule_head *r, *tmp;

	iptc_fn = TC_FLUSH_ENTRIES;
	if (!(c = iptcc_find_label(chain, handle))) {
		errno = ENOENT;
		return 0;
	}

	list_for_each_entry_safe(r, tmp, &c->rules, list) {
		iptcc_delete_rule(r);
	}

	c->num_rules = 0;

	set_changed(handle);

	return 1;
}

int
TC_ZERO_ENTRIES(const IPT_CHAINLABEL chain, struct xtc_handle *handle)
{
	struct chain_head *c;
	struct rule_head *r;

	iptc_fn = TC_ZERO_ENTRIES;
	if (!(c = iptcc_find_label(chain, handle))) {
		errno = ENOENT;
		return 0;
	}

	if (c->counter_map.maptype == COUNTER_MAP_NORMAL_MAP)
		c->counter_map.maptype = COUNTER_MAP_ZEROED;

	list_for_each_entry(r, &c->rules, list) {
		if (r->counter_map.maptype == COUNTER_MAP_NORMAL_MAP)
			r->counter_map.maptype = COUNTER_MAP_ZEROED;
	}

	set_changed(handle);

	return 1;
}

STRUCT_COUNTERS *
TC_READ_COUNTER(const IPT_CHAINLABEL chain,
		unsigned int rulenum,
		struct xtc_handle *handle)
{
	struct chain_head *c;
	struct rule_head *r;

	iptc_fn = TC_READ_COUNTER;
	CHECK(*handle);

	if (!(c = iptcc_find_label(chain, handle))) {
		errno = ENOENT;
		return NULL;
	}

	if (!(r = iptcc_get_rule_num(c, rulenum))) {
		errno = E2BIG;
		return NULL;
	}

	return &r->entry[0].counters;
}

int
TC_ZERO_COUNTER(const IPT_CHAINLABEL chain,
		unsigned int rulenum,
		struct xtc_handle *handle)
{
	struct chain_head *c;
	struct rule_head *r;

	iptc_fn = TC_ZERO_COUNTER;
	CHECK(handle);

	if (!(c = iptcc_find_label(chain, handle))) {
		errno = ENOENT;
		return 0;
	}

	if (!(r = iptcc_get_rule_num(c, rulenum))) {
		errno = E2BIG;
		return 0;
	}

	if (r->counter_map.maptype == COUNTER_MAP_NORMAL_MAP)
		r->counter_map.maptype = COUNTER_MAP_ZEROED;

	set_changed(handle);

	return 1;
}

int
TC_SET_COUNTER(const IPT_CHAINLABEL chain,
	       unsigned int rulenum,
	       STRUCT_COUNTERS *counters,
	       struct xtc_handle *handle)
{
	struct chain_head *c;
	struct rule_head *r;
	STRUCT_ENTRY *e;

	iptc_fn = TC_SET_COUNTER;
	CHECK(handle);

	if (!(c = iptcc_find_label(chain, handle))) {
		errno = ENOENT;
		return 0;
	}

	if (!(r = iptcc_get_rule_num(c, rulenum))) {
		errno = E2BIG;
		return 0;
	}

	e = r->entry;
	r->counter_map.maptype = COUNTER_MAP_SET;

	memcpy(&e->counters, counters, sizeof(STRUCT_COUNTERS));

	set_changed(handle);

	return 1;
}

int
TC_CREATE_CHAIN(const IPT_CHAINLABEL chain, struct xtc_handle *handle)
{
	static struct chain_head *c;
	int capacity;
	int exceeded;

	iptc_fn = TC_CREATE_CHAIN;

	if (iptcc_find_label(chain, handle)
	    || strcmp(chain, LABEL_DROP) == 0
	    || strcmp(chain, LABEL_ACCEPT) == 0
	    || strcmp(chain, LABEL_QUEUE) == 0
	    || strcmp(chain, LABEL_RETURN) == 0) {
		DEBUGP("Chain `%s' already exists\n", chain);
		errno = EEXIST;
		return 0;
	}

	if (strlen(chain)+1 > sizeof(IPT_CHAINLABEL)) {
		DEBUGP("Chain name `%s' too long\n", chain);
		errno = EINVAL;
		return 0;
	}

	c = iptcc_alloc_chain_head(chain, 0);
	if (!c) {
		DEBUGP("Cannot allocate memory for chain `%s'\n", chain);
		errno = ENOMEM;
		return 0;

	}
	handle->num_chains++; 

	DEBUGP("Creating chain `%s'\n", chain);
	iptc_insert_chain(handle, c); 

	capacity = handle->chain_index_sz * CHAIN_INDEX_BUCKET_LEN;
	exceeded = handle->num_chains - capacity;
	if (exceeded > CHAIN_INDEX_INSERT_MAX) {
		debug("Capacity(%d) exceeded(%d) rebuild (chains:%d)\n",
		      capacity, exceeded, handle->num_chains);
		iptcc_chain_index_rebuild(handle);
	}

	set_changed(handle);

	return 1;
}

int
TC_GET_REFERENCES(unsigned int *ref, const IPT_CHAINLABEL chain,
		  struct xtc_handle *handle)
{
	struct chain_head *c;

	iptc_fn = TC_GET_REFERENCES;
	if (!(c = iptcc_find_label(chain, handle))) {
		errno = ENOENT;
		return 0;
	}

	*ref = c->references;

	return 1;
}

int
TC_DELETE_CHAIN(const IPT_CHAINLABEL chain, struct xtc_handle *handle)
{
	unsigned int references;
	struct chain_head *c;

	iptc_fn = TC_DELETE_CHAIN;

	if (!(c = iptcc_find_label(chain, handle))) {
		DEBUGP("cannot find chain `%s'\n", chain);
		errno = ENOENT;
		return 0;
	}

	if (TC_BUILTIN(chain, handle)) {
		DEBUGP("cannot remove builtin chain `%s'\n", chain);
		errno = EINVAL;
		return 0;
	}

	if (!TC_GET_REFERENCES(&references, chain, handle)) {
		DEBUGP("cannot get references on chain `%s'\n", chain);
		return 0;
	}

	if (references > 0) {
		DEBUGP("chain `%s' still has references\n", chain);
		errno = EMLINK;
		return 0;
	}

	if (c->num_rules) {
		DEBUGP("chain `%s' is not empty\n", chain);
		errno = ENOTEMPTY;
		return 0;
	}

	if (c == handle->chain_iterator_cur)
		iptcc_chain_iterator_advance(handle);

	handle->num_chains--; 

	
	iptcc_chain_index_delete_chain(c, handle);
	free(c);

	DEBUGP("chain `%s' deleted\n", chain);

	set_changed(handle);

	return 1;
}

int TC_RENAME_CHAIN(const IPT_CHAINLABEL oldname,
		    const IPT_CHAINLABEL newname,
		    struct xtc_handle *handle)
{
	struct chain_head *c;
	iptc_fn = TC_RENAME_CHAIN;

	if (iptcc_find_label(newname, handle)
	    || strcmp(newname, LABEL_DROP) == 0
	    || strcmp(newname, LABEL_ACCEPT) == 0
	    || strcmp(newname, LABEL_QUEUE) == 0
	    || strcmp(newname, LABEL_RETURN) == 0) {
		errno = EEXIST;
		return 0;
	}

	if (!(c = iptcc_find_label(oldname, handle))
	    || TC_BUILTIN(oldname, handle)) {
		errno = ENOENT;
		return 0;
	}

	if (strlen(newname)+1 > sizeof(IPT_CHAINLABEL)) {
		errno = EINVAL;
		return 0;
	}

	
	iptcc_chain_index_delete_chain(c, handle);

	
	strncpy(c->name, newname, sizeof(IPT_CHAINLABEL));

	
	iptc_insert_chain(handle, c);

	set_changed(handle);

	return 1;
}

int
TC_SET_POLICY(const IPT_CHAINLABEL chain,
	      const IPT_CHAINLABEL policy,
	      STRUCT_COUNTERS *counters,
	      struct xtc_handle *handle)
{
	struct chain_head *c;

	iptc_fn = TC_SET_POLICY;

	if (!(c = iptcc_find_label(chain, handle))) {
		DEBUGP("cannot find chain `%s'\n", chain);
		errno = ENOENT;
		return 0;
	}

	if (!iptcc_is_builtin(c)) {
		DEBUGP("cannot set policy of userdefinedchain `%s'\n", chain);
		errno = ENOENT;
		return 0;
	}

	if (strcmp(policy, LABEL_ACCEPT) == 0)
		c->verdict = -NF_ACCEPT - 1;
	else if (strcmp(policy, LABEL_DROP) == 0)
		c->verdict = -NF_DROP - 1;
	else {
		errno = EINVAL;
		return 0;
	}

	if (counters) {
		
		memcpy(&c->counters, counters, sizeof(STRUCT_COUNTERS));
		c->counter_map.maptype = COUNTER_MAP_SET;
	} else {
		c->counter_map.maptype = COUNTER_MAP_NOMAP;
	}

	set_changed(handle);

	return 1;
}

static void
subtract_counters(STRUCT_COUNTERS *answer,
		  const STRUCT_COUNTERS *a,
		  const STRUCT_COUNTERS *b)
{
	answer->pcnt = a->pcnt - b->pcnt;
	answer->bcnt = a->bcnt - b->bcnt;
}


static void counters_nomap(STRUCT_COUNTERS_INFO *newcounters, unsigned int idx)
{
	newcounters->counters[idx] = ((STRUCT_COUNTERS) { 0, 0});
	DEBUGP_C("NOMAP => zero\n");
}

static void counters_normal_map(STRUCT_COUNTERS_INFO *newcounters,
				STRUCT_REPLACE *repl, unsigned int idx,
				unsigned int mappos)
{
	newcounters->counters[idx] = repl->counters[mappos];
	DEBUGP_C("NORMAL_MAP => mappos %u \n", mappos);
}

static void counters_map_zeroed(STRUCT_COUNTERS_INFO *newcounters,
				STRUCT_REPLACE *repl, unsigned int idx,
				unsigned int mappos, STRUCT_COUNTERS *counters)
{
	subtract_counters(&newcounters->counters[idx],
			  &repl->counters[mappos],
			  counters);
	DEBUGP_C("ZEROED => mappos %u\n", mappos);
}

static void counters_map_set(STRUCT_COUNTERS_INFO *newcounters,
                             unsigned int idx, STRUCT_COUNTERS *counters)
{
	

	memcpy(&newcounters->counters[idx], counters,
		sizeof(STRUCT_COUNTERS));

	DEBUGP_C("SET\n");
}


int
TC_COMMIT(struct xtc_handle *handle)
{
	
	STRUCT_REPLACE *repl;
	STRUCT_COUNTERS_INFO *newcounters;
	struct chain_head *c;
	int ret;
	size_t counterlen;
	int new_number;
	unsigned int new_size;

	iptc_fn = TC_COMMIT;
	CHECK(*handle);

	
	if (!handle->changed)
		goto finished;

	new_number = iptcc_compile_table_prep(handle, &new_size);
	if (new_number < 0) {
		errno = ENOMEM;
		goto out_zero;
	}

	repl = malloc(sizeof(*repl) + new_size);
	if (!repl) {
		errno = ENOMEM;
		goto out_zero;
	}
	memset(repl, 0, sizeof(*repl) + new_size);

#if 0
	TC_DUMP_ENTRIES(*handle);
#endif

	counterlen = sizeof(STRUCT_COUNTERS_INFO)
			+ sizeof(STRUCT_COUNTERS) * new_number;

	
	repl->counters = malloc(sizeof(STRUCT_COUNTERS)
				* handle->info.num_entries);
	if (!repl->counters) {
		errno = ENOMEM;
		goto out_free_repl;
	}
	
	newcounters = malloc(counterlen);
	if (!newcounters) {
		errno = ENOMEM;
		goto out_free_repl_counters;
	}
	memset(newcounters, 0, counterlen);

	strcpy(repl->name, handle->info.name);
	repl->num_entries = new_number;
	repl->size = new_size;

	repl->num_counters = handle->info.num_entries;
	repl->valid_hooks  = handle->info.valid_hooks;

	DEBUGP("num_entries=%u, size=%u, num_counters=%u\n",
		repl->num_entries, repl->size, repl->num_counters);

	ret = iptcc_compile_table(handle, repl);
	if (ret < 0) {
		errno = ret;
		goto out_free_newcounters;
	}


#ifdef IPTC_DEBUG2
	{
		int fd = open("/tmp/libiptc-so_set_replace.blob",
				O_CREAT|O_WRONLY);
		if (fd >= 0) {
			write(fd, repl, sizeof(*repl) + repl->size);
			close(fd);
		}
	}
#endif

	ret = setsockopt(handle->sockfd, TC_IPPROTO, SO_SET_REPLACE, repl,
			 sizeof(*repl) + repl->size);
	if (ret < 0)
		goto out_free_newcounters;

	
	strcpy(newcounters->name, handle->info.name);
	newcounters->num_counters = new_number;

	list_for_each_entry(c, &handle->chains, list) {
		struct rule_head *r;

		
		if (iptcc_is_builtin(c)) {
			DEBUGP("counter for chain-index %u: ", c->foot_index);
			switch(c->counter_map.maptype) {
			case COUNTER_MAP_NOMAP:
				counters_nomap(newcounters, c->foot_index);
				break;
			case COUNTER_MAP_NORMAL_MAP:
				counters_normal_map(newcounters, repl,
						    c->foot_index,
						    c->counter_map.mappos);
				break;
			case COUNTER_MAP_ZEROED:
				counters_map_zeroed(newcounters, repl,
						    c->foot_index,
						    c->counter_map.mappos,
						    &c->counters);
				break;
			case COUNTER_MAP_SET:
				counters_map_set(newcounters, c->foot_index,
						 &c->counters);
				break;
			}
		}

		list_for_each_entry(r, &c->rules, list) {
			DEBUGP("counter for index %u: ", r->index);
			switch (r->counter_map.maptype) {
			case COUNTER_MAP_NOMAP:
				counters_nomap(newcounters, r->index);
				break;

			case COUNTER_MAP_NORMAL_MAP:
				counters_normal_map(newcounters, repl,
						    r->index,
						    r->counter_map.mappos);
				break;

			case COUNTER_MAP_ZEROED:
				counters_map_zeroed(newcounters, repl,
						    r->index,
						    r->counter_map.mappos,
						    &r->entry->counters);
				break;

			case COUNTER_MAP_SET:
				counters_map_set(newcounters, r->index,
						 &r->entry->counters);
				break;
			}
		}
	}

#ifdef IPTC_DEBUG2
	{
		int fd = open("/tmp/libiptc-so_set_add_counters.blob",
				O_CREAT|O_WRONLY);
		if (fd >= 0) {
			write(fd, newcounters, counterlen);
			close(fd);
		}
	}
#endif

	ret = setsockopt(handle->sockfd, TC_IPPROTO, SO_SET_ADD_COUNTERS,
			 newcounters, counterlen);
	if (ret < 0)
		goto out_free_newcounters;

	free(repl->counters);
	free(repl);
	free(newcounters);

finished:
	return 1;

out_free_newcounters:
	free(newcounters);
out_free_repl_counters:
	free(repl->counters);
out_free_repl:
	free(repl);
out_zero:
	return 0;
}

const char *
TC_STRERROR(int err)
{
	unsigned int i;
	struct table_struct {
		void *fn;
		int err;
		const char *message;
	} table [] =
	  { { TC_INIT, EPERM, "Permission denied (you must be root)" },
	    { TC_INIT, EINVAL, "Module is wrong version" },
	    { TC_INIT, ENOENT,
		    "Table does not exist (do you need to insmod?)" },
	    { TC_DELETE_CHAIN, ENOTEMPTY, "Chain is not empty" },
	    { TC_DELETE_CHAIN, EINVAL, "Can't delete built-in chain" },
	    { TC_DELETE_CHAIN, EMLINK,
	      "Can't delete chain with references left" },
	    { TC_CREATE_CHAIN, EEXIST, "Chain already exists" },
	    { TC_INSERT_ENTRY, E2BIG, "Index of insertion too big" },
	    { TC_REPLACE_ENTRY, E2BIG, "Index of replacement too big" },
	    { TC_DELETE_NUM_ENTRY, E2BIG, "Index of deletion too big" },
	    { TC_READ_COUNTER, E2BIG, "Index of counter too big" },
	    { TC_ZERO_COUNTER, E2BIG, "Index of counter too big" },
	    { TC_INSERT_ENTRY, ELOOP, "Loop found in table" },
	    { TC_INSERT_ENTRY, EINVAL, "Target problem" },
	    
	    { TC_DELETE_ENTRY, ENOENT,
	      "Bad rule (does a matching rule exist in that chain?)" },
	    { TC_SET_POLICY, ENOENT,
	      "Bad built-in chain name" },
	    { TC_SET_POLICY, EINVAL,
	      "Bad policy name" },

	    { NULL, 0, "Incompatible with this kernel" },
	    { NULL, ENOPROTOOPT, "iptables who? (do you need to insmod?)" },
	    { NULL, ENOSYS, "Will be implemented real soon.  I promise ;)" },
	    { NULL, ENOMEM, "Memory allocation problem" },
	    { NULL, ENOENT, "No chain/target/match by that name" },
	  };

	for (i = 0; i < sizeof(table)/sizeof(struct table_struct); i++) {
		if ((!table[i].fn || table[i].fn == iptc_fn)
		    && table[i].err == err)
			return table[i].message;
	}

	return strerror(err);
}

const struct xtc_ops TC_OPS = {
	.commit        = TC_COMMIT,
	.free          = TC_FREE,
	.builtin       = TC_BUILTIN,
	.is_chain      = TC_IS_CHAIN,
	.flush_entries = TC_FLUSH_ENTRIES,
	.create_chain  = TC_CREATE_CHAIN,
	.set_policy    = TC_SET_POLICY,
	.strerror      = TC_STRERROR,
};
