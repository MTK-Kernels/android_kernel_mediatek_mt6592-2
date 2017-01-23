/*
 * gen_uuid_nt.c -- Use NT api to generate uuid
 *
 * Written by Andrey Shedel (andreys@ns.cr.cyco.com)
 */


#include "uuidP.h"

#pragma warning(push,4)

#pragma comment(lib, "ntdll.lib")


unsigned long
__stdcall
NtAllocateUuids(
   void* p1,  
   void* p2,  
   void* p3   
   );

typedef
unsigned long
(__stdcall*
NtAllocateUuids_2000)(
   void* p1,  
   void* p2,  
   void* p3,  
   void* seed 
   );




__declspec(dllimport)
struct _TEB*
__stdcall
NtCurrentTeb(void);



static
int
Nt5(void)
{
	
	return (int)*(int*)((char*)(int)(*(int*)((char*)NtCurrentTeb() + 0x30)) + 0xA4) >= 5;
}




void uuid_generate(uuid_t out)
{
	if(Nt5())
	{
		unsigned char seed[6];
		((NtAllocateUuids_2000)NtAllocateUuids)(out, ((char*)out)+8, ((char*)out)+12, &seed[0] );
	}
	else
	{
		NtAllocateUuids(out, ((char*)out)+8, ((char*)out)+12);
	}
}
