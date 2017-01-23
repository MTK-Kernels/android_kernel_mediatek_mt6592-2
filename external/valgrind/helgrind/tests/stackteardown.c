#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "../../config.h"

#define VG_STRINGIFZ(__str)  #__str
#define VG_STRINGIFY(__str)  VG_STRINGIFZ(__str)

extern void _exit_with_stack_teardown(void*, size_t);


#if defined(VGP_x86_linux)
asm("\n"
    ".text\n"
    "\t.globl _exit_with_stack_teardown\n"
    "\t.type  _exit_with_stack_teardown,@function\n"
    "_exit_with_stack_teardown:\n"
    
    "\tmov 4(%esp), %ebx\n"             
    "\tmov 8(%esp), %ecx\n"             
    "\tmov $"VG_STRINGIFY(__NR_munmap)", %eax\n"
    "\tint $0x80\n"
    

    "\tmov $0, %ebx\n"                  
    "\tmovl $"VG_STRINGIFY(__NR_exit)", %eax\n"
    "\tint $0x80\n");
    

#elif defined(VGP_amd64_linux)
asm("\n"
    ".text\n"
    "\t.globl _exit_with_stack_teardown\n"
    "\t.type  _exit_with_stack_teardown,@function\n"
    "_exit_with_stack_teardown:\n"
    "\tmov $"VG_STRINGIFY(__NR_munmap)", %eax\n"
    "\tsyscall\n"
    
    "\tmov $0, %rdi\n"
    "\tmov $"VG_STRINGIFY(__NR_exit)", %eax\n"
    "\tsyscall\n");
  

#elif defined(VGP_arm_linux)
asm("\n"
    ".text\n"
    "\t.globl _exit_with_stack_teardown\n"
    "\t.type  _exit_with_stack_teardown,%function\n"
    "_exit_with_stack_teardown:\n"
    "\tldr r7, ="VG_STRINGIFY(__NR_munmap)"\n"
    "\tswi #0\n"
    

    "\tmov r0, #0\n"
    "\tldr r7, ="VG_STRINGIFY(__NR_exit)"\n"
    "\tswi #0\n");
  

#else
void _exit_with_stack_teardown(void*stack, size_t sz)
{
   
   
}

#endif
static void *stack;
static size_t sz = 64 * 1024;

void* child_fn ( void* arg )
{
  int r;
  r= pthread_detach( pthread_self() ); assert(!r);
  _exit_with_stack_teardown(stack, sz);
  return NULL;
}

int main ( void )
{
   int r;
   pthread_attr_t attr;
   pthread_t child;

   r = pthread_attr_init(&attr); assert(!r);
# if !defined(VGO_darwin)
   stack = mmap(NULL, sz, PROT_READ|PROT_WRITE,  MAP_PRIVATE | MAP_ANONYMOUS,
                -1, 0);
# else
   stack = mmap(NULL, sz, PROT_READ|PROT_WRITE,  MAP_PRIVATE | MAP_ANON,
                -1, 0);
# endif
   assert(stack != (void *)-1);
   r = pthread_attr_setstack(&attr, stack, sz);
   r = pthread_create(&child, &attr, child_fn, NULL); assert(!r);
   sleep(1);

   return 0;
}
