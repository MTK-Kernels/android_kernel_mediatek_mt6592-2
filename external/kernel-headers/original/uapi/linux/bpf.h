/* Copyright (c) 2011-2014 PLUMgrid, http://plumgrid.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 */
#ifndef _UAPI__LINUX_BPF_H__
#define _UAPI__LINUX_BPF_H__

#include <linux/types.h>
#include <linux/bpf_common.h>


#define BPF_ALU64	0x07	

#define BPF_DW		0x18	
#define BPF_XADD	0xc0	

#define BPF_MOV		0xb0	
#define BPF_ARSH	0xc0	

#define BPF_END		0xd0	
#define BPF_TO_LE	0x00	
#define BPF_TO_BE	0x08	
#define BPF_FROM_LE	BPF_TO_LE
#define BPF_FROM_BE	BPF_TO_BE

#define BPF_JNE		0x50	
#define BPF_JSGT	0x60	
#define BPF_JSGE	0x70	
#define BPF_CALL	0x80	
#define BPF_EXIT	0x90	

enum {
	BPF_REG_0 = 0,
	BPF_REG_1,
	BPF_REG_2,
	BPF_REG_3,
	BPF_REG_4,
	BPF_REG_5,
	BPF_REG_6,
	BPF_REG_7,
	BPF_REG_8,
	BPF_REG_9,
	BPF_REG_10,
	__MAX_BPF_REG,
};

#define MAX_BPF_REG	__MAX_BPF_REG

struct bpf_insn {
	__u8	code;		
	__u8	dst_reg:4;	
	__u8	src_reg:4;	
	__s16	off;		
	__s32	imm;		
};

enum bpf_cmd {
	BPF_MAP_CREATE,

	BPF_MAP_LOOKUP_ELEM,

	BPF_MAP_UPDATE_ELEM,

	BPF_MAP_DELETE_ELEM,

	BPF_MAP_GET_NEXT_KEY,

	/* verify and load eBPF program
	 * prog_fd = bpf(BPF_PROG_LOAD, union bpf_attr *attr, u32 size)
	 * Using attr->prog_type, attr->insns, attr->license
	 * returns fd or negative error
	 */
	BPF_PROG_LOAD,
};

enum bpf_map_type {
	BPF_MAP_TYPE_UNSPEC,
};

enum bpf_prog_type {
	BPF_PROG_TYPE_UNSPEC,
};

union bpf_attr {
	struct { 
		__u32	map_type;	
		__u32	key_size;	
		__u32	value_size;	
		__u32	max_entries;	
	};

	struct { 
		__u32		map_fd;
		__aligned_u64	key;
		union {
			__aligned_u64 value;
			__aligned_u64 next_key;
		};
	};

	struct { 
		__u32		prog_type;	
		__u32		insn_cnt;
		__aligned_u64	insns;
		__aligned_u64	license;
		__u32		log_level;	
		__u32		log_size;	
		__aligned_u64	log_buf;	
	};
} __attribute__((aligned(8)));

enum bpf_func_id {
	BPF_FUNC_unspec,
	__BPF_FUNC_MAX_ID,
};

#endif 
