#define DECLARE_LEAK_COUNTERS \
   long L0_bytes = 0, L_bytes = 0, L0_blocks = 0, L_blocks = 0; \
   long D0_bytes = 0, D_bytes = 0, D0_blocks = 0, D_blocks = 0; \
   long R0_bytes = 0, R_bytes = 0, R0_blocks = 0, R_blocks = 0; \
   long S0_bytes = 0, S_bytes = 0, S0_blocks = 0, S_blocks = 0

#define GET_INITIAL_LEAK_COUNTS \
   do { \
      VALGRIND_DO_QUICK_LEAK_CHECK; \
      VALGRIND_COUNT_LEAKS(      L0_bytes,  D0_bytes,  R0_bytes,  S0_bytes );\
      VALGRIND_COUNT_LEAK_BLOCKS(L0_blocks, D0_blocks, R0_blocks, S0_blocks); \
   } while (0)

#define GET_FINAL_LEAK_COUNTS \
   do { \
      VALGRIND_DO_QUICK_LEAK_CHECK; \
      VALGRIND_COUNT_LEAKS(      L_bytes,  D_bytes,  R_bytes,  S_bytes ); \
      VALGRIND_COUNT_LEAK_BLOCKS(L_blocks, D_blocks, R_blocks, S_blocks); \
      L_bytes -= L0_bytes;  L_blocks -= L0_blocks; \
      D_bytes -= D0_bytes;  D_blocks -= D0_blocks; \
      R_bytes -= R0_bytes;  R_blocks -= R0_blocks; \
      S_bytes -= S0_bytes;  S_blocks -= S0_blocks; \
   } while (0)

#define PRINT_LEAK_COUNTS(where) \
   do { \
      fprintf(where,"leaked:     %3ld bytes in %2ld blocks\n", \
                     L_bytes,L_blocks); \
      fprintf(where,"dubious:    %3ld bytes in %2ld blocks\n", \
                     D_bytes,D_blocks); \
      fprintf(where,"reachable:  %3ld bytes in %2ld blocks\n", \
                     R_bytes,R_blocks); \
      fprintf(where,"suppressed: %3ld bytes in %2ld blocks\n", \
                     S_bytes,S_blocks); \
   } while (0)

#if defined __powerpc__
#define CLEAR_CALLER_SAVED_REGS \
  do { \
   __asm__ __volatile__( "li 3, 0" : : :"r3" ); \
   __asm__ __volatile__( "li 4, 0" : : :"r4" ); \
   __asm__ __volatile__( "li 5, 0" : : :"r5" ); \
   __asm__ __volatile__( "li 6, 0" : : :"r6" ); \
   __asm__ __volatile__( "li 7, 0" : : :"r7" ); \
   __asm__ __volatile__( "li 8, 0" : : :"r8" ); \
   __asm__ __volatile__( "li 9, 0" : : :"r9" ); \
   __asm__ __volatile__( "li 10, 0" : : :"r10" ); \
   __asm__ __volatile__( "li 11, 0" : : :"r11" ); \
   __asm__ __volatile__( "li 12, 0" : : :"r12" ); \
  } while (0)
#else
#define CLEAR_CALLER_SAVED_REGS  
#endif


