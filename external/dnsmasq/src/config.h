/* dnsmasq is Copyright (c) 2000-2009 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define VERSION "2.51"

#define FTABSIZ 150 
#define MAX_PROCS 20 
#define CHILD_LIFETIME 150 
#define EDNS_PKTSZ 1280 
#define TIMEOUT 10 
#define FORWARD_TEST 50 
#define FORWARD_TIME 10 
#define RANDOM_SOCKS 64 
#define LEASE_RETRY 60 
#define CACHESIZ 150 
#define MAXLEASES 150 
#define PING_WAIT 3 
#define PING_CACHE_TIME 30 
#define DECLINE_BACKOFF 600 
#define DHCP_PACKET_MAX 16384 
#define SMALLDNAME 40 
#define HOSTSFILE "/etc/hosts"
#define ETHERSFILE "/etc/ethers"
#ifdef __uClinux__
#  define RESOLVFILE "/etc/config/resolv.conf"
#else
#  define RESOLVFILE "/etc/resolv.conf"
#endif
#define RUNFILE "/var/run/dnsmasq.pid"

#ifndef LEASEFILE
#   if defined(__FreeBSD__) || defined (__OpenBSD__) || defined(__DragonFly__) || defined(__NetBSD__)
#      define LEASEFILE "/var/db/dnsmasq.leases"
#   elif defined(__sun__) || defined (__sun)
#      define LEASEFILE "/var/cache/dnsmasq.leases"
#   elif defined(__ANDROID__)
#      define LEASEFILE "/data/misc/dhcp/dnsmasq.leases"
#   else
#      define LEASEFILE "/var/lib/misc/dnsmasq.leases"
#   endif
#endif

#ifndef CONFFILE
#   if defined(__FreeBSD__)
#      define CONFFILE "/usr/local/etc/dnsmasq.conf"
#   else
#      define CONFFILE "/etc/dnsmasq.conf"
#   endif
#endif

#define DEFLEASE 3600 
#define CHUSER "nobody"
#define CHGRP "dip"
#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_ALTPORT 1067
#define DHCP_CLIENT_ALTPORT 1068
#define TFTP_PORT 69
#define TFTP_MAX_CONNECTIONS 50 
#define LOG_MAX 5 
#define RANDFILE "/dev/urandom"
#define DAD_WAIT 20 

#define DNSMASQ_SERVICE "uk.org.thekelleys.dnsmasq"
#define DNSMASQ_PATH "/uk/org/thekelleys/dnsmasq"


#ifndef T_SIG
#  define T_SIG 24
#endif

#ifndef T_SRV
#  define T_SRV 33
#endif

#ifndef T_OPT
#  define T_OPT 41
#endif

#ifndef T_TKEY
#  define T_TKEY 249
#endif

#ifndef T_TSIG
#  define T_TSIG 250
#endif


/* Follows system specific switches. If you run on a 
   new system, you may want to edit these. 
   May replace this with Autoconf one day. 

HAVE_LINUX_NETWORK
HAVE_BSD_NETWORK
HAVE_SOLARIS_NETWORK
   define exactly one of these to alter interaction with kernel networking.

HAVE_BROKEN_RTC
   define this on embedded systems which don't have an RTC
   which keeps time over reboots. Causes dnsmasq to use uptime
   for timing, and keep lease lengths rather than expiry times
   in its leases file. This also make dnsmasq "flash disk friendly".
   Normally, dnsmasq tries very hard to keep the on-disk leases file
   up-to-date: rewriting it after every renewal.  When HAVE_BROKEN_RTC 
   is in effect, the lease file is only written when a new lease is 
   created, or an old one destroyed. (Because those are the only times 
   it changes.) This vastly reduces the number of file writes, and makes
   it viable to keep the lease file on a flash filesystem.
   NOTE: when enabling or disabling this, be sure to delete any old
   leases file, otherwise dnsmasq may get very confused.

HAVE_TFTP
   define this to get dnsmasq's built-in TFTP server.

HAVE_DHCP
   define this to get dnsmasq's DHCP server.

HAVE_SCRIPT
   define this to get the ability to call scripts on lease-change

HAVE_GETOPT_LONG
   define this if you have GNU libc or GNU getopt. 

HAVE_ARC4RANDOM
   define this if you have arc4random() to get better security from DNS spoofs
   by using really random ids (OpenBSD) 

HAVE_SOCKADDR_SA_LEN
   define this if struct sockaddr has sa_len field (*BSD) 

HAVE_DBUS
   Define this if you want to link against libdbus, and have dnsmasq
   define some methods to allow (re)configuration of the upstream DNS 
   servers via DBus.

NOTES:
   For Linux you should define 
      HAVE_LINUX_NETWORK
      HAVE_GETOPT_LONG
  you should NOT define 
      HAVE_ARC4RANDOM
      HAVE_SOCKADDR_SA_LEN

   For *BSD systems you should define 
     HAVE_BSD_NETWORK
     HAVE_SOCKADDR_SA_LEN
   and you MAY define  
     HAVE_ARC4RANDOM - OpenBSD and FreeBSD and NetBSD version 2.0 or later
     HAVE_GETOPT_LONG - NetBSD, later FreeBSD 
                       (FreeBSD and OpenBSD only if you link GNU getopt) 

*/

#define HAVE_DHCP
#define HAVE_TFTP
#define HAVE_SCRIPT

#ifdef NO_TFTP
#undef HAVE_TFTP
#endif

#ifdef NO_DHCP
#undef HAVE_DHCP
#endif

#ifdef NO_SCRIPT
#undef HAVE_SCRIPT
#endif




#if defined(__uClinux__)
#define HAVE_LINUX_NETWORK
#define HAVE_GETOPT_LONG
#undef HAVE_ARC4RANDOM
#undef HAVE_SOCKADDR_SA_LEN
#define NO_FORK

#elif defined(__UCLIBC__)
#define HAVE_LINUX_NETWORK
#if defined(__UCLIBC_HAS_GNU_GETOPT__) || \
   ((__UCLIBC_MAJOR__==0) && (__UCLIBC_MINOR__==9) && (__UCLIBC_SUBLEVEL__<21))
#    define HAVE_GETOPT_LONG
#endif
#undef HAVE_ARC4RANDOM
#undef HAVE_SOCKADDR_SA_LEN
#if !defined(__ARCH_HAS_MMU__) && !defined(__UCLIBC_HAS_MMU__)
#  define NO_FORK
#endif
#if defined(__UCLIBC_HAS_IPV6__)
#  ifndef IPV6_V6ONLY
#    define IPV6_V6ONLY 26
#  endif
#endif

#elif defined(__linux__)
#define HAVE_LINUX_NETWORK
#define HAVE_GETOPT_LONG
#undef HAVE_ARC4RANDOM
#undef HAVE_SOCKADDR_SA_LEN

#elif defined(__FreeBSD__) || \
      defined(__OpenBSD__) || \
      defined(__DragonFly__) || \
      defined (__FreeBSD_kernel__)
#define HAVE_BSD_NETWORK
#if defined(optional_argument) && defined(required_argument)
#   define HAVE_GETOPT_LONG
#endif
#if !defined (__FreeBSD_kernel__)
#   define HAVE_ARC4RANDOM
#endif
#define HAVE_SOCKADDR_SA_LEN

#elif defined(__APPLE__)
#define HAVE_BSD_NETWORK
#undef HAVE_GETOPT_LONG
#define HAVE_ARC4RANDOM
#define HAVE_SOCKADDR_SA_LEN
#define _BSD_SOCKLEN_T_
 
#elif defined(__NetBSD__)
#define HAVE_BSD_NETWORK
#define HAVE_GETOPT_LONG
#undef HAVE_ARC4RANDOM
#define HAVE_SOCKADDR_SA_LEN

#elif defined(__sun) || defined(__sun__)
#define HAVE_SOLARIS_NETWORK
#define HAVE_GETOPT_LONG
#undef HAVE_ARC4RANDOM
#undef HAVE_SOCKADDR_SA_LEN
#define _XPG4_2
#define __EXTENSIONS__
#define ETHER_ADDR_LEN 6 
 
#endif


#if defined(INET6_ADDRSTRLEN) && defined(IPV6_V6ONLY) && !defined(NO_IPV6)
#  define HAVE_IPV6
#  define ADDRSTRLEN INET6_ADDRSTRLEN
#  if defined(SOL_IPV6)
#    define IPV6_LEVEL SOL_IPV6
#  else
#    define IPV6_LEVEL IPPROTO_IPV6
#  endif
#elif defined(INET_ADDRSTRLEN)
#  undef HAVE_IPV6
#  define ADDRSTRLEN INET_ADDRSTRLEN
#else
#  undef HAVE_IPV6
#  define ADDRSTRLEN 16 
#endif

#ifdef NOFORK
#  undef HAVE_SCRIPT
#endif

