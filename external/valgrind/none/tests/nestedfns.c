

#include <stdio.h> 
 
 static void call_func(void (*sel)(void)) 
 { 
    sel(); 
 } 
 
 void test1() 
 { 
    void test1_inner() 
    { 
       printf( "Inside test1\n" ); 
    } 
    call_func( test1_inner ); 
 } 
 
 void test2() 
 { 
    void test2_inner() 
    { 
       printf( "Inside test2\n" ); 
    } 
    call_func( test2_inner ); 
 } 
 
 int main(int argc, char** argv) 
 { 
    test1(); 
    test2(); 
    return( 0 ); 
 } 
 
