/*
  One-Way Shared Memory
  Runs on patmos

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include "onewaysim.h"

// the functions which are called by threads are appended "work"
// cores other than core 0 are called "slave" cores because core 0 have a dual role
//   both as a thread work core and as the noc control core.

void statework(State **state, int cpuid) {
  // stack allocation
  State statevar;
  memset(&statevar, 0, sizeof(statevar));
  *state = &statevar;
  (*state)->runcore = true;
  holdandgowork(cpuid);
}

void nocmeminit()
{
  // map ms's core mem arrray for indexed access
  for (int i = 0; i < CORES; i++)
  { 
    for (int j = 0; j < TDMSLOTS; j++)
    { 
      //core[i].txmem[j] will point to a row of WORDS words 
      // SPM will point to core local memory. Not the same as on the PC
      core[i].tx[j] = (volatile _IODEV int *) (ONEWAY_BASE + j*WORDS);
      core[i].rx[j] = (volatile _IODEV int *) (ONEWAY_BASE + j*WORDS);
    }
  }
}

// patmos memory initialization
// function pointer to one of the test cases
void nocinit()
{
  printf("in nocinit()...\n");
  txrxmapsinit();
  showmappings();

  nocmeminit();

  // init signals and counters
  HYPERPERIOD_REGISTER = 0;
  TDMROUND_REGISTER = 0;
}

// start the slave cores
void nocstart(void (*corefuncptr)(void *)){
  // START STEPS 0, 1, and 2
  
  //0. start the other cores
  runcores = true;
  
  for(int i = 0; i < CORES; i++){ 
    coreready[i] = false;
    coredone[i] = false;
    coreid[i] = i;
  }

  // 1. start the slave cores 1..CORES-1
  for (int c = 1; c < CORES; c++)
    corethread_create(c, corefuncptr, &coreid[c]);

  // 2. "start" ourself (core 0)
  corefuncptr(&coreid[0]);
}

void nocwaitdone()
{
  sync_printf(0, "waiting for slave cores to join ...\n");
  int *retval;
  // now let the others join
  for (int c = 1; c < CORES; c++)
  {
    sync_printf(0, "slave core/thread %d to join...\n", c);
    // the cores join here when done...
    corethread_join(c, (void **)&retval);
   sync_printf(0, "slave core/thread %d joined\n", c);
  }
}

///////////////////////////////////////////////////////////////////////////////
//patmos main
///////////////////////////////////////////////////////////////////////////////

// we are core 0
int main(int argc, char *argv[])
{
  setlocale(LC_ALL,"");
  printf("*******************************************\n");
  printf("****onewaymem: run use-case %d on patmos****\n", USECASE);
  printf("*******************************************\n");

  // thread function pointer which is the use case
  void (*corefuncptr)(void *);
  
#if USECASE==0
  printf("USECASE = 0; corethreadtestwork\n");
  corefuncptr = &corethreadtestwork;
#elif USECASE==1
  printf("USECASE == 1: corethreadtbswork\n");
  corefuncptr = &corethreadtbswork;
#elif USECASE==2
  printf("USECASE == 2: corethreadhswork\n");
  corefuncptr = &corethreadhswork;
#elif USECASE==3
  printf("USECASE == 3: corethreadeswork\n");
  corefuncptr = &corethreadeswork;
#elif USECASE==4
  printf("USECASE == 4: corethreadsdbwork\n");
  corefuncptr = &corethreadsdbwork;
#else
  printf("Unimplemented USECASE value. Exit...\n");
  exit(0);
#endif

  printf("Init...\n");
  nocinit();
  printf("Start...\n");
  
  nocstart(corefuncptr);
  
  printf("Wait...\n");
  
  // core 0 is done, wait for the others
  nocwaitdone();
  printf("Done...\n");
  
  for (int i=0; i<CORES; ++i) {
    printf("Sync print from core %d:\n", i);
    sync_print_core(i);
  }

  printf("***********************************\n");
  printf("Use-case %d result [pass/fail]: %s\n", USECASE, allfinishedok());
  printf("***********************************\n");
  
  return 0;
}
