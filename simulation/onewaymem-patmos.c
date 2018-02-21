/*
  One-Way Shared Memory
  Runs on the patmos

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

//#include <machine/patmos.h>
#include "onewaysim.h"

///////////////////////////////////////////////////////////////////////////////
//TEST AND PLAYGROUND ON PATMOS
//just using this as a quick way to test some things
//use the main function in onewaysim-common.c to make sure this is called
//no need to use the other threads when this playground is used
///////////////////////////////////////////////////////////////////////////////

void playandtest()
{
  printf("in playandtest()...\n");
#ifdef RUNONPATMOS
  // test: reading the clock back to back
  volatile _IODEV int *io_ptr = (volatile _IODEV int *)0xf0020004;
  unsigned long clockread0 = (unsigned long)*io_ptr;
  unsigned long clockread1 = (unsigned long)*io_ptr;
  unsigned long clockread2 = (unsigned long)*io_ptr;
  unsigned long clockread3 = (unsigned long)*io_ptr;
  unsigned long clockread4 = (unsigned long)*io_ptr;
  unsigned long clockread5 = (unsigned long)*io_ptr;
  printf("clockread0 %lu\n", clockread0);
  printf("clockread1 %lu\n", clockread1);
  printf("clockread2 %lu\n", clockread2);
  printf("clockread3 %lu\n", clockread3);
  printf("clockread4 %lu\n", clockread4);
  printf("clockread5 %lu\n", clockread5);

  // test: how long does it take to read a word from shared memory
  volatile int index = 0;
  unsigned long starttestread0 = (unsigned long)*io_ptr;
  volatile unsigned long testread0 = core[index].txmem[index][index];
  unsigned long stoptestread0 = (unsigned long)*io_ptr;
  printf("starttestread0 %lu\n", starttestread0);
  printf("stoptestread0 %lu\n", stoptestread0);

  // test: how long does it take to write a word to shared memory
  volatile int writeindex = 0;
  volatile unsigned long writeval = 1;
  unsigned long starttestwrite0 = (unsigned long)*io_ptr;
  core[index].txmem[writeindex][writeindex] = writeval;
  unsigned long stoptestwrite0 = (unsigned long)*io_ptr;
  printf("starttestwrite0 %lu\n", starttestwrite0);
  printf("stoptestwrite0 %lu\n", stoptestwrite0);
#endif
}
///////////////////////////////////////////////////////////////////////////////
//patmos main
///////////////////////////////////////////////////////////////////////////////

// we are core 0
int main(int argc, char *argv[])
{
  printf("***********************************************************\n");
  printf("onewaymem: run usecase on patmos***************************\n");
  printf("***********************************************************\n");
  //start the other cores
  runcores = true;
  nocinit();
  // "start" ourself (core 0)
  corethreadtbs(&coreid[0]);
  // core 0 is done, wait for the others
  nocdone();
  info_printf("done...\n");
  sync_printall();

  printf("leaving main...\n");
  printf("***********************************************************\n");
  printf("***********************************************************\n");
  return 0;
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////


