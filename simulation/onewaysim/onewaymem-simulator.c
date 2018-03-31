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

int alltxmem[CORES][CORES - 1][MEMBUF];
int allrxmem[CORES][CORES - 1][MEMBUF];

int cpuid[CORES];

pthread_t *threadHandles;

// called repeatedly from core 0
void simcontrol()
{
  sync_printf(0, "in simcontrol()...because we are simulating on the PC\n");

  // change this to the number of runs you want
  const int hyperperiodstorun = 5;

  // hyperperiod starts
  static int hyperperiod = 0;
  for (int m = 0; m < MEMBUF; m++)
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

void nocmem() {
  // map ms's alltxmem and allrxmem to each core's local memory
  for (int i = 0; i < CORES; i++) { 
    for (int j = 0; j < CORES - 1; j++) { 
      // assign membuf addresses from allrx and alltx to rxmem and txmem
      core[i].tx[j] = alltxmem[i][j];
      core[i].rx[j] = allrxmem[i][j];
      for (int m = 0; m < MEMBUF; m++) { 
        // zero the tx and rx membuffer slots
        core[i].tx[j][m] = 0;
        core[i].rx[j][m] = 0;
      }
    }
  }
}

// patmos memory initialization
// function pointer to one of the test cases
void nocinit()
{
  sync_printf(0, "in nocinit()...\n");
  txrxmapsinit();
  showmappings();
  
  threadHandles = calloc(CORES, sizeof(pthread_t));
  
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
    pthread_create(&threadHandles[c], NULL, (void *(*)(void *)) &corethreadtbswork, 
                   &coreid[c]);
  }
}

void noccontrol()
{
  sync_printf(0, "simulation control when just running on host PC\n");
  simcontrol();
}

void nocdone()
{
  sync_printf(0, "waiting for cores to join ...\n");
  int *retval;
  // now let the others join
  for (int c = 1; c < CORES; c++)
  {
    sync_printf(0, "core thread %d to join...\n", c);
// the cores *should* join here...

    pthread_join(threadHandles[c], NULL);
    sync_printf(0, "core thread %d joined\n", c);
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
  //start the other cores
  runcores = true;
  nocinit();
  // "start" ourself (core 0)
  corethreadtbswork(&coreid[0]);
  // core 0 is done, wait for the others
  nocdone();
  printf("Done...\n");
  
  for (int i=0; i<CORES; ++i) {
    printf("Sync print from core %d:\n", i);
    sync_print_core(i);
  }

  printf("leaving main...\n");
  printf("***********************************************************\n");
  printf("***********************************************************\n");
  return 0;
}

