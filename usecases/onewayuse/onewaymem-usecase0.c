/*
  Software layer for One-Way Shared Memory
  Use-case 0

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
//#include "libcorethread/corethread.h"
#ifndef RUNONPATMOS
#include <time.h>
#endif

#include "onewaysim.h"
#include "syncprint.h"

///////////////////////////////////////////////////////////////////////////////
//CORE TEST USECASE 0: Sanity check of the NoC itself
///////////////////////////////////////////////////////////////////////////////

// This use-case check that the all-to-all delivery works.
// It checks if the HW NoC (and the simulated NoC) works and if the SW works
// (i.e., it can detect the all-to-all schedule for different route schedules and 
// different NoC configurations, 2x2, 3x3, ...). 
// For the route string:
//   nel
//    nl
//     el
// it verifies this corresponding mapping:
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
//   are used for showing rx information. It will allow the rx side to verify both
//   that the tx side knew who it was sending to and that it (the rx side) knows where
//   the information (i.e., word) came from.
//   word (bits) endoding for this double tx and rx test:
// tx cpuid mask:      0xF000_0000 (limit:  16 cores)
// tx tdmslot mask:    0x0F00_0000 (limit:  15 slots)
// tx word index mask: 0x00FF_0000 (limit: 255 words)
// rx cpuid mask:      0x0000_F000
// rx tdmslot mask:    0x0000_0F00
// rx word index mask: 0x0000_00FF  
  void corethreadtestwork(void *cpuidptr) {
    int cpuid = getcpuidfromptr(cpuidptr);
    printf("in corethreadtestwork(%d)...\n", cpuid);
    State *state;
  #ifdef RUNONPATMOS
    // stack allocation
    State statevar;
    memset(statevar, 0, sizeof(statevar));
    state = &statevar;
  #else 
    state = &states[cpuid];
  #endif

    // do control loop
    {
      // individual core states
      switch (state->state) {
        case 0: { // tx
          printf("State 0, Core %d\n", cpuid);
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

              // set tx word
              core[tx_cpuid].tx[tx_tdmslot][tx_word_index] = txword;
            }
          }
          if (true) {
            state->state++;
          }
          break;
        } // case 0


        // state 1: 
        case 1: { // rx 
          printf("State 1, Core %d\n", cpuid);
          // check rx words
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
              
              // decode and check
              unsigned int decoded_tx_cpuid = (rxword & 0xF0000000)>>28;
              unsigned int decoded_tx_tdmslot = (rxword & 0x0F000000)>>24;
              unsigned int decoded_tx_word_index = (rxword & 0x00FF0000)>>16;
              unsigned int decoded_rx_cpuid = (rxword & 0x0000F000)>>12;
              unsigned int decoded_rx_tdmslot = (rxword & 0x00000F00)>>8;
              unsigned int decoded_rx_word_index = rxword & 0x000000FF;

              // check this rxword
              bool rxword_ok = true;
              rxword_ok = rxword_ok && (decoded_tx_cpuid == tx_cpuid);
              rxword_ok = rxword_ok && (decoded_tx_tdmslot == tx_tdmslot);
              rxword_ok = rxword_ok && (decoded_tx_word_index == tx_word_index);
              rxword_ok = rxword_ok && (decoded_rx_cpuid == rx_cpuid);
              rxword_ok = rxword_ok && (decoded_rx_tdmslot == rx_tdmslot);
              rxword_ok = rxword_ok && (decoded_rx_word_index == rx_word_index);

              // all rxwords so far
              rxwords_ok = rxwords_ok && rxword_ok;
            }
          }
          // next state
          if (rxwords_ok) {
            state->coredone = true;
            state->state++;
          }

          break;
        }

        default: {
          // no work, just "looping" until runcores == false is signaled from core 0
          printf("State default, Core %d\n", cpuid);
          if (!state->coredone){
            state->coredone = true;
            sync_printf(cpuid, "Core %d done ok!\n", cpuid);
          }
          break;
        }
      }


    }

    if (cpuid == 0){
      if(state->loopcount == 3) {
        // signal to stop the slave cores
        state->runcores = false; 
        sync_printf(0, "Core 0: roundstate == false signalled\n");
      }  
    }
    state->loopcount++;
  }
