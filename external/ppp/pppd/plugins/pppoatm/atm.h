 
/* Written 1995-2000 by Werner Almesberger, EPFL-LRC/ICA */
 

#ifndef _ATM_H
#define _ATM_H

#include <stdint.h>
#include <sys/socket.h>
#include <linux/atm.h>



#ifndef AF_ATMPVC
#define AF_ATMPVC	8
#endif

#ifndef AF_ATMSVC
#define AF_ATMSVC	20
#endif

#ifndef PF_ATMPVC
#define PF_ATMPVC	AF_ATMPVC
#endif

#ifndef PF_ATMSVC
#define PF_ATMSVC	AF_ATMSVC
#endif

#ifndef SOL_ATM
#define SOL_ATM		264
#endif

#ifndef SOL_AAL
#define SOL_AAL		265
#endif


#define HOSTS_ATM "/etc/hosts.atm"

#define T2A_PVC		  1	
#define T2A_SVC		  2	
#define T2A_UNSPEC	  4	
#define T2A_WILDCARD	  8	
#define T2A_NNI		 16	
#define T2A_NAME	 32	
#define T2A_REMOTE	 64	
#define T2A_LOCAL	128	

#define A2T_PRETTY	 1	
#define A2T_NAME	 2	
#define A2T_REMOTE	 4	
#define A2T_LOCAL	 8	

#define AXE_WILDCARD	 1	
#define AXE_PRVOPT	 2	

#define T2Q_DEFAULTS	 1	

#define T2S_NAME	 1	
#define T2S_LOCAL	 2	

#define S2T_NAME	 1	
#define S2T_LOCAL	 2	

#define SXE_COMPATIBLE	 1	
#define SXE_NEGOTIATION	 2	
#define SXE_RESULT	 4	

#define MAX_ATM_ADDR_LEN (2*ATM_ESA_LEN+ATM_E164_LEN+5)
				
#define MAX_ATM_NAME_LEN 256	
#define MAX_ATM_QOS_LEN 116	
#define MAX_ATM_SAP_LEN	255	


int text2atm(const char *text,struct sockaddr *addr,int length,int flags);
int atm2text(char *buffer,int length,const struct sockaddr *addr,int flags);
int atm_equal(const struct sockaddr *a,const struct sockaddr *b,int len,
  int flags);

int sdu2cell(int s,int sizes,const int *sdu_size,int *num_sdu);

int text2qos(const char *text,struct atm_qos *qos,int flags);
int qos2text(char *buffer,int length,const struct atm_qos *qos,int flags);
int qos_equal(const struct atm_qos *a,const struct atm_qos *b);

int text2sap(const char *text,struct atm_sap *sap,int flags);
int sap2text(char *buffer,int length,const struct atm_sap *sap,int flags);
int sap_equal(const struct atm_sap *a,const struct atm_sap *b,int flags,...);

int __t2q_get_rate(const char **text,int up);
int __atmlib_fetch(const char **pos,...); 

#endif
