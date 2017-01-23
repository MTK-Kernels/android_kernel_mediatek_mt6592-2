#include <unistd.h>
#include "tests/sys_mman.h"
#include <assert.h>
#include <stdlib.h>

#include "../drd.h"

#define SUPERBLOCK_SIZE    100000


void* get_superblock(void)
{
   void* p = mmap( 0, SUPERBLOCK_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC,
                   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );

   assert(p != ((void*)(-1)));

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
#undef RZ




void make_leak(void)
{
   int* array2 __attribute__((unused)) = custom_alloc(sizeof(int) * 10);
   array2 = 0;          
   return;
}

int main(void)
{
   int* array;
   int* array3;

   array = custom_alloc(sizeof(int) * 10);
   array[8]  = 8;
   array[9]  = 8;
   array[10] = 10;      

   custom_free(array);  

   custom_free(NULL);   

   array3 = malloc(sizeof(int) * 10);
   custom_free(array3); 

   make_leak();
   return array[0];     
                        
                        
                        
                        

   
}
