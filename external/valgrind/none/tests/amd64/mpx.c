int
main (int argc, char **argv)
{
  
  
  
  

  
  asm ("bndmov %bnd0, (%rsp)");
  asm ("bndmov %bnd1, 16(%rsp)");
  asm ("bndmov %bnd2, 32(%rsp)");
  asm ("bndmov %bnd3, 48(%rsp)");

  
  asm ("bndmk (%rax,%rdx), %bnd0");
  asm ("bndcl (%rax,%rdi,4), %bnd0");
  asm ("bndcu 3(%rax,%rdi,4), %bnd0");
  asm ("bndcn 3(%rax,%rdi,4), %bnd0");

  
  asm ("bndldx 3(%rbx,%rdx), %bnd2");
  asm ("bndstx %bnd2, 3(,%r12,1)");

  
  asm ("bnd call foo\n\
        bnd jmp  end\n\
        foo: bnd ret\n\
        end: nop");

  
  asm ("bndmov 48(%rsp), %bnd3");
  asm ("bndmov 32(%rsp), %bnd2");
  asm ("bndmov 16(%rsp), %bnd1");
  asm ("bndmov (%rsp), %bnd0");

  return 0;
}
