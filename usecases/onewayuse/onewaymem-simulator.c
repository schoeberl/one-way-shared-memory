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

void showmem() {
  printf("showmem():\n");
  for (int coreid = 0; coreid < CORES; coreid++) {
    printf("Core %d\n", coreid);
    for (int slot = 0; slot < TDMSLOTS; slot++) {
      printf("  tx slot %d (to core %d)\n    ", slot, getrxcorefromtxcoreslot(coreid, slot));
      for (int w = 0; w < WORDS; w++) {
        printf("0x%08x ", alltxmem[coreid][slot][w]);
        if ((w + 1) % 8 == 0)
          printf("\n    ");
      }
      printf("\n");
      printf("  rx slot %d (from core %d)\n    ", slot, gettxcorefromrxcoreslot(coreid, slot));
      for (int w = 0; w < WORDS; w++) {
        printf("0x%08x ", allrxmem[coreid][slot][w]);
        if ((w + 1) % 8 == 0)
          printf("\n    ");
      }
      printf("\n");
    }
  }
}

// called repeatedly from core 0 when *simulating* on the PC
void simcontrol()
{
  sync_printf(0, "entering simcontrol()...(simulating on the PC)\n");

  // change this to the number of runs you want
  const int hyperperiodstorun = 5;
  // hyperperiod starts
  static int hyperperiod = 0;

  // it is important run these simulater loop with the words in the outer loop
  // that way it is as close to what happens on patmos HW as possible (even though it is still serial)
  // it enables testing use cases such as doublebuffer
  for (int w = 0; w < WORDS; w++) {
    for (int txslot = 0; txslot < TDMSLOTS; txslot++) {
      for (int txcoreid = 0; txcoreid < CORES; txcoreid++) {
        int rxcoreid = getrxcorefromtxcoreslot(txcoreid, txslot);
        for (int rxslot = 0; rxslot < TDMSLOTS; rxslot++) {
          int txcoreid_for_rxcoreidrxslot = gettxcorefromrxcoreslot(rxcoreid, rxslot);
          if (txcoreid == txcoreid_for_rxcoreidrxslot) {
            core[rxcoreid].rx[rxslot][w] = core[txcoreid].tx[txslot][w];
            break;
          }
        }
      }

    }
    //All cores have tx'ed one word to each of the other cores
    TDMROUND_REGISTER++;
  }
  // all words have been tx'ed by all cores
  TDMROUND_REGISTER = 0;
  HYPERPERIOD_REGISTER++;
  sync_printf(0, "HYPERPERIOD_REGISTER = %lu\n", HYPERPERIOD_REGISTER);

  hyperperiod++;
  //runcores = false;
  sync_printf(0, "leaving simcontrol(), hyperperiod = %d...\n", hyperperiod);
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
        // init the tx and rx membuffer slots to known vals
        core[i].tx[j][m] = 0x10000*i + 0x1000*j+m;
        core[i].rx[j][m] = 0x10000*i + 0x1000*j+m;
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

  //showmem();

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

// we are core 0
int main(int argc, char *argv[])
{
  printf("***********************************************************\n");
  printf("onewaysim-target main(): **********************************\n");
  printf("***********************************************************\n");

  // thread function pointer which is the use case
  void (*corefuncptr)(void *);
  
#if USECASE==0
  printf("USECASE = 0\n");
  corefuncptr = &corethreadtbswork;
#elif USECASE==1
  printf("USECASE == 1\n");
  corefuncptr = &corethreadhswork;
#elif USECASE==2
  printf("USECASE == 2\n");
  corefuncptr = &corethreadeswork;
#elif USECASE==3
  printf("USECASE == 3\n");
  corefuncptr = &corethreadsdbwork;        
#else
  printf("Unimplemented USECASE value. Exit\n");
  exit(0);
#endif

  runcores = true;

  // start the slave threads
  nocinit(corefuncptr);

  // "start" core 0
  corefuncptr(&coreid[0]);

  nocdone();

  printf("Done...\n");

  for (int i = 0; i < CORES; ++i) {
    printf("Sync print from core %d:\n", i);
    sync_print_core(i);
  }

  printf("****************************************************************\n");
  printf("Leaving main...done with highlevel simulation\n");
  printf("(Remember: Cycles on the PC simulator is *not* real HW cycles)\n");
  printf("****************************************************************\n");
  return 0;
}
