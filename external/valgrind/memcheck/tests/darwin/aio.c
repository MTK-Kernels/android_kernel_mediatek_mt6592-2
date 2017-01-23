#include <assert.h>
#include <aio.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int x;

int main(void)
{
   #define LEN 10
   char buf[LEN];

   struct aiocb a;
   struct sigevent s;

   
   memset(&s, 0, sizeof(struct sigevent));

   a.aio_fildes     = -1;
   a.aio_offset     = 0;
   a.aio_buf        = NULL;
   a.aio_nbytes     = LEN;
   a.aio_reqprio    = 0;
   a.aio_sigevent   = s;
   a.aio_lio_opcode = 0;   

   
   
   

   
   
   
   

   
   assert( aio_read(&a) < 0);       

   
   a.aio_fildes = open("aio.c", O_RDONLY);
   assert(a.aio_fildes >= 0);

   assert( aio_read(&a) < 0);       

   
   a.aio_buf = buf;

   assert( aio_read(&a) == 0 );

   assert( aio_read(&a)  < 0 );     

   while (0 != aio_error(&a)) { };

   if (buf[0] == buf[9]) x++;       

   assert( aio_return(&a) > 0 );    

   if (buf[0] == buf[9]) x++;

   assert( aio_return(&a) < 0 );    
                                    

   
   a.aio_buf    = 0;
   a.aio_fildes = creat("mytmpfile", S_IRUSR|S_IWUSR);
   assert(a.aio_fildes >= 0);

   assert( aio_write(&a) < 0);      

   
   a.aio_buf = buf;

   assert( aio_write(&a) == 0 );

   assert( aio_write(&a)  < 0 );    

   while (0 != aio_error(&a)) { };

   assert( aio_return(&a) > 0 );

   assert( aio_return(&a) < 0 );    
                                    

   unlink("mytmpfile");

   return x;
};



