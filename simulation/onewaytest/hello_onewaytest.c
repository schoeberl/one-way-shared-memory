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

void print_hex(int val) {
  for (int j=0; j<8; ++j) {
    int c = XDIGIT((val >> (4 * (7-j))) & 0x0f);
    WRITECHAR(c);
  }
  WRITECHAR('\n');
}


#define CNT 4
#define ONEWAY_BASE *((volatile _SPM int *) 0xE8000000)
#define WORDS 256

// Shared data in main memory for the return value
volatile _UNCACHED static int field;
volatile _UNCACHED static int end_time;

// The main function for the other threads on the another cores
void work(void* arg) {

  // Measure execution time with the clock cycle timer
  volatile _IODEV int *timer_ptr = (volatile _IODEV int *) (PATMOS_IO_TIMER+4);
  volatile _SPM int *mem_ptr = (volatile _IODEV int *) (0xE8000000);

  *mem_ptr = 0xabcd;

  int id = get_cpuid();
  for (int i=0; i<CNT; ++i) {
    for (int j=0; j<WORDS; ++j) {
      *mem_ptr++ = id*0x100 + i*0x10 + j;
    }
  }
  
  // burn some cycles so RX is ready
  for (int i=0; i<1000; ++i) {
    field = field + 1;
  }

  // TODO
  // get the RX table from Core 2 printed out
  // it should look like this (Fig 3 in the paper):
  // [2] from core 3
  // [1] from core 0
  // [0] from core 1
  // but the code below prints
  //[cycle=688384776, core=2, msg#= 1] RX: id 2, CNT 0, WORD 1, *mem_ptr 0x00000101
  //[cycle=688471464, core=2, msg#= 2] RX: id 2, CNT 0, WORD 2, *mem_ptr 0x00000100
  //[cycle=688558152, core=2, msg#= 3] RX: id 2, CNT 0, WORD 3, *mem_ptr 0x00000103
  // which make is seem like Core 2 only get words from core 1  
  for (int i=0; i<CNT; ++i) {
    for (int j=0; j<4; ++j) {
      if(i == 0 && id == 2)
	sync_printf(id, "RX: id %d, CNT %d, WORD %d, *mem_ptr 0x%08x\n", id, i, j, mem_ptr[i*WORDS + j]);
    }
  }
}

int main() {

  // Pointer to the deadline device
  volatile _IODEV int *dead_ptr = (volatile _IODEV int *) PATMOS_IO_DEADLINE;
  // Measure execution time with the clock cycle timer
  volatile _IODEV int *timer_ptr = (volatile _IODEV int *) (PATMOS_IO_TIMER+4);
  volatile _SPM int *mem_ptr = (volatile _IODEV int *) (0xE8000000);


  for (int i=1; i<get_cpucnt(); ++i) {
    corethread_create(i, &work, NULL); 
  }

  printf("Results\n:");
  sync_printall();

  return 0;
}
