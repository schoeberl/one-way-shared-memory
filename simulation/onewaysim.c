/*
  A software simulation of the One-Way Shared Memory
  Runs on the PC

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include "onewaysim.h"

//target tables
//information on where to send tx slots
//example: rxslot[1][3][2] = 1 means that core 1 will tx a word to core 3 
//           from tx slot 2 into rx slot 1 respectively
//useful when working on code in the transmitting end
int rxslots[CORES][CORES][CORES - 1];

//find the tx core that filled a certain rxslot.
//useful when working in code running in the receiving end
int txcoreids[CORES][CORES-1];

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
        //printf("rxslots[txcoreid:%d][rxcoreid:%d][txslot:%d] = %d\n", txcoreid, rxcoreid, txslot, rxslot);
        //printf("txcoreids[rxcoreid:%d][rxslot:%d] = %d\n", rxcoreid, rxslot, txcoreid);
        txslot++;
      }
    }
    printf("\n");
  }
}

// rx slot from txcoreid, rxcoreid, and txslot
int getrxslot(int txcoreid, int rxcoreid, int txslot){
   //printf("rxslots[txcoreid:%d][rxcoreid:%d][txslot:%d] = %d\n", txcoreid, rxcoreid, txslot, rxslots[txcoreid][rxcoreid][txslot]);
   return rxslots[txcoreid][rxcoreid][txslot];
}

// txcoreid from rxcoreid and rxslot
int gettxcoreid(int rxcoreid, int rxslot){
  //printf("txcoreids[rxcoreid:%d][rxslot:%d] = %d\n", rxcoreid, rxslot, txcoreids[rxcoreid][rxslot]);
  return txcoreids[rxcoreid][rxslot];
}

//print support so the threads call printf in order
static pthread_mutex_t printf_mutex;
int sync_printf(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  pthread_mutex_lock(&printf_mutex);
  vprintf(format, args);
  pthread_mutex_unlock(&printf_mutex);

  va_end(args);
}

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
  static pthread_t tid[4];
  static int id[4];

  printf("\nmain1(): Simulation starts\n");
  for (int i = 0; i < 4; ++i)
  {
    id[i] = i;
    int returncode = pthread_create(&tid[i], NULL, coredo, &id[i]);
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
    pthread_join(tid[i], NULL);
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// init patmos (simulated) internals
Core core[CORES];

// the NoC (same for all comm. patterns)
// TDMROUND:
void *nocthreadfunc(void *coreid)
{
  printf("NoC here ...\n");
  TDMROUND_REGISTER = 0;
  for (int n = 0; n < 1; n++)
  { //number of runs
    HYPERPERIOD_REGISTER = 0;
    // one round
    for (int m = 0; m < MEMBUF; m++)
    {                                 // one word from each to each
      for (int i = 0; i < CORES; i++) // from each core
      {
        int jj = 0;
        int ii = 0;
        for (int j = 0; j < CORES - 1; j++) // to the *other* cores
        {
          //if(j == i)
          //  jj++;

          //if(jj > i)
          //  ii--;

          // TODO: map to the real HW

          core[jj].rxmem[ii][m] = core[i].txmem[j][m];
          //jj++;
          //ii++;
        }
      }
    }
    TDMROUND_REGISTER++;
    sync_printf("nocthread: TDMROUND_REGISTER=%lu\n", TDMROUND_REGISTER);
    usleep(100); //much slower than the cores on purpose, so they can poll

    // This should be it:
    HYPERPERIOD_REGISTER++;
    sync_printf("nocthread: HYPERPERIOD_REGISTER=%lu\n", HYPERPERIOD_REGISTER);
    usleep(100);
  }
  // stop the cores
  runnoc = false;
}

// signal used to stop terminate the cores
bool runnoc;

// patmos memory initialization
void initpatmos()
{
  TDMROUND_REGISTER = 0;
  HYPERPERIOD_REGISTER = 0;

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

  //start noc test control
  runnoc = true;
  pthread_t nocthread;
  int returncode = pthread_create(&nocthread, NULL, nocthreadfunc, NULL);
  if (returncode)
  {
    printf("Error %d ... \n", returncode);
    exit(-1);
  }
  sync_printf("runtesttbs(): noc thread created ...\n");
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERNS///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//a noc test controller ('nocthread') will drive the Patmos registers and inject
//  test data for the cores to use
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Time-Based Synchronization (tbs)
///////////////////////////////////////////////////////////////////////////////

void runtesttbs()
{
  sync_printf("start: runtesttbs() ...\n");
  //start cores
  // Just to make it clear that those are accessed by threads
  int id[CORES];
  pthread_t corethreads[CORES];
  for (long i = 0; i < CORES; i++)
  {
    id[i] = i;
    int returncode = pthread_create(&corethreads[i], NULL, corethreadtbs, &id[i]);
    if (returncode)
    {
      sync_printf("Error %d ... \n", returncode);
      exit(-1);
    }
    sync_printf("runtesttbs(): core %ld created ...\n", i);
  }

  // wait
  for (int i = 0; i < CORES; i++)
  {
    pthread_join(corethreads[i], NULL);
  }
  //pthread_join(nocthread, NULL);
  sync_printf("done: runtesttbs() ...\n");
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Handshaking Protocol
///////////////////////////////////////////////////////////////////////////////

void runhandshakeprotocoltest()
{
  sync_printf("start: runtesthsp() ...\n");

  //start cores
  int id[CORES];
  pthread_t corethreads[CORES];

  for (long i = 0; i < CORES; i++)
  {
    id[i] = i;
    int returncode = pthread_create(&corethreads[i], NULL, corethreadhsp, &id[i]);
    if (returncode)
    {
      sync_printf("Error %d ... \n", returncode);
      exit(-1);
    }
    sync_printf("runtesthsp(): core %ld created ...\n", i);
  }

  // wait
  for (int i = 0; i < CORES; i++)
  {
    pthread_join(corethreads[i], NULL);
  }
  //pthread_join(nocthread, NULL);
  sync_printf("done: runtesthsp() ...\n");
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Exchange of state
///////////////////////////////////////////////////////////////////////////////

void runexchangestatetest()
{
  sync_printf("start: runexchangestatetest() ...\n");

  //start cores
  pthread_t corethreads[CORES];
  int id[CORES];
  for (long i = 0; i < CORES; i++)
  {
    id[i] = i;
    int returncode = pthread_create(&corethreads[i], NULL, corethreades, &id[i]);
    if (returncode)
    {
      sync_printf("Error %d ... \n", returncode);
      exit(-1);
    }
    sync_printf("runexchangestatetest(): core %ld created ...\n", i);
  }

  // wait
  for (int i = 0; i < CORES; i++)
  {
    pthread_join(corethreads[i], NULL);
  }
  //pthread_join(nocthread, NULL);
  sync_printf("done: runexchangestatetest() ...\n");
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Streaming Double Buffer (sdb)
///////////////////////////////////////////////////////////////////////////////

void runstreamingdoublebuffertest()
{
  sync_printf("start: runstreamingdoublebuffertest() ...\n");

  //start cores
  pthread_t corethreads[CORES];
  int id[CORES];
  for (long i = 0; i < CORES; i++)
  {
    id[i] = i;
    int returncode = pthread_create(&corethreads[i], NULL, corethreadsdb, &id[i]);
    if (returncode)
    {
      sync_printf("Error %d ... \n", returncode);
      exit(-1);
    }
    sync_printf("runstreamingdoublebuffertest(): core %ld created ...\n", i);
  }

  // wait
  for (int i = 0; i < CORES; i++)
  {
    pthread_join(corethreads[i], NULL);
  }
  //pthread_join(nocthread, NULL);
  sync_printf("done: runstreamingdoublebuffertest() ...\n");
}

///////////////////////////////////////////////////////////////////////////////
//MAIN2
///////////////////////////////////////////////////////////////////////////////

void initsim()
{
  pthread_mutex_init(&printf_mutex, NULL);
  inittxrxmaps();
  //initpatmos();
  //runtesttbs(); // time-based synchronization
  //initpatmos();
  //runhandshakeprotocoltest();
  //initpatmos();
  //runexchangestatetest();
  //initpatmos();
  //runstreamingdoublebuffertest();
}
