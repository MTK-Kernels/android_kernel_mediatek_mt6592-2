
#ifndef __PCAP_DOS_H
#define __PCAP_DOS_H

#ifdef __DJGPP__
#include <pc.h>    
#else
#include <conio.h>
#endif

typedef int            BOOL;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned long  DWORD;
typedef BYTE           ETHER[6];

#define ETH_ALEN       sizeof(ETHER)   
#define ETH_HLEN       (2*ETH_ALEN+2)  
#define ETH_MTU        1500
#define ETH_MIN        60
#define ETH_MAX        (ETH_MTU+ETH_HLEN)

#ifndef TRUE
  #define TRUE   1
  #define FALSE  0
#endif

#define PHARLAP  1
#define DJGPP    2
#define DOS4GW   4

#ifdef __DJGPP__
  #undef  DOSX
  #define DOSX DJGPP
#endif

#ifdef __WATCOMC__
  #undef  DOSX
  #define DOSX DOS4GW
#endif

#ifdef __HIGHC__
  #include <pharlap.h>
  #undef  DOSX
  #define DOSX PHARLAP
  #define inline
#else
  typedef unsigned int UINT;
#endif


#if defined(__GNUC__) || defined(__HIGHC__)
  typedef unsigned long long  uint64;
  typedef unsigned long long  QWORD;
#endif

#if defined(__WATCOMC__)
  typedef unsigned __int64  uint64;
  typedef unsigned __int64  QWORD;
#endif

#define ARGSUSED(x)  (void) x

#if defined (__SMALL__) || defined(__LARGE__)
  #define DOSX 0

#elif !defined(DOSX)
  #error DOSX not defined; 1 = PharLap, 2 = djgpp, 4 = DOS4GW
#endif

#ifdef __HIGHC__
#define min(a,b) _min(a,b)
#define max(a,b) _max(a,b)
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) ((a) < (b) ? (b) : (a))
#endif

#if !defined(_U_) && defined(__GNUC__)
#define _U_  __attribute__((unused))
#endif

#ifndef _U_
#define _U_
#endif

#if defined(USE_32BIT_DRIVERS)
  #include "msdos/pm_drvr/lock.h"

  #ifndef RECEIVE_QUEUE_SIZE
  #define RECEIVE_QUEUE_SIZE  60
  #endif

  #ifndef RECEIVE_BUF_SIZE
  #define RECEIVE_BUF_SIZE   (ETH_MAX+20)
  #endif

  extern struct device el2_dev     LOCKED_VAR;  
  extern struct device el3_dev     LOCKED_VAR;  
  extern struct device tc59_dev    LOCKED_VAR;  
  extern struct device tc515_dev   LOCKED_VAR;
  extern struct device tc90x_dev   LOCKED_VAR;
  extern struct device tc90bcx_dev LOCKED_VAR;
  extern struct device wd_dev      LOCKED_VAR;
  extern struct device ne_dev      LOCKED_VAR;
  extern struct device acct_dev    LOCKED_VAR;
  extern struct device cs89_dev    LOCKED_VAR;
  extern struct device rtl8139_dev LOCKED_VAR;

  struct rx_ringbuf {
         volatile int in_index;   
         int          out_index;  
         int          elem_size;  
         int          num_elem;   
         char        *buf_start;  
       };

  struct rx_elem {
         DWORD size;              
         BYTE  data[ETH_MAX+10];  
       };                         

  extern BYTE *get_rxbuf     (int len) LOCKED_FUNC;
  extern int   peek_rxbuf    (BYTE **buf);
  extern int   release_rxbuf (BYTE  *buf);

#else
  #define LOCKED_VAR
  #define LOCKED_FUNC

  struct device {
         const char *name;
         const char *long_name;
         DWORD  base_addr;      
         int    irq;            
         int    dma;            
         DWORD  mem_start;      
         DWORD  mem_end;        
         DWORD  rmem_start;     
         DWORD  rmem_end;       

         struct device *next;   

         
         int   (*probe)(struct device *dev);
         int   (*open) (struct device *dev);
         void  (*close)(struct device *dev);
         int   (*xmit) (struct device *dev, const void *buf, int len);
         void *(*get_stats)(struct device *dev);
         void  (*set_multicast_list)(struct device *dev);

         
         int   (*copy_rx_buf) (BYTE *buf, int max); 
         BYTE *(*get_rx_buf) (int len);             
         int   (*peek_rx_buf) (BYTE **buf);         
         int   (*release_rx_buf) (BYTE *buf);       

         WORD   flags;          
         void  *priv;           
       };

  typedef struct net_device_stats {
          DWORD  rx_packets;            
          DWORD  tx_packets;            
          DWORD  rx_bytes;              
          DWORD  tx_bytes;              
          DWORD  rx_errors;             
          DWORD  tx_errors;             
          DWORD  rx_dropped;            
          DWORD  tx_dropped;            
          DWORD  multicast;             

          
          DWORD  rx_length_errors;
          DWORD  rx_over_errors;        
          DWORD  rx_osize_errors;       
          DWORD  rx_crc_errors;         
          DWORD  rx_frame_errors;       
          DWORD  rx_fifo_errors;        
          DWORD  rx_missed_errors;      

          
          DWORD  tx_aborted_errors;
          DWORD  tx_carrier_errors;
          DWORD  tx_fifo_errors;
          DWORD  tx_heartbeat_errors;
          DWORD  tx_window_errors;
          DWORD  tx_collisions;
          DWORD  tx_jabbers;
        } NET_STATS;
#endif

extern struct device       *active_dev  LOCKED_VAR;
extern const struct device *dev_base    LOCKED_VAR;
extern struct device       *probed_dev;

extern int pcap_pkt_debug;

extern void _w32_os_yield (void); 

#ifdef NDEBUG
  #define PCAP_ASSERT(x) ((void)0)

#else
  void pcap_assert (const char *what, const char *file, unsigned line);  

  #define PCAP_ASSERT(x) do { \
                           if (!(x)) \
                              pcap_assert (#x, __FILE__, __LINE__); \
                         } while (0)
#endif

#endif  
