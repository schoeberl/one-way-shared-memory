/*
  Software layer for One-Way Shared Memory
  Use-case 4: Streaming buffer

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include "onewaysim.h"

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Streaming Double Buffer (sdb)
///////////////////////////////////////////////////////////////////////////////
// there is a number of buffers overlaid on the tx and rx memory for each core
// the size of each of these double buffers is DBUFSIZE 

void corethreadsdbwork(void *cpuidptr) {
  int cpuid = *((int*)cpuidptr);
  State *state;
  #ifdef RUNONPATMOS
  State statevar;
  memset(&statevar, 0, sizeof(statevar));
  state = &statevar;
  #else 
  state = &states[cpuid];
  #endif
  if (state->loopcount == 0) 
    sync_printf(cpuid, "in corethreaddbwork(%d)...\n", cpuid);
  
  //sync_printf(cpuid, "Core %d started...DOUBLEBUFFERS=%d, TDMSLOTS=%d, DBUFSIZE=%d, WORDS=%d\n", 
  //  cpuid, DOUBLEBUFFERS, TDMSLOTS, DBUFSIZE, WORDS);

  // use casse setup: each tx and rx tdm slot have two (or more) double buffers
  // each buffer_t represents one (of at least two) for each TDM slot (each 
  //   TDM slot is WORDS long)
  // so for a double buffer on TDM slot 0, there would be two buffer_t stuct objects
  // they look like arrays (of arrays) as written below, but 
  // inside each buffer_t is a pointer to an unsigned int, which is
  // is set up to point to the part of either the rx or tx memory buffer that 
  // a particular buffer_t "controls".

  // init the data pointer in all of the double buffers
  // it will print something like this:
  //   [cy592233042 id00 #01] buf_out[0][0].data address = 0xe8000000
  //   [cy592305786 id00 #02] buf_in[0][0].data address  = 0xe8000000
  //   as the tx and rx are memory mapped to the same address offset (ONEWAY_BASE)

  for(int i=0; i<TDMSLOTS; i++){
    for(int j=0; j < DOUBLEBUFFERS; j++){
      state->buf_out[i][j].data = (volatile _IODEV int *) (core[cpuid].tx[i] + (j*DBUFSIZE));  
      //sync_printf(cpuid, "&buf_out[tdm=%d][dbuf=%d].data[1] address = %p\n", 
      //  i, j, &state->buf_out[i][j].data[1]);     

    }    
  }

  for(int i=0; i<TDMSLOTS; i++){
    for(int j=0; j < DOUBLEBUFFERS; j++){
      state->buf_in[i][j].data = (volatile _IODEV int *) (core[cpuid].rx[i] + (j*DBUFSIZE));   
      //sync_printf(cpuid, "buf_in[%d][%d].data address  = %p\n", 
      //  i, j, state->buf_in[i][j].data);   
    }
  }

  // CORE WORK SECTION //  
  // individual core states incl 0
  switch (state->state) {
    // tx buffers
    case 0: {
      // UNCOMMENT THIS WHEN NOT DOING MEASUREMENTS
      //sync_printf(cpuid, "core %d to tx it's buffer in state 0\n", cpuid);
      if (cpuid == 1) {
        // first round of tx for the first buffer
        for(int i=0; i<TDMSLOTS; i++) {
          //buf_out[i][0].data[0] = cpuid*0x10000000 + i*0x1000000 + 0*10000 + txcnt; 
          for(int j=0; j < DBUFSIZE; j++){
            state->buf_out[i][0].data[j] = 2*cpuid*0x10000000 + i*0x1000000 + j*10000 + state->txcnt;
          }
          //sync_printf(cpuid, 
          //  "buf_out[%d](%d->).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
          //  i, getrxcorefromtxcoreslot(cpuid, i), buf_out[i][0].data[0], 
          //  buf_out[i][0].data[1], DBUFSIZE-1, buf_out[i][0].data[DBUFSIZE-1]);
        }
        state->txcnt++;  
      }

      // next state
      if (true) {
        state->state++;
      }
      break;
    }

    case 1: {
      if(cpuid == 1) {
        // second round of tx for the second buffer
        for(int i=0; i<TDMSLOTS; i++) {
          //buf_out[i][1].data[0] = cpuid*0x10000000 + i*0x1000000 + 0*10000 + txcnt; 
          for(int j=0; j < DBUFSIZE; j++){
            state->buf_out[i][1].data[j] = cpuid*0x10000000 + i*0x1000000 + j*10000 + state->txcnt;
          }
          //sync_printf(cpuid, 
          //  "buf_out[%d](%d->).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
          //  i, getrxcorefromtxcoreslot(cpuid, i), buf_out[i][1].data[0], 
          //  buf_out[i][1].data[1], DBUFSIZE-1, buf_out[i][1].data[DBUFSIZE-1]);
        }
        state->txcnt++;  
      }

      // next state
      if (true) {
        state->state++;
      }
        
      break;
    }

    
    case 2: {
      // rx buffer core 0
      if(cpuid == 0){
        // state work
        sync_printf(cpuid, "core %d state 2 on cycle %d\n", cpuid, getcycles());
          //if (cpuid == 0) holdandgowork(cpuid);
        const int num = 5;

        int cyclestamp[DOUBLEBUFFERS][num];
        int lastword[DOUBLEBUFFERS][num];
        int aword = -1;
        volatile int tmpsum = 0;
        // Now for real-time measurements between core 1 and core 0
        //   remember to comment out the sync_print line that is the first statement for
        //   state case 0 in the switch
        const int rxslot = 2; // just pick one
        const int txcoreid = gettxcorefromrxcoreslot(cpuid, rxslot);
        // start by monitoring the last word of the first buffer
        aword = state->buf_in[rxslot][0].data[DBUFSIZE-1];
        for(int i=0; i < num; i++) {
          for(int j=0; j < DOUBLEBUFFERS; j++){
  //          while(aword == buf_in[rxslot][j].data[DBUFSIZE-1]);
            aword = state->buf_in[rxslot][j].data[DBUFSIZE-1];
            cyclestamp[j][i] = getcycles();
            // core 1 is in TDM slot 2
            lastword[j][i] = state->buf_in[rxslot][j].data[DBUFSIZE-1];
            // some actual work use the active buffer (summing it as an example)
            for(int k=0; k < DBUFSIZE; k++)
              tmpsum += state->buf_in[rxslot][j].data[k];
          }
        }
        // capture the real end-time. The last 'cyclestamp' is taken before the 
        //   buffer is used and would report a too small number
        int endtime = getcycles();
        // done with real-time stuff. The rest is for "fun". 
        for(int i=0; i < num; i++){
          for(int j=0; j < DOUBLEBUFFERS; j++){
            sync_printf(cpuid, "double buffer %d: cyclestamp[%d] = %d, lastword[%d](1->) = 0x%08x\n",
              j, i, cyclestamp[j][i], i, lastword[j][i]);
          }
        }
        int totalcycles = endtime - cyclestamp[0][0];
        sync_printf(cpuid, "total cycles for %d rounds: %d, average = %d\n",
          num, totalcycles, totalcycles/num);


        sync_printf(cpuid, "core 0 in double buffer 0 rx state 1\n", cpuid);
        for(int i=0; i<TDMSLOTS; i++) {
          sync_printf(cpuid, 
            "buf_in[%d](->%d).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
            i, gettxcorefromrxcoreslot(cpuid, i), state->buf_in[i][0].data[0], 
            state->buf_in[i][0].data[1], DBUFSIZE-1, state->buf_in[i][0].data[DBUFSIZE-1]);
        }

        sync_printf(cpuid, "core 0 in double buffer 1 rx state 1\n", cpuid);
        for(int i=0; i<TDMSLOTS; i++) {
          sync_printf(cpuid, 
            "buf_in[%d](->%d).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
            i, gettxcorefromrxcoreslot(cpuid, i), state->buf_in[i][1].data[0], 
            state->buf_in[i][1].data[1], DBUFSIZE-1, state->buf_in[i][1].data[DBUFSIZE-1]);
        }

        // check rough timing
        if(totalcycles < 1e6)
          sync_printf(cpuid, "double buffer use case ok!\n", cpuid);

      } else {
        sync_printf(cpuid, "core %d have no rx work in state 1\n", cpuid);
      }


      // next state    
      if (true) { 
        state->state++;
      }
      break;
    }

    default: {
      // no work, just "looping" until runcores == false is signaled from core 0
      if (!state->coredone){
        state->coredone = true;
        sync_printf(cpuid, "core %d done (default final state)\n", cpuid);
      }
      break;
    }

  }
  
  // NOC CONTROL SECTION BY CORE 0//
  // max state before core 0 stops the mission
  if (cpuid == 0){
    state->roundstate++;
    if(state->roundstate >= 10) {
      // signal to stop the slave cores (and core 0)
      state->runcores = false; 
      sync_printf(0, "core 0 is done, roundstate == false signalled\n");
      //printf("core 0 is done, roundstate == false signalled\n");
    }  
  } 
}
