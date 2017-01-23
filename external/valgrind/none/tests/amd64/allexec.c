#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#define S(...) (fprintf(stdout, __VA_ARGS__),fflush(stdout))
#define FORKEXECWAIT(exec_call) do { \
      int status;\
      pid_t child = fork(); \
      if (child == 0) {exec_call; perror ("exec failed");} \
      else if (child == -1) perror ("cannot fork\n"); \
      else if (child != wait (&status)) perror ("error waiting child"); \
      else S("child exited\n"); \
   } while (0)

void test_allexec (char *exec)
{
   FORKEXECWAIT (execlp(exec, exec, NULL));
   FORKEXECWAIT (execlp(exec, exec, "constant_arg1", "constant_arg2", NULL));
   FORKEXECWAIT (execve(exec, NULL, environ));
}


int main(int argc, char **argv, char **envp)
{
   if ( (argc == 2) && (strcmp (argv[1], "exec") == 0)) {
      S("%s will exec ./allexec32\n", argv[0]);
      test_allexec ("./allexec32");
      S("%s will exec ./allexec64\n", argv[0]);
      test_allexec ("./allexec64");
   } else {
      int i;
      S("program exec-ed:");
      for (i = 0; i < argc; i++) S(" %s", argv[i]);
      S("\n");
   }
   return 0;
}
