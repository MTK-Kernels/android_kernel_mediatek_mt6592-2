/*
 * Copyright (C) Andrew Tridgell 1995-1999
 *
 * This software may be distributed either under the terms of the
 * BSD-style license that accompanies tcpdump or the GNU GPL version 2
 * or later
 */

#define SMBMIN(a,b) ((a)<(b)?(a):(b))

#define SMBmkdir      0x00   
#define SMBrmdir      0x01   
#define SMBopen       0x02   
#define SMBcreate     0x03   
#define SMBclose      0x04   
#define SMBflush      0x05   
#define SMBunlink     0x06   
#define SMBmv         0x07   
#define SMBgetatr     0x08   
#define SMBsetatr     0x09   
#define SMBread       0x0A   
#define SMBwrite      0x0B   
#define SMBlock       0x0C   
#define SMBunlock     0x0D   
#define SMBctemp      0x0E   
#define SMBmknew      0x0F   
#define SMBchkpth     0x10   
#define SMBexit       0x11   
#define SMBlseek      0x12   
#define SMBtcon       0x70   
#define SMBtconX      0x75   
#define SMBtdis       0x71   
#define SMBnegprot    0x72   
#define SMBdskattr    0x80   
#define SMBsearch     0x81   
#define SMBsplopen    0xC0   
#define SMBsplwr      0xC1   
#define SMBsplclose   0xC2   
#define SMBsplretq    0xC3   
#define SMBsends      0xD0   
#define SMBsendb      0xD1   
#define SMBfwdname    0xD2   
#define SMBcancelf    0xD3   
#define SMBgetmac     0xD4   
#define SMBsendstrt   0xD5   
#define SMBsendend    0xD6   
#define SMBsendtxt    0xD7   

#define SMBlockread	  0x13   
#define SMBwriteunlock 0x14 
#define SMBreadbraw   0x1a  
#define SMBwritebraw  0x1d  
#define SMBwritec     0x20  
#define SMBwriteclose 0x2c  

#define SMBreadBraw      0x1A   
#define SMBreadBmpx      0x1B   
#define SMBreadBs        0x1C   
#define SMBwriteBraw     0x1D   
#define SMBwriteBmpx     0x1E   
#define SMBwriteBs       0x1F   
#define SMBwriteC        0x20   
#define SMBsetattrE      0x22   
#define SMBgetattrE      0x23   
#define SMBlockingX      0x24   
#define SMBtrans         0x25   
#define SMBtranss        0x26   
#define SMBioctl         0x27   
#define SMBioctls        0x28   
#define SMBcopy          0x29   
#define SMBmove          0x2A   
#define SMBecho          0x2B   
#define SMBopenX         0x2D   
#define SMBreadX         0x2E   
#define SMBwriteX        0x2F   
#define SMBsesssetupX    0x73   
#define SMBffirst        0x82   
#define SMBfunique       0x83   
#define SMBfclose        0x84   
#define SMBinvalid       0xFE   

#define SMBtrans2        0x32   
#define SMBtranss2       0x33   
#define SMBfindclose     0x34   
#define SMBfindnclose    0x35   
#define SMBulogoffX      0x74   

#define SMBnttrans       0xA0   
#define SMBnttranss      0xA1   
#define SMBntcreateX     0xA2   
#define SMBntcancel      0xA4   

#define pSETDIR '\377'


#define TRANSACT2_OPEN          0
#define TRANSACT2_FINDFIRST     1
#define TRANSACT2_FINDNEXT      2
#define TRANSACT2_QFSINFO       3
#define TRANSACT2_SETFSINFO     4
#define TRANSACT2_QPATHINFO     5
#define TRANSACT2_SETPATHINFO   6
#define TRANSACT2_QFILEINFO     7
#define TRANSACT2_SETFILEINFO   8
#define TRANSACT2_FSCTL         9
#define TRANSACT2_IOCTL           10
#define TRANSACT2_FINDNOTIFYFIRST 11
#define TRANSACT2_FINDNOTIFYNEXT  12
#define TRANSACT2_MKDIR           13

#define PTR_DIFF(p1, p2) ((size_t)(((char *)(p1)) - (char *)(p2)))

const u_char *smb_fdata(const u_char *, const char *, const u_char *, int);
