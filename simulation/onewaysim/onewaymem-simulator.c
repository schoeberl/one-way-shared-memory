/*
  A software simulation of the One-Way Shared Memory
  Runs on the PC

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


#include "onewaysim.h"
#include "syncprint.h"

int alltxmem[CORES][TDMSLOTS][WORDS];
int allrxmem[CORES][TDMSLOTS][WORDS];

pthread_t *threadHandles;

// called repeatedly from core 0 when *simulating* on the PC
void simcontrol()
{
  sync_printf(0, "in simcontrol()...because we are simulating on the PC\n");

  // change this to the number of runs you want
  const int hyperperiodstorun = 5;

  // hyperperiod starts
  static int hyperperiod = 0;
  for (int m = 0; m < WORDS; m++)
  {                                                      // one word from each to each
    for (int txcoreid = 0; txcoreid < CORES; txcoreid++) // sender core
    {
      int txslot = 0;
      for (int rxcoreid = 0; rxcoreid < CORES; rxcoreid++) // receiver core
      {
        if (txcoreid != rxcoreid) // no tx to itself
        {
          // sync_printf(0,"rxcoreid = %d\n", rxcoreid);
          // sync_printf(0,"txcoreid = %d\n", txcoreid);
          // sync_printf(0,"txslot   = %d\n", txslot);
          // sync_printf(0,"getrxslot(txcoreid, rxcoreid, txslot)=%d\n", getrxslot(txcoreid, rxcoreid, txslot));
          // sync_printf(0,"m        = %d\n", m);
          //core[rxcoreid].rxmem[getrxslot(txcoreid, rxcoreid, txslot)][m] = core[txcoreid].txmem[txslot][m];
          core[rxcoreid].rx[0][m] = core[txcoreid].tx[txslot][m];
 txslot++;
        }
      }
    } // tdmround ends
    TDMROUND_REGISTER++;
    //sync_printf(0, "TDMROUND_REGISTER = %lu\n", TDMROUND_REGISTER);
    //usleep(100000); //much slower than the cores on purpose, so they can poll
  }
  HYPERPERIOD_REGISTER++;
  sync_printf(0, "HYPERPERIOD_REGISTER = %lu\n", HYPERPERIOD_REGISTER);
  //usleep(100000);
  // current hyperperiod ends

  TDMROUND_REGISTER = 0;

  hyperperiod++;
  //runcores = false;
  sync_printf(0, "leaving simcontrol()...\n");
}

// this is the big difference between simulating on the PC
// on patmos this is done in HW
// on tht PC this is done using one large memory block (alltxmem and allrxmem)
void nocmem() {
  // map ms's alltxmem and allrxmem to each core's local memory
  for (int i = 0; i < CORES; i++) { 
    for (int j = 0; j < TDMSLOTS; j++) { 
      // assign membuf addresses from allrx and alltx to rxmem and txmem
      core[i].tx[j] = alltxmem[i][j];
      core[i].rx[j] = allrxmem[i][j];
      for (int m = 0; m < WORDS; m++) { 
        // zero out the tx and rx membuffer slots
        core[i].tx[j][m] = 0;
        core[i].rx[j][m] = 0;
      }
    }
  }
}

// simulator memory initialization
// function pointer to one of the test cases
void nocinit(void (*corefuncptr)(void *))
{
  sync_printf(0, "in nocinit()...\n");
  txrxmapsinit();
  showmappings();
  
  // not using 0
  threadHandles = calloc(CORES, sizeof(pthread_t));
  
  // do the memory mapping for running the simulator on the PC
  nocmem();
  
  // init signals and counters
  HYPERPERIOD_REGISTER = 0;
  TDMROUND_REGISTER = 0;
  for (int c = 0; c < CORES; c++) {
    coreid[c] = c;
  }

  // start the "slave" cores
  for (int c = 1; c < CORES; c++) {
    //the typecast 'void * (*)(void *)' is because pthread expects a function that returns a void*
    pthread_create(&threadHandles[c], NULL, (void *(*)(void *)) corefuncptr, 
                   &coreid[c]);
  }
}

void noccontrol()
{
  sync_printf(0, "in noccontrol: simulation control when just running on host PC\n");
  simcontrol();
}

void nocdone()
{
  sync_printf(0, "in nocdone: waiting for cores to join ...\n");
  int *retval;
  // now let the others join
  for (int c = 1; c < CORES; c++)
  {
    sync_printf(0, "in nocdone: core thread %d to join...\n", c);
// the cores *should* join here...

    pthread_join(threadHandles[c], NULL);
    sync_printf(0, "in nocdone: core thread %d joined\n", c);
  }
}

///////////////////////////////////////////////////////////////////////////////
//Simulator main
///////////////////////////////////////////////////////////////////////////////

/* define this somewhere */
//#ifdef __i386
//__inline__ uint64_t rdtsc() {
//  uint64_t x;
//  __asm__ volatile ("rdtsc" : "=A" (x));
//  return x;
//}
//#elif __amd64
//unsigned int rdtsc() {
//  unsigned long a, d;
//  __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
  //return (d<<32) | a;
//  return (unsigned int) a;
//}
//#endif

// we are core 0
int main(int argc, char *argv[])
{
  printf("***********************************************************\n");
  printf("onewaysim-target main(): **********************************\n");
  printf("***********************************************************\n");

  // thread function pointer which is the use case
  void (*corefuncptr)(void *);  
  // set a default 
  corefuncptr = &corethreadtbswork;

  if( argc == 2 ) {
    if (strcmp(argv[1], "U0") == 0) 
    {
      corefuncptr = &corethreadtbswork;
      printf("Active use-case %s: %s\n", argv[1], "corethreadtbswork");
    } 
    else if (strcmp(argv[1], "U1") == 0) {
      corefuncptr = &corethreadhswork;
      printf("Active use-case %s: %s\n", argv[1], "corethreadhswork");
    } 
    else if (strcmp(argv[1], "U2") == 0) {
      corefuncptr = &corethreadeswork;
      printf("Active use-case %s: %s\n", argv[1], "corethreadeswork");
    } 
    else if (strcmp(argv[1], "U3") == 0) {
      corefuncptr = &corethreadsdbwork;
      printf("Active use-case %s: %s\n", argv[1], "corethreadsdbwork");
    } 
    else printf("The unknown argument supplied is %s\n", argv[1]);
  }
  else if( argc > 2 ) {
    printf("Too many arguments supplied.\n");
  }
  else {
    printf("One argument expected such as \"U1\"\n");
  }

  

  
  
  //start the other cores
  runcores = true;
  
  // Use case 1
  //void (*corefuncptr)(void *) = &corethreadtbswork;
  //corethreadtbswork(&coreid[0]);
  
  // use case 2, handshake:       corethreadhswork
  // void (*corefuncptr)(void *) =corethreadhswork;

  // use case 3, state exchange:  corethreadeswork
  //void (*corefuncptr)(void *) = &corethreadeswork;

  // use case 4: corethreadsdbwork
  //void (*corefuncptr)(void *) = &corethreadsdbwork; 
  // core 0 is done, wait for the others
  
  // start the slave threads
  nocinit(corefuncptr);
  
  // "start" core 0
  corefuncptr(&coreid[0]);
  
  nocdone();
  
  printf("Done...\n");
  
  for (int i=0; i<CORES; ++i) {
    printf("Sync print from core %d:\n", i);
    sync_print_core(i);
  }

  printf("****************************************************************\n");
  printf("Leaving main...done with highlevel simulation of use-case %s\n", argv[1]);
  printf("(Remember: Cycles on the PC simulator is *not* cycles on patmos)\n");
  printf("****************************************************************\n");
  return 0;
}
