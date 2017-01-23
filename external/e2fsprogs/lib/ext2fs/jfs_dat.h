
#define JFS_MAGIC_NUMBER 0xc03b3998U 



#define JFS_DESCRIPTOR_BLOCK	1
#define JFS_COMMIT_BLOCK	2
#define JFS_SUPERBLOCK		3

typedef struct journal_header_s
{
	__u32		h_magic;
	__u32		h_blocktype;
	__u32		h_sequence;
} journal_header_t;


typedef struct journal_block_tag_s
{
	__u32		t_blocknr;	
	__u32		t_flags;	
} journal_block_tag_t;

#define JFS_FLAG_ESCAPE		1	
#define JFS_FLAG_SAME_UUID	2	
#define JFS_FLAG_DELETED	4	
#define JFS_FLAG_LAST_TAG	8	


typedef struct journal_superblock_s
{
	journal_header_t s_header;

	
	__u32		s_blocksize;	
	__u32		s_maxlen;	
	__u32		s_first;	

	
	__u32		s_sequence;	
	__u32		s_start;	

} journal_superblock_t;

