
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>

int main(void)
{
  pid_t pid;

  pid = fork ();


  printf("%s", pid==0 ? "X" : "XX");

  if (pid != 0)
     waitpid(pid, NULL, 0);

  return 0;
}
