/*
  A software simulation of the One-Way Shared Memory
  Runs on the PC

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include "onewaysim.h"

int alltxmem[CORES][TDMSLOTS][WORDS];
int allrxmem[CORES][TDMSLOTS][WORDS];

//pthread_t *threadHandles;

void showmem() {
  printf("showmem():\n");
  for (int coreid = 0; coreid < CORES; coreid++) {
    printf("Core %d\n", coreid);
    for (int slot = 0; slot < TDMSLOTS; slot++) {
      printf("  tx slot %d (to rx core %d, rx slot %d)\n    ", slot, 
             getrxcorefromtxcoreslot(coreid, slot),
             getrxslotfromrxcoretxcoreslot(getrxcorefromtxcoreslot(coreid, slot), coreid, slot));
      for (int w = 0; w < WORDS; w++) {
        printf("0x%08x ", alltxmem[coreid][slot][w]);
        if ((w + 1) % 8 == 0)
          printf("\n    ");
      }
      printf("\n");
      printf("  rx slot %d (from tx core %d, tx slot %d)\n    ", slot, 
             gettxcorefromrxcoreslot(coreid, slot),
             gettxslotfromtxcorerxcoreslot(gettxcorefromrxcoreslot(coreid, slot), coreid, slot));
      for (int w = 0; w < WORDS; w++) {
        printf("0x%08x ", allrxmem[coreid][slot][w]);
        if ((w + 1) % 8 == 0)
          printf("\n    ");
      }
      printf("\n");
    }
  }
}

// called repeatedly from core 0 when *simulating* on the PC
// the granularity is set to a tdmround, which means one word (index w) is 
// delivered (instantly) from all cores to to all cores individually
void simcontrol(int w)
{
  //sync_printf(0, "entering simcontrol()...(simulating on the PC)\n");
  for (int txcoreid = 0; txcoreid < CORES; txcoreid++) {
    for (int txslot = 0; txslot < TDMSLOTS; txslot++) {
      int rxcoreid = getrxcorefromtxcoreslot(txcoreid, txslot);
      for (int rxslot = 0; rxslot < TDMSLOTS; rxslot++) {
        int txcoreid_for_rxcoreidrxslot = gettxcorefromrxcoreslot(rxcoreid, rxslot);
        if (txcoreid == txcoreid_for_rxcoreidrxslot) {
          core[rxcoreid].rx[rxslot][w] = core[txcoreid].tx[txslot][w];
          break;
        }
      }
    }
  }
}

// Clear alltxmem and allrxmem
void nocmem() {
  // map (pointers) alltxmem and allrxmem to core memory
  for (int i = 0; i < CORES; i++) {
    for (int j = 0; j < TDMSLOTS; j++) {
      core[i].tx[j] = alltxmem[i][j];
      core[i].rx[j] = allrxmem[i][j];
    }
  }

  // clear mem
  for (int i = 0; i < CORES; i++) {
    for (int j = 0; j < TDMSLOTS; j++) {
      for (int m = 0; m < WORDS; m++) {
        core[i].tx[j][m] = 0;
        core[i].rx[j][m] = 0;
      }
    }
  }
}

// simulator memory initialization
// function pointer to one of the test cases
void nocinit(void (*corefuncptr)(void *))
{
  sync_printf(0, "in nocinit()...\n");
  txrxmapsinit();
  showmappings();

  // set the state struct to zero
  memset(states, 0, sizeof(states));
  for (int c = 0; c < CORES; c++) {
    states[c].runcores = true;
  }

  // not using 0
  //  threadHandles = calloc(CORES, sizeof(pthread_t));

  // do the memory mapping for running the simulator on the PC
  nocmem();

  //showmem();

  // init signals and counters
  HYPERPERIOD_REGISTER = 0;
  TDMROUND_REGISTER = 0;
  for (int c = 0; c < CORES; c++) {
    coreid[c] = c;
  }

  // start the cores
  for (int c = 0; c < CORES; c++) {
    //the typecast 'void * (*)(void *)' is because pthread expects a function that returns a void*
    //pthread_create(&threadHandles[c], NULL, (void *(*)(void *)) corefuncptr,
    //               &coreid[c]);
    //corefuncptr(&coreid[c]);
  }

  // route like the NoC would have done
  for (int w = 0; w < WORDS; w++)
    simcontrol(w);
}

void noccontrol(void (*corefuncptr)(void *))
{
  //sync_printf(0, "in noccontrol: simulation control when just running on host PC\n");

  while (states[0].runcores){
    for (int c = 0; c < CORES; c++){
      corefuncptr(&c);
    }

    // route like the NoC
    for (int w = 0; w < WORDS; w++)
      simcontrol(w);

  }
}

void nocdone()
{
  sync_printf(0, "in nocdone: cores to join ...\n");
  for (int c = 0; c < CORES; c++) {
    sync_printf(0, "  Core %d success: %d\n", c, states[c].coredone);
  }
}

// simulator: called by each core each time it starts a new loop in the control loop
void precoreloopwork(int loopcnt) {
  //printf("precoreloopwork: loopcnt=%d\n", loopcnt);
  //sched_yield();
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

  // thread function pointer which is the use case
  void (*corefuncptr)(void *);

#if USECASE==0
  printf("USECASE = 0\n");
  corefuncptr = &corethreadtestwork;
#elif USECASE==1
  printf("USECASE == 1\n");
  corefuncptr = &corethreadtbswork;
#elif USECASE==2
  printf("USECASE == 2\n");
  corefuncptr = &corethreadhswork;
#elif USECASE==3
  printf("USECASE == 3\n");
  corefuncptr = &corethreadeswork;
#elif USECASE==4
  printf("USECASE == 4\n");
  corefuncptr = &corethreadsdbwork;
#else
  printf("Unimplemented USECASE value. Exit\n");
  exit(0);
#endif

  runcores = true;

  // start the slave threads
  nocinit(corefuncptr);

  noccontrol(corefuncptr);

  // "start" core 0
  //corefuncptr(&coreid[0]);

  nocdone();

  printf("Done...\n");

  for (int i = 0; i < CORES; ++i) {
    printf("Sync print from core %d:\n", i);
    sync_print_core(i);
  }

  printf("****************************************************************\n");
  printf("Leaving main...done with highlevel simulation\n");
  printf("(Remember: Cycles on the PC simulator are *not* real HW cycles)\n");
  printf("****************************************************************\n");
  return 0;
}
