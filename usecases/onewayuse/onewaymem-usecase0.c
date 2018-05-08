/*
  Software layer for One-Way Shared Memory
  Use-case 0: Sanity check and verification of actual HW NoC and PC simulator

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include "onewaysim.h"

///////////////////////////////////////////////////////////////////////////////
//CORE TEST USECASE 0: Sanity check of the NoC itself
///////////////////////////////////////////////////////////////////////////////

// This use-case check that the all-to-all delivery works. This use-case checks
// the following 2x2 "route string":
//   nel
//    nl
//     el
// It checks if the HW NoC (and the simulated NoC) works and if the SW works
// (i.e., it can detect the all-to-all schedule for different route schedules and 
// different NoC configurations, 2x2, 3x3, ...).
// Use-case 0 verifies this corresponding mapping (derived from the route string):
// Core 0 TX TDM slots:
//   TDM tx slot 0: to rx core 3
//   TDM tx slot 1: to rx core 2
//   TDM tx slot 2: to rx core 1
// Core 1 TX TDM slots:
//   TDM tx slot 0: to rx core 2
//   TDM tx slot 1: to rx core 3
//   TDM tx slot 2: to rx core 0
// Core 2 TX TDM slots:
//   TDM tx slot 0: to rx core 1
//   TDM tx slot 1: to rx core 0
//   TDM tx slot 2: to rx core 3
// Core 3 TX TDM slots:
//   TDM tx slot 0: to rx core 0
//   TDM tx slot 1: to rx core 1
//   TDM tx slot 2: to rx core 2
// Core 0 RX TDM slots:
//   TDM rx slot 0: from tx core 3
//   TDM rx slot 1: from tx core 2
//   TDM rx slot 2: from tx core 1
// Core 1 RX TDM slots:
//   TDM rx slot 0: from tx core 2
//   TDM rx slot 1: from tx core 3
//   TDM rx slot 2: from tx core 0
// Core 2 RX TDM slots:
//   TDM rx slot 0: from tx core 1
//   TDM rx slot 1: from tx core 0
//   TDM rx slot 2: from tx core 3
// Core 3 RX TDM slots:
//   TDM rx slot 0: from tx core 0
//   TDM rx slot 1: from tx core 1
//   TDM rx slot 2: from tx core 2
// 
// Each word is encoded in a special way:
//   The 2 top bytes are used for storing tx information and the lower two bytes are
//   are used for showing rx information (known to the tx core at transmit time). 
//   It will allow on the rx side to verify both
//   that the tx side knew who (core and tdm slot) it was sending to and that it 
//   (the rx side) knows where the information (i.e., word) came from (i.e., tx core 
//   and tx tdm slot). Each 
// Word (bits) endoding for this tx / rx test:
//   tx cpuid mask:      0xF000_0000 (limit:  16 cores)
//   tx tdmslot mask:    0x0F00_0000 (limit:  15 slots)
//   tx word index mask: 0x00FF_0000 (limit: 255 words)
//   rx cpuid mask:      0x0000_F000
//   rx tdmslot mask:    0x0000_0F00
//   rx word index mask: 0x0000_00FF  

void corethreadtestwork(void *cpuidptr) {
  int cpuid = getcpuidfromptr(cpuidptr);
  printf("in corethreadtestwork(%d)...\n", cpuid);
  State *state;
  statework(&state, cpuid);

#ifdef RUNONPATMOS
  while(runcores)
#endif
  {
    // switch on core state
    switch (state->state) {
      case 0: { // state 0: encode and tx words
        sync_printf(cpuid, "state 0, core %d\n", cpuid);
        for (int w = 0; w < WORDS; w++) {
          for (int txslot = 0; txslot < TDMSLOTS; txslot++) {
            int tx_cpuid = cpuid;
            int tx_tdmslot = txslot;
            int tx_word_index = w;
            int rx_cpuid = getrxcorefromtxcoreslot(tx_cpuid, tx_tdmslot);
            int rx_tdmslot = -1;
            for (int rxslot = 0; rxslot < TDMSLOTS; rxslot++) {
              int txcore = gettxcorefromrxcoreslot(rx_cpuid, rxslot);
              if (txcore == tx_cpuid) {
                rx_tdmslot = rxslot;
                break;
              }
            }
            int rx_word_index = w;
            
            // encode
            unsigned int txword = 0x10000000 * tx_cpuid + 
            0x01000000 * tx_tdmslot + 
            0x00010000 * tx_word_index + 
            0x00001000 * rx_cpuid + 
            0x00000100 * rx_tdmslot +
            0x00000001 * rx_word_index;

            // set tx word in the tdm slot
            // on Patmos the NoC HW will route it to its rx core rx tdm slot
            // on the PC the simcontrol function will copy it from the tx slot to the 
            //  rx core rx tdm slot
            core[tx_cpuid].tx[tx_tdmslot][tx_word_index] = txword;
          }
        }
        if (true) {
          state->state++;
        }
        break;
      }

      case 1: { // state 1: rx, decode, and verify all words 
        sync_printf(cpuid, "state 1, core %d\n", cpuid);
        // check all rx words
        bool rxwords_ok = true;
        for (int w = 0; w < WORDS; w++) {
          for (int rxslot = 0; rxslot < TDMSLOTS; rxslot++) {
            unsigned int rx_cpuid = cpuid;
            unsigned int rx_tdmslot = rxslot;
            unsigned int rx_word_index = w;
            unsigned int tx_cpuid = gettxcorefromrxcoreslot(rx_cpuid, rx_tdmslot);
            unsigned int tx_tdmslot = -1;
            for (unsigned int txslot = 0; txslot < TDMSLOTS; txslot++) {
              int rxcore = getrxcorefromtxcoreslot(tx_cpuid, txslot);
              if (rxcore == rx_cpuid) {
                tx_tdmslot = txslot;
                break;
              }
            }
            unsigned int tx_word_index = w;
            
            unsigned int rxword = core[rx_cpuid].rx[rx_tdmslot][rx_word_index];
            
            // decode 
            unsigned int decoded_tx_cpuid = (rxword & 0xF0000000)>>28;
            unsigned int decoded_tx_tdmslot = (rxword & 0x0F000000)>>24;
            unsigned int decoded_tx_word_index = (rxword & 0x00FF0000)>>16;
            unsigned int decoded_rx_cpuid = (rxword & 0x0000F000)>>12;
            unsigned int decoded_rx_tdmslot = (rxword & 0x00000F00)>>8;
            unsigned int decoded_rx_word_index = rxword & 0x000000FF;

            // , check this rx word
            bool rxword_ok = true;
            rxword_ok = rxword_ok && (decoded_tx_cpuid == tx_cpuid);
            rxword_ok = rxword_ok && (decoded_tx_tdmslot == tx_tdmslot);
            rxword_ok = rxword_ok && (decoded_tx_word_index == tx_word_index);
            rxword_ok = rxword_ok && (decoded_rx_cpuid == rx_cpuid);
            rxword_ok = rxword_ok && (decoded_rx_tdmslot == rx_tdmslot);
            rxword_ok = rxword_ok && (decoded_rx_word_index == rx_word_index);

            // , and remember result for all rxwords so far
            rxwords_ok = rxwords_ok && rxword_ok;
          }
        }
        // final state if rxwords are ok
        // otherwise stay in state until the NoC has delivered all the words
        if (rxwords_ok) {
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
