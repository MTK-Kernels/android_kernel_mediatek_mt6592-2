/*
 *  linux/include/linux/ext2_fs.h
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/include/linux/minix_fs.h
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#ifndef _LINUX_EXT2_FS_H
#define _LINUX_EXT2_FS_H

#include <ext2fs/ext2_types.h>		


#undef EXT2FS_DEBUG

#define EXT2_PREALLOCATE
#define EXT2_DEFAULT_PREALLOC_BLOCKS	8

#define EXT2FS_DATE		"95/08/09"
#define EXT2FS_VERSION		"0.5b"

#define EXT2_BAD_INO		 1	
#define EXT2_ROOT_INO		 2	
#define EXT4_USR_QUOTA_INO	 3	
#define EXT4_GRP_QUOTA_INO	 4	
#define EXT2_BOOT_LOADER_INO	 5	
#define EXT2_UNDEL_DIR_INO	 6	
#define EXT2_RESIZE_INO		 7	
#define EXT2_JOURNAL_INO	 8	
#define EXT2_EXCLUDE_INO	 9	
#define EXT4_REPLICA_INO	10	

#define EXT2_GOOD_OLD_FIRST_INO	11

#define EXT2_SUPER_MAGIC	0xEF53

#ifdef __KERNEL__
#define EXT2_SB(sb)	(&((sb)->u.ext2_sb))
#else
#define EXT2_SB(sb)	(sb)
#endif

#define EXT2_LINK_MAX		65000

#define EXT2_MIN_BLOCK_LOG_SIZE		10	
#define EXT2_MAX_BLOCK_LOG_SIZE		16	
#define EXT2_MIN_BLOCK_SIZE	(1 << EXT2_MIN_BLOCK_LOG_SIZE)
#define EXT2_MAX_BLOCK_SIZE	(1 << EXT2_MAX_BLOCK_LOG_SIZE)
#ifdef __KERNEL__
#define EXT2_BLOCK_SIZE(s)	((s)->s_blocksize)
#define EXT2_BLOCK_SIZE_BITS(s)	((s)->s_blocksize_bits)
#define EXT2_ADDR_PER_BLOCK_BITS(s)	(EXT2_SB(s)->addr_per_block_bits)
#define EXT2_INODE_SIZE(s)	(EXT2_SB(s)->s_inode_size)
#define EXT2_FIRST_INO(s)	(EXT2_SB(s)->s_first_ino)
#else
#define EXT2_BLOCK_SIZE(s)	(EXT2_MIN_BLOCK_SIZE << (s)->s_log_block_size)
#define EXT2_BLOCK_SIZE_BITS(s)	((s)->s_log_block_size + 10)
#define EXT2_INODE_SIZE(s)	(((s)->s_rev_level == EXT2_GOOD_OLD_REV) ? \
				 EXT2_GOOD_OLD_INODE_SIZE : (s)->s_inode_size)
#define EXT2_FIRST_INO(s)	(((s)->s_rev_level == EXT2_GOOD_OLD_REV) ? \
				 EXT2_GOOD_OLD_FIRST_INO : (s)->s_first_ino)
#endif
#define EXT2_ADDR_PER_BLOCK(s)	(EXT2_BLOCK_SIZE(s) / sizeof(__u32))

#define EXT2_MIN_CLUSTER_LOG_SIZE	EXT2_MIN_BLOCK_LOG_SIZE
#define EXT2_MAX_CLUSTER_LOG_SIZE	29	
#define EXT2_MIN_CLUSTER_SIZE		EXT2_MIN_BLOCK_SIZE
#define EXT2_MAX_CLUSTER_SIZE		(1 << EXT2_MAX_CLUSTER_LOG_SIZE)
#define EXT2_CLUSTER_SIZE(s)		(EXT2_MIN_BLOCK_SIZE << \
						(s)->s_log_cluster_size)
#define EXT2_CLUSTER_SIZE_BITS(s)	((s)->s_log_cluster_size + 10)

#define EXT2_MIN_FRAG_SIZE              EXT2_MIN_BLOCK_SIZE
#define EXT2_MAX_FRAG_SIZE              EXT2_MAX_BLOCK_SIZE
#define EXT2_MIN_FRAG_LOG_SIZE          EXT2_MIN_BLOCK_LOG_SIZE
#define EXT2_FRAG_SIZE(s)		EXT2_BLOCK_SIZE(s)
#define EXT2_FRAGS_PER_BLOCK(s)		1

struct ext2_acl_header	
{
	__u32	aclh_size;
	__u32	aclh_file_count;
	__u32	aclh_acle_count;
	__u32	aclh_first_acle;
};

struct ext2_acl_entry	
{
	__u32	acle_size;
	__u16	acle_perms;	
	__u16	acle_type;	
	__u16	acle_tag;	
	__u16	acle_pad1;
	__u32	acle_next;	
					
};

struct ext2_group_desc
{
	__u32	bg_block_bitmap;	
	__u32	bg_inode_bitmap;	
	__u32	bg_inode_table;		
	__u16	bg_free_blocks_count;	
	__u16	bg_free_inodes_count;	
	__u16	bg_used_dirs_count;	
	__u16	bg_flags;
	__u32	bg_exclude_bitmap_lo;	
	__u16	bg_block_bitmap_csum_lo;
	__u16	bg_inode_bitmap_csum_lo;
	__u16	bg_itable_unused;	
	__u16	bg_checksum;		
};

struct ext4_group_desc
{
	__u32	bg_block_bitmap;	
	__u32	bg_inode_bitmap;	
	__u32	bg_inode_table;		
	__u16	bg_free_blocks_count;	
	__u16	bg_free_inodes_count;	
	__u16	bg_used_dirs_count;	
	__u16	bg_flags;		
	__u32	bg_exclude_bitmap_lo;	
	__u16	bg_block_bitmap_csum_lo;
	__u16	bg_inode_bitmap_csum_lo;
	__u16	bg_itable_unused;	
	__u16	bg_checksum;		
	__u32	bg_block_bitmap_hi;	
	__u32	bg_inode_bitmap_hi;	
	__u32	bg_inode_table_hi;	
	__u16	bg_free_blocks_count_hi;
	__u16	bg_free_inodes_count_hi;
	__u16	bg_used_dirs_count_hi;	
	__u16	bg_itable_unused_hi;	
	__u32	bg_exclude_bitmap_hi;	
	__u16	bg_block_bitmap_csum_hi;
	__u16	bg_inode_bitmap_csum_hi;
	__u32	bg_reserved;
};

#define EXT2_BG_INODE_UNINIT	0x0001 
#define EXT2_BG_BLOCK_UNINIT	0x0002 
#define EXT2_BG_INODE_ZEROED	0x0004 


struct ext2_dx_root_info {
	__u32 reserved_zero;
	__u8 hash_version; 
	__u8 info_length; 
	__u8 indirect_levels;
	__u8 unused_flags;
};

#define EXT2_HASH_LEGACY		0
#define EXT2_HASH_HALF_MD4		1
#define EXT2_HASH_TEA			2
#define EXT2_HASH_LEGACY_UNSIGNED	3 
#define EXT2_HASH_HALF_MD4_UNSIGNED	4 
#define EXT2_HASH_TEA_UNSIGNED		5 

#define EXT2_HASH_FLAG_INCOMPAT	0x1

struct ext2_dx_entry {
	__u32 hash;
	__u32 block;
};

struct ext2_dx_countlimit {
	__u16 limit;
	__u16 count;
};


#define EXT2_MIN_DESC_SIZE             32
#define EXT2_MIN_DESC_SIZE_64BIT       64
#define EXT2_MAX_DESC_SIZE             EXT2_MIN_BLOCK_SIZE
#define EXT2_DESC_SIZE(s)                                                \
       ((EXT2_SB(s)->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) ? \
	(s)->s_desc_size : EXT2_MIN_DESC_SIZE)

#define EXT2_BLOCKS_PER_GROUP(s)	(EXT2_SB(s)->s_blocks_per_group)
#define EXT2_INODES_PER_GROUP(s)	(EXT2_SB(s)->s_inodes_per_group)
#define EXT2_CLUSTERS_PER_GROUP(s)	(EXT2_SB(s)->s_clusters_per_group)
#define EXT2_INODES_PER_BLOCK(s)	(EXT2_BLOCK_SIZE(s)/EXT2_INODE_SIZE(s))
#define EXT2_MAX_BLOCKS_PER_GROUP(s)	((((unsigned) 1 << 16) - 8) *	\
					 (EXT2_CLUSTER_SIZE(s) / \
					  EXT2_BLOCK_SIZE(s)))
#define EXT2_MAX_CLUSTERS_PER_GROUP(s)	(((unsigned) 1 << 16) - 8)
#define EXT2_MAX_INODES_PER_GROUP(s)	(((unsigned) 1 << 16) - \
					 EXT2_INODES_PER_BLOCK(s))
#ifdef __KERNEL__
#define EXT2_DESC_PER_BLOCK(s)		(EXT2_SB(s)->s_desc_per_block)
#define EXT2_DESC_PER_BLOCK_BITS(s)	(EXT2_SB(s)->s_desc_per_block_bits)
#else
#define EXT2_DESC_PER_BLOCK(s)		(EXT2_BLOCK_SIZE(s) / EXT2_DESC_SIZE(s))
#endif

#define EXT2_NDIR_BLOCKS		12
#define EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)

#define EXT2_SECRM_FL			0x00000001 
#define EXT2_UNRM_FL			0x00000002 
#define EXT2_COMPR_FL			0x00000004 
#define EXT2_SYNC_FL			0x00000008 
#define EXT2_IMMUTABLE_FL		0x00000010 
#define EXT2_APPEND_FL			0x00000020 
#define EXT2_NODUMP_FL			0x00000040 
#define EXT2_NOATIME_FL			0x00000080 
#define EXT2_DIRTY_FL			0x00000100
#define EXT2_COMPRBLK_FL		0x00000200 
#define EXT2_NOCOMPR_FL			0x00000400 
#define EXT2_ECOMPR_FL			0x00000800 
#define EXT2_BTREE_FL			0x00001000 
#define EXT2_INDEX_FL			0x00001000 
#define EXT2_IMAGIC_FL			0x00002000
#define EXT3_JOURNAL_DATA_FL		0x00004000 
#define EXT2_NOTAIL_FL			0x00008000 
#define EXT2_DIRSYNC_FL 		0x00010000 
#define EXT2_TOPDIR_FL			0x00020000 
#define EXT4_HUGE_FILE_FL               0x00040000 
#define EXT4_EXTENTS_FL 		0x00080000 
#define EXT4_EA_INODE_FL	        0x00200000 
#define FS_NOCOW_FL			0x00800000 
#define EXT4_SNAPFILE_FL		0x01000000  
#define EXT4_SNAPFILE_DELETED_FL	0x04000000  
#define EXT4_SNAPFILE_SHRUNK_FL		0x08000000  
#define EXT2_RESERVED_FL		0x80000000 

#define EXT2_FL_USER_VISIBLE		0x004BDFFF 
#define EXT2_FL_USER_MODIFIABLE		0x004B80FF 


struct ext2_new_group_input {
	__u32 group;		
	__u32 block_bitmap;	
	__u32 inode_bitmap;	
	__u32 inode_table;	
	__u32 blocks_count;	
	__u16 reserved_blocks;	
	__u16 unused;		
};

struct ext4_new_group_input {
	__u32 group;		
	__u64 block_bitmap;	
	__u64 inode_bitmap;	
	__u64 inode_table;	
	__u32 blocks_count;	
	__u16 reserved_blocks;	
	__u16 unused;
};

#ifdef __GNU__			
#define _IOT_ext2_new_group_input _IOT (_IOTS(__u32), 5, _IOTS(__u16), 2, 0, 0)
#endif

#define EXT2_IOC_GETFLAGS		_IOR('f', 1, long)
#define EXT2_IOC_SETFLAGS		_IOW('f', 2, long)
#define EXT2_IOC_GETVERSION		_IOR('v', 1, long)
#define EXT2_IOC_SETVERSION		_IOW('v', 2, long)
#define EXT2_IOC_GETVERSION_NEW		_IOR('f', 3, long)
#define EXT2_IOC_SETVERSION_NEW		_IOW('f', 4, long)
#define EXT2_IOC_GROUP_EXTEND		_IOW('f', 7, unsigned long)
#define EXT2_IOC_GROUP_ADD		_IOW('f', 8,struct ext2_new_group_input)
#define EXT4_IOC_GROUP_ADD		_IOW('f', 8,struct ext4_new_group_input)
#define EXT4_IOC_RESIZE_FS		_IOW('f', 16, __u64)

struct ext2_inode {
	__u16	i_mode;		
	__u16	i_uid;		
	__u32	i_size;		
	__u32	i_atime;	
	__u32	i_ctime;	
	__u32	i_mtime;	
	__u32	i_dtime;	
	__u16	i_gid;		
	__u16	i_links_count;	
	__u32	i_blocks;	
	__u32	i_flags;	
	union {
		struct {
			__u32	l_i_version; 
		} linux1;
		struct {
			__u32  h_i_translator;
		} hurd1;
	} osd1;				
	__u32	i_block[EXT2_N_BLOCKS];
	__u32	i_generation;	
	__u32	i_file_acl;	
	__u32	i_size_high;	
	__u32	i_faddr;	
	union {
		struct {
			__u16	l_i_blocks_hi;
			__u16	l_i_file_acl_high;
			__u16	l_i_uid_high;	
			__u16	l_i_gid_high;	
			__u16	l_i_checksum_lo; 
			__u16	l_i_reserved;
		} linux2;
		struct {
			__u8	h_i_frag;	
			__u8	h_i_fsize;	
			__u16	h_i_mode_high;
			__u16	h_i_uid_high;
			__u16	h_i_gid_high;
			__u32	h_i_author;
		} hurd2;
	} osd2;				
};

struct ext2_inode_large {
	__u16	i_mode;		
	__u16	i_uid;		
	__u32	i_size;		
	__u32	i_atime;	
	__u32	i_ctime;	
	__u32	i_mtime;	
	__u32	i_dtime;	
	__u16	i_gid;		
	__u16	i_links_count;	
	__u32	i_blocks;	
	__u32	i_flags;	
	union {
		struct {
			__u32	l_i_version; 
		} linux1;
		struct {
			__u32  h_i_translator;
		} hurd1;
	} osd1;				
	__u32	i_block[EXT2_N_BLOCKS];
	__u32	i_generation;	
	__u32	i_file_acl;	
	__u32	i_size_high;	
	__u32	i_faddr;	
	union {
		struct {
			__u16	l_i_blocks_hi;
			__u16	l_i_file_acl_high;
			__u16	l_i_uid_high;	
			__u16	l_i_gid_high;	
			__u16	l_i_checksum_lo; 
			__u16	l_i_reserved;
		} linux2;
		struct {
			__u8	h_i_frag;	
			__u8	h_i_fsize;	
			__u16	h_i_mode_high;
			__u16	h_i_uid_high;
			__u16	h_i_gid_high;
			__u32	h_i_author;
		} hurd2;
	} osd2;				
	__u16	i_extra_isize;
	__u16	i_checksum_hi;	
	__u32	i_ctime_extra;	
	__u32	i_mtime_extra;	
	__u32	i_atime_extra;	
	__u32	i_crtime;	
	__u32	i_crtime_extra;	
	__u32	i_version_hi;	
};

#define i_dir_acl	i_size_high

#if defined(__KERNEL__) || defined(__linux__)
#define i_reserved1	osd1.linux1.l_i_reserved1
#define i_frag		osd2.linux2.l_i_frag
#define i_fsize		osd2.linux2.l_i_fsize
#define i_uid_low	i_uid
#define i_gid_low	i_gid
#define i_uid_high	osd2.linux2.l_i_uid_high
#define i_gid_high	osd2.linux2.l_i_gid_high
#else
#if defined(__GNU__)

#define i_translator	osd1.hurd1.h_i_translator
#define i_frag		osd2.hurd2.h_i_frag;
#define i_fsize		osd2.hurd2.h_i_fsize;
#define i_uid_high	osd2.hurd2.h_i_uid_high
#define i_gid_high	osd2.hurd2.h_i_gid_high
#define i_author	osd2.hurd2.h_i_author

#endif  
#endif	

#define inode_uid(inode)	((inode).i_uid | (inode).osd2.linux2.l_i_uid_high << 16)
#define inode_gid(inode)	((inode).i_gid | (inode).osd2.linux2.l_i_gid_high << 16)
#define ext2fs_set_i_uid_high(inode,x) ((inode).osd2.linux2.l_i_uid_high = (x))
#define ext2fs_set_i_gid_high(inode,x) ((inode).osd2.linux2.l_i_gid_high = (x))

#define EXT2_VALID_FS			0x0001	
#define EXT2_ERROR_FS			0x0002	
#define EXT3_ORPHAN_FS			0x0004	

#define EXT2_FLAGS_SIGNED_HASH		0x0001  
#define EXT2_FLAGS_UNSIGNED_HASH	0x0002  
#define EXT2_FLAGS_TEST_FILESYS		0x0004	
#define EXT2_FLAGS_IS_SNAPSHOT		0x0010	
#define EXT2_FLAGS_FIX_SNAPSHOT		0x0020	
#define EXT2_FLAGS_FIX_EXCLUDE		0x0040	

#define EXT2_MOUNT_CHECK		0x0001	
#define EXT2_MOUNT_GRPID		0x0004	
#define EXT2_MOUNT_DEBUG		0x0008	
#define EXT2_MOUNT_ERRORS_CONT		0x0010	
#define EXT2_MOUNT_ERRORS_RO		0x0020	
#define EXT2_MOUNT_ERRORS_PANIC		0x0040	
#define EXT2_MOUNT_MINIX_DF		0x0080	
#define EXT2_MOUNT_NO_UID32		0x0200  

#define clear_opt(o, opt)		o &= ~EXT2_MOUNT_##opt
#define set_opt(o, opt)			o |= EXT2_MOUNT_##opt
#define test_opt(sb, opt)		(EXT2_SB(sb)->s_mount_opt & \
					 EXT2_MOUNT_##opt)
#define EXT2_DFL_MAX_MNT_COUNT		20	
#define EXT2_DFL_CHECKINTERVAL		0	

#define EXT2_ERRORS_CONTINUE		1	
#define EXT2_ERRORS_RO			2	
#define EXT2_ERRORS_PANIC		3	
#define EXT2_ERRORS_DEFAULT		EXT2_ERRORS_CONTINUE

#if (__GNUC__ >= 4)
#define ext4_offsetof(TYPE,MEMBER) __builtin_offsetof(TYPE,MEMBER)
#else
#define ext4_offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

struct ext2_super_block {
	__u32	s_inodes_count;		
	__u32	s_blocks_count;		
	__u32	s_r_blocks_count;	
	__u32	s_free_blocks_count;	
	__u32	s_free_inodes_count;	
	__u32	s_first_data_block;	
	__u32	s_log_block_size;	
	__u32	s_log_cluster_size;	
	__u32	s_blocks_per_group;	
	__u32	s_clusters_per_group;	
	__u32	s_inodes_per_group;	
	__u32	s_mtime;		
	__u32	s_wtime;		
	__u16	s_mnt_count;		
	__s16	s_max_mnt_count;	
	__u16	s_magic;		
	__u16	s_state;		
	__u16	s_errors;		
	__u16	s_minor_rev_level;	
	__u32	s_lastcheck;		
	__u32	s_checkinterval;	
	__u32	s_creator_os;		
	__u32	s_rev_level;		
	__u16	s_def_resuid;		
	__u16	s_def_resgid;		
	__u32	s_first_ino;		
	__u16   s_inode_size;		
	__u16	s_block_group_nr;	
	__u32	s_feature_compat;	
	__u32	s_feature_incompat;	
	__u32	s_feature_ro_compat;	
	__u8	s_uuid[16];		
	char	s_volume_name[16];	
	char	s_last_mounted[64];	
	__u32	s_algorithm_usage_bitmap; 
	__u8	s_prealloc_blocks;	
	__u8	s_prealloc_dir_blocks;	
	__u16	s_reserved_gdt_blocks;	
	__u8	s_journal_uuid[16];	
	__u32	s_journal_inum;		
	__u32	s_journal_dev;		
	__u32	s_last_orphan;		
	__u32	s_hash_seed[4];		
	__u8	s_def_hash_version;	
	__u8	s_jnl_backup_type; 	
	__u16	s_desc_size;		
	__u32	s_default_mount_opts;
	__u32	s_first_meta_bg;	
	__u32	s_mkfs_time;		
	__u32	s_jnl_blocks[17]; 	
	__u32	s_blocks_count_hi;	
	__u32	s_r_blocks_count_hi;	
	__u32	s_free_blocks_hi; 	
	__u16	s_min_extra_isize;	
	__u16	s_want_extra_isize; 	
	__u32	s_flags;		
	__u16   s_raid_stride;		
	__u16   s_mmp_update_interval;  
	__u64   s_mmp_block;            
	__u32   s_raid_stripe_width;    
	__u8	s_log_groups_per_flex;	
	__u8    s_reserved_char_pad;
	__u16	s_reserved_pad;		
	__u64	s_kbytes_written;	/* nr of lifetime kilobytes written */
	__u32	s_snapshot_inum;	
	__u32	s_snapshot_id;		
	__u64	s_snapshot_r_blocks_count; 
	__u32	s_snapshot_list;	
#define EXT4_S_ERR_START ext4_offsetof(struct ext2_super_block, s_error_count)
	__u32	s_error_count;		
	__u32	s_first_error_time;	
	__u32	s_first_error_ino;	
	__u64	s_first_error_block;	
	__u8	s_first_error_func[32];	
	__u32	s_first_error_line;	
	__u32	s_last_error_time;	
	__u32	s_last_error_ino;	
	__u32	s_last_error_line;	
	__u64	s_last_error_block;	
	__u8	s_last_error_func[32];	
#define EXT4_S_ERR_END ext4_offsetof(struct ext2_super_block, s_mount_opts)
	__u8	s_mount_opts[64];
	__u32	s_usr_quota_inum;	
	__u32	s_grp_quota_inum;	
	__u32	s_overhead_blocks;	
	__u32   s_reserved[108];        
	__u32	s_checksum;		
};

#define EXT4_S_ERR_LEN (EXT4_S_ERR_END - EXT4_S_ERR_START)

#define EXT2_OS_LINUX		0
#define EXT2_OS_HURD		1
#define EXT2_OBSO_OS_MASIX	2
#define EXT2_OS_FREEBSD		3
#define EXT2_OS_LITES		4

#define EXT2_GOOD_OLD_REV	0	
#define EXT2_DYNAMIC_REV	1	

#define EXT2_CURRENT_REV	EXT2_GOOD_OLD_REV
#define EXT2_MAX_SUPP_REV	EXT2_DYNAMIC_REV

#define EXT2_GOOD_OLD_INODE_SIZE 128

#define EXT3_JNL_BACKUP_BLOCKS	1


#define EXT2_HAS_COMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_feature_compat & (mask) )
#define EXT2_HAS_RO_COMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_feature_ro_compat & (mask) )
#define EXT2_HAS_INCOMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_feature_incompat & (mask) )

#define EXT2_FEATURE_COMPAT_DIR_PREALLOC	0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES	0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR		0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INODE	0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX		0x0020
#define EXT2_FEATURE_COMPAT_LAZY_BG		0x0040
#define EXT2_FEATURE_COMPAT_EXCLUDE_BITMAP	0x0100


#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE	0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM		0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK	0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE	0x0040
#define EXT4_FEATURE_RO_COMPAT_HAS_SNAPSHOT	0x0080
#define EXT4_FEATURE_RO_COMPAT_QUOTA		0x0100
#define EXT4_FEATURE_RO_COMPAT_BIGALLOC		0x0200
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM	0x0400
#define EXT4_FEATURE_RO_COMPAT_REPLICA		0x0800

#define EXT2_FEATURE_INCOMPAT_COMPRESSION	0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004 
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008 
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT3_FEATURE_INCOMPAT_EXTENTS		0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT		0x0080
#define EXT4_FEATURE_INCOMPAT_MMP		0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG		0x0200
#define EXT4_FEATURE_INCOMPAT_EA_INODE		0x0400
#define EXT4_FEATURE_INCOMPAT_DIRDATA		0x1000
#define EXT4_FEATURE_INCOMPAT_LARGEDIR		0x4000 
#define EXT4_FEATURE_INCOMPAT_INLINEDATA	0x8000 

#define EXT2_FEATURE_COMPAT_SUPP	0
#define EXT2_FEATURE_INCOMPAT_SUPP    (EXT2_FEATURE_INCOMPAT_FILETYPE| \
				       EXT4_FEATURE_INCOMPAT_MMP)
#define EXT2_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT4_FEATURE_RO_COMPAT_DIR_NLINK| \
					 EXT2_FEATURE_RO_COMPAT_BTREE_DIR)

#define EXT2_DEF_RESUID		0
#define EXT2_DEF_RESGID		0

#define EXT2_DEFM_DEBUG		0x0001
#define EXT2_DEFM_BSDGROUPS	0x0002
#define EXT2_DEFM_XATTR_USER	0x0004
#define EXT2_DEFM_ACL		0x0008
#define EXT2_DEFM_UID16		0x0010
#define EXT3_DEFM_JMODE		0x0060
#define EXT3_DEFM_JMODE_DATA	0x0020
#define EXT3_DEFM_JMODE_ORDERED	0x0040
#define EXT3_DEFM_JMODE_WBACK	0x0060
#define EXT4_DEFM_NOBARRIER	0x0100
#define EXT4_DEFM_BLOCK_VALIDITY 0x0200
#define EXT4_DEFM_DISCARD	0x0400
#define EXT4_DEFM_NODELALLOC	0x0800

#define EXT2_NAME_LEN 255

struct ext2_dir_entry {
	__u32	inode;			
	__u16	rec_len;		
	__u16	name_len;		
	char	name[EXT2_NAME_LEN];	
};

struct ext2_dir_entry_2 {
	__u32	inode;			
	__u16	rec_len;		
	__u8	name_len;		
	__u8	file_type;
	char	name[EXT2_NAME_LEN];	
};

#define EXT2_FT_UNKNOWN		0
#define EXT2_FT_REG_FILE	1
#define EXT2_FT_DIR		2
#define EXT2_FT_CHRDEV		3
#define EXT2_FT_BLKDEV		4
#define EXT2_FT_FIFO		5
#define EXT2_FT_SOCK		6
#define EXT2_FT_SYMLINK		7

#define EXT2_FT_MAX		8

#define EXT2_DIR_PAD			4
#define EXT2_DIR_ROUND			(EXT2_DIR_PAD - 1)
#define EXT2_DIR_REC_LEN(name_len)	(((name_len) + 8 + EXT2_DIR_ROUND) & \
					 ~EXT2_DIR_ROUND)

/*
 * This structure is used for multiple mount protection. It is written
 * into the block number saved in the s_mmp_block field in the superblock.
 * Programs that check MMP should assume that if SEQ_FSCK (or any unknown
 * code above SEQ_MAX) is present then it is NOT safe to use the filesystem,
 * regardless of how old the timestamp is.
 *
 * The timestamp in the MMP structure will be updated by e2fsck at some
 * arbitary intervals (start of passes, after every few groups of inodes
 * in pass1 and pass1b).  There is no guarantee that e2fsck is updating
 * the MMP block in a timely manner, and the updates it does are purely
 * for the convenience of the sysadmin and not for automatic validation.
 *
 * Note: Only the mmp_seq value is used to determine whether the MMP block
 *	is being updated.  The mmp_time, mmp_nodename, and mmp_bdevname
 *	fields are only for informational purposes for the administrator,
 *	due to clock skew between nodes and hostname HA service takeover.
 */
#define EXT4_MMP_MAGIC     0x004D4D50U 
#define EXT4_MMP_SEQ_CLEAN 0xFF4D4D50U 
#define EXT4_MMP_SEQ_FSCK  0xE24D4D50U 
#define EXT4_MMP_SEQ_MAX   0xE24D4D4FU 

struct mmp_struct {
	__u32	mmp_magic;		
	__u32	mmp_seq;		
	__u64	mmp_time;		
	char	mmp_nodename[64];	
	char	mmp_bdevname[32];	
	__u16	mmp_check_interval;	
	__u16	mmp_pad1;
	__u32	mmp_pad2[227];
};

#define EXT4_MMP_UPDATE_INTERVAL	5

#define EXT4_MMP_MAX_UPDATE_INTERVAL	300

#define EXT4_MMP_MIN_CHECK_INTERVAL     5

#endif	
