#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H

#define __ALIGN_KERNEL(x, a)		__ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask)	(((x) + (mask)) & ~(mask))


#define SI_LOAD_SHIFT	16
struct sysinfo {
	long uptime;			
	unsigned long loads[3];		
	unsigned long totalram;		
	unsigned long freeram;		
	unsigned long sharedram;	
	unsigned long bufferram;	
	unsigned long totalswap;	
	unsigned long freeswap;		
	unsigned short procs;		
	unsigned short pad;		
	unsigned long totalhigh;	
	unsigned long freehigh;		
	unsigned int mem_unit;		
	char _f[20-2*sizeof(long)-sizeof(int)];	
};

#endif
