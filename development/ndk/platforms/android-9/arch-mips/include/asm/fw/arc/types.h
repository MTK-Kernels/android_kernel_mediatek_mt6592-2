/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef _ASM_ARC_TYPES_H
#define _ASM_ARC_TYPES_H
typedef CHAR *PCHAR;
typedef SHORT *PSHORT;
typedef LARGE_INTEGER *PLARGE_INTEGER;
typedef LONG *PLONG;
typedef UCHAR *PUCHAR;
typedef USHORT *PUSHORT;
typedef ULONG *PULONG;
typedef VOID *PVOID;
typedef struct {
 USHORT CursorXPosition;
 USHORT CursorYPosition;
 USHORT CursorMaxXPosition;
 USHORT CursorMaxYPosition;
 USHORT ForegroundColor;
 USHORT BackgroundColor;
 UCHAR HighIntensity;
 UCHAR Underscored;
 UCHAR ReverseVideo;
} DISPLAY_STATUS;
#endif
