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

int tbstriggerwork(int cpuid, int txcid) {
	unsigned int cyclecnt = getcycles();
	//sync_printf(cpuid, "use-case: Time-synced trigger by tx core %d!!!\n", txcid);
	return cyclecnt;
}

// detect cycle/time in rx registers
void corethreadtbswork(void *cpuidptr) {
	int cpuid = *((int*) cpuidptr);
	State *state;
#ifdef RUNONPATMOS
	State statevar;
	memset(&statevar, 0, sizeof(statevar));
	state = &statevar;
  state->runcore = true;
  holdandgowork(cpuid);
#else 
	state = &states[cpuid];
#endif

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
			for(int i = 0; i < TDMSLOTS; i++){
				state->prevhyperperiod[i]  = core[cpuid].rx[i][0];
        sync_printf(cpuid, "state->prevhyperperiod[%d] = %d\n", i, state->prevhyperperiod[i]);
      }
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
			sync_printf(cpuid, "core %d rx state 1\n", cpuid);
			bool triggeredbyall = true;
			for(int i = 0; i < TDMSLOTS; i++){
				unsigned int rxval = core[cpuid].rx[i][0];
        sync_printf(cpuid, "core[cpuid].rx[%d][0] = %d\n", i, core[cpuid].rx[i][0]);
				bool triggered = (rxval > state->prevhyperperiod[i]);
				if (triggered){
          int txcoreid = gettxcorefromrxcoreslot(cpuid, i);
					tbstriggerwork(cpuid, txcoreid);
        }
				triggeredbyall = triggeredbyall && (rxval > state->prevhyperperiod[i]);
			}

      // next state when time-triggered by tx->rx
			if (triggeredbyall) {
      //if(true) {
				state->state++;
			}

			break;
		}

		default: {
      // Only log first time this state is reached (it may loop for a while)
      // until the other cores also reach their final state or max loops 
      if (cpuid == 0){
        runcores = false;
      }
			if (!state->coredone){
				state->coredone = true;
        state->runcore = false; 
				sync_printf(cpuid, "core %d done (default final state)\n", cpuid);
			}
			break;
		}
  }	

	if (cpuid == 0){
		if(state->loopcount == 50000) {
      // signal to stop the slave cores
			state->runcore = false; 
      runcores = false;
			sync_printf(0, "core 0: roundstate == false signalled\n");
		}  
	}
	state->loopcount++;
  } // while
}
