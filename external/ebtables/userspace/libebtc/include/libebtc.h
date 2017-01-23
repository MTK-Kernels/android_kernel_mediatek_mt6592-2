/*
 * ==[ FILENAME: libebtc.h ]====================================================
 *
 *  Project
 *
 *      Library for ethernet bridge tables.
 *
 *
 *  Description
 *
 *      See project.
 *
 *
 *  Copyright
 *
 *      Copyright 2005 by Jens Götze
 *      All rights reserved.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307,
 *      USA.
 *
 *
 * =============================================================================
 */


#ifndef __LIB_EBTC_H__
#define __LIB_EBTC_H__ 1


#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/netfilter_bridge/ebtables.h>



#ifndef EBTC_MIN_ALIGN
#  define EBTC_MIN_ALIGN                (__alignof__(struct ebt_entry_target))
#endif
#define EBTC_ALIGN(s)                   (((s) + (EBTC_MIN_ALIGN - 1)) & \
                                         ~(EBTC_MIN_ALIGN - 1))

#define EBTC_SIZEOF(a)                  EBTC_ALIGN(sizeof(a))
#define EBTC_NEXT(a)                    ((char *)(a) + EBTC_ALIGN(sizeof(*(a))))
#define EBTC_ADDOFFSET(a, b)            ((char *)(a) + (b))

#define EBTC_INIT                       0x0000
#define EBTC_INIT_WITHFLUSH             0x0001

#define EBTC_FALSE                      -1
#define EBTC_TRUE                       0



typedef struct ebtc_handle_st *ebtc_handle_t;

typedef struct ebt_replace ebt_replace_t;

typedef struct ebt_entries ebt_entries_t;

typedef struct ebt_entry ebt_entry_t;

typedef struct ebt_counter ebt_counter_t;

typedef struct ebt_entry_target ebt_entry_target_t;

typedef struct ebt_standard_target ebt_standard_target_t;



extern int ebtc_is_chain (const char *chainname, const ebtc_handle_t handle);


extern const char *ebtc_first_chain (ebtc_handle_t *handle);


extern const char *ebtc_next_chain (ebtc_handle_t *handle);


extern const struct ebt_entry *ebtc_first_rule (const char *chainname,
                                                ebtc_handle_t *handle);


extern const struct ebt_entry *ebtc_next_rule (const char *chainname,
                                               ebtc_handle_t *handle);


extern const char *ebtc_get_target (const struct ebt_entry *entry,
                                    const ebtc_handle_t *handle);


extern int ebtc_is_builtin (const char *chainname, const ebtc_handle_t *handle);


extern int ebtc_set_policy (const char *chainname, const char *policy,
                            ebtc_handle_t *handle);


extern const char *ebtc_get_policy (const char *chainname,
                                    const ebtc_handle_t *handle);


extern int ebtc_insert_entry (const char *chainname,
                              const struct ebt_entry *entry,
                              unsigned int rulenum, ebtc_handle_t *handle);


extern int ebtc_replace_entry (const char *chainname,
                               const struct ebt_entry *entry,
                               unsigned int rulenum, ebtc_handle_t *handle);


extern int ebtc_append_entry (const char *chainname,
                              const struct ebt_entry *entry,
                              ebtc_handle_t *handle);


extern int ebtc_delete_entry (const char *chainname, unsigned int rulenum,
                              ebtc_handle_t *handle);


extern int ebtc_target_jumptochain (ebt_standard_target_t *target,
                                    char *chainname, ebtc_handle_t *handle);


extern int ebtc_flush_entries (const char *chainname, ebtc_handle_t *handle);


extern int ebtc_zero_entries (const char *chainname, ebtc_handle_t *handle);


extern int ebtc_rename_chain (const char *chainname_old,
                              const char *chainname_new, ebtc_handle_t *handle);


extern int ebtc_create_chain (const char *chainname, ebtc_handle_t *handle);


extern int ebtc_delete_chain (const char *chainname, ebtc_handle_t *handle);


extern const struct ebt_counter *ebtc_read_counter (const char *chainname, 
                                                    unsigned int rulenum,
                                                    ebtc_handle_t *handle);


extern int ebtc_zero_counter (const char *chainname, unsigned int rulenum,
                              ebtc_handle_t *handle);


extern int ebtc_set_counter (const char *chainname, unsigned int rulenum,
                             const struct ebt_counter *counters,
                             ebtc_handle_t *handle);


extern ebtc_handle_t ebtc_init (const char *tablename, int options);


extern int ebtc_commit (ebtc_handle_t *handle);


extern void ebtc_free (ebtc_handle_t *handle);


extern const char *ebtc_strerror (const ebtc_handle_t *handle);


#endif


