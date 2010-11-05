#include <unistd.h>
#include <stdio.h>
//#include <arch/x86/include/asm/unistd_32.h>
//#include <unistd_32.h>


int main( int argc, char ** argv ) {

  unsigned int free, claimed;

  claimed = syscall( /*__NR_sys_get_slob_amt_claimed*/ 338 );
  free = syscall( /*__NR_sys_get_slob_amt_free*/ 339 );

  printf( "Claimed memory: %d\n", claimed );
  printf( "Free memory: %d\n", free );

  return 0;

}
