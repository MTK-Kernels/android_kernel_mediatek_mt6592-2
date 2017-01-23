
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

static const struct timespec bip = { 0, 1000000000 / 5 };   

static void handler(int sig)
{
}

static void do_kill(int pid, int sig)
{
   int status;
   int killer;
   int ret;

   killer = vfork();
   if (killer == -1) {
      perror("killer/vfork");
      exit(1);
   }

   
   if (killer == 0) {
      char sigbuf[20];
      char pidbuf[20];
      sprintf(sigbuf, "-%d", sig);
      sprintf(pidbuf, "%d", pid);
      execl("/bin/kill", "kill", sigbuf, pidbuf, NULL);
      perror("exec failed");
      exit(1);
   }

   
   do 
      ret = waitpid(killer, &status, 0);
   while (ret == -1 && errno == EINTR);

   if (ret != killer) {
      perror("kill/waitpid");
      exit(1);
   }

   if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      fprintf(stderr, "kill %d failed status=%s %d\n", killer, 
             WIFEXITED(status) ? "exit" : "signal", 
             WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status));
      exit(1);
   }
}

static void test(int block, int caughtsig, int fatalsig)
{
   int pid;
   int status;
   int i;

   fprintf(stderr, "testing: blocking=%d caught=%d fatal=%d... ",
      block, caughtsig, fatalsig);

   pid = fork();
   if (pid == -1) {
      perror("fork");
      exit(1);
   }

   
   
   
   
   
   if (pid == 0) {
      signal(caughtsig, handler);
      alarm(10);

      for (;;)
         if (block) {
            pause();
         }
   }

   
   nanosleep(&bip, 0);           

   for (i = 0; i < 5; i++) {
      do_kill(pid, caughtsig);   
      nanosleep(&bip, 0);
      do_kill(pid, caughtsig);   
      do_kill(pid, caughtsig);   
   }

   nanosleep(&bip, 0);

   do_kill(pid, fatalsig);       
   
   
   if (waitpid(pid, &status, 0) != pid) {
      fprintf(stderr, "FAILED: waitpid failed: %s\n", strerror(errno));

   } else if (!WIFSIGNALED(status) || WTERMSIG(status) != fatalsig) {
      fprintf(stderr, "FAILED: child exited with unexpected status %s %d\n",
             WIFEXITED(status) ? "exit" : "signal", 
             WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status));

   } else {
      fprintf(stderr, "PASSED\n");
   }
}

int main()
{
   test(0, SIGSEGV, SIGBUS);
   test(0, SIGSEGV, SIGHUP);
   test(0, SIGUSR1, SIGBUS);
   test(0, SIGUSR1, SIGHUP);
   test(1, SIGSEGV, SIGBUS);
   test(1, SIGSEGV, SIGHUP);
   test(1, SIGUSR1, SIGBUS);
   test(1, SIGUSR1, SIGHUP);

   return 0;
}
