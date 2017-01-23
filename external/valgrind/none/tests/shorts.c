
#include <stdio.h>

typedef struct { short ot; short ob; short nt; short nb; } Stuff;

void PaintThumb( Stuff* w )
{
    short oldtop = w->ot;
    short oldbot = w->ob;
    short newtop = w->nt;
    short newbot = w->nb;

        if (newtop < oldtop) { fprintf(stderr,"case1\n");
	
	}
	if (newtop > oldtop) { fprintf(stderr,"case2\n");
	
	}
	if (newbot < oldbot) { fprintf(stderr,"case3\n");
	
	}
	if (newbot > oldbot) { fprintf(stderr,"case4\n");
	
	}
}

int main ( void )
{
  Stuff st;
  st.ot = -332;
  st.ob = -301;
  st.nt = 0;
  st.nb = 31;
  PaintThumb( &st );
  return 0;
}
