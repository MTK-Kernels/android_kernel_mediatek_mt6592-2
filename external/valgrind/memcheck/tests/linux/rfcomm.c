#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../../memcheck.h"

typedef struct {
   uint8_t b[6];
} __attribute__((packed)) vui_bdaddr_t;

struct vui_sockaddr_rc {
	sa_family_t	rc_family;
	vui_bdaddr_t	rc_bdaddr;
	uint8_t		rc_channel;
};

#define VUI_AF_BLUETOOTH 31
#define VUI_BTPROTO_RFCOMM 3

#define VUI_BDADDR_ANY (&(vui_bdaddr_t) {{0, 0, 0, 0, 0, 0}})

int
main (int argc, char **argv)
{
  int nSocket;

  nSocket = socket(VUI_AF_BLUETOOTH, SOCK_STREAM, VUI_BTPROTO_RFCOMM);

  if (nSocket < 0)
    {
      
      return 1;
    }

  struct vui_sockaddr_rc aAddr;

  
  
  
  aAddr.rc_family = VUI_AF_BLUETOOTH;
  aAddr.rc_bdaddr = *VUI_BDADDR_ANY;
  aAddr.rc_channel = 5;
  VALGRIND_MAKE_MEM_UNDEFINED(&aAddr, sizeof(aAddr));
  
  


  

  
  bind(nSocket, (struct sockaddr *) &aAddr, sizeof(aAddr));

  
  aAddr.rc_family = 12345;
  
  VALGRIND_MAKE_MEM_UNDEFINED(&aAddr, sizeof(aAddr));
  bind(nSocket, (struct sockaddr *) &aAddr, sizeof(aAddr));

  aAddr.rc_family = VUI_AF_BLUETOOTH;
  
  bind(nSocket, (struct sockaddr *) &aAddr, sizeof(aAddr));

  aAddr.rc_bdaddr = *VUI_BDADDR_ANY;
  
  bind(nSocket, (struct sockaddr *) &aAddr, sizeof(aAddr));

  aAddr.rc_channel = 5;
  
  bind(nSocket, (struct sockaddr *) &aAddr, sizeof(aAddr));

  return 0;
}
