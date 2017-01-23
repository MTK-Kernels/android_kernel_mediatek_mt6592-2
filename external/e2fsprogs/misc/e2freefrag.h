#include <sys/types.h>

#define DEFAULT_CHUNKSIZE (1024*1024)

#define MAX_HIST	32
struct free_chunk_histogram {
	unsigned long fc_chunks[MAX_HIST];
	unsigned long fc_blocks[MAX_HIST];
};

struct chunk_info {
	unsigned long chunkbytes;	
	int chunkbits;			
	unsigned long free_chunks;	
	unsigned long real_free_chunks; 
	int blocksize_bits;		
	int blks_in_chunk;		
	unsigned long min, max, avg;	
	struct free_chunk_histogram histogram; 
};
