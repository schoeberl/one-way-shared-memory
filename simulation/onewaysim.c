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

int mainms()
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
#define flit unsigned long

// noc configuration
#define CORES 4
// one core configuration
#define MEMBUF 4 // 32-bit words
#define TXRXMEMSIZE (MEMBUF * (CORES - 1))

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





// end route planner
///////////////////////////////////////////////////////////////////////////////

int mainrup()
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

// first merge by retaining both versions fully
int main(int argc, char *argv[])
{
  printf("*********************************************************************************\n");
  printf("Calling 'onewaysim' mainms(): ***************************************************\n");
  printf("*********************************************************************************\n");
  mainms();
  printf("\n");

  printf("*********************************************************************************\n");
  printf("Calling 'nocsim' mainrup(): *****************************************************\n");
  printf("*********************************************************************************\n");
  mainrup();
  printf("\n");
}

// Temp notes for a little design thinking //
// For a simulation harness, we can make a "nocsim" harness that models the main components in
// C structs, but are not meant to be run on the target (patmos). It has structs like
// one-way-mem-noc struct, router structs, links structs, ni struct, tx mem struct, rx mem struct,
// core struct, node (consists of core, rx, tx, ni) struct, tile (consists of node, router,
// and links) struct.
// Clocking: It is perhaps best to think in cycles as each cycle is a mini-transaction. Milli-second
// based clocking is not going to work. It is fairly easy to use this clocking "scheme" in a
// multithreaded simulator as the (nice) mesochronous clock property makes it possible
// to read the current clock cycle, but care has to be taken to remember that another
// router is perhaps already shifted into the next clock cycle.
// Ideas for hard simulations, hard calculations:
//   1) Each core wants to read on each cycle (we must be able to *calculate* WCET here as the
//        sum of routing, latency, TDM scheduling *and* local app code wcet (platin analysis of a
//        given benchmark C code).
//   2) Each core wants to to write on each cycle (same argument as above, we should be able to
//        calculate it.

// https://llvm.org/docs/CodingStandards.html
// https://stackoverflow.com/questions/31082559/warning-passing-argument-1-of-pthread-join-makes-integer-from-pointer-without