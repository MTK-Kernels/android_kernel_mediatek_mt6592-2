
/* Written 1996,1998 by Werner Almesberger, EPFL-LRC/ICA */


#ifndef _ATMRES_H
#define _ATMRES_H

#include <arpa/nameser.h>
#include <linux/atm.h>



#ifndef T_ATMA
#define T_ATMA		34
#endif
#ifndef ATMA_AESA
#define ATMA_AESA	0
#endif
#ifndef ATMA_E164
#define ATMA_E164	1
#endif


#define TRY_OTHER -2
#define FATAL	  -1 


int ans_byname(const char *text,struct sockaddr_atmsvc *addr,int length,
  int flags);
int ans_byaddr(char *buffer,int length,const struct sockaddr_atmsvc *addr,
  int flags);

#endif
