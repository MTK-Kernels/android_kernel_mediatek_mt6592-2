#ifndef CBCP_H
#define CBCP_H

typedef struct cbcp_state {
    int    us_unit;	
    u_char us_id;		
    u_char us_allowed;
    int    us_type;
    char   *us_number;    
} cbcp_state;

extern cbcp_state cbcp[];

extern struct protent cbcp_protent;

#define CBCP_MINLEN 4

#define CBCP_REQ    1
#define CBCP_RESP   2
#define CBCP_ACK    3

#define CB_CONF_NO     1
#define CB_CONF_USER   2
#define CB_CONF_ADMIN  3
#define CB_CONF_LIST   4
#endif
