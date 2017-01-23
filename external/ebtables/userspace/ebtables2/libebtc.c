/*
 * libebtc.c, January 2004
 *
 * Contains the functions with which to make a table in userspace.
 *
 * Author: Bart De Schuymer
 *
 *  This code is stongly inspired on the iptables code which is
 *  Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "include/ebtables_u.h"
#include "include/ethernetdb.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

static void decrease_chain_jumps(struct ebt_u_replace *replace);
static int iterate_entries(struct ebt_u_replace *replace, int type);

const char *ebt_hooknames[NF_BR_NUMHOOKS] =
{
	[NF_BR_PRE_ROUTING]"PREROUTING",
	[NF_BR_LOCAL_IN]"INPUT",
	[NF_BR_FORWARD]"FORWARD",
	[NF_BR_LOCAL_OUT]"OUTPUT",
	[NF_BR_POST_ROUTING]"POSTROUTING",
	[NF_BR_BROUTING]"BROUTING"
};

const char* ebt_standard_targets[NUM_STANDARD_TARGETS] =
{
	"ACCEPT",
	"DROP",
	"CONTINUE",
	"RETURN",
};

struct ebt_u_table *ebt_tables;
struct ebt_u_match *ebt_matches;
struct ebt_u_watcher *ebt_watchers;
struct ebt_u_target *ebt_targets;

struct ebt_u_target *ebt_find_target(const char *name)
{
	struct ebt_u_target *t = ebt_targets;

	while (t && strcmp(t->name, name))
		t = t->next;
	return t;
}

struct ebt_u_match *ebt_find_match(const char *name)
{
	struct ebt_u_match *m = ebt_matches;

	while (m && strcmp(m->name, name))
		m = m->next;
	return m;
}

struct ebt_u_watcher *ebt_find_watcher(const char *name)
{
	struct ebt_u_watcher *w = ebt_watchers;

	while (w && strcmp(w->name, name))
		w = w->next;
	return w;
}

struct ebt_u_table *ebt_find_table(const char *name)
{
	struct ebt_u_table *t = ebt_tables;

	while (t && strcmp(t->name, name))
		t = t->next;
	return t;
}

void ebt_list_extensions()
{
	struct ebt_u_table *tbl = ebt_tables;
        struct ebt_u_target *t = ebt_targets;
        struct ebt_u_match *m = ebt_matches;
        struct ebt_u_watcher *w = ebt_watchers;

	PRINT_VERSION;
	printf("Loaded userspace extensions:\n\nLoaded tables:\n");
        while (tbl) {
		printf("%s\n", tbl->name);
                tbl = tbl->next;
	}
	printf("\nLoaded targets:\n");
        while (t) {
		printf("%s\n", t->name);
                t = t->next;
	}
	printf("\nLoaded matches:\n");
        while (m) {
		printf("%s\n", m->name);
                m = m->next;
	}
	printf("\nLoaded watchers:\n");
        while (w) {
		printf("%s\n", w->name);
                w = w->next;
	}
}

#ifndef LOCKFILE
#define LOCKDIR "/var/lib/ebtables"
#define LOCKFILE LOCKDIR"/lock"
#endif
static int lockfd = -1, locked;
int use_lockfd;
static int lock_file()
{
	int try = 0;
	int ret = 0;
	sigset_t sigset;

tryagain:
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigprocmask(SIG_BLOCK, &sigset, NULL);
	lockfd = open(LOCKFILE, O_CREAT | O_EXCL | O_WRONLY, 00600);
	if (lockfd < 0) {
		if (errno == EEXIST)
			ret = -1;
		else if (try == 1)
			ret = -2;
		else {
			if (mkdir(LOCKDIR, 00700))
				ret = -2;
			else {
				try = 1;
				goto tryagain;
			}
		}
	} else {
		close(lockfd);
		locked = 1;
	}
	sigprocmask(SIG_UNBLOCK, &sigset, NULL);
	return ret;
}

void unlock_file()
{
	if (locked) {
		remove(LOCKFILE);
		locked = 0;
	}
}

void __attribute__ ((destructor)) onexit()
{
	if (use_lockfd)
		unlock_file();
}
int ebt_get_kernel_table(struct ebt_u_replace *replace, int init)
{
	int ret;

	if (!ebt_find_table(replace->name)) {
		ebt_print_error("Bad table name '%s'", replace->name);
		return -1;
	}
	while (use_lockfd && (ret = lock_file())) {
		if (ret == -2) {
			ebt_print_error2("Unable to create lock file "LOCKFILE);
		}
		fprintf(stderr, "Trying to obtain lock %s\n", LOCKFILE);
		sleep(1);
	}
	
	if (ebt_get_table(replace, init)) {
		if (ebt_errormsg[0] != '\0')
			return -1;
		ebtables_insmod("ebtables");
		if (ebt_get_table(replace, init)) {
			ebt_print_error("The kernel doesn't support the ebtables '%s' table", replace->name);
			return -1;
		}
	}
	return 0;
}

void ebt_initialize_entry(struct ebt_u_entry *e)
{
	e->bitmask = EBT_NOPROTO;
	e->invflags = 0;
	e->ethproto = 0;
	strcpy(e->in, "");
	strcpy(e->out, "");
	strcpy(e->logical_in, "");
	strcpy(e->logical_out, "");
	e->m_list = NULL;
	e->w_list = NULL;
	e->t = (struct ebt_entry_target *)ebt_find_target(EBT_STANDARD_TARGET);
	ebt_find_target(EBT_STANDARD_TARGET)->used = 1;
	e->cnt.pcnt = e->cnt.bcnt = e->cnt_surplus.pcnt = e->cnt_surplus.bcnt = 0;

	if (!e->t)
		ebt_print_bug("Couldn't load standard target");
	((struct ebt_standard_target *)((struct ebt_u_target *)e->t)->t)->verdict = EBT_CONTINUE;
}

void ebt_cleanup_replace(struct ebt_u_replace *replace)
{
	int i;
	struct ebt_u_entries *entries;
	struct ebt_cntchanges *cc1, *cc2;
	struct ebt_u_entry *u_e1, *u_e2;

	replace->name[0] = '\0';
	replace->valid_hooks = 0;
	replace->nentries = 0;
	replace->num_counters = 0;
	replace->flags = 0;
	replace->command = 0;
	replace->selected_chain = -1;
	free(replace->filename);
	replace->filename = NULL;
	free(replace->counters);
	replace->counters = NULL;

	for (i = 0; i < replace->num_chains; i++) {
		if (!(entries = replace->chains[i]))
			continue;
		u_e1 = entries->entries->next;
		while (u_e1 != entries->entries) {
			ebt_free_u_entry(u_e1);
			u_e2 = u_e1->next;
			free(u_e1);
			u_e1 = u_e2;
		}
		free(entries->entries);
		free(entries);
		replace->chains[i] = NULL;
	}
	cc1 = replace->cc->next;
	while (cc1 != replace->cc) {
		cc2 = cc1->next;
		free(cc1);
		cc1 = cc2;
	}
	replace->cc->next = replace->cc->prev = replace->cc;
}

void ebt_reinit_extensions()
{
	struct ebt_u_match *m;
	struct ebt_u_watcher *w;
	struct ebt_u_target *t;
	int size;

	for (m = ebt_matches; m; m = m->next) {
		if (m->used) {
			size = EBT_ALIGN(m->size) + sizeof(struct ebt_entry_match);
			m->m = (struct ebt_entry_match *)malloc(size);
			if (!m->m)
				ebt_print_memory();
			strcpy(m->m->u.name, m->name);
			m->m->match_size = EBT_ALIGN(m->size);
			m->used = 0;
		}
		m->flags = 0; 
		m->init(m->m);
	}
	for (w = ebt_watchers; w; w = w->next) {
		if (w->used) {
			size = EBT_ALIGN(w->size) + sizeof(struct ebt_entry_watcher);
			w->w = (struct ebt_entry_watcher *)malloc(size);
			if (!w->w)
				ebt_print_memory();
			strcpy(w->w->u.name, w->name);
			w->w->watcher_size = EBT_ALIGN(w->size);
			w->used = 0;
		}
		w->flags = 0;
		w->init(w->w);
	}
	for (t = ebt_targets; t; t = t->next) {
		if (t->used) {
			size = EBT_ALIGN(t->size) + sizeof(struct ebt_entry_target);
			t->t = (struct ebt_entry_target *)malloc(size);
			if (!t->t)
				ebt_print_memory();
			strcpy(t->t->u.name, t->name);
			t->t->target_size = EBT_ALIGN(t->size);
			t->used = 0;
		}
		t->flags = 0;
		t->init(t->t);
	}
}

void ebt_free_u_entry(struct ebt_u_entry *e)
{
	struct ebt_u_match_list *m_l, *m_l2;
	struct ebt_u_watcher_list *w_l, *w_l2;

	m_l = e->m_list;
	while (m_l) {
		m_l2 = m_l->next;
		free(m_l->m);
		free(m_l);
		m_l = m_l2;
	}
	w_l = e->w_list;
	while (w_l) {
		w_l2 = w_l->next;
		free(w_l->w);
		free(w_l);
		w_l = w_l2;
	}
	free(e->t);
}

static char *get_modprobe(void)
{
	int procfile;
	char *ret;

	procfile = open(PROC_SYS_MODPROBE, O_RDONLY);
	if (procfile < 0)
		return NULL;

	ret = malloc(1024);
	if (ret) {
		if (read(procfile, ret, 1024) == -1)
			goto fail;
		
		ret[1023] = '\n';
		*strchr(ret, '\n') = '\0';
		close(procfile);
		return ret;
	}
 fail:
	free(ret);
	close(procfile);
	return NULL;
}

char *ebt_modprobe;
int ebtables_insmod(const char *modname)
{
	char *buf = NULL;
	char *argv[3];

	
	if (!ebt_modprobe) {
		buf = get_modprobe();
		if (!buf)
			return -1;
		ebt_modprobe = buf; 
	}

	switch (fork()) {
	case 0:
		argv[0] = (char *)ebt_modprobe;
		argv[1] = (char *)modname;
		argv[2] = NULL;
		execv(argv[0], argv);

		
		exit(0);
	case -1:
		return -1;

	default: 
		wait(NULL);
	}

	return 0;
}

struct ebt_u_entries *ebt_name_to_chain(const struct ebt_u_replace *replace, const char* arg)
{
	int i;
	struct ebt_u_entries *chain;

	for (i = 0; i < replace->num_chains; i++) {
		if (!(chain = replace->chains[i]))
			continue;
		if (!strcmp(arg, chain->name))
			return chain;
	}
	return NULL;
}

int ebt_get_chainnr(const struct ebt_u_replace *replace, const char* arg)
{
	int i;

	for (i = 0; i < replace->num_chains; i++) {
		if (!replace->chains[i])
			continue;
		if (!strcmp(arg, replace->chains[i]->name))
			return i;
	}
	return -1;
}


void ebt_change_policy(struct ebt_u_replace *replace, int policy)
{
	struct ebt_u_entries *entries = ebt_to_chain(replace);

	if (policy < -NUM_STANDARD_TARGETS || policy == EBT_CONTINUE)
		ebt_print_bug("Wrong policy: %d", policy);
	entries->policy = policy;
}

void ebt_delete_cc(struct ebt_cntchanges *cc)
{
	if (cc->type == CNT_ADD) {
		cc->prev->next = cc->next;
		cc->next->prev = cc->prev;
		free(cc);
	} else
		cc->type = CNT_DEL;
}

void ebt_empty_chain(struct ebt_u_entries *entries)
{
	struct ebt_u_entry *u_e = entries->entries->next, *tmp;
	while (u_e != entries->entries) {
		ebt_delete_cc(u_e->cc);
		ebt_free_u_entry(u_e);
		tmp = u_e->next;
		free(u_e);
		u_e = tmp;
	}
	entries->entries->next = entries->entries->prev = entries->entries;
	entries->nentries = 0;
}

void ebt_flush_chains(struct ebt_u_replace *replace)
{
	int i, numdel;
	struct ebt_u_entries *entries = ebt_to_chain(replace);

	
	if (!entries) {
		if (replace->nentries == 0)
			return;
		replace->nentries = 0;

		
		for (i = 0; i < replace->num_chains; i++) {
			if (!(entries = replace->chains[i]))
				continue;
			entries->counter_offset = 0;
			ebt_empty_chain(entries);
		}
		return;
	}

	if (entries->nentries == 0)
		return;
	replace->nentries -= entries->nentries;
	numdel = entries->nentries;

	
	for (i = replace->selected_chain+1; i < replace->num_chains; i++) {
		if (!(entries = replace->chains[i]))
			continue;
		entries->counter_offset -= numdel;
	}

	entries = ebt_to_chain(replace);
	ebt_empty_chain(entries);
}

#define OPT_COUNT	0x1000 
int ebt_check_rule_exists(struct ebt_u_replace *replace,
			  struct ebt_u_entry *new_entry)
{
	struct ebt_u_entry *u_e;
	struct ebt_u_match_list *m_l, *m_l2;
	struct ebt_u_match *m;
	struct ebt_u_watcher_list *w_l, *w_l2;
	struct ebt_u_watcher *w;
	struct ebt_u_target *t = (struct ebt_u_target *)new_entry->t;
	struct ebt_u_entries *entries = ebt_to_chain(replace);
	int i, j, k;

	u_e = entries->entries->next;
	for (i = 0; i < entries->nentries; i++, u_e = u_e->next) {
		if (u_e->ethproto != new_entry->ethproto)
			continue;
		if (strcmp(u_e->in, new_entry->in))
			continue;
		if (strcmp(u_e->out, new_entry->out))
			continue;
		if (strcmp(u_e->logical_in, new_entry->logical_in))
			continue;
		if (strcmp(u_e->logical_out, new_entry->logical_out))
			continue;
		if (new_entry->bitmask & EBT_SOURCEMAC &&
		    memcmp(u_e->sourcemac, new_entry->sourcemac, ETH_ALEN))
			continue;
		if (new_entry->bitmask & EBT_DESTMAC &&
		    memcmp(u_e->destmac, new_entry->destmac, ETH_ALEN))
			continue;
		if (new_entry->bitmask != u_e->bitmask ||
		    new_entry->invflags != u_e->invflags)
			continue;
		if (replace->flags & OPT_COUNT && (new_entry->cnt.pcnt !=
		    u_e->cnt.pcnt || new_entry->cnt.bcnt != u_e->cnt.bcnt))
			continue;
		
		m_l = new_entry->m_list;
		j = 0;
		while (m_l) {
			m = (struct ebt_u_match *)(m_l->m);
			m_l2 = u_e->m_list;
			while (m_l2 && strcmp(m_l2->m->u.name, m->m->u.name))
				m_l2 = m_l2->next;
			if (!m_l2 || !m->compare(m->m, m_l2->m))
				goto letscontinue;
			j++;
			m_l = m_l->next;
		}
		
		k = 0;
		m_l = u_e->m_list;
		while (m_l) {
			k++;
			m_l = m_l->next;
		}
		if (j != k)
			continue;

		
		w_l = new_entry->w_list;
		j = 0;
		while (w_l) {
			w = (struct ebt_u_watcher *)(w_l->w);
			w_l2 = u_e->w_list;
			while (w_l2 && strcmp(w_l2->w->u.name, w->w->u.name))
				w_l2 = w_l2->next;
			if (!w_l2 || !w->compare(w->w, w_l2->w))
				goto letscontinue;
			j++;
			w_l = w_l->next;
		}
		k = 0;
		w_l = u_e->w_list;
		while (w_l) {
			k++;
			w_l = w_l->next;
		}
		if (j != k)
			continue;
		if (strcmp(t->t->u.name, u_e->t->u.name))
			continue;
		if (!t->compare(t->t, u_e->t))
			continue;
		return i;
letscontinue:;
	}
	return -1;
}

void ebt_add_rule(struct ebt_u_replace *replace, struct ebt_u_entry *new_entry, int rule_nr)
{
	int i;
	struct ebt_u_entry *u_e;
	struct ebt_u_match_list *m_l;
	struct ebt_u_watcher_list *w_l;
	struct ebt_u_entries *entries = ebt_to_chain(replace);
	struct ebt_cntchanges *cc, *new_cc;

	if (rule_nr <= 0)
		rule_nr += entries->nentries;
	else
		rule_nr--;
	if (rule_nr > entries->nentries || rule_nr < 0) {
		ebt_print_error("The specified rule number is incorrect");
		return;
	}
	
	if (rule_nr == entries->nentries)
		u_e = entries->entries;
	else {
		u_e = entries->entries->next;
		for (i = 0; i < rule_nr; i++)
			u_e = u_e->next;
	}
	
	replace->nentries++;
	entries->nentries++;
	
	new_entry->next = u_e;
	new_entry->prev = u_e->prev;
	u_e->prev->next = new_entry;
	u_e->prev = new_entry;
	new_cc = (struct ebt_cntchanges *)malloc(sizeof(struct ebt_cntchanges));
	if (!new_cc)
		ebt_print_memory();
	new_cc->type = CNT_ADD;
	new_cc->change = 0;
	if (new_entry->next == entries->entries) {
		for (i = replace->selected_chain+1; i < replace->num_chains; i++)
			if (!replace->chains[i] || replace->chains[i]->nentries == 0)
				continue;
			else
				break;
		if (i == replace->num_chains)
			cc = replace->cc;
		else
			cc = replace->chains[i]->entries->next->cc;
	} else
		cc = new_entry->next->cc;
	new_cc->next = cc;
	new_cc->prev = cc->prev;
	cc->prev->next = new_cc;
	cc->prev = new_cc;
	new_entry->cc = new_cc;

	
	m_l = new_entry->m_list;
	while (m_l) {
		m_l->m = ((struct ebt_u_match *)m_l->m)->m;
		m_l = m_l->next;
	}
	w_l = new_entry->w_list;
	while (w_l) {
		w_l->w = ((struct ebt_u_watcher *)w_l->w)->w;
		w_l = w_l->next;
	}
	new_entry->t = ((struct ebt_u_target *)new_entry->t)->t;
	
	for (i = replace->selected_chain+1; i < replace->num_chains; i++) {
		entries = replace->chains[i];
		if (!(entries = replace->chains[i]))
			continue;
		entries->counter_offset++;
	}
}

static int check_and_change_rule_number(struct ebt_u_replace *replace,
   struct ebt_u_entry *new_entry, int *begin, int *end)
{
	struct ebt_u_entries *entries = ebt_to_chain(replace);

	if (*begin < 0)
		*begin += entries->nentries + 1;
	if (*end < 0)
		*end += entries->nentries + 1;

	if (*begin < 0 || *begin > *end || *end > entries->nentries) {
		ebt_print_error("Sorry, wrong rule numbers");
		return -1;
	}

	if ((*begin * *end == 0) && (*begin + *end != 0))
		ebt_print_bug("begin and end should be either both zero, "
			      "either both non-zero");
	if (*begin != 0) {
		(*begin)--;
		(*end)--;
	} else {
		*begin = ebt_check_rule_exists(replace, new_entry);
		*end = *begin;
		if (*begin == -1) {
			ebt_print_error("Sorry, rule does not exist");
			return -1;
		}
	}
	return 0;
}

void ebt_delete_rule(struct ebt_u_replace *replace,
		     struct ebt_u_entry *new_entry, int begin, int end)
{
	int i,  nr_deletes;
	struct ebt_u_entry *u_e, *u_e2, *u_e3;
	struct ebt_u_entries *entries = ebt_to_chain(replace);

	if (check_and_change_rule_number(replace, new_entry, &begin, &end))
		return;
	
	nr_deletes = end - begin + 1;
	replace->nentries -= nr_deletes;
	entries->nentries -= nr_deletes;
	
	u_e = entries->entries->next;
	for (i = 0; i < begin; i++)
		u_e = u_e->next;
	u_e3 = u_e->prev;
	
	for (i = 0; i < nr_deletes; i++) {
		u_e2 = u_e;
		ebt_delete_cc(u_e2->cc);
		u_e = u_e->next;
		
		ebt_free_u_entry(u_e2);
		free(u_e2);
	}
	u_e3->next = u_e;
	u_e->prev = u_e3;
	
	for (i = replace->selected_chain+1; i < replace->num_chains; i++) {
		if (!(entries = replace->chains[i]))
			continue;
		entries->counter_offset -= nr_deletes;
	}
}

void ebt_change_counters(struct ebt_u_replace *replace,
		     struct ebt_u_entry *new_entry, int begin, int end,
		     struct ebt_counter *cnt, int mask)
{
	int i;
	struct ebt_u_entry *u_e;
	struct ebt_u_entries *entries = ebt_to_chain(replace);

	if (check_and_change_rule_number(replace, new_entry, &begin, &end))
		return;
	u_e = entries->entries->next;
	for (i = 0; i < begin; i++)
		u_e = u_e->next;
	for (i = end-begin+1; i > 0; i--) {
		if (mask % 3 == 0) {
			u_e->cnt.pcnt = (*cnt).pcnt;
			u_e->cnt_surplus.pcnt = 0;
		} else {
#ifdef EBT_DEBUG
			if (u_e->cc->type != CNT_NORM)
				ebt_print_bug("cc->type != CNT_NORM");
#endif
			u_e->cnt_surplus.pcnt = (*cnt).pcnt;
		}

		if (mask / 3 == 0) {
			u_e->cnt.bcnt = (*cnt).bcnt;
			u_e->cnt_surplus.bcnt = 0;
		} else {
#ifdef EBT_DEBUG
			if (u_e->cc->type != CNT_NORM)
				ebt_print_bug("cc->type != CNT_NORM");
#endif
			u_e->cnt_surplus.bcnt = (*cnt).bcnt;
		}
		if (u_e->cc->type != CNT_ADD)
			u_e->cc->type = CNT_CHANGE;
		u_e->cc->change = mask;
		u_e = u_e->next;
	}
}

void ebt_zero_counters(struct ebt_u_replace *replace)
{
	struct ebt_u_entries *entries = ebt_to_chain(replace);
	struct ebt_u_entry *next;
	int i;

	if (!entries) {
		for (i = 0; i < replace->num_chains; i++) {
			if (!(entries = replace->chains[i]))
				continue;
			next = entries->entries->next;
			while (next != entries->entries) {
				if (next->cc->type == CNT_NORM)
					next->cc->type = CNT_CHANGE;
				next->cnt.bcnt = next->cnt.pcnt = 0;
				next->cc->change = 0;
				next = next->next;
			}
		}
	} else {
		if (entries->nentries == 0)
			return;

		next = entries->entries->next;
		while (next != entries->entries) {
			if (next->cc->type == CNT_NORM)
				next->cc->type = CNT_CHANGE;
			next->cnt.bcnt = next->cnt.pcnt = 0;
			next = next->next;
		}
	}
}

void ebt_new_chain(struct ebt_u_replace *replace, const char *name, int policy)
{
	struct ebt_u_entries *new;

	if (replace->num_chains == replace->max_chains)
		ebt_double_chains(replace);
	new = (struct ebt_u_entries *)malloc(sizeof(struct ebt_u_entries));
	if (!new)
		ebt_print_memory();
	replace->chains[replace->num_chains++] = new;
	new->nentries = 0;
	new->policy = policy;
	new->counter_offset = replace->nentries;
	new->hook_mask = 0;
	strcpy(new->name, name);
	new->entries = (struct ebt_u_entry *)malloc(sizeof(struct ebt_u_entry));
	if (!new->entries)
		ebt_print_memory();
	new->entries->next = new->entries->prev = new->entries;
	new->kernel_start = NULL;
}

static int ebt_delete_a_chain(struct ebt_u_replace *replace, int chain, int print_err)
{
	int tmp = replace->selected_chain;
	replace->selected_chain = chain;
	if (ebt_check_for_references(replace, print_err))
		return -1;
	decrease_chain_jumps(replace);
	ebt_flush_chains(replace);
	replace->selected_chain = tmp;
	free(replace->chains[chain]->entries);
	free(replace->chains[chain]);
	memmove(replace->chains+chain, replace->chains+chain+1, (replace->num_chains-chain-1)*sizeof(void *));
	replace->num_chains--;
	return 0;
}

void ebt_delete_chain(struct ebt_u_replace *replace)
{
	if (replace->selected_chain != -1 && replace->selected_chain < NF_BR_NUMHOOKS)
		ebt_print_bug("You can't remove a standard chain");
	if (replace->selected_chain == -1) {
		int i = NF_BR_NUMHOOKS;

		while (i < replace->num_chains)
			if (ebt_delete_a_chain(replace, i, 0))
				i++;
	} else
		ebt_delete_a_chain(replace, replace->selected_chain, 1);
}

void ebt_rename_chain(struct ebt_u_replace *replace, const char *name)
{
	struct ebt_u_entries *entries = ebt_to_chain(replace);

	if (!entries)
		ebt_print_bug("ebt_rename_chain: entries == NULL");
	strcpy(entries->name, name);
}




void ebt_double_chains(struct ebt_u_replace *replace)
{
	struct ebt_u_entries **new;

	replace->max_chains *= 2;
	new = (struct ebt_u_entries **)malloc(replace->max_chains*sizeof(void *));
	if (!new)
		ebt_print_memory();
	memcpy(new, replace->chains, replace->max_chains/2*sizeof(void *));
	free(replace->chains);
	replace->chains = new;
}

void ebt_do_final_checks(struct ebt_u_replace *replace, struct ebt_u_entry *e,
			 struct ebt_u_entries *entries)
{
	struct ebt_u_match_list *m_l;
	struct ebt_u_watcher_list *w_l;
	struct ebt_u_target *t;
	struct ebt_u_match *m;
	struct ebt_u_watcher *w;

	m_l = e->m_list;
	w_l = e->w_list;
	while (m_l) {
		m = ebt_find_match(m_l->m->u.name);
		m->final_check(e, m_l->m, replace->name,
		   entries->hook_mask, 1);
		if (ebt_errormsg[0] != '\0')
			return;
		m_l = m_l->next;
	}
	while (w_l) {
		w = ebt_find_watcher(w_l->w->u.name);
		w->final_check(e, w_l->w, replace->name,
		   entries->hook_mask, 1);
		if (ebt_errormsg[0] != '\0')
			return;
		w_l = w_l->next;
	}
	t = ebt_find_target(e->t->u.name);
	t->final_check(e, e->t, replace->name,
	   entries->hook_mask, 1);
}

int ebt_check_for_references(struct ebt_u_replace *replace, int print_err)
{
	if (print_err)
		return iterate_entries(replace, 1);
	else
		return iterate_entries(replace, 2);
}

int ebt_check_for_references2(struct ebt_u_replace *replace, int chain_nr,
                              int print_err)
{
	int tmp = replace->selected_chain, ret;

	replace->selected_chain = chain_nr;
	if (print_err)
		ret = iterate_entries(replace, 1);
	else
		ret = iterate_entries(replace, 2);
	replace->selected_chain = tmp;
	return ret;
}

struct ebt_u_stack
{
	int chain_nr;
	int n;
	struct ebt_u_entry *e;
	struct ebt_u_entries *entries;
};

void ebt_check_for_loops(struct ebt_u_replace *replace)
{
	int chain_nr , i, j , k, sp = 0, verdict;
	struct ebt_u_entries *entries, *entries2;
	struct ebt_u_stack *stack = NULL;
	struct ebt_u_entry *e;

	
	for (i = 0; i < replace->num_chains; i++) {
		if (!(entries = replace->chains[i]))
			continue;
		if (i < NF_BR_NUMHOOKS)
			entries->hook_mask = (1 << i) | (1 << NF_BR_NUMHOOKS);
		else
			entries->hook_mask = 0;
	}
	if (replace->num_chains == NF_BR_NUMHOOKS)
		return;
	stack = (struct ebt_u_stack *)malloc((replace->num_chains - NF_BR_NUMHOOKS) * sizeof(struct ebt_u_stack));
	if (!stack)
		ebt_print_memory();

	
	for (i = 0; i < NF_BR_NUMHOOKS; i++) {
		if (!(entries = replace->chains[i]))
			continue;
		chain_nr = i;

		e = entries->entries->next;
		for (j = 0; j < entries->nentries; j++) {
			if (strcmp(e->t->u.name, EBT_STANDARD_TARGET))
				goto letscontinue;
			verdict = ((struct ebt_standard_target *)(e->t))->verdict;
			if (verdict < 0)
				goto letscontinue;
			
			for (k = 0; k < sp; k++)
				if (stack[k].chain_nr == verdict + NF_BR_NUMHOOKS) {
					ebt_print_error("Loop from chain '%s' to chain '%s'",
					   replace->chains[chain_nr]->name,
					   replace->chains[stack[k].chain_nr]->name);
					goto free_stack;
				}
			entries2 = replace->chains[verdict + NF_BR_NUMHOOKS];
			
			if (entries2->hook_mask & (1<<i))
				goto letscontinue;
			entries2->hook_mask |= entries->hook_mask;
			
			stack[sp].chain_nr = chain_nr;
			stack[sp].n = j;
			stack[sp].entries = entries;
			stack[sp].e = e;
			sp++;
			j = -1;
			e = entries2->entries->next;
			chain_nr = verdict + NF_BR_NUMHOOKS;
			entries = entries2;
			continue;
letscontinue:
			e = e->next;
		}
		
		if (sp == 0)
			continue;
		
		sp--;
		j = stack[sp].n;
		chain_nr = stack[sp].chain_nr;
		e = stack[sp].e;
		entries = stack[sp].entries;
		goto letscontinue;
	}
free_stack:
	free(stack);
	return;
}

void ebt_add_match(struct ebt_u_entry *new_entry, struct ebt_u_match *m)
{
	struct ebt_u_match_list **m_list, *new;

	for (m_list = &new_entry->m_list; *m_list; m_list = &(*m_list)->next);
	new = (struct ebt_u_match_list *)
	   malloc(sizeof(struct ebt_u_match_list));
	if (!new)
		ebt_print_memory();
	*m_list = new;
	new->next = NULL;
	new->m = (struct ebt_entry_match *)m;
}

void ebt_add_watcher(struct ebt_u_entry *new_entry, struct ebt_u_watcher *w)
{
	struct ebt_u_watcher_list **w_list;
	struct ebt_u_watcher_list *new;

	for (w_list = &new_entry->w_list; *w_list; w_list = &(*w_list)->next);
	new = (struct ebt_u_watcher_list *)
	   malloc(sizeof(struct ebt_u_watcher_list));
	if (!new)
		ebt_print_memory();
	*w_list = new;
	new->next = NULL;
	new->w = (struct ebt_entry_watcher *)w;
}




static int iterate_entries(struct ebt_u_replace *replace, int type)
{
	int i, j, chain_nr = replace->selected_chain - NF_BR_NUMHOOKS;
	struct ebt_u_entries *entries;
	struct ebt_u_entry *e;

	if (chain_nr < 0)
		ebt_print_bug("iterate_entries: udc = %d < 0", chain_nr);
	for (i = 0; i < replace->num_chains; i++) {
		if (!(entries = replace->chains[i]))
			continue;
		e = entries->entries->next;
		for (j = 0; j < entries->nentries; j++) {
			int chain_jmp;

			if (strcmp(e->t->u.name, EBT_STANDARD_TARGET)) {
				e = e->next;
				continue;
			}
			chain_jmp = ((struct ebt_standard_target *)e->t)->
				    verdict;
			switch (type) {
			case 1:
			case 2:
			if (chain_jmp == chain_nr) {
				if (type == 2)
					return 1;
				ebt_print_error("Can't delete the chain '%s', it's referenced in chain '%s', rule %d",
				                replace->chains[chain_nr + NF_BR_NUMHOOKS]->name, entries->name, j);
				return 1;
			}
			break;
			case 0:
			
			if (chain_jmp > chain_nr)
				((struct ebt_standard_target *)e->t)->verdict--;
			break;
			} 
			e = e->next;
		}
	}
	return 0;
}

static void decrease_chain_jumps(struct ebt_u_replace *replace)
{
	iterate_entries(replace, 0);
}

void ebt_register_match(struct ebt_u_match *m)
{
	int size = EBT_ALIGN(m->size) + sizeof(struct ebt_entry_match);
	struct ebt_u_match **i;

	m->m = (struct ebt_entry_match *)malloc(size);
	if (!m->m)
		ebt_print_memory();
	strcpy(m->m->u.name, m->name);
	m->m->match_size = EBT_ALIGN(m->size);
	m->init(m->m);

	for (i = &ebt_matches; *i; i = &((*i)->next));
	m->next = NULL;
	*i = m;
}

void ebt_register_watcher(struct ebt_u_watcher *w)
{
	int size = EBT_ALIGN(w->size) + sizeof(struct ebt_entry_watcher);
	struct ebt_u_watcher **i;

	w->w = (struct ebt_entry_watcher *)malloc(size);
	if (!w->w)
		ebt_print_memory();
	strcpy(w->w->u.name, w->name);
	w->w->watcher_size = EBT_ALIGN(w->size);
	w->init(w->w);

	for (i = &ebt_watchers; *i; i = &((*i)->next));
	w->next = NULL;
	*i = w;
}

void ebt_register_target(struct ebt_u_target *t)
{
	int size = EBT_ALIGN(t->size) + sizeof(struct ebt_entry_target);
	struct ebt_u_target **i;

	t->t = (struct ebt_entry_target *)malloc(size);
	if (!t->t)
		ebt_print_memory();
	strcpy(t->t->u.name, t->name);
	t->t->target_size = EBT_ALIGN(t->size);
	t->init(t->t);

	for (i = &ebt_targets; *i; i = &((*i)->next));
	t->next = NULL;
	*i = t;
}

void ebt_register_table(struct ebt_u_table *t)
{
	t->next = ebt_tables;
	ebt_tables = t;
}

void ebt_iterate_matches(void (*f)(struct ebt_u_match *))
{
	struct ebt_u_match *i;

	for (i = ebt_matches; i; i = i->next)
		f(i);
}

void ebt_iterate_watchers(void (*f)(struct ebt_u_watcher *))
{
	struct ebt_u_watcher *i;

	for (i = ebt_watchers; i; i = i->next)
		f(i);
}

void ebt_iterate_targets(void (*f)(struct ebt_u_target *))
{
	struct ebt_u_target *i;

	for (i = ebt_targets; i; i = i->next)
		f(i);
}

void __ebt_print_bug(char *file, int line, char *format, ...)
{
	va_list l;

	va_start(l, format);
	fprintf(stderr, PROGNAME" v"PROGVERSION":%s:%d:--BUG--: \n", file, line);
	vfprintf(stderr, format, l);
	fprintf(stderr, "\n");
	va_end(l);
	exit (-1);
}

char ebt_errormsg[ERRORMSG_MAXLEN];
int ebt_silent;
void __ebt_print_error(char *format, ...)
{
	va_list l;

	va_start(l, format);
	if (ebt_silent && ebt_errormsg[0] == '\0') {
		vsnprintf(ebt_errormsg, ERRORMSG_MAXLEN, format, l);
		va_end(l);
	} else {
		vfprintf(stderr, format, l);
		fprintf(stderr, ".\n");
		va_end(l);
		exit (-1);
	}
}
