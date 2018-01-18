/*
  A software simulation of the One-Way Shared Memory
  Runs on the PC

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#ifdef USEPTHREAD_FLAG
#include <pthread.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include "onewaysim.h"

//print support so the threads call printf in order
//static pthread_mutex_t printf_mutex;
int sync_printf(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  //pthread_mutex_lock(&printf_mutex);
  vprintf(format, args);
  //pthread_mutex_unlock(&printf_mutex);

  va_end(args);
}

//target tables
//information on where to send tx slots
//example: rxslot[1][3][2] = 1 means that core 1 will tx a word to core 3
//           from tx slot 2 into rx slot 1 respectively
//useful when working on code in the transmitting end
int rxslots[CORES][CORES][CORES - 1];

//find the tx core that filled a certain rxslot.
//useful when working in code running in the receiving end
int txcoreids[CORES][CORES - 1];

// init rxslot and txcoreid lookup tables
void inittxrxmaps()
{
  for (int txcoreid = 0; txcoreid < CORES; txcoreid++) // sender core
  {
    int txslot = 0;
    for (int rxcoreid = 0; rxcoreid < CORES; rxcoreid++) // receiver core
    {
      if (rxcoreid != txcoreid) // cannot tx to itself
      {
        int rxslot = 0;
        if (txcoreid > rxcoreid)
          rxslot = txcoreid - 1;
        else
          rxslot = txcoreid;
        rxslots[txcoreid][rxcoreid][txslot] = rxslot;
        txcoreids[rxcoreid][rxslot] = txcoreid;
        //sync_printf("rxslots[txcoreid:%d][rxcoreid:%d][txslot:%d] = %d\n", txcoreid, rxcoreid, txslot, rxslot);
        //sync_printf("txcoreids[rxcoreid:%d][rxslot:%d] = %d\n", rxcoreid, rxslot, txcoreid);
        txslot++;
      }
    }
    //sync_printf("\n");
  }
}

// rx slot from txcoreid, rxcoreid, and txslot
int getrxslot(int txcoreid, int rxcoreid, int txslot)
{
  //sync_printf("rxslots[txcoreid:%d][rxcoreid:%d][txslot:%d] = %d\n", txcoreid, rxcoreid, txslot, rxslots[txcoreid][rxcoreid][txslot]);
  return rxslots[txcoreid][rxcoreid][txslot];
}

// txcoreid from rxcoreid and rxslot
int gettxcoreid(int rxcoreid, int rxslot)
{
  //sync_printf("txcoreids[rxcoreid:%d][rxslot:%d] = %d\n", rxcoreid, rxslot, txcoreids[rxcoreid][rxslot]);
  return txcoreids[rxcoreid][rxslot];
}

// init patmos (simulated) internals
Core core[CORES];
// signal used to stop terminate the cores
bool runnoc;

// the NoC (same for all comm. patterns)
void *nocthreadfunc(void *coreid)
{
  printf("NoC thread here ...\n");

  for (int m = 0; m < MEMBUF; m++)
  {                                                      // one word from each to each
    for (int txcoreid = 0; txcoreid < CORES; txcoreid++) // sender core
    {
      int txslot = 0;
      for (int rxcoreid = 0; rxcoreid < CORES; rxcoreid++) // receiver core
      {
        if (txcoreid != rxcoreid) // no tx to itself
        {
          // printf("rxcoreid = %d\n", rxcoreid);
          // printf("txcoreid = %d\n", txcoreid);
          // printf("txslot   = %d\n", txslot);
          // printf("getrxslot(txcoreid, rxcoreid, txslot)=%d\n", getrxslot(txcoreid, rxcoreid, txslot));
          // printf("m        = %d\n", m);
          core[rxcoreid].rxmem[getrxslot(txcoreid, rxcoreid, txslot)][m] = core[txcoreid].txmem[txslot][m];
          txslot++;
        }
      }
      printf("\n");
    }
    TDMROUND_REGISTER++;
    sync_printf("nocthread:    TDMROUND_REGISTER=%lu\n", TDMROUND_REGISTER);
    usleep(100); //much slower than the cores on purpose, so they can poll
  }
  HYPERPERIOD_REGISTER++;
  sync_printf("nocthread: HYPERPERIOD_REGISTER=%lu\n", HYPERPERIOD_REGISTER);
  usleep(100);
}

///////////////////////////////////////////////////////////////////////////////
// Control code for communication patterns (see the core functions in onewaysim-target.c)
///////////////////////////////////////////////////////////////////////////////

// patmos memory initialization
void patmosinit()
{
  sync_printf("in patmosinit()...\n");
  // map ms's alltxmem and allrxmem to cores' local memory
  for (int i = 0; i < CORES; i++)
  { // allocate pointers to each core's membuf array
    core[i].txmem = (unsigned long **)malloc((CORES - 1) * sizeof(unsigned long **));
    core[i].rxmem = (unsigned long **)malloc((CORES - 1) * sizeof(unsigned long **));
    for (int j = 0; j < CORES - 1; j++)
    { // assign membuf addresses from allrx and alltx to rxmem and txmem
      core[i].txmem[j] = alltxmem[i][j];
      core[i].rxmem[j] = allrxmem[i][j];
      for (int m = 0; m < MEMBUF; m++)
      { // zero the tx and rx membuffer slots
        // see the comment on 'struct Core'
        core[i].txmem[j][m] = 0;
        core[i].rxmem[j][m] = 0;
      }
    }
  }
}

// takes one of the target core test functions
// i.e.: corethreadtbs, corethreadhsp, corethreades, or corethreadsdb
void patmoscontrol(void *(*corefp)(void *))
{
  static int id[CORES];
  HYPERPERIOD_REGISTER = 0;
  TDMROUND_REGISTER = 0;

  for (int c = 0; c < CORES; c++)
    id[c] = c;

  //number of runs
  for (int n = 0; n < 1; n++)
  {
    for (int c = 0; c < CORES; c++)
    {
      (*corefp)(&id[c]);
    }
    nocthreadfunc(NULL);
  }

  // pthread
  // "signal" to stop the cores
  // runnoc = false;

  // //start noc test control
  // runnoc = true;
  // pthread_t nocthread;
  // int returncode = pthread_create(&nocthread, NULL, nocthreadfunc, NULL);
  // if (returncode)
  // {
  //   sync_printf("Error %d ... \n", returncode);
  //   exit(-1);
  // }
  sync_printf("runtesttbs(): noc thread created ...\n");
}

///////////////////////////////////////////////////////////////////////////////
void memtxprint(int coreid)
{
  for (int m = 0; m < MEMBUF; m++)
    for (int i = 0; i < CORES - 1; i++)
    {
      sync_printf("core[coreid:%d].txmem[i:%d][m:%d] = %lu\n", coreid, i, m, core[coreid].txmem[i][m]);
    }
}

void memrxprint(int coreid)
{
  for (int m = 0; m < MEMBUF; m++)
    for (int i = 0; i < CORES - 1; i++)
    {
      sync_printf("core[coreid:%d].rxmem[i:%d][m:%d] = %lu\n", coreid, i, m, core[coreid].rxmem[i][m]);
    }
}

void memallprint()
{
  for (int c = 0; c < CORES; c++)
  {
    memtxprint(c);
    memrxprint(c);
  }
}

void printpatmoscounters()
{
  sync_printf("HYPERPERIOD_REGISTER = %d\n", HYPERPERIOD_REGISTER);
  sync_printf("TDMROUND_REGISTER    = %d\n", TDMROUND_REGISTER);
}

void initsim()
{
  inittxrxmaps();
  //memtxprint(0);
  //memallprint();
  
  patmosinit();
  patmoscontrol(&corethreadtbs);
  patmosinit();
  patmoscontrol(&corethreadhsp);
  patmosinit();
  patmoscontrol(&corethreades);
  patmosinit();
  patmoscontrol(&corethreadsdb);
  printpatmoscounters();
}

//////////////////////////////////////////////////////////////////
//should be updated 
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