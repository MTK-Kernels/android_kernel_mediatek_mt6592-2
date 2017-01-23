#ifndef BAR_H
#define BAR_H

#ifndef FOO
#error FOO should be defined here !
#endif

#if FOO != 1
#error FOO is not correctly defined here !
#endif

extern int  bar(int  x);

#endif 
