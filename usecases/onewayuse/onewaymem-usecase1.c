/*
  Software layer for One-Way Shared Memory
  Use-case 1: Time-based synchronization

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include "onewaysim.h"

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Time-Based Synchronization (tbs)
///////////////////////////////////////////////////////////////////////////////

void tbstriggerwork(int cpuid, int txcid, unsigned int diff) {
	sync_printf(cpuid, "Time-synced trigger by tx core %d: diff=%d\n", txcid, diff);
}

// detect cycle/time in rx registers
void corethreadtbswork(void *cpuidptr) {
	int cpuid = *((int*) cpuidptr);
  State *state;
  statework(&state, cpuid);

  if (state->loopcount == 0) 
    sync_printf(cpuid, "in corethreadtbs(%d)...\n", cpuid);

#ifdef RUNONPATMOS
  while(runcores)
#endif
  {
  	switch (state->state) {
  		case 0: { 
  			sync_printf(cpuid, "core %d tx state 0\n", cpuid);
  		    // state 0: record the current hyperperiod
  			for(int i = 0; i < TDMSLOTS; i++)
  				core[cpuid].tx[i][0] = getcycles();

        // next state
  			if (true) {
  				state->state++;
  			}
  			break;
  		}
  		case 1: {
        // state work
        unsigned int state1cycle = getcycles();
  			sync_printf(cpuid, "core %d rx state 1\n", cpuid);
  			bool triggeredbyall = true;
  			for(int i = 0; i < TDMSLOTS; i++){
  				sync_printf(cpuid, "core[cpuid].rx[%d][0] = %d\n", i, core[cpuid].rx[i][0]);
          
  				bool triggered = (core[cpuid].rx[i][0] < state1cycle);
  				if (triggered){
            int txcoreid = gettxcorefromrxcoreslot(cpuid, i);
  					tbstriggerwork(cpuid, txcoreid, state1cycle - core[cpuid].rx[i][0]);
          }
  				triggeredbyall = triggeredbyall && triggered;
  			}

        // next state when time-triggered by tx->rx
  			if (triggeredbyall) {
        //if(true) {
  				state->state++;
  			}

  			break;
  		}

  		default: {
        // default state: final exit by shared signal 'runcores = false'
        defaultstatework(&state, cpuid);
        break;
  		}
    } // switch	

  	if (cpuid == 0){
      timeoutcheckcore0(&state);
  	}
    
  	state->loopcount++;
  } // while
}
