/*
    Small test program for the One-Way Shared Memory

    Author: Martin Schoeberl
    Copyright: DTU, BSD License
*/

#include <stdio.h>
#include <stdlib.h>
#include <machine/patmos.h>
#include <machine/spm.h>


#include "libcorethread/corethread.h"

// for faster debugging
#include "patio.h"
#include "syncprint.h"

#define CNT 4
#define ONEWAY_BASE ((volatile _IODEV int *) 0xE8000000)
#define WORDS 256

// Shared data in main memory for the return value
volatile _UNCACHED static int field;
volatile _UNCACHED static int end_time;

void spin(unsigned int waitcycles){
  unsigned int start = getcycles();
  while((getcycles()-start) <= waitcycles);
}

void tx(int id){
  volatile _SPM int *txMem = (volatile _IODEV int *) ONEWAY_BASE;
  int msg = 0;
  for (int j=0; j<WORDS; ++j) {
    for (int i=0; i<CNT-1; ++i) {
      txMem[i*WORDS + j] = id*0x1000000 + i*0x100000 + j*0x10000 + msg;
      msg++;
    }
  }

}	

void rx(int id){
  volatile _SPM int *rxMem = (volatile _IODEV int *) ONEWAY_BASE;
  for (int j=0; j<WORDS; ++j) {
    for (int i=0; i<CNT-1; ++i) {
      if(j < 4)	    
        sync_printf(id, "RX: id %d, CNT %d, WORD %d, rxMem[0x%04x] 0x%04x_%04x\n", 
                  id, i, j, i*WORDS + j, rxMem[i*WORDS + j]>>16, rxMem[i*WORDS + j]&0xFFFF);
    }
  }
}

// The main function for the other cores
void work(void* arg) {
  tx(get_cpuid());
  spin(CNT*WORDS);
  rx(get_cpuid());
}

// main function for core 0
int main() {
  info_printf("Starting...\n");

  // TX on core 0
  tx(0);

  // Create/run the other NoC threads
  for (int i=1; i<get_cpucnt(); ++i) {
    corethread_create(i, &work, NULL); 
  }

  // wait for tx to "settle"
  spin(CNT*WORDS);
  
  // RX on core 0
  rx(0);

  for (int i=0; i<CNT; ++i) {
    printf("Core %d:\n", i);
    sync_print_core(i);
  }
 
  return 0;
}
