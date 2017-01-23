


int main ( void ) { 
  __asm__ __volatile__( "subw $0x28, %%sp\n" 
                        "movl $0, 0(%%esp)\n" 
                        "addw $0x28, %%sp" : : : "memory" ); 
  return 0;
} 
 
