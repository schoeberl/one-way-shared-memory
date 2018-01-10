/*
  A software simulation of the One-Way Shared Memeory

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

// start with hard coded 4 cores for a quick start

unsigned long alltxmem[4][1000];
unsigned long allrxmem[4][1000];

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
    fflush(stdout);
    alltxmem[id][0] = id * 10 + id;
    usleep(100000);
  }
  //return NULL;
}

int main1()
{

  // Just to make it clear that those are accessed by threads
  static pthread_t tid[4];
  static int id[4];

  printf("Simulation starts\n");
  for (int i = 0; i < 4; ++i)
  {
    id[i] = i;
    // (void *)i: Do not pass a pointer local variable that goes out
    // of scope into a thread. The address may point to the i from the next
    // loop when accessed by a thread.
    // rup: It was not a pointer local variable.
    //      The value of the void pointer was cast as long on the first line in coredo
    //      to give the thread id.
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
//TODO: merge the code above and below
///////////////////////////////////////////////////////////////////////////////

// Notes
// Structs are pointer
// Data, such as flits and arrays of unsigned longs are not pointers

//#include <pthread.h>
//#include <stdio.h>
//#include <stdlib.h>
//#define flit unsigned long

// noc configuration
#define CORES 4
// one core configuration
#define MEMBUF 4 // 32-bit words
#define TXRXMEMSIZE (MEMBUF * (CORES - 1))

// patmos hardware registers provided via Scala HDL
typedef volatile unsigned long PATMOS_REGISTER;

// defined patmos hardware registers (simulated)
// one word delivered from all to all
PATMOS_REGISTER TDMROUND_REGISTER; 
// one memory block delivered (word for word) from all to all
PATMOS_REGISTER TDMCYCLE_REGISTER;


// simulation structs

typedef struct Core
{
  // some function to be run by the core
  //int (*run)(int)
  unsigned long tx[TXRXMEMSIZE];
  unsigned long rx[TXRXMEMSIZE];
} Core;

// declare noc consisting of cores
static Core core[CORES];

// functions //

// init the "network-on-chip" NoC
void initnoc()
{
  // noc tiles init
  for (int i = 0; i < CORES; i++)
  {
      core[i].tx[0] = i;     // "i,j" tx test data ...
  }
}

void *corerun(void *coreid)
{
  long cid = (long)coreid;
  printf("NoC: core #%ld here ...\n", cid);
  pthread_exit(NULL);
}

int main2()
{

  initnoc();

  for (int i = 0; i < CORES; i++)
  {
    printf("core[%d] tx data: 0x%08lx\n", i, core[i].tx[0]);
  }



  // pthread_t threads[CORES];
  // for (long i = 0; i < CORES; i++)
  // {
  //   printf("Init nocsim: core %ld created ...\n", i);
  //   int returncode = pthread_create(&threads[i], NULL, corerun, (void *)i);
  //   if (returncode)
  //   {
  //     printf("Error %d ... \n", returncode);
  //     exit(-1);
  //   }
  // }

  //pthread_exit(NULL);
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Time-Based Synchronization////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void runtimebasedsynchronizationtest(){
  //start noc test control

  //start cores 

}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Handshaking Protocol//////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void runhandshakeprotocoltest(){
  //start noc test control
  
  //start cores  
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Exchange of state/////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void runexchangestatetest(){
  //start noc test control
  
  //start cores
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Streaming Double Buffer///////////////////////////////
///////////////////////////////////////////////////////////////////////////////


void runstreamingdoublebuffertest(){
  //start noc test control
  
  //start cores 
}










// first merge by retaining both versions fully
int main(int argc, char *argv[])
{
  printf("*********************************************************************************\n");
  printf("Calling 'onewaysim' main1(): ***************************************************\n");
  printf("*********************************************************************************\n");
  main1();
  printf("\n");

  printf("*********************************************************************************\n");
  printf("Calling 'nocsim' main2(): *****************************************************\n");
  printf("*********************************************************************************\n");
  main2();
  printf("\n");
}
