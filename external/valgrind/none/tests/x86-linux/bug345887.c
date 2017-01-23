
__attribute__((noinline))
static void inner(void)
{
   
   __asm__ __volatile__(
      "movl $0x101, %%eax\n"
      "movl $0x102, %%ebx\n"
      "movl $0x103, %%ecx\n"
      "movl $0x104, %%edx\n"
      "movl $0x105, %%esi\n"
      "movl $0x106, %%edi\n"
      
      "movl $0x108, %%esp\n"
      "movl $0x1234, (%%eax)\n"  
      "ud2"                      
      : 
      : 
      : "memory", "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "%esp");
}

__attribute__((noinline))
static void outer(void)
{
   inner();
}

int main(int argc, const char *argv[])
{
   outer();
   return 0;
}
