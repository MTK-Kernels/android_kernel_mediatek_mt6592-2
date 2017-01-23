#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main (int argc, char **argv)
{
  fprintf(stderr, "starting ...\n");

  
  if (argc > 1)
    {
      
      if (strcmp (argv[1], "exit") == 0)
	{
	  fprintf(stderr, "exiting ...\n");
	  exit (1);
	}

      
      if (strcmp (argv[1], "abort") == 0)
	{
	  fprintf(stderr, "aborting ...\n");
	  kill(getpid(), SIGABRT);
	}
    }

  
  fprintf(stderr, "returning ...\n");
  return 0;
}
