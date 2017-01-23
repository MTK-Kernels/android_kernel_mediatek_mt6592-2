/*
 *  pcap-sita.c: Packet capture interface additions for SITA ACN devices
 *
 *  Copyright (c) 2007 Fulko Hew, SITA INC Canada, Inc <fulko.hew@sita.aero>
 *
 *  License: BSD
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  3. The names of the authors may not be used to endorse or promote
 *     products derived from this software without specific prior
 *     written permission.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

 

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "pcap-int.h"

#include "pcap-sita.h"

	

#define IOP_SNIFFER_PORT	49152			
#define MAX_LINE_SIZE		255				
#define MAX_CHASSIS			8				
#define MAX_GEOSLOT			8				

#define FIND			0
#define LIVE			1

typedef struct iface {
	struct iface	*next;		
	char		*name;		
	char		*IOPname;	
	uint32_t	iftype;		
} iface_t;

typedef struct unit {
	char			*ip;		
	int			fd;		
	int			find_fd;	
	int			first_time;	
	struct sockaddr_in	*serv_addr;	
	int			chassis;
	int			geoslot;
	iface_t			*iface;		
	char			*imsg;		
	int			len;		
} unit_t;

static unit_t		units[MAX_CHASSIS+1][MAX_GEOSLOT+1];	
static fd_set		readfds;				
static int		max_fs;

pcap_if_t		*acn_if_list;		

static void dump_interface_list(void) {
	pcap_if_t		*iff;
	pcap_addr_t		*addr;
	int			longest_name_len = 0;
	char			*n, *d, *f;
	int			if_number = 0;

	iff = acn_if_list;
	while (iff) {
		if (iff->name && (strlen(iff->name) > longest_name_len)) longest_name_len = strlen(iff->name);
		iff = iff->next;
	}
	iff = acn_if_list;
	printf("Interface List:\n");
	while (iff) {
		n = (iff->name)							? iff->name			: "";
		d = (iff->description)					? iff->description	: "";
		f = (iff->flags == PCAP_IF_LOOPBACK)	? "L"				: "";
		printf("%3d: %*s %s '%s'\n", if_number++, longest_name_len, n, f, d);
		addr = iff->addresses;
		while (addr) {
			printf("%*s ", (5 + longest_name_len), "");		
			printf("%15s  ", (addr->addr)		? inet_ntoa(((struct sockaddr_in *)addr->addr)->sin_addr)		: "");
			printf("%15s  ", (addr->netmask)	? inet_ntoa(((struct sockaddr_in *)addr->netmask)->sin_addr)	: "");
			printf("%15s  ", (addr->broadaddr)	? inet_ntoa(((struct sockaddr_in *)addr->broadaddr)->sin_addr)	: "");
			printf("%15s  ", (addr->dstaddr)	? inet_ntoa(((struct sockaddr_in *)addr->dstaddr)->sin_addr)	: "");
			printf("\n");
			addr = addr->next;
		}
		iff = iff->next;
	}
}

static void dump(unsigned char *ptr, int i, int indent) {
	fprintf(stderr, "%*s", indent, " ");
	for (; i > 0; i--) {
		fprintf(stderr, "%2.2x ", *ptr++);
	}
	fprintf(stderr, "\n");
}

static void dump_interface_list_p(void) {
	pcap_if_t		*iff;
	pcap_addr_t		*addr;
	int				if_number = 0;

	iff = acn_if_list;
	printf("Interface Pointer @ %p is %p:\n", &acn_if_list, iff);
	while (iff) {
		printf("%3d: %p %p next: %p\n", if_number++, iff->name, iff->description, iff->next);
		dump((unsigned char *)iff, sizeof(pcap_if_t), 5);
		addr = iff->addresses;
		while (addr) {
			printf("          %p %p %p %p, next: %p\n", addr->addr, addr->netmask, addr->broadaddr, addr->dstaddr, addr->next);
			dump((unsigned char *)addr, sizeof(pcap_addr_t), 10);
			addr = addr->next;
		}
		iff = iff->next;
	}
}

static void dump_unit_table(void) {
	int		chassis, geoslot;
	iface_t	*p;

	printf("%c:%c %s %s\n", 'C', 'S', "fd", "IP Address");
	for (chassis = 0; chassis <= MAX_CHASSIS; chassis++) {
		for (geoslot = 0; geoslot <= MAX_GEOSLOT; geoslot++) {
			if (units[chassis][geoslot].ip != NULL)
				printf("%d:%d %2d %s\n", chassis, geoslot, units[chassis][geoslot].fd, units[chassis][geoslot].ip);
			p = units[chassis][geoslot].iface;
			while (p) {
				char *n = (p->name)			? p->name			: "";
				char *i = (p->IOPname)		? p->IOPname		: "";
				p = p->next;
				printf("   %12s    -> %12s\n", i, n);
			}
		}
	}
}

static int find_unit_by_fd(int fd, int *chassis, int *geoslot, unit_t **unit_ptr) {
	int		c, s;

	for (c = 0; c <= MAX_CHASSIS; c++) {
		for (s = 0; s <= MAX_GEOSLOT; s++) {
			if (units[c][s].fd == fd || units[c][s].find_fd == fd) {
				if (chassis)	*chassis = c;
				if (geoslot)	*geoslot = s;
				if (unit_ptr)	*unit_ptr = &units[c][s];
				return 1;
			}
		}
	}
	return 0;
}

static int read_client_nbytes(int fd, int count, unsigned char *buf) {
	unit_t			*u;
	int				chassis, geoslot;
	int				len;

	find_unit_by_fd(fd, &chassis, &geoslot, &u);
	while (count) {
		if ((len = recv(fd, buf, count, 0)) <= 0)	return -1;	
		count -= len;	
		buf += len;
	}															
	return 0;
}

static void empty_unit_iface(unit_t *u) {
	iface_t	*p, *cur;

	cur = u->iface;
	while (cur) {											
		if (cur->name)			free(cur->name);			
		if (cur->IOPname)		free(cur->IOPname);
		p = cur->next;
		free(cur);											
		cur = p;
	}
	u->iface = 0;											
}

static void empty_unit(int chassis, int geoslot) {
	unit_t	*u = &units[chassis][geoslot];

	empty_unit_iface(u);
	if (u->imsg) {											
		u->imsg = (char *)realloc(u->imsg, 1);				
		if (u->imsg == NULL) {	
			fprintf(stderr, "Warning...call to realloc() failed, value of errno is %d\n", errno);
		
	}
}

static void empty_unit_table(void) {
	int		chassis, geoslot;

	for (chassis = 0; chassis <= MAX_CHASSIS; chassis++) {
		for (geoslot = 0; geoslot <= MAX_GEOSLOT; geoslot++) {
			if (units[chassis][geoslot].ip != NULL) {
				free(units[chassis][geoslot].ip);			
				units[chassis][geoslot].ip = 0;				
			}
			empty_unit(chassis, geoslot);
		}
	}
}

static char *find_nth_interface_name(int n) {
	int		chassis, geoslot;
	iface_t	*p;
	char	*last_name = 0;

	if (n < 0) n = 0;												
	for (chassis = 0; chassis <= MAX_CHASSIS; chassis++) {			
		for (geoslot = 0; geoslot <= MAX_GEOSLOT; geoslot++) {
			if (units[chassis][geoslot].ip != NULL) {
				p = units[chassis][geoslot].iface;
				while (p) {											
					if (p->IOPname) last_name = p->name;			
					if (n-- == 0) return last_name;					
					p = p->next;
				}
			}
		}
	}
											
	if (last_name)	return last_name;		
	return "";								
}

int acn_parse_hosts_file(char *errbuf) {				
	FILE	*fp;
	char	buf[MAX_LINE_SIZE];
	char	*ptr, *ptr2;
	int		pos;
	int		chassis, geoslot;
	unit_t	*u;

	empty_unit_table();
	if ((fp = fopen("/etc/hosts", "r")) == NULL) {										
		snprintf(errbuf, PCAP_ERRBUF_SIZE, "Cannot open '/etc/hosts' for reading.");	
		return -1;
	}
	while (fgets(buf, MAX_LINE_SIZE-1, fp)) {			

		pos = strcspn(buf, "#\n\r");					
		*(buf + pos) = '\0';							

		pos = strspn(buf, " \t");						
		if (pos == strlen(buf))							
			continue;									
		ptr = buf + pos;								

		if ((ptr2 = strstr(ptr, "_I_")) == NULL)		
			continue;
		if (*(ptr2 + 4) != '_')							
			continue;
		*(ptr + strcspn(ptr, " \t")) = '\0';			

		chassis = *(ptr2 + 3) - '0';					
		geoslot = *(ptr2 + 5) - '0';					
		if (chassis < 1 || chassis > MAX_CHASSIS ||
			geoslot < 1 || geoslot > MAX_GEOSLOT) {		
			snprintf(errbuf, PCAP_ERRBUF_SIZE, "Invalid ACN name in '/etc/hosts'.");	
			continue;																	
		}
		if ((ptr2 = (char *)malloc(strlen(ptr) + 1)) == NULL) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE, "malloc: %s", pcap_strerror(errno));
			continue;
		}
		strcpy(ptr2, ptr);								
		u = &units[chassis][geoslot];
		u->ip = ptr2;									
		u->chassis = chassis;
		u->geoslot = geoslot;
	}
	fclose(fp);
	if (*errbuf)	return -1;
	else			return 0;
}

static int open_with_IOP(unit_t  *u, int flag) {
	int					sockfd;
	char				*ip;

	if (u->serv_addr == NULL) {
		u->serv_addr = malloc(sizeof(struct sockaddr_in));

		
		if (u->serv_addr == NULL) {	
			fprintf(stderr, "malloc() request for u->serv_addr failed, value of errno is: %d\n", errno);
			return 0;
		}

	}
	ip = u->ip;
	
	memset((char *)u->serv_addr, 0, sizeof(struct sockaddr_in));
	u->serv_addr->sin_family		= AF_INET;
	u->serv_addr->sin_addr.s_addr	= inet_addr(ip);
	u->serv_addr->sin_port			= htons(IOP_SNIFFER_PORT);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "pcap can't open a socket for connecting to IOP at %s\n", ip);
		return 0;
	}
	if (connect(sockfd, (struct sockaddr *)u->serv_addr, sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr, "pcap can't connect to IOP at %s\n", ip);
		return 0;
	}
	if (flag == LIVE)	u->fd = sockfd;
	else				u->find_fd = sockfd;
	u->first_time = 0;
	return sockfd;			
}

static void close_with_IOP(int chassis, int geoslot, int flag) {
	int		*id;

	if (flag == LIVE)	id = &units[chassis][geoslot].fd;
	else				id = &units[chassis][geoslot].find_fd;

	if (*id) {										
		close(*id);									
		*id = 0;									
	}
}

static void pcap_cleanup_acn(pcap_t *handle) {
	int		chassis, geoslot;
	unit_t	*u;

	if (find_unit_by_fd(handle->fd, &chassis, &geoslot, &u) == 0)
		return;
	close_with_IOP(chassis, geoslot, LIVE);
	if (u)
		u->first_time = 0;
	pcap_cleanup_live_common(handle);
}

static void send_to_fd(int fd, int len, unsigned char *str) {
	int		nwritten;
	int		chassis, geoslot;

	while (len > 0) {
		if ((nwritten = write(fd, str, len)) <= 0) {
			find_unit_by_fd(fd, &chassis, &geoslot, NULL);
			if (units[chassis][geoslot].fd == fd)			close_with_IOP(chassis, geoslot, LIVE);
			else if (units[chassis][geoslot].find_fd == fd)	close_with_IOP(chassis, geoslot, FIND);
			empty_unit(chassis, geoslot);
			return;
		}
		len -= nwritten;
		str += nwritten;
	}
}

static void acn_freealldevs(void) {

	pcap_if_t	*iff, *next_iff;
	pcap_addr_t	*addr, *next_addr;

	for (iff = acn_if_list; iff != NULL; iff = next_iff) {
		next_iff = iff->next;
		for (addr = iff->addresses; addr != NULL; addr = next_addr) {
			next_addr = addr->next;
			if (addr->addr)			free(addr->addr);
			if (addr->netmask)		free(addr->netmask);
			if (addr->broadaddr)	free(addr->broadaddr);
			if (addr->dstaddr)		free(addr->dstaddr);
			free(addr);
		}
		if (iff->name)			free(iff->name);
		if (iff->description)	free(iff->description);
		free(iff);
	}
}

static void nonUnified_IOP_port_name(char *buf, size_t bufsize, const char *proto, unit_t *u) {

	snprintf(buf, bufsize, "%s_%d_%d", proto, u->chassis, u->geoslot);
}

static void unified_IOP_port_name(char *buf, size_t bufsize, const char *proto, unit_t *u, int IOPportnum) {
	int			portnum;

	portnum = ((u->chassis - 1) * 64) + ((u->geoslot - 1) * 8) + IOPportnum + 1;
	snprintf(buf, bufsize, "%s_%d", proto, portnum);
}

static char *translate_IOP_to_pcap_name(unit_t *u, char *IOPname, bpf_u_int32 iftype) {
	iface_t		*iface_ptr, *iface;
	char		*name;
	char		buf[32];
	char		*proto;
	char		*port;
	int			IOPportnum = 0;

	iface = malloc(sizeof(iface_t));		
	if (iface == NULL) {	
		fprintf(stderr, "Error...couldn't allocate memory for interface structure...value of errno is: %d\n", errno);
		return NULL;
	}
	memset((char *)iface, 0, sizeof(iface_t));	

	iface->iftype = iftype;					

	name = malloc(strlen(IOPname) + 1);		
        if (name == NULL) {    
                fprintf(stderr, "Error...couldn't allocate memory for IOPname...value of errno is: %d\n", errno);
                return NULL;
        }

	strcpy(name, IOPname);					
	iface->IOPname = name;					

	if (strncmp(IOPname, "lo", 2) == 0) {
		IOPportnum = atoi(&IOPname[2]);
		switch (iftype) {
			case DLT_EN10MB:
				nonUnified_IOP_port_name(buf, sizeof buf, "lo", u);
				break;
			default:
				unified_IOP_port_name(buf, sizeof buf, "???", u, IOPportnum);
				break;
		}
	} else if (strncmp(IOPname, "eth", 3) == 0) {
		IOPportnum = atoi(&IOPname[3]);
		switch (iftype) {
			case DLT_EN10MB:
				nonUnified_IOP_port_name(buf, sizeof buf, "eth", u);
				break;
			default:
				unified_IOP_port_name(buf, sizeof buf, "???", u, IOPportnum);
				break;
		}
	} else if (strncmp(IOPname, "wan", 3) == 0) {
		IOPportnum = atoi(&IOPname[3]);
		switch (iftype) {
			case DLT_SITA:
				unified_IOP_port_name(buf, sizeof buf, "wan", u, IOPportnum);
				break;
			default:
				unified_IOP_port_name(buf, sizeof buf, "???", u, IOPportnum);
				break;
		}
	} else {
		fprintf(stderr, "Error... invalid IOP name %s\n", IOPname);
		return NULL;
	}

	name = malloc(strlen(buf) + 1);			
        if (name == NULL) {    
                fprintf(stderr, "Error...couldn't allocate memory for IOP port name...value of errno is: %d\n", errno);
                return NULL;
        }

	strcpy(name, buf);						
	iface->name = name;						

	if (u->iface == 0) {					
		u->iface = iface;					
	} else {
		iface_ptr = u->iface;
		while (iface_ptr->next) {			
			iface_ptr = iface_ptr->next;	
		}
		iface_ptr->next = iface;			
	}
	return iface->name;
}

static int if_sort(char *s1, char *s2) {
	char	*s1_p2, *s2_p2;
	char	str1[MAX_LINE_SIZE], str2[MAX_LINE_SIZE];
	int		s1_p1_len, s2_p1_len;
	int		retval;

	if ((s1_p2 = strchr(s1, '_'))) {	
		s1_p1_len = s1_p2 - s1;			
		s1_p2++;						
	} else {							
		s1_p1_len = strlen(s1);			
		s1_p2 = 0;						
	}
	if ((s2_p2 = strchr(s2, '_'))) {	
		s2_p1_len = s2_p2 - s2;
		s2_p2++;
	} else {
		s2_p1_len = strlen(s2);
		s2_p2 = 0;
	}
	strncpy(str1, s1, (s1_p1_len > sizeof(str1)) ? s1_p1_len : sizeof(str1));   *(str1 + s1_p1_len) = 0;
	strncpy(str2, s2, (s2_p1_len > sizeof(str2)) ? s2_p1_len : sizeof(str2));   *(str2 + s2_p1_len) = 0;
	retval = strcmp(str1, str2);
	if (retval != 0) return retval;		
	return strcmp(s1_p2, s2_p2);		
}

static void sort_if_table(void) {
	pcap_if_t	*p1, *p2, *prev, *temp;
	int			has_swapped;

	if (!acn_if_list) return;				

	while (1) {
		p1 = acn_if_list;					
		prev = 0;
		has_swapped = 0;
		while ((p2 = p1->next)) {
			if (if_sort(p1->name, p2->name) > 0) {
				if (prev) {					
					temp = p2->next;
					prev->next = p2;
					p2->next = p1;
					p1->next = temp;
				} else {					
					temp = p2->next;
					acn_if_list= p2;
					p2->next = p1;
					p1->next = temp;
				}
				p1 = p2;
				prev = p1;
				has_swapped = 1;
			}
			prev = p1;
			p1 = p1->next;
		}
		if (has_swapped == 0)
			return;
	}	
	return;
}
	
static int process_client_data (char *errbuf) {								
	int					chassis, geoslot;
	unit_t				*u;
	pcap_if_t			*iff, *prev_iff;
	pcap_addr_t			*addr, *prev_addr;
	char				*ptr;
	int					address_count;
	struct sockaddr_in	*s;
	char				*newname;
	bpf_u_int32				interfaceType;
	unsigned char		flags;

	prev_iff = 0;
	for (chassis = 0; chassis <= MAX_CHASSIS; chassis++) {
		for (geoslot = 0; geoslot <= MAX_GEOSLOT; geoslot++) {				
			u = &units[chassis][geoslot];
			empty_unit_iface(u);
			ptr = u->imsg;													
			while (ptr < (u->imsg + u->len)) {
				if ((iff = malloc(sizeof(pcap_if_t))) == NULL) {
					snprintf(errbuf, PCAP_ERRBUF_SIZE, "malloc: %s", pcap_strerror(errno));
					return -1;
				}
				memset((char *)iff, 0, sizeof(pcap_if_t)); 
				if (acn_if_list == 0)	acn_if_list = iff;					
				if (prev_iff)			prev_iff->next = iff;				

				if (*ptr) {													
					if ((iff->name = malloc(*ptr + 1)) == NULL) {			
						snprintf(errbuf, PCAP_ERRBUF_SIZE, "malloc: %s", pcap_strerror(errno));
						return -1;
					}
					memcpy(iff->name, (ptr + 1), *ptr);						
					*(iff->name + *ptr) = 0;								
					ptr += *ptr;											
				}
				ptr++;

				if (*ptr) {													
					if ((iff->description = malloc(*ptr + 1)) == NULL) {	
						snprintf(errbuf, PCAP_ERRBUF_SIZE, "malloc: %s", pcap_strerror(errno));
						return -1;
					}
					memcpy(iff->description, (ptr + 1), *ptr);				
					*(iff->description + *ptr) = 0;							
					ptr += *ptr;											
				}
				ptr++;

				interfaceType = ntohl(*(bpf_u_int32 *)ptr);
				ptr += 4;													

				flags = *ptr++;
				if (flags) iff->flags = PCAP_IF_LOOPBACK;					

				address_count = *ptr++;

				prev_addr = 0;
				while (address_count--) {
					if ((addr = malloc(sizeof(pcap_addr_t))) == NULL) {
						snprintf(errbuf, PCAP_ERRBUF_SIZE, "malloc: %s", pcap_strerror(errno));
						return -1;
					}
+					memset((char *)addr, 0, sizeof(pcap_addr_t)); 
					if (iff->addresses == 0) iff->addresses = addr;
					if (prev_addr) prev_addr->next = addr;							
					if (*ptr) {														
						if ((s = malloc(sizeof(struct sockaddr_in))) == NULL) {		
							snprintf(errbuf, PCAP_ERRBUF_SIZE, "malloc: %s", pcap_strerror(errno));
							return -1;
						}
						memset((char *)s, 0, sizeof(struct sockaddr_in)); 
						addr->addr = (struct sockaddr *)s;
						s->sin_family		= AF_INET;
						s->sin_addr.s_addr	= *(bpf_u_int32 *)(ptr + 1);			
						ptr += *ptr;										
					}
					ptr++;													
					if (*ptr) {												
						if ((s = malloc(sizeof(struct sockaddr_in))) == NULL) {
							snprintf(errbuf, PCAP_ERRBUF_SIZE, "malloc: %s", pcap_strerror(errno));
							return -1;
						}
						
						memset((char *)s, 0, sizeof(struct sockaddr_in));

						addr->netmask = (struct sockaddr *)s;
						s->sin_family		= AF_INET;
						s->sin_addr.s_addr	= *(bpf_u_int32*)(ptr + 1);
						ptr += *ptr;
					}
					ptr++;
					if (*ptr) {												
						if ((s = malloc(sizeof(struct sockaddr_in))) == NULL) {
							snprintf(errbuf, PCAP_ERRBUF_SIZE, "malloc: %s", pcap_strerror(errno));
							return -1;
						}
						
						memset((char *)s, 0, sizeof(struct sockaddr_in));

						addr->broadaddr = (struct sockaddr *)s;
						s->sin_family		= AF_INET;
						s->sin_addr.s_addr	= *(bpf_u_int32*)(ptr + 1);
						ptr += *ptr;
					}
					ptr++;
					if (*ptr) {												
						if ((s = malloc(sizeof(struct sockaddr_in))) == NULL) {
							snprintf(errbuf, PCAP_ERRBUF_SIZE, "malloc: %s", pcap_strerror(errno));
							return -1;
						}
						
						memset((char *)s, 0, sizeof(struct sockaddr_in));

						addr->dstaddr = (struct sockaddr *)s;
						s->sin_family		= AF_INET;
						s->sin_addr.s_addr	= *(bpf_u_int32*)(ptr + 1);
						ptr += *ptr;
					}
					ptr++;
					prev_addr = addr;
				}
				prev_iff = iff;

				newname = translate_IOP_to_pcap_name(u, iff->name, interfaceType);		
				if ((iff->name = realloc(iff->name, strlen(newname) + 1)) == NULL) {	
					snprintf(errbuf, PCAP_ERRBUF_SIZE, "realloc: %s", pcap_strerror(errno));
					return -1;
				}
				strcpy(iff->name, newname);												
			}
		}
	}
	return 0;
}

static int read_client_data (int fd) {
	unsigned char	buf[256];
	int				chassis, geoslot;
	unit_t			*u;
	int				len;

	find_unit_by_fd(fd, &chassis, &geoslot, &u);

	if ((len = recv(fd, buf, sizeof(buf), 0)) <= 0)	return 0;	

	if ((u->imsg = realloc(u->imsg, (u->len + len))) == NULL)	
		return 0;
	memcpy((u->imsg + u->len), buf, len);						
	u->len += len;
	return 1;
}

static void wait_for_all_answers(void) {
	int		retval;
	struct	timeval tv;
	int		fd;
	int		chassis, geoslot;

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	while (1) {
		int flag = 0;
		fd_set working_set;

		for (fd = 0; fd <= max_fs; fd++) {								
			if (FD_ISSET(fd, &readfds)) flag = 1;						
		}
		if (flag == 0) return;											

		memcpy(&working_set, &readfds, sizeof(readfds));				
		retval = select(max_fs + 1, &working_set, NULL, NULL, &tv);
		if (retval == -1) {												
			return;
		} else if (retval == 0) {										
			printf("timeout\n");
			return;
		} else {
			for (fd = 0; fd <= max_fs; fd++) {							
				if (FD_ISSET(fd, &working_set)) {
					if (read_client_data(fd) == 0) {					
						FD_CLR(fd, &readfds);							
						find_unit_by_fd(fd, &chassis, &geoslot, NULL);
						close_with_IOP(chassis, geoslot, FIND);			
					}
				}
			}
		}
	}
}

static char *get_error_response(int fd, char *errbuf) {		
	char	byte;
	int		len = 0;

	while (1) {
		recv(fd, &byte, 1, 0);							
		if (errbuf && (len++ < PCAP_ERRBUF_SIZE)) {		
			*errbuf++ = byte;							
			*errbuf = '\0';								
		}
		if (byte == '\0') {
			if (len > 1)	{ return errbuf;	}
			else			{ return NULL;		}
		}
	}
}

int acn_findalldevs(char *errbuf) {								
	int		chassis, geoslot;
	unit_t	*u;

	FD_ZERO(&readfds);
	max_fs = 0;
	for (chassis = 0; chassis <= MAX_CHASSIS; chassis++) {
		for (geoslot = 0; geoslot <= MAX_GEOSLOT; geoslot++) {
			u = &units[chassis][geoslot];
			if (u->ip && (open_with_IOP(u, FIND))) {			
				send_to_fd(u->find_fd, 1, (unsigned char *)"\0");
				if (get_error_response(u->find_fd, errbuf))
					close_with_IOP(chassis, geoslot, FIND);
				else {
					if (u->find_fd > max_fs)
						max_fs = u->find_fd;								
					FD_SET(u->find_fd, &readfds);						
					u->len = 0;
					send_to_fd(u->find_fd, 1, (unsigned char *)"Q");		
				}
			}
		}
	}
	wait_for_all_answers();
	if (process_client_data(errbuf))
		return -1;
	sort_if_table();
	return 0;
}

static int pcap_stats_acn(pcap_t *handle, struct pcap_stat *ps) {
	unsigned char	buf[12];

	send_to_fd(handle->fd, 1, (unsigned char *)"S");						

	if (read_client_nbytes(handle->fd, sizeof(buf), buf) == -1) return -1;	

	ps->ps_recv		= ntohl(*(uint32_t *)&buf[0]);							
	ps->ps_drop		= ntohl(*(uint32_t *)&buf[4]);
	ps->ps_ifdrop	= ntohl(*(uint32_t *)&buf[8]);

	return 0;
}

static int acn_open_live(const char *name, char *errbuf, int *linktype) {		
	int			chassis, geoslot;
	unit_t		*u;
	iface_t		*p;
	pcap_if_t	*alldevsp;

	pcap_findalldevs_interfaces(&alldevsp, errbuf);
	for (chassis = 0; chassis <= MAX_CHASSIS; chassis++) {										
		for (geoslot = 0; geoslot <= MAX_GEOSLOT; geoslot++) {
			u = &units[chassis][geoslot];
			if (u->ip != NULL) {
				p = u->iface;
				while (p) {																		
					if (p->IOPname && p->name && (strcmp(p->name, name) == 0)) {				
						*linktype = p->iftype;
						open_with_IOP(u, LIVE);													
						send_to_fd(u->fd, strlen(p->IOPname)+1, (unsigned char *)p->IOPname);	
						if (get_error_response(u->fd, errbuf)) {
							return -1;
						}
						return u->fd;															
					}
					p = p->next;
				}
			}
		}
	}
	return -1;																				
}

static void acn_start_monitor(int fd, int snaplen, int timeout, int promiscuous, int direction) {
	unsigned char	buf[8];
	unit_t			*u;

	
	find_unit_by_fd(fd, NULL, NULL, &u);
	if (u->first_time == 0) {
		buf[0]					= 'M';
		*(uint32_t *)&buf[1]	= htonl(snaplen);
		buf[5]					= timeout;
		buf[6]					= promiscuous;
		buf[7]					= direction;
	
		send_to_fd(fd, 8, buf);								
		u->first_time = 1;
	}
	
}

static int pcap_inject_acn(pcap_t *p, const void *buf _U_, size_t size _U_) {
	strlcpy(p->errbuf, "Sending packets isn't supported on ACN adapters",
	    PCAP_ERRBUF_SIZE);
	return (-1);
}

static int pcap_setfilter_acn(pcap_t *handle, struct bpf_program *bpf) {
	int				fd = handle->fd;
	int				count;
	struct bpf_insn	*p;
	uint16_t		shortInt;
	uint32_t		longInt;

	send_to_fd(fd, 1, (unsigned char *)"F");			
	count = bpf->bf_len;
	longInt = htonl(count);
	send_to_fd(fd, 4, (unsigned char *)&longInt);		
	p = bpf->bf_insns;
	while (count--) {									
		shortInt = htons(p->code);
		longInt = htonl(p->k);
		send_to_fd(fd, 2, (unsigned char *)&shortInt);
		send_to_fd(fd, 1, (unsigned char *)&p->jt);
		send_to_fd(fd, 1, (unsigned char *)&p->jf);
		send_to_fd(fd, 4, (unsigned char *)&longInt);
		p++;
	}
	if (get_error_response(fd, NULL))
		return -1;
	return 0;
}

static int pcap_setdirection_acn(pcap_t *handle, pcap_direction_t d) {
	snprintf(handle->errbuf, sizeof(handle->errbuf),
	    "Setting direction is not supported on ACN adapters");
	return -1;
}

static int acn_read_n_bytes_with_timeout(pcap_t *handle, int count) {
	struct		timeval tv;
	int			retval, fd;
	fd_set		r_fds;
	fd_set		w_fds;
	u_char		*bp;
	int			len = 0;
	int			offset = 0;

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	fd = handle->fd;
	FD_ZERO(&r_fds);
	FD_SET(fd, &r_fds);
	memcpy(&w_fds, &r_fds, sizeof(r_fds));
	bp = handle->bp;
	while (count) {
		retval = select(fd + 1, &w_fds, NULL, NULL, &tv);
		if (retval == -1) {											
			return -1;												
		} else if (retval == 0) {									
			return -1;
		} else {
			if ((len = recv(fd, (bp + offset), count, 0)) <= 0) {
				return -1;
			}
			count -= len;
			offset += len;
		}
	}
	return 0;
}

static int pcap_read_acn(pcap_t *handle, int max_packets, pcap_handler callback, u_char *user) {
	#define HEADER_SIZE (4 * 4)
	unsigned char		packet_header[HEADER_SIZE];
	struct pcap_pkthdr	pcap_header;

	
	acn_start_monitor(handle->fd, handle->snapshot, handle->opt.timeout, handle->opt.promisc, handle->direction);	
	

	handle->bp = packet_header;
	if (acn_read_n_bytes_with_timeout(handle, HEADER_SIZE) == -1) return 0;			

	pcap_header.ts.tv_sec	= ntohl(*(uint32_t *)&packet_header[0]);				
	pcap_header.ts.tv_usec	= ntohl(*(uint32_t *)&packet_header[4]);				
	pcap_header.caplen		= ntohl(*(uint32_t *)&packet_header[8]);				
	pcap_header.len			= ntohl(*(uint32_t *)&packet_header[12]);				

	handle->bp = handle->buffer + handle->offset;									
	if (acn_read_n_bytes_with_timeout(handle, pcap_header.caplen) == -1) return 0;	

	callback(user, &pcap_header, handle->bp);										
	return 1;
}

static int pcap_activate_sita(pcap_t *handle) {
	int		fd;

	if (handle->opt.rfmon) {
		return PCAP_ERROR_RFMON_NOTSUP;
	}

	

	handle->inject_op = pcap_inject_acn;
	handle->setfilter_op = pcap_setfilter_acn;
	handle->setdirection_op = pcap_setdirection_acn;
	handle->set_datalink_op = NULL;	
	handle->getnonblock_op = pcap_getnonblock_fd;
	handle->setnonblock_op = pcap_setnonblock_fd;
	handle->cleanup_op = pcap_cleanup_acn;
	handle->read_op = pcap_read_acn;
	handle->stats_op = pcap_stats_acn;

	fd = acn_open_live(handle->opt.source, handle->errbuf,
	    &handle->linktype);
	if (fd == -1)
		return PCAP_ERROR;
	handle->fd = fd;
	handle->bufsize = handle->snapshot;

	

	handle->buffer	 = malloc(handle->bufsize + handle->offset);
	if (!handle->buffer) {
	        snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			 "malloc: %s", pcap_strerror(errno));
		pcap_cleanup_acn(handle);
		return PCAP_ERROR;
	}

	handle->selectable_fd = handle->fd;

	return 0;
}

pcap_t *pcap_create_interface(const char *device, char *ebuf) {
	pcap_t *p;

	p = pcap_create_common(device, ebuf, 0);
	if (p == NULL)
		return (NULL);

	p->activate_op = pcap_activate_sita;
	return (p);
}
