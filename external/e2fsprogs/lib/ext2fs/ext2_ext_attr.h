
#ifndef _EXT2_EXT_ATTR_H
#define _EXT2_EXT_ATTR_H
#define EXT2_EXT_ATTR_MAGIC_v1		0xEA010000
#define EXT2_EXT_ATTR_MAGIC		0xEA020000

#define EXT2_EXT_ATTR_REFCOUNT_MAX	1024

struct ext2_ext_attr_header {
	__u32	h_magic;	
	__u32	h_refcount;	
	__u32	h_blocks;	
	__u32	h_hash;		
	__u32	h_reserved[4];	
};

struct ext2_ext_attr_entry {
	__u8	e_name_len;	
	__u8	e_name_index;	
	__u16	e_value_offs;	
	__u32	e_value_block;	
	__u32	e_value_size;	
	__u32	e_hash;		
#if 0
	char	e_name[0];	
#endif
};

#define EXT2_EXT_ATTR_PAD_BITS		2
#define EXT2_EXT_ATTR_PAD		((unsigned) 1<<EXT2_EXT_ATTR_PAD_BITS)
#define EXT2_EXT_ATTR_ROUND		(EXT2_EXT_ATTR_PAD-1)
#define EXT2_EXT_ATTR_LEN(name_len) \
	(((name_len) + EXT2_EXT_ATTR_ROUND + \
	sizeof(struct ext2_ext_attr_entry)) & ~EXT2_EXT_ATTR_ROUND)
#define EXT2_EXT_ATTR_NEXT(entry) \
	( (struct ext2_ext_attr_entry *)( \
	  (char *)(entry) + EXT2_EXT_ATTR_LEN((entry)->e_name_len)) )
#define EXT2_EXT_ATTR_SIZE(size) \
	(((size) + EXT2_EXT_ATTR_ROUND) & ~EXT2_EXT_ATTR_ROUND)
#define EXT2_EXT_IS_LAST_ENTRY(entry) (*((__u32 *)(entry)) == 0UL)
#define EXT2_EXT_ATTR_NAME(entry) \
	(((char *) (entry)) + sizeof(struct ext2_ext_attr_entry))
#define EXT2_XATTR_LEN(name_len) \
	(((name_len) + EXT2_EXT_ATTR_ROUND + \
	sizeof(struct ext2_xattr_entry)) & ~EXT2_EXT_ATTR_ROUND)
#define EXT2_XATTR_SIZE(size) \
	(((size) + EXT2_EXT_ATTR_ROUND) & ~EXT2_EXT_ATTR_ROUND)

#ifdef __KERNEL__
# ifdef CONFIG_EXT2_FS_EXT_ATTR
extern int ext2_get_ext_attr(struct inode *, const char *, char *, size_t, int);
extern int ext2_set_ext_attr(struct inode *, const char *, char *, size_t, int);
extern void ext2_ext_attr_free_inode(struct inode *inode);
extern void ext2_ext_attr_put_super(struct super_block *sb);
extern int ext2_ext_attr_init(void);
extern void ext2_ext_attr_done(void);
# else
#  define ext2_get_ext_attr NULL
#  define ext2_set_ext_attr NULL
# endif
#endif  
#endif  
