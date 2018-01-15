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
  txmem = alltxmem[id];
  unsigned long *rxmem;
  rxmem = allrxmem[id];

  printf("I am core %d\n", id);
  // this should be the work done
  for (int i = 0; i < 10; ++i)
  {
    printf("%d: %lu, ", id, allrxmem[id][0]);
    //"bug": it does not flush at it runs, only at the end
    // Martin: it does it on my machine
    // rup: I think the threads are preempted a few times, so that is why I thought the 
    // printf output was strange:-) it is intermixed
    fflush(stdout);
    alltxmem[id][0] = id * 10 + id;
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
    // (void *)i: Do not pass a pointer local variable that goes out
    // of scope into a thread. The address may point to the i from the next
    // loop when accessed by a thread.
    // rup: It was not a pointer local variable.
    //      The value of the void pointer was cast as long on the first line in coredo
    //      to give the thread id.
    // Ok, my comment was not exact. Don't want to be picky, but
    // it was a pointer *to* a local variable, which should be avoided.
    int returncode = pthread_create(&tid[i], NULL, coredo, &id[i]);
  }

  // main is simulating the data movement by the NIs through the NoC
  for (int i = 0; i < 10; ++i)
  {
    // This is VERY incomplete and does not reflect the actual indexing
    // And that it is more than one word in the memory blocks
    allrxmem[0][0] = alltxmem[1][3];
    allrxmem[1][0] = alltxmem[3][0];
    allrxmem[2][0] = alltxmem[2][0];
    allrxmem[3][0] = alltxmem[1][0];
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
    printf("alltxmem[%d][0]: 0x%08lx\n", i, alltxmem[i][0]);
  }

  for (int i = 0; i < 4; ++i)
  {
    printf("allrxmem[%d][0]: 0x%08lx\n", i, allrxmem[i][0]);
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
  for (int n=0; n<1; n++) { //number of runs
    HYPERPERIOD_REGISTER = 0;
    // one round
    for(int j=0; j<MEMBUF; j++){
      // one word from each to each
      for(int i=0; i<CORES; i++){ // tx
        for(int k=0; k<CORES; k++){ //rx
          // The following is one memory mapping, but not the one from the real hardware
          core[k].rxmem[i*MEMBUF+j] = core[i].txmem[k*MEMBUF+j];  
        }
      }
      TDMROUND_REGISTER++;
      sync_printf("nocthread: TDMROUND_REGISTER=%lu\n", TDMROUND_REGISTER);
      usleep(100); //much slower than the cores on purpose, so they can poll
    }
    // This should be it:
    HYPERPERIOD_REGISTER++;
    sync_printf("nocthread: HYPERPERIOD_REGISTER=%lu\n", HYPERPERIOD_REGISTER);
    usleep(100);
  }
  // stop the cores
  runnoc = false;
}

bool runnoc;
void initpatmos()
{
  TDMROUND_REGISTER = 0;
  HYPERPERIOD_REGISTER = 0;

  // zero tx and rx
  for(int i=0; i<CORES; i++){ // tx
    core[i].rxmem = allrxmem[i];
    core[i].txmem = alltxmem[i];
    for(int j=0; j<MEMBUF; j++){
      core[i].txmem[j] = 0;
      core[i].rxmem[j] = 0;
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

void runtesttbs(){
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

void runhandshakeprotocoltest(){
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

void runexchangestatetest(){
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

void runstreamingdoublebuffertest(){
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
 
  initpatmos();
  runtesttbs(); // time-based synchronization
  initpatmos();
  runhandshakeprotocoltest();
  initpatmos();
  runexchangestatetest();
  initpatmos();
  runstreamingdoublebuffertest();
}
