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

// called repeatedly from core 0
void simcontrol()
{
  sync_printf(0, "in noccontrol()...\n");

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
          core[rxcoreid].rxmem[0][m] = core[txcoreid].txmem[txslot][m];
 txslot++;
        }
      }
    } // tdmround ends
    TDMROUND_REGISTER++;
    sync_printf(0, "TDMROUND_REGISTER = %lu\n", TDMROUND_REGISTER);
    //usleep(100000); //much slower than the cores on purpose, so they can poll
  }
  HYPERPERIOD_REGISTER++;
  sync_printf(0, "HYPERPERIOD_REGISTER = %lu\n", HYPERPERIOD_REGISTER);
  //usleep(100000);
  // current hyperperiod ends

  TDMROUND_REGISTER = 0;

  hyperperiod++;
  runcores = false;
  sync_printf(0, "leaving noccontrol()...\n");
}


void nocmem()
{
  // map ms's alltxmem and allrxmem to each core's local memory
  for (int i = 0; i < CORES; i++)
  { // allocate pointers to each core's membuf array
    core[i].txmem[0] = (volatile _IODEV int *) ONEWAY_BASE;
    //(unsigned long **)malloc((CORES - 1) * sizeof(unsigned long **));
    core[i].rxmem[0] = (volatile _IODEV int *) ONEWAY_BASE;
    //(unsigned long **)malloc((CORES - 1) * sizeof(unsigned long **));
    for (int j = 0; j < CORES - 1; j++)
    { // assign membuf addresses from allrx and alltx to rxmem and txmem
      //core[i].txmem[j] = alltxmem[i][j];
      //core[i].rxmem[j] = allrxmem[i][j];
      for (int m = 0; m < MEMBUF; m++)
      { // zero the tx and rx membuffer slots
        // see the comment on 'struct Core'
        //core[i].txmem[j][m] = 0;
        //core[i].rxmem[j][m] = 0;
      }
    }
  }
}

// patmos memory initialization
// function pointer to one of the test cases
void nocinit()
{
  info_printf("in nocinit()...\n");
  //inittxrxmaps();

  nocmem();
  

  // init signals and counters
  HYPERPERIOD_REGISTER = 0;
  TDMROUND_REGISTER = 0;
  for (int c = 0; c < CORES; c++)
  {
    coreid[c] = c;
#ifdef RUNONPATMOS
    //corethread[c] = c; // which core it will run on
#endif
  }

  // start the "slave" cores
  for (int c = 1; c < CORES; c++)
  {
#ifdef RUNONPATMOS
    corethread_create(c, &corethreadtbs, NULL);
#else
    // the typecast 'void * (*)(void *)' is because pthread expects a function that returns a void *
    pthread_create(&corethread[c], NULL, (void *(*)(void *)) & corethreadtbs, (void *)&coreid[c]);
#endif
  }
}

void noccontrol()
{
#ifdef RUNONPATMOS
  info_printf("noc control is done in HW when running on patmos\n");
  runcores = false; // stop the cores here or somewhere else...
#else
  info_printf("simulation control when just running on host PC\n");
  simcontrol();
#endif
}

void nocdone()
{
  info_printf("waiting for cores to join ...\n");
  int *retval;
  // now let the others join
  for (int c = 1; c < CORES; c++)
  {
    info_printf("core thread %d to join...\n", c);
// the cores *should* join here...
#ifdef RUNONPATMOS
    corethread_join(c, (void **)&retval);
#else
    pthread_join(corethread[c], NULL);
#endif
    info_printf("core thread %d joined\n", c);
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
  corethreadtbs(&coreid[0]);
  // core 0 is done, wait for the others
  nocdone();
  info_printf("done...\n");
  sync_printall();

  printf("leaving main...\n");
  printf("***********************************************************\n");
  printf("***********************************************************\n");
  printf("TX AND RX MAPPING TESTING:\n");
  initroutestrings();
  inittxrxmaps();
  showmappings();
  return 0;
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////












































////////////////////////////////////////////////////////////////
//should be updated (ms ?)
//////////////////////////////////////////////////////////////////
// MAIN1 begin
void *coredo(void *vargp)
{
  int id = *((int *)vargp);
  unsigned long *txmem;
  txmem = alltxmem[id][0];
  unsigned long *rxmem;
  rxmem = allrxmem[id][0];

  printf("I am core %d\n", id);
  // this should be the work done
  for (int i = 0; i < 10; ++i)
  {
    printf("%d: %lu, ", id, allrxmem[id][0][0]);
    fflush(stdout);
    txmem[id] = id * 10 + id;
    usleep(10000);
  }
  return NULL; // it gave a warning to have it commented out
  //return NULL;
}

int main1()
{
  // Just to make it clear that those are accessed by threads
  //static pthread_t tid[4];
  static int id[4];

  printf("\nmain1(): Simulation starts\n");
  for (int i = 0; i < 4; ++i)
  {
    id[i] = i;
    //int returncode = pthread_create(&tid[i], NULL, coredo, &id[i]);
  }

  // main is simulating the data movement by the NIs through the NoC
  for (int i = 0; i < 10; ++i)
  {
    // This is VERY incomplete and does not reflect the actual indexing
    // And that it is more than one word in the memory blocks
    allrxmem[0][0][0] = alltxmem[1][3][0];
    allrxmem[1][0][0] = alltxmem[3][0][0];
    allrxmem[2][0][0] = alltxmem[2][0][0];
    allrxmem[3][0][0] = alltxmem[1][0][0];
    usleep(100000);
    // or maybe better ues yield() to give
    // TODO: find the buffer mapping from the real hardware for a more
    // realistic setup
  }

  usleep(1000);

  for (int i = 0; i < 4; ++i)
  {
    //pthread_join(tid[i], NULL);
  }

  printf("\n");
  for (int i = 0; i < 4; ++i)
  {
    printf("alltxmem[%d][0]: 0x%08lx\n", i, alltxmem[i][0][0]);
  }

  for (int i = 0; i < 4; ++i)
  {
    printf("allrxmem[%d][0]: 0x%08lx\n", i, allrxmem[i][0][0]);
  }

  printf("End of simulation\n");
  return 0;
}

// MAIN1 end
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
