#if defined(linux)


#include <stdio.h>            
#include <unistd.h>           
#include <sys/syscall.h>      
#include <sys/types.h>        
#include <linux/capability.h> 


int main()
{
  struct __user_cap_header_struct h;
  struct __user_cap_data_struct d;
  int syscall_result;

  if (getuid() == 0)
    fprintf(stderr, "Running as root\n");
  h.version = _LINUX_CAPABILITY_VERSION;
  h.pid = 0;
  syscall_result = syscall(__NR_capget, &h, &d);
  if (syscall_result >= 0)
  {
    fprintf(stderr,
            "capget result:\n"
            "effective   %#x\n"
            "permitted   %#x\n"
            "inheritable %#x\n",
            d.effective,
            d.permitted,
            d.inheritable);
  }
  else
  {
    perror("capget");
  }
  return 0;
}


#else


#include <stdio.h>

int main()
{
  fprintf(stderr, "This program is Linux-specific\n");
  return 0;
}


#endif
