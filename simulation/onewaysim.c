/*
  A software simulation of the One-Way Shared Memory

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

// init patmos (simulated) internals 
static bool runnoc;

// the NoC (same for all comm. patterns)
void *nocthreadfunc(void *coreid)
{
  printf("NoC here ...\n");
  TDMROUND_REGISTER = 0;
  for (int n=0; n<10; ++n) { //number of runs
    sync_printf("nocthread: TDMCYCLE_REGISTER=%lu\n", TDMCYCLE_REGISTER);
    for(int j=0; j<MEMBUF; j++){
      for(int i=0; i<CORES; i++){ // tx
        sync_printf("nocthread: TDMROUND_REGISTER=%lu\n", TDMROUND_REGISTER);
        for(int k=0; k<CORES; k++){ //rx
          core[k].rx[j] = core[i].tx[j];  
        }
      }
      usleep(100); //much slower than the cores on purpose, so they can poll
    }
    // This should be it:
    TDMCYCLE_REGISTER++;
    usleep(100);
  }
  // stop the cores
  runnoc = false;
}

void initpatmos()
{
  TDMROUND_REGISTER = 0;
  TDMCYCLE_REGISTER = 0;
  // zero tx and rx
  for(int i=0; i<CORES; i++){ // tx
    for(int j=0; j<MEMBUF; j++){
      core[i].tx[j] = 0;
      core[i].rx[j] = 0;
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

// the cores
void *corethreadtbs(void *coreid)
{
  long cid = (long)coreid;
  int step = 0;
  unsigned long tdmcycle = 0xFFFFFFFF;
  unsigned long tdmround = 0xFFFFFFFF;
  while(runnoc){
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
  //pthread_join(nocthread, NULL);
  sync_printf("done: runtesttbs() ...\n");
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Handshaking Protocol
///////////////////////////////////////////////////////////////////////////////

// a struct for handshaking
typedef struct handshakemsg_t{
  // if non-zero then valid msg
  // if respid = 0 then it is a request
  unsigned long reqid; 
  // if non-zero then it is a response
  unsigned long respid;
  unsigned long data1;
  unsigned long data2;
} handshakemsg_t;

// the cores
void *corethreadhsp(void *coreid)
{
  long cid = (long)coreid;
  int step = 0;
  unsigned long tdmcycle = 0xFFFFFFFF;
  unsigned long tdmround = 0xFFFFFFFF;
  handshakemsg_t txmsg;
  memset(&txmsg, 0, sizeof(txmsg));
  handshakemsg_t rxmsg;
  memset(&rxmsg, 0, sizeof(rxmsg));
    
  //kickstart with sending a request to each of the other cores
  txmsg.reqid = 1; // want to get message 1 from another core (and no more)
  memcpy(&core[cid].tx[0], &txmsg, sizeof(txmsg));

  while(runnoc){
    // the cores are aware of the global cycle times for time-based-synchronization
    // the cores print their output after the cycle count changes are detected
    if(tdmcycle != TDMCYCLE_REGISTER){
      //sync_printf("Core #%ld: TDMCYCLE_REGISTER(*)=%lu, TDMROUND_REGISTER   =%lu\n", cid, TDMCYCLE_REGISTER, TDMROUND_REGISTER);  
      tdmcycle = TDMCYCLE_REGISTER;
    }
    if(tdmround != TDMROUND_REGISTER){
      // copy what we got
      memcpy(&rxmsg, core[cid].rx, sizeof(rxmsg));
      // first we will see the request for message id 1 arriving at each core
      // then we will see that each core responds with message 1 and some data
      // it keeps repeating the same message until it gets another request for a new
      // message id
      sync_printf("Core #%ld: TDMCYCLE_REGISTER   =%lu, TDMROUND_REGISTER(*)=%lu,\n  rxmsg.reqid=%lu,\n  rxmsg.respid=%lu,\n  rxmsg.data1=%lu,\n  rxmsg.data2=%lu\n", cid, TDMCYCLE_REGISTER, TDMROUND_REGISTER, rxmsg.reqid, rxmsg.respid, rxmsg.data1, rxmsg.data2);  
      // is it a request?
      if (rxmsg.reqid > 0 && rxmsg.respid == 0){
        memset(&txmsg, 0, sizeof(txmsg));
        txmsg.reqid = rxmsg.reqid;
        txmsg.respid = rxmsg.reqid;
        txmsg.data1 = step;
        txmsg.data2 = cid;
        // schedule for sending (tx)
        memcpy(core[cid].tx, &txmsg, sizeof(txmsg));
      }
      
      tdmround = TDMROUND_REGISTER;
    }
    step++;
  }
}

void runhandshakeprotocoltest(){
  sync_printf("start: runtesthsp() ...\n");

  //start cores 
  pthread_t corethreads[CORES];
  for (long i = 0; i < CORES; i++)
  {
    int returncode = pthread_create(&corethreads[i], NULL, corethreadhsp, (void *)i);
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

// a struct for handshaking
typedef struct es_msg_t{
  // if non-zero then valid msg
  // if respid = 0 then it is a request
  unsigned long reqid; 
  // if non-zero then it is a response
  unsigned long respid;
  //sensor id
  unsigned long sensorid;
  //sensor value
  unsigned long sensorval;
} es_msg_t;

// the cores
void *corethreades(void *coreid)
{
  long cid = (long)coreid;
  int step = 0;
  unsigned long tdmcycle = 0xFFFFFFFF;
  unsigned long tdmround = 0xFFFFFFFF;
  handshakemsg_t txmsg;
  memset(&txmsg, 0, sizeof(txmsg));
  handshakemsg_t rxmsg;
  memset(&rxmsg, 0, sizeof(rxmsg));
    
  //kickstart with sending a request to each of the other cores
  txmsg.reqid = 1; // want to get message 1 from another core (and no more)
  memcpy(&core[cid].tx[0], &txmsg, sizeof(txmsg));

  while(runnoc){
    // the cores are aware of the global cycle times for time-based-synchronization
    // the cores print their output after the cycle count changes are detected
    if(tdmcycle != TDMCYCLE_REGISTER){
      //sync_printf("Core #%ld: TDMCYCLE_REGISTER(*)=%lu, TDMROUND_REGISTER   =%lu\n", cid, TDMCYCLE_REGISTER, TDMROUND_REGISTER);  
      tdmcycle = TDMCYCLE_REGISTER;
    }
    if(tdmround != TDMROUND_REGISTER){
      // copy what we got
      memcpy(&rxmsg, core[cid].rx, sizeof(rxmsg));

      // see handshaking protocol (which this is built upon)

      // is it a request?
      if (rxmsg.reqid > 0 && rxmsg.respid == 0){
        sync_printf("Core #%ld sensor request: TDMCYCLE_REGISTER   =%lu, TDMROUND_REGISTER(*)=%lu,\n  rxmsg.reqid=%lu,\n  rxmsg.respid=%lu,\n  rxmsg.data1=%lu,\n  rxmsg.data2=%lu\n", cid, TDMCYCLE_REGISTER, TDMROUND_REGISTER, rxmsg.reqid, rxmsg.respid, rxmsg.data1, rxmsg.data2);  
        memset(&txmsg, 0, sizeof(txmsg));
        txmsg.reqid = rxmsg.reqid;
        txmsg.respid = rxmsg.reqid;
        txmsg.data1 = cid;
        txmsg.data2 = rxmsg.reqid + rxmsg.reqid + cid; // not real
        // schedule for sending (tx)
        memcpy(core[cid].tx, &txmsg, sizeof(txmsg));
      }

      // is it a response?
      if (rxmsg.reqid > 0 && rxmsg.respid == rxmsg.reqid){
        sync_printf("Core #%ld sensor reply: TDMCYCLE_REGISTER   =%lu, TDMROUND_REGISTER(*)=%lu,\n  rxmsg.reqid=%lu,\n  rxmsg.respid=%lu,\n  rxmsg.sensorid=%lu,\n  rxmsg.sensorval=%lu\n", cid, TDMCYCLE_REGISTER, TDMROUND_REGISTER, rxmsg.reqid, rxmsg.respid, rxmsg.data1, rxmsg.data2);  
      }
      
      tdmround = TDMROUND_REGISTER;
    }
    step++;
  }
}

void runexchangestatetest(){
  sync_printf("start: runtesthsp() ...\n");
  
  //start cores 
  pthread_t corethreads[CORES];
  for (long i = 0; i < CORES; i++)
  {
    int returncode = pthread_create(&corethreads[i], NULL, corethreades, (void *)i);
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
  //runtesttbs(); // time-based synchronization
  //runhandshakeprotocoltest();
  runexchangestatetest();
  //runstreamingdoublebuffertest();
}

///////////////////////////////////////////////////////////////////////////////
//shared main
///////////////////////////////////////////////////////////////////////////////
// MS: yes, we are missing Java with the option on having more than one main entry
// into a project :-(

int main(int argc, char *argv[])
{
  // synchronized printf 
  pthread_mutex_init(&printf_mutex, NULL);

  printf("\n");
  printf("*******************************************************************************\n");
  printf("Calling 'onewaysim' main(): ***************************************************\n");
  printf("*******************************************************************************\n");
  //main1();
  main2();
  printf("\n");
}
