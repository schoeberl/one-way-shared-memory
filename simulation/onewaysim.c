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
#include <stdarg.h>

//print stuff
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

// NoC configuration
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

// shared common simulation structs

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

// init patmos (simulated) internals 
void initpatmos()
{
  TDMROUND_REGISTER = 0;
  TDMCYCLE_REGISTER = 0;
}



///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERNS///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//a noc test controller will drive the patmost registers and inject
//  test data for the cores to use
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Time-Based Synchronization (tbs)
///////////////////////////////////////////////////////////////////////////////

static bool runtbs;

// the NoC
void *nocthreadtbs(void *coreid)
{
  printf("NoC here ...\n");
  for(int i=0; i<CORES; i++){ // tx
    sync_printf("nocthread: TDMCYCLE_REGISTER=%lu\n", TDMCYCLE_REGISTER);
    for(int j=0; j<MEMBUF; j++){
      sync_printf("nocthread: TDMROUND_REGISTER=%lu\n", TDMROUND_REGISTER);
      for(int k=0; k<CORES; k++){ //rx
        core[k].rx[j] = core[i].tx[j];  
      }
      TDMROUND_REGISTER++;
      usleep(1000); // 10 ms
    }
    TDMROUND_REGISTER = 0;
    TDMCYCLE_REGISTER++;
  }
  // stop the cores
  runtbs = false;
}

// the cores
void *corethreadtbs(void *coreid)
{
  long cid = (long)coreid;
  int step = 0;
  unsigned long tdmcycle = 0xFFFFFFFF;
  unsigned long tdmround = 0xFFFFFFFF;
  while(runtbs){
    // the cores are aware of the global cycle times for time-based-synchronization
    // the cores print their output after the cycle count changes are detected
    if(tdmcycle != TDMCYCLE_REGISTER){
      sync_printf("Core #%ld: TDMCYCLE_REGISTER(*)=%lu, TDMROUND_REGISTER   =%lu\n", cid, TDMCYCLE_REGISTER, TDMROUND_REGISTER);  
      tdmcycle = TDMCYCLE_REGISTER;
    }
    if(tdmround != TDMROUND_REGISTER){
      sync_printf("Core #%ld: TDMCYCLE_REGISTER   =%lu, TDMROUND_REGISTER(*)=%lu\n", cid, TDMCYCLE_REGISTER, TDMROUND_REGISTER);  
      tdmround = TDMROUND_REGISTER;
    }
    step++;
  }
}

void runtesttbs(){
  sync_printf("start: runtesttbs() ...\n");
  runtbs = true;
  //start noc test control
  pthread_t nocthread;
  int returncode = pthread_create(&nocthread, NULL, nocthreadtbs, NULL);
  if (returncode)
  {
    printf("Error %d ... \n", returncode);
    exit(-1);
  }
  sync_printf("runtesttbs(): noc thread created ...\n");
  
  //start cores 
  pthread_t corethreads[CORES];
  for (long i = 0; i < CORES; i++)
  {
    int returncode = pthread_create(&corethreads[i], NULL, corethreadtbs, (void *)i);
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
  pthread_join(nocthread, NULL);
  sync_printf("done: runtesttbs() ...\n");
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Handshaking Protocol
///////////////////////////////////////////////////////////////////////////////

void runhandshakeprotocoltest(){
  //start noc test control
  
  //start cores  
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Exchange of state
///////////////////////////////////////////////////////////////////////////////

void runexchangestatetest(){
  //start noc test control
  
  //start cores
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Streaming Double Buffer
///////////////////////////////////////////////////////////////////////////////


void runstreamingdoublebuffertest(){
  //start noc test control
  
  //start cores 
}

// main2
int main2()
{
  // always called
  initpatmos();

  // call one of these tests (and create your own as needed)
  runtesttbs(); // time-based synchronization
  //runhandshakeprotocoltest();
  //runexchangestatetest();
  //runstreamingdoublebuffertest();
}

///////////////////////////////////////////////////////////////////////////////
//shared main
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
  // synchronized printf 
  pthread_mutex_init(&printf_mutex, NULL);

  printf("\n");
  printf("*******************************************************************************\n");
  printf("Calling 'onewaysim' main(): ***************************************************\n");
  printf("*******************************************************************************\n");
  main1();
  main2();
  printf("\n");
}
