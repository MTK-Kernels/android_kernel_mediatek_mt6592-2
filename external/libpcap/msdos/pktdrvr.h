#ifndef __PKTDRVR_H
#define __PKTDRVR_H

#define PUBLIC
#define LOCAL        static

#define RX_BUF_SIZE  ETH_MTU   
#define TX_BUF_SIZE  ETH_MTU   

#ifdef __HIGHC__
#pragma Off(Align_members)
#else
#pragma pack(1)
#endif

typedef enum  {                
        PD_ETHER      = 1,
        PD_PRONET10   = 2,
        PD_IEEE8025   = 3,
        PD_OMNINET    = 4,
        PD_APPLETALK  = 5,
        PD_SLIP       = 6,
        PD_STARTLAN   = 7,
        PD_ARCNET     = 8,
        PD_AX25       = 9,
        PD_KISS       = 10,
        PD_IEEE8023_2 = 11,
        PD_FDDI8022   = 12,
        PD_X25        = 13,
        PD_LANstar    = 14,
        PD_PPP        = 18
      } PKT_CLASS;

typedef enum  {             
        PDRX_OFF    = 1,    
        PDRX_DIRECT,        
        PDRX_BROADCAST,     
        PDRX_MULTICAST1,    
        PDRX_MULTICAST2,    
        PDRX_ALL_PACKETS,   
      } PKT_RX_MODE;

typedef struct {
        char type[8];
        char len;
      } PKT_FRAME;


typedef struct {
        BYTE  class;        
        BYTE  number;       
        WORD  type;         
        BYTE  funcs;        
        WORD  intr;         
        WORD  handle;       
        BYTE  name [15];    
        BOOL  quiet;        
        const char *error;  
        BYTE  majVer;       
        BYTE  minVer;       
        BYTE  dummyLen;     
        WORD  MAClength;    
        WORD  MTU;          
        WORD  multicast;    
        WORD  rcvrBuffers;  
        WORD  UMTbufs;      
        WORD  postEOIintr;  
      } PKT_INFO;

#define PKT_PARAM_SIZE  14    


typedef struct {
        DWORD inPackets;          
        DWORD outPackets;         
        DWORD inBytes;            
        DWORD outBytes;           
        DWORD inErrors;           
        DWORD outErrors;          
        DWORD lost;               
      } PKT_STAT;
                   

typedef struct {
        ETHER destin;
        ETHER source;
        WORD  proto;
        BYTE  data [TX_BUF_SIZE];
      } TX_ELEMENT;

typedef struct {
        WORD  firstCount;         
        WORD  secondCount;        
        WORD  handle;             
        ETHER destin;             
        ETHER source;             
        WORD  proto;              
        BYTE  data [RX_BUF_SIZE];
      } RX_ELEMENT;


#ifdef __HIGHC__
#pragma pop(Align_members)
#else
#pragma pack()
#endif



#ifdef __cplusplus
extern "C" {
#endif

extern PKT_STAT    pktStat;     
extern PKT_INFO    pktInfo;     

extern PKT_RX_MODE receiveMode;
extern ETHER       myAddress, ethBroadcast;

extern BOOL  PktInitDriver (PKT_RX_MODE mode);
extern BOOL  PktExitDriver (void);

extern const char *PktGetErrorStr    (int errNum);
extern const char *PktGetClassName   (WORD class);
extern const char *PktRXmodeStr      (PKT_RX_MODE mode);
extern BOOL        PktSearchDriver   (void);
extern int         PktReceive        (BYTE *buf, int max);
extern BOOL        PktTransmit       (const void *eth, int len);
extern DWORD       PktRxDropped      (void);
extern BOOL        PktReleaseHandle  (WORD handle);
extern BOOL        PktTerminHandle   (WORD handle);
extern BOOL        PktResetInterface (WORD handle);
extern BOOL        PktSetReceiverMode(PKT_RX_MODE  mode);
extern BOOL        PktGetReceiverMode(PKT_RX_MODE *mode);
extern BOOL        PktGetStatistics  (WORD handle);
extern BOOL        PktSessStatistics (WORD handle);
extern BOOL        PktResetStatistics(WORD handle);
extern BOOL        PktGetAddress     (ETHER *addr);
extern BOOL        PktSetAddress     (const ETHER *addr);
extern BOOL        PktGetDriverInfo  (void);
extern BOOL        PktGetDriverParam (void);
extern void        PktQueueBusy      (BOOL busy);
extern WORD        PktBuffersUsed    (void);

#ifdef __cplusplus
}
#endif

#endif 

