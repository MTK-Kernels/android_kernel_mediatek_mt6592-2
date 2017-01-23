#include <unistd.h>
#include "tests/sys_mman.h"
#include <assert.h>
#include <stdlib.h>

#include "../memcheck.h"

#define SUPERBLOCK_SIZE    100000


void* get_superblock(void)
{
   void* p = mmap( 0, SUPERBLOCK_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC,
                   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );

   assert(p != ((void*)(-1)));

   
   

   
   (void) VALGRIND_MAKE_MEM_NOACCESS(p, SUPERBLOCK_SIZE);

   return p;
}

static void* custom_alloc(int size)
{
#define RZ  8
   static void* hp     = 0;    
   static void* hp_lim = 0;    
   int          size2  = size + RZ*2;
   void*        p;

   if (hp + size2 > hp_lim) {
      hp = get_superblock();
      hp_lim = hp + SUPERBLOCK_SIZE - 1;
   }  

   p = hp + RZ;
   hp += size2;

   VALGRIND_MALLOCLIKE_BLOCK( p, size, RZ, 1 );
   return (void*)p;
}     

static void custom_free(void* p)
{
   
   VALGRIND_FREELIKE_BLOCK( p, RZ );
}

static void checkredzone(void)
{
   char superblock[1 + RZ + 20 + RZ + 1];
   char *p = 1 + RZ + superblock;
   assert(RZ > 0);

   
   VALGRIND_MALLOCLIKE_BLOCK( p, 20, RZ, 1 );
   p[0] = 0; 
   p[-1] = p[0]; 
   p[-RZ] = p[0]; 
   p[-RZ-1] = p[0]; 
   
   p[19] = 0; 
   p[19 + 1]  = p[0]; 
   p[19 + RZ] = p[0]; 
   p[19 + RZ + 1] = p[0]; 

   VALGRIND_FREELIKE_BLOCK( p, RZ );

   
   
   VALGRIND_MALLOCLIKE_BLOCK( p, 10, RZ, 1 );
   p[0] = 0; 
   p[-1] = p[0]; 
   p[-RZ] = p[0]; 
   p[-RZ-1] = p[0]; 
   
   p[9] = 0; 
   p[9 + 1]  = p[0]; 
   p[9 + RZ] = p[0]; 
   p[9 + RZ + 1] = p[0]; 

   VALGRIND_FREELIKE_BLOCK( p, RZ );

}




void make_leak(void)
{
   int* array2 __attribute__((unused)) = custom_alloc(sizeof(int) * 10);
   array2 = 0;          
   return;
}

int main(void)
{
   int *array, *array3;
   int x;

   array = custom_alloc(sizeof(int) * 10);
   array[8]  = 8;
   array[9]  = 8;
   array[10] = 10;      

   VALGRIND_RESIZEINPLACE_BLOCK(array, sizeof(int) * 10, sizeof(int) * 5, RZ);
   array[4] = 7;
   array[5] = 9; 

   
   
   (void) VALGRIND_MAKE_MEM_DEFINED(array, sizeof(int) * 10);

   VALGRIND_RESIZEINPLACE_BLOCK(array, sizeof(int) * 5, sizeof(int) * 7, RZ);
   if (array[5]) array[4]++; 
   array[5]  = 11;
   array[6]  = 7;
   array[7] = 8; 

   
   VALGRIND_RESIZEINPLACE_BLOCK(array+1, sizeof(int) * 7, sizeof(int) * 8, RZ);

   custom_free(array);  

   custom_free((void*)0x1);  

   array3 = malloc(sizeof(int) * 10);
   custom_free(array3); 

   make_leak();
   x = array[0];        

   
   
   VALGRIND_MALLOCLIKE_BLOCK(0,0,0,0);
   VALGRIND_FREELIKE_BLOCK(0,0);

   checkredzone();

   return x;

   
}

#undef RZ
