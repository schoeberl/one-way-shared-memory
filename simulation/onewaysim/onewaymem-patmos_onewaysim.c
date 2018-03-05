/*
  One-Way Shared Memory
  Runs on patmos

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
#include "syncprint.h"

// the functions which are called by threads are appended "work"
// cores other than core 0 are called "slave" cores because core 0 have a dual role
//   both as a thread work core and as the noc control core.

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
  volatile unsigned long testread0 = core[index].tx[index][index];
  unsigned long stoptestread0 = (unsigned long)*io_ptr;
  printf("starttestread0 %lu\n", starttestread0);
  printf("stoptestread0 %lu\n", stoptestread0);

  // test: how long does it take to write a word to shared memory
  volatile int writeindex = 0;
  volatile unsigned long writeval = 1;
  unsigned long starttestwrite0 = (unsigned long)*io_ptr;
  core[index].tx[writeindex][writeindex] = writeval;
  unsigned long stoptestwrite0 = (unsigned long)*io_ptr;
  printf("starttestwrite0 %lu\n", starttestwrite0);
  printf("stoptestwrite0 %lu\n", stoptestwrite0);
#endif
}

void nocmeminit()
{
  // map ms's core mem arrray for indexed access
  for (int i = 0; i < CORES; i++)
  { 
    for (int j = 0; j < TDMSLOTS; j++)
    { 
      //core[i].txmem[j] will point to a row of WORDS words 
      core[i].tx[j] = (volatile _IODEV int *) (ONEWAY_BASE + j*WORDS);
      core[i].rx[j] = (volatile _IODEV int *) (ONEWAY_BASE + j*WORDS);
    }
  }
}

// patmos memory initialization
// function pointer to one of the test cases
void nocinit()
{
  printf("in nocinit()...\n");
  txrxmapsinit();
  showmappings();

  nocmeminit();

  // init signals and counters
  HYPERPERIOD_REGISTER = 0;
  TDMROUND_REGISTER = 0;
}

// start the slave cores
void nocstart(void (*corefuncptr)(void *)){
  // START STEPS 0, 1, and 2
  
  //0. start the other cores
  runcores = true;
  for(int i = 0; i < CORES; i++) 
    coreready[i] = false;

  // 1. start the slave cores 1..CORES-1
  for (int c = 1; c < CORES; c++)
    corethread_create(c, corefuncptr, NULL);

  // 2. "start" ourself (core 0)
  corefuncptr(NULL);
}

void nocwaitdone()
{
  printf("waiting for slave cores to join ...\n");
  int *retval;
  // now let the others join
  for (int c = 1; c < CORES; c++)
  {
    printf("slave core thread %d to join...\n", c);
    // the cores join here when done...
    corethread_join(c, (void **)&retval);
    printf("slave core thread %d joined\n", c);
  }
}

///////////////////////////////////////////////////////////////////////////////
//patmos main
///////////////////////////////////////////////////////////////////////////////

// we are core 0
int main(int argc, char *argv[])
{
  setlocale(LC_ALL,"");
  printf("****************************************\n");
  printf("****onewaymem: run usecase on patmos****\n");
  printf("****************************************\n");
  printf("Init...\n");
  nocinit();
  printf("Start...\n");
  
  // Howto: Enable one of the following use cases //
  
  // use case 1, time-based sync: corethreadtbswork
  // void (*corefuncptr)(void *) = &corethreadtbswork;

  // use case 2, handshake:       corethreadhswork
  //void (*corefuncptr)(void *) = &corethreadhswork;

  // use case 3, state exchange:  corethreadeswork
  //void (*corefuncptr)(void *) = &corethreadeswork;

  // use case 4: corethreadsdbwork
  void (*corefuncptr)(void *) = &corethreadsdbwork;  
  
  nocstart(corefuncptr);
  
  printf("Wait...\n");
  
  // core 0 is done, wait for the others
  nocwaitdone();
  printf("Done...\n");
  
  for (int i=0; i<CORES; ++i) {
    printf("Sync print from core %d:\n", i);
    sync_print_core(i);
  }
  
  printf("****************************************\n");
  printf("****************************************\n");
  
  return 0;
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////


