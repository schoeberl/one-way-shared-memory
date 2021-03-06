/*
  Software layer for One-Way Shared Memory
  Use-case 3: Exchange of state

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include "onewaysim.h"

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Exchange of state
///////////////////////////////////////////////////////////////////////////////

void corethreadeswork(void *cpuidptr) {
  int printon = 0;
  const int cpuid = *((int*)cpuidptr);
  State *state;
  statework(&state, cpuid);

  if (state->loopcount == 0) 
    sync_printf(cpuid, "in corethreadeswork(%d)...printon=%d\n", cpuid, printon);

  state->txcnt = 1;
  unsigned int SENSORID0 = 0x11223344;
 
  // CORE WORK SECTION //  
  // individual core states incl 0
#ifdef RUNONPATMOS
  while(runcores)
#endif
  {  
    switch (state->state) {
      // tx messages
      case 0: {
        // state work
        if(cpuid == 1){
          sync_printf(cpuid, "core %d to tx sensor state exchange\n", cpuid);
          sync_printf(cpuid, "sizeof(es_msg_t) = %d\n", sizeof(state->esmsg_out));
        }
        else
          sync_printf(cpuid, "core %d no work in state 0\n", cpuid);

        unsigned int sensestart = getcycles();
        // while(getcycles()-sensestart < CPUFREQ/SENSORUPDATEHZ);

        // Prepare the sensor reading that is transmitted from core 1 to all the other cores
        if (cpuid == 1) {
          for(int i=0; i<TDMSLOTS; i++) { 
            state->txcnt++; 
            // create reading message
            state->esmsg_out.txstamp   = cpuid*0x10000000 + i*0x1000000 + 0*10000 + state->txcnt;
            state->esmsg_out.sensorid  = SENSORID0;
            state->esmsg_out.sensorval = sensestart;//getcycles(); // the artificial "temperature" proxy
            // send reading
            core[cpuid].tx[i][0] = state->esmsg_out.txstamp;
            core[cpuid].tx[i][1] = state->esmsg_out.sensorid;          
            core[cpuid].tx[i][2] = state->esmsg_out.sensorval;
          }
        }
          
        // next state
        if (true) {
          state->state++;
        }
        break;
      }
        // rx sensor val from 1 
      case 1: {
        // state work
        // now slave cores receive the message
        if(cpuid == 0) {
          if (printon) sync_printf(cpuid, "core %d es msg rx in state 1\n", cpuid);
          int core1id = 1;
          int core1slot = -1;
          for(int i=0; i<TDMSLOTS; i++) {
            if (gettxcorefromrxcoreslot(cpuid, i) == core1id)
              core1slot = i;
          }

          state->esmsg_in[cpuid].txstamp   = core[cpuid].rx[core1slot][0];
          state->esmsg_in[cpuid].sensorid  = core[cpuid].rx[core1slot][1];          
          state->esmsg_in[cpuid].sensorval = core[cpuid].rx[core1slot][2];
          unsigned int endtime = getcycles();
          unsigned int starttime = state->esmsg_in[cpuid].sensorval;

          // only let core 0 move to final state if it has received the sensor reading
          if (state->esmsg_in[cpuid].sensorid == SENSORID0) {
            sync_printf(cpuid, "esmsg_in[%d](%d) 0x%08x 0x%08x 0x%08x\n",
              core1slot, 0, state->esmsg_in[cpuid].txstamp, state->esmsg_in[cpuid].sensorid, 
              state->esmsg_in[cpuid].sensorval);
            sync_printf(cpuid, "core 1 sensor state to core %d ok, timediff = %d cycles\n", cpuid, 
                        endtime - starttime);
            state->state++;
          }
        } else {
          // let the other cores move on
          state->state++;
        }
      
        break;
      }

      default: {
        // default state: final exit by shared signal 'runcores = false'
        defaultstatework(&state, cpuid);
        break;
      }
    }

    if (cpuid == 0){
      timeoutcheckcore0(&state);
    }
    
    state->loopcount++;
  } // while
}
