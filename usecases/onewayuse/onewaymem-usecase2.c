/*
  Software layer for One-Way Shared Memory
  Use-case 2: Handshaking

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include "onewaysim.h"

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Handshaking Protocol
///////////////////////////////////////////////////////////////////////////////

void triggerhandshakework(int cpuid) {
  sync_printf(cpuid, "handshaketriggger in core %d...\n", cpuid);
}

void corethreadhswork(void *cpuidptr) {
  // set this to 0 when doing measurements
  //   if you forget then print statements add about 1e6 cycles to the result!
  int printon = 1;
  int cpuid = *((int*)cpuidptr);
  sync_printf(cpuid, "in corethreadhsp(%d)...\n", cpuid);

  State *state;
  #ifdef RUNONPATMOS
  State statevar;
  memset(statevar, 0, sizeof(statevar));
  state = &statevar;
  #else 
  state = &states[cpuid];
  #endif

  switch (state->state) {
      // tx messages
    case 0: {
      // state work
      sync_printf(cpuid, "core %d tx state 0\n", cpuid);
      state->starttime = getcycles();
      state->blockno = 0xCAFE;
      // prepare the handshaked messages
      for(int i=0; i < TDMSLOTS; i++){ 
        // txstamp in first word
        state->hmsg_out[i].txstamp =  cpuid*0x10000000 + i*0x1000000 + 0*10000 + state->txcnt;
        state->hmsg_out[i].fromcore = cpuid;          
        state->hmsg_out[i].tocore   = gettxcorefromrxcoreslot(cpuid, i);
        // fixed words and 4 data words
        state->hmsg_out[i].length   = HANDSHAKEMSGSIZE; 
        // some test data
        state->hmsg_out[i].data0    = getcycles();
        state->hmsg_out[i].data1    = getcycles() + 1;
        state->hmsg_out[i].data2    = getcycles() + 2;
        // block identifier
        state->hmsg_out[i].blockno  = state->blockno;
      } 

        // tx the messages
      for(int i=0; i<TDMSLOTS; i++) { 
        core[cpuid].tx[i][0] = state->hmsg_out[i].txstamp;
        core[cpuid].tx[i][1] = state->hmsg_out[i].fromcore;          
        core[cpuid].tx[i][2] = state->hmsg_out[i].tocore;
        // fixed words ad 4 data words
        core[cpuid].tx[i][3] = state->hmsg_out[i].length; 
        // some test data
        core[cpuid].tx[i][4] = state->hmsg_out[i].data0;
        core[cpuid].tx[i][5] = state->hmsg_out[i].data1;
        core[cpuid].tx[i][6] = state->hmsg_out[i].data2;
        // block identifier
        core[cpuid].tx[i][7] = state->hmsg_out[i].blockno;
      }

      // next state
      if (true) {
        state->state++;
      }
      break;
    }
    // rx 1st batch of messages, check them and ack them 
    case 1: {
      // state work
      if(printon) 
        sync_printf(cpuid, "core %d msg rx state 1\n", cpuid);        

      bool allrxok = true;
      for(int i=0; i<TDMSLOTS; i++) { 
        state->hmsg_in[i].txstamp  = core[cpuid].rx[i][0];
        state->hmsg_in[i].fromcore = core[cpuid].rx[i][1];          
        state->hmsg_in[i].tocore   = core[cpuid].rx[i][2];
          // fixed words a3d 4 data words
        state->hmsg_in[i].length   = core[cpuid].rx[i][3]; 
          // some test data
        state->hmsg_in[i].data0    = core[cpuid].rx[i][4];
        state->hmsg_in[i].data1    = core[cpuid].rx[i][5];
        state->hmsg_in[i].data2    = core[cpuid].rx[i][6];
          // block identifier
        state->hmsg_in[i].blockno  = core[cpuid].rx[i][7];

        bool rxok = (state->blockno == state->hmsg_in[i].blockno);
        allrxok = allrxok && rxok;

        if(printon) sync_printf(cpuid, "hmsg_in[%d](%d) 0x%08x 0x%08x 0x%08x 0x%08x\n",
          i, getrxcorefromtxcoreslot(cpuid,i),
          core[cpuid].rx[i][0], core[cpuid].rx[i][1], core[cpuid].rx[i][2], 
          core[cpuid].rx[i][3]);
          if(printon) sync_printf(cpuid, "              0x%08x 0x%08x 0x%08x 0x%08x\n",
            core[cpuid].rx[i][4], core[cpuid].rx[i][5], core[cpuid].rx[i][6], 
            core[cpuid].rx[i][7]);
      }

      // next state only if rx messages are received   
      if (allrxok) { 
        state->state++;
      }
      break;
    }

    case 2: {
        // state work
      if(printon) sync_printf(cpuid, "core %d ack tx state 2\n", cpuid);        
       
      // prepare the acks (if the msg was ok)
      bool allmsgok = true;
      for(int i=0; i < TDMSLOTS; i++){ 
        bool msgok = (state->hmsg_in[i].tocore == cpuid) && (state->hmsg_in[i].length == HANDSHAKEMSGSIZE);
        allmsgok = allmsgok && msgok;
        if (msgok) {
          // txstamp in first word
          state->hmsg_ack_out[i].txstamp  = cpuid*0x10000000 + i*0x1000000 + 0*10000 + state->txcnt;
          state->hmsg_ack_out[i].fromcore = cpuid;          
          state->hmsg_ack_out[i].tocore   = getrxcorefromtxcoreslot(cpuid, i);
          // block identifier (to be acknowledged)
          state->hmsg_ack_out[i].blockno  = state->hmsg_in[i].blockno;

          core[cpuid].tx[i][0] = state->hmsg_ack_out[i].txstamp;
          core[cpuid].tx[i][1] = state->hmsg_ack_out[i].fromcore;          
          core[cpuid].tx[i][2] = state->hmsg_ack_out[i].tocore;
          core[cpuid].tx[i][3] = state->hmsg_ack_out[i].blockno;

          if(printon) sync_printf(cpuid, "hmsg_ack[%d] ack (blockno %d) sent to core %d\n",
            i, state->hmsg_ack_out[i].blockno, state->hmsg_ack_out[i].tocore);
        }
      }       
      
      //next state if incoming was ok and acks are tx'ed
      if (allmsgok) {
        state->state++;
      }
      break;
    }

    case 3: {
      // state work
      if (printon) sync_printf(cpuid, "core %d ack rx state 3\n", cpuid);    
      for(int i=0; i<TDMSLOTS; i++) { 
        state->hmsg_ack_in[i].txstamp =  core[cpuid].rx[i][0];
        state->hmsg_ack_in[i].fromcore = core[cpuid].rx[i][1];          
        state->hmsg_ack_in[i].tocore =   core[cpuid].rx[i][2];
        // block identifier that is acknowledged
        state->hmsg_ack_in[i].blockno =  core[cpuid].rx[i][3];
      }
      state->endtime = getcycles();
      // end of real-time measurement

      sync_printf(cpuid, "core %d (done) ack rx state 3\n", cpuid);  
      for(int i=0; i<TDMSLOTS; i++) { 
        sync_printf(cpuid, "hmsg_ack_in[%d](%d) 0x%08x 0x%08x 0x%08x 0x%08x\n",
          i, getrxcorefromtxcoreslot(cpuid,i),
          state->hmsg_ack_in[i].txstamp, state->hmsg_ack_in[i].fromcore, 
          state->hmsg_ack_in[i].tocore, state->hmsg_ack_in[i].blockno);
      }     
      sync_printf(cpuid, "endtime %d - starttime %d = %d cycles\n",
        state->endtime, state->starttime, state->endtime - state->starttime); 

      //check use case 1 on HW
      bool allackok = true;
      for(int i=0; i<TDMSLOTS; i++) { 
        bool ackok = (state->hmsg_out[i].blockno == state->hmsg_ack_in[i].blockno) && 
          (state->hmsg_ack_in[i].fromcore == gettxcorefromrxcoreslot(cpuid, i)) &&
          (state->hmsg_ack_in[i].tocore == cpuid);

        allackok = allackok && ackok;
          
        if(ackok)
          sync_printf(cpuid, "use case 1 ok (ack from tx core %d ok)!\n", state->hmsg_ack_in[i].fromcore);
        else
          sync_printf(cpuid, "Error: use case 1 not ok (from %d)!\n", state->hmsg_ack_in[i].fromcore);
      }

      // next state    
      if (allackok) { 
        state->state++;
      }
      break;  
    }

    default: {
      // no work, just "looping" until runcores == false is signaled from core 0
      if (!state->coredone){
        state->coredone = true;
        sync_printf(cpuid, "Core %d done (default final state)\n", cpuid);
      }
      break;
    }
  }

  if (cpuid == 0){
    if(state->loopcount == 5) {
      // signal to stop the slave cores
      state->runcores = false; 
      sync_printf(0, "Core 0: roundstate == false signalled\n");
    }  
  }
  state->loopcount++;
}
