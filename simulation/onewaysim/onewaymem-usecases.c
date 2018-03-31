/*
  Software layer for One-Way Shared Memory
  Runs both on the PC and on patmos

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

#ifdef RUNONPATMOS
#define ONEWAY_BASE ((volatile _IODEV int *) 0xE8000000)
volatile _SPM int *alltxmem = ONEWAY_BASE;
volatile _SPM int *allrxmem = ONEWAY_BASE;
#else
int alltxmem[CORES][CORES-1][MEMBUF];
int allrxmem[CORES][CORES-1][MEMBUF];
#endif

static volatile _UNCACHED int testval = -1;

void spinwork(unsigned int waitcycles){
  unsigned int start = getcycles();
  while((getcycles()-start) <= waitcycles);
}

// called by each core thread before starting the state machine
void holdandgowork(int cpuid){
  coreready[cpuid] = true;
  bool allcoresready = false;
  while(!allcoresready){
    allcoresready = true;
    for(int i = 0; i < CORES; i++) 
      if (coreready[i] == false)
        allcoresready = false;
  }
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Time-Based Synchronization (tbs)
///////////////////////////////////////////////////////////////////////////////

int tbstriggerwork(int cpuid, int txcid) {
  unsigned int cyclecnt = getcycles();
  sync_printf(cpuid, "Usecase: Time-synced (on hyperperiod) trigger on rx core %d from tx core %d!!!\n", 
              cpuid, txcid);
  return cyclecnt;
}

void txwork(int cpuid) {
  
  volatile _SPM int *txMem;
  #ifdef RUNONPATMOS
    txMem = (volatile _IODEV int *) ONEWAY_BASE;
  #else
    txMem = alltxmem[cpuid][0];
  #endif
  static int msgid = 0;
  for (int j=0; j<WORDS; ++j) {
    for (int i=0; i<TDMSLOTS; ++i) {
      // first word (txMem[i][0]) will be two bytes id, tdmslot, and word.
      // last two bytes of that first word is used for msgid, which is used for 
      // both checking the noc mapping, and also detecting a new message (on the msgid)
      //                   tx core        TDM slot     TDM slot word  "Message"
      //                                  TDMSLOTS     WORDS               
      msgid < 0x10000 ? msgid++ : 0;
      msgid++;
      int jj = (j<0x100)?j:0xFF;
      txMem[i*WORDS + j] = cpuid*0x10000000 + i*0x1000000 + jj*0x10000 + msgid;
    }
  }
}	

void rxwork(int cpuid) {
  volatile _SPM int *rxMem;

  #ifdef RUNONPATMOS
    rxMem = (volatile _IODEV int *) ONEWAY_BASE;
  #else
    rxMem = allrxmem[cpuid][0];
  #endif

  volatile int tmp = 0;
      
  for (int j=0; j<WORDS; ++j) {
    for (int i=0; i<TDMSLOTS; ++i) {
      volatile unsigned int val = core[cpuid].rx[i][j];
      if(rxMem[i*WORDS + j] != val)
        sync_printf(cpuid, "Error!\n");
      if(j < 4){
        sync_printf(cpuid, "RX: TDM slot %d, TDM slot word %d, rxMem[0x%04x] 0x%04x_%04x\n", 
                         i, j, i*WORDS + j, val>>16, val&0xFFFF);
        sync_printf(cpuid, "      rx unit test core[id=%02d].rx[i=%02d][j=%03d] 0x%04x_%04x (tx'ed from core %d)\n", 
                         cpuid, i, j, val>>16, val&0xFFFF, gettxcorefromrxcoreslot(cpuid, i));
      }               
    }
  }
}

//  saves the current hyperperiod for each TDMSLOT in prevhyperperiod[TDMSLOTS]
//  as seen from each rx core
void recordhyperperiodwork(int cpuid, unsigned int* hyperperiods){
  for(int i = 0; i < TDMSLOTS; i++){
    hyperperiods[i] = core[cpuid].rx[i][0];
    //sync_printf(cpuid, "prevhyperperiod[i=%d] = 0x%08x (from core %d)\n", 
     //           i, hyperperiods[i], gettxcorefromrxcoreslot(cpuid, i));        
  }
}

// the cores
// detect changes in HYPERPERIOD_REGISTER and TDMROUND_REGISTER
void corethreadtbswork(void *cpuidptr) {
  int state = 0;
  int cpuid = *((int*) cpuidptr);
  sync_printf(cpuid, "in corethreadtbs(%d)...\n", cpuid);
  unsigned int tdmround[TDMSLOTS];
  // previous hyperperiod (used to detect/poll for new hyperperiod)
  unsigned int prevhyperperiod[TDMSLOTS];
  // point to first word in each TDM message
  volatile _SPM int* hyperperiodptr[TDMSLOTS];
  
  for(int i = 0; i < TDMSLOTS; i++)
    hyperperiodptr[i] = &core[cpuid].rx[i][0];
  
  holdandgowork(cpuid);
  sync_printf(cpuid, "core %d mission start on cycle %d\n", cpuid, getcycles());

  // test harness stuff
  // instruction: comment out sync_printf in the measured section/path etc.
  // use getcycles() to record startcycle and then stopcycle
  // sync_print them after the measurement
  int startcycle;
  int stopcycle[TDMSLOTS];

  while (runcores) {
    // overall "noc state" handled by poor core 0 as a sidejob 
    static int roundstate = 0;
    // the statemachine will reach this state below
    const int MAXROUNDSTATE = 4;
    if (cpuid == 0){
      #ifndef RUNONPATMOS
        simcontrol();
      #endif
      if(roundstate == MAXROUNDSTATE) {
        // signal to stop the slave cores
        runcores = false; 
        sync_printf(0, "core 0 is done, roundstate == false signalled\n");
      }  
      roundstate++;
    }
      
    // individual core states incl 0
    switch (state) {
      case 0: {
        // state work
        sync_printf(cpuid, "core %d tx state 0\n", cpuid);
        txwork(cpuid);
        for(int i = 0; i < TDMSLOTS; i++)
            prevhyperperiod[i] = core[cpuid].rx[i][0];

        //recordhyperperiodwork(&prevhyperperiod[0]);
        //spinwork(TDMSLOTS*WORDS);

        // next state
        if (true) {
          state++;
        }
        
        if(cpuid == 0)
          state = 3;
        else 
          state = 2;
        
        
        break;
      }
        
      case 1: {
        // state work
        //sync_printf(cpuid, "core %d rx state 1\n", cpuid);        
        //rxwork(cpuid);
        
        
        //for(int i = 0; i < TDMSLOTS; i++)
         // hyperperiodptr[i] = &core[cpuid].rx[i][0];
        //recordhyperperiodwork(&prevhyperperiod[0]);
        
        //spinwork(TDMSLOTS*WORDS);
        // next state    
        if (true) { 
          state++;
        }
        
        if(cpuid == 0)
          state = 3;

        break;
      }
        
      case 2: {
        // state work
        if(cpuid != 0)
          sync_printf(cpuid, "core %d tx state 2\n", cpuid);        
          txwork(cpuid);
        
        //spinwork(TDMSLOTS*WORDS);
        //next state
        if (true) {
          state++;
        }
        

        break;
      }
      
      case 3: {
        // state work
        startcycle = getcycles();
        sync_printf(cpuid, "core %d rx state 3\n", cpuid);  
        bool nextstatetbstrigger = false;
        
        bool trigger[TDMSLOTS] = {false};
        
        if(cpuid == 0) {
          while(!nextstatetbstrigger) {
            nextstatetbstrigger = true;
            for(int i = 0; i < TDMSLOTS; i++){
              //stopcycle[i] = getcycles();
              if((!trigger[i]) && core[0].rx[i][0] != prevhyperperiod[i]){
                // use case 1 tbs trigger
                stopcycle[i] = getcycles();
                trigger[i] = true;
                //tbstriggerwork(gettxcorefromrxcoreslot(cpuid, i));
              }
              else {
                //stopcycle[i] = getcycles(); //...
                nextstatetbstrigger = false;              
              }
            }    
          }
          
          for(int i = 0; i < TDMSLOTS; i++)
            sync_printf(cpuid, "core %d(%d) (stopcycle=%d) - (startcycle=%d) = %d\n", 
                   cpuid, gettxcorefromrxcoreslot(cpuid, i), stopcycle[i], 
                   startcycle, (stopcycle[i]-startcycle));

          
          rxwork(cpuid);
          recordhyperperiodwork(cpuid, &prevhyperperiod[0]);
          spinwork(TDMSLOTS*WORDS);
        } else {
          // for the slave tx cores
          nextstatetbstrigger = true;  
        }
        
        sync_printf(cpuid, "core %d rx state 3 done\n", cpuid);  
        // next state    
        if (nextstatetbstrigger) { 
          state++;
        }
        break;  
      }      
      default: {
          // no work, just "looping" until runcores == false is signaled from core 0
          break;
      }
    }
  
  }
  sync_printf(cpuid, "leaving corethreadtbswork(%d)...\n", cpuid);
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Handshaking Protocol
///////////////////////////////////////////////////////////////////////////////

void triggerhandshakework(int cpuid)
{
  sync_printf(cpuid, "handshaketriggger in core %d...\n", cpuid);
}

void corethreadhswork(void *cpuidptr)
{
  // set this to 0 when doing measurements
  //   if you forget then print statements add about 1e6 cycles to the result!
  int printon = 1;
  int cpuid = *((int*)cpuidptr);
  int step = 0;
  int txcnt = 1;
  unsigned int hyperperiod = 0xFFFFFFFF;
  int starttime = -1;
  int endtime = -1;

  // set up the use case so all cores will send a handshake message to the other cores
  // and receive the appropriate acknowledgement
  handshakemsg_t hmsg_out[TDMSLOTS];
  handshakemsg_t hmsg_in[TDMSLOTS];
  handshakeack_t hmsg_ack_out[TDMSLOTS];
  handshakeack_t hmsg_ack_in[TDMSLOTS];

  sync_printf(cpuid, "in corethreadhsp(%d)...\n", cpuid);
  sync_printf(cpuid, "sizeof(handshakemsg_t)=%d\n", sizeof(handshakemsg_t));
  sync_printf(cpuid, "sizeof(handshakeack_t)=%d\n", sizeof(handshakeack_t));
  
  //unsigned int tdmround[TDMSLOTS];
  // previous hyperperiod (used to detect/poll for new hyperperiod)
  unsigned int prevhyperperiod[TDMSLOTS];
  volatile _SPM int* hyperperiodptr[TDMSLOTS];
  
  for(int i = 0; i < TDMSLOTS; i++)
    hyperperiodptr[i] = &core[cpuid].rx[i][0];

  int state = 0;
  const int donestate = 100;
  holdandgowork(cpuid);
  while (runcores) {
    // NOC CONTROL SECTION //
    // overall "noc state" handled by poor core 0 as a sidejob 
    static int roundstate = 0;
    // max state before core 0 stops the mission
    const int MAXROUNDSTATE = 4;
    if (cpuid == 0){
      if(roundstate == MAXROUNDSTATE) {
        // signal to stop the slave cores
        runcores = false; 
        sync_printf(0, "core 0 is done, roundstate == false signalled\n");
      }  
      roundstate++;
    }
     
    // CORE WORK SECTION //  
    // individual core states incl 0
    switch (state) {
      // tx messages
      case 0: {
        // state work
        sync_printf(cpuid, "core %d tx state 0\n", cpuid);
        starttime = getcycles();
        // prepare the handshaked messages
        for(int i=0; i < TDMSLOTS; i++){ 
          // txstamp in first word
          hmsg_out[i].txstamp =  cpuid*0x10000000 + i*0x1000000 + 0*10000 + txcnt;
          hmsg_out[i].fromcore = cpuid;          
          hmsg_out[i].tocore   = gettxcorefromrxcoreslot(cpuid, i);
          // fixed words and 4 data words
          hmsg_out[i].length   = HANDSHAKEMSGSIZE; 
          // some test data
          hmsg_out[i].data0    = getcycles();
          hmsg_out[i].data1    = getcycles();
          hmsg_out[i].data2    = getcycles();
          // block identifier
          hmsg_out[i].blockno  = txcnt;
        } 
        
        // tx the messages
        for(int i=0; i<TDMSLOTS; i++) { 
          core[cpuid].tx[i][0] = hmsg_out[i].txstamp;
          core[cpuid].tx[i][1] = hmsg_out[i].fromcore;          
          core[cpuid].tx[i][2] = hmsg_out[i].tocore;
          // fixed words ad 4 data words
          core[cpuid].tx[i][3] = hmsg_out[i].length; 
          // some test data
          core[cpuid].tx[i][4] = hmsg_out[i].data0;
          core[cpuid].tx[i][5] = hmsg_out[i].data1;
          core[cpuid].tx[i][6] = hmsg_out[i].data2;
          // block identifier
          core[cpuid].tx[i][7] = hmsg_out[i].blockno;
        }
        
        txcnt++;  
        //spinwork(TDMSLOTS*WORDS);
        
        // next state
        if (true) {
          state++;
        }
        break;
      }
      // rx messages, check them and ack them 
      case 1: {
        // state work
        if(printon) sync_printf(cpuid, "core %d msg rx state 1\n", cpuid);        
        
        for(int i=0; i<TDMSLOTS; i++) { 
          hmsg_in[i].txstamp =  core[cpuid].rx[i][0];
          hmsg_in[i].fromcore = core[cpuid].rx[i][1];          
          hmsg_in[i].tocore =   core[cpuid].rx[i][2];
          // fixed words a3d 4 data words
          hmsg_in[i].length =   core[cpuid].rx[i][3]; 
          // some test data
          hmsg_in[i].data0 =    core[cpuid].rx[i][4];
          hmsg_in[i].data1 =    core[cpuid].rx[i][5];
          hmsg_in[i].data2 =    core[cpuid].rx[i][6];
          // block identifier
          hmsg_in[i].blockno =  core[cpuid].rx[i][7];
          if(printon) sync_printf(cpuid, "hmsg_in[%d](%d) 0x%08x 0x%08x 0x%08x 0x%08x\n",
            i, getrxcorefromtxcoreslot(cpuid,i),
            core[cpuid].rx[i][0], core[cpuid].rx[i][1], core[cpuid].rx[i][2], 
            core[cpuid].rx[i][3]);
          if(printon) sync_printf(cpuid, "              0x%08x 0x%08x 0x%08x 0x%08x\n",
            core[cpuid].rx[i][4], core[cpuid].rx[i][5], core[cpuid].rx[i][6], 
            core[cpuid].rx[i][7]);
        }
        recordhyperperiodwork(cpuid, &prevhyperperiod[0]);

        txcnt++;    

        // next state    
        if (true) { 
          state++;
        }
        break;
      }
        
      case 2: {
        // state work
        if(printon) sync_printf(cpuid, "core %d ack tx state 2\n", cpuid);        
        //spinwork(TDMSLOTS*WORDS);
        // prepare the acks (if the msg was ok)
        for(int i=0; i < TDMSLOTS; i++){ 
          bool msgok = (hmsg_in[i].tocore == cpuid) && (hmsg_in[i].length == HANDSHAKEMSGSIZE);
          if (msgok) {
            // txstamp in first word
            hmsg_ack_out[i].txstamp  = cpuid*0x10000000 + i*0x1000000 + 0*10000 + txcnt;
            hmsg_ack_out[i].fromcore = cpuid;          
            hmsg_ack_out[i].tocore   = getrxcorefromtxcoreslot(cpuid, i);
            // block identifier (acknowledged)
            hmsg_ack_out[i].blockno  = hmsg_in[i].blockno;
          }
          
          if(msgok){
            core[cpuid].tx[i][0] = hmsg_ack_out[i].txstamp;
            core[cpuid].tx[i][1] = hmsg_ack_out[i].fromcore;          
            core[cpuid].tx[i][2] = hmsg_ack_out[i].tocore;
            core[cpuid].tx[i][3] = hmsg_ack_out[i].blockno;
          }
          
          if(msgok){
            if(printon) sync_printf(cpuid, "hmsg_ack[%d] ack (blockno %d) sent to core %d\n",
                        i, hmsg_ack_out[i].blockno, hmsg_ack_out[i].tocore);
          }
        }       
        recordhyperperiodwork(cpuid, &prevhyperperiod[0]);
        //spinwork(TDMSLOTS*WORDS);
        //next state
        if (true) {
          state++;
        }
        break;
      }
      
      case 3: {
        // state work
        if (printon) sync_printf(cpuid, "core %d ack rx state 3\n", cpuid);    
        for(int i=0; i<TDMSLOTS; i++) { 
          hmsg_ack_in[i].txstamp =  core[cpuid].rx[i][0];
          hmsg_ack_in[i].fromcore = core[cpuid].rx[i][1];          
          hmsg_ack_in[i].tocore =   core[cpuid].rx[i][2];
          // block identifier that is acknowledged
          hmsg_ack_in[i].blockno =  core[cpuid].rx[i][3];

        }
        endtime = getcycles();
        // end of real-time measurement
        
        sync_printf(cpuid, "core %d (done) ack rx state 3\n", cpuid);  
        for(int i=0; i<TDMSLOTS; i++) { 
          sync_printf(cpuid, "hmsg_ack_in[%d](%d) 0x%08x 0x%08x 0x%08x 0x%08x\n",
          i, getrxcorefromtxcoreslot(cpuid,i),
          hmsg_ack_in[i].txstamp, hmsg_ack_in[i].fromcore, 
          hmsg_ack_in[i].tocore, hmsg_ack_in[i].blockno);
        }     
        sync_printf(cpuid, "endtime %d - starttime %d = %d cycles\n",
                         endtime, starttime, endtime-starttime); 
        
        //check use 2
        for(int i=0; i<TDMSLOTS; i++) { 
          int ackok = (hmsg_out[i].blockno == hmsg_ack_in[i].blockno) && 
                      (hmsg_ack_in[i].fromcore == gettxcorefromrxcoreslot(cpuid, i)) &&
                      (hmsg_ack_in[i].tocore == cpuid);
          if(ackok)
            sync_printf(cpuid, "use case 2 ok (ack from tx core %d ok)!\n", hmsg_ack_in[i].fromcore);
          else
            sync_printf(cpuid, "Error: use case 2 not ok (from %d)!\n", hmsg_ack_in[i].fromcore);
            
        }
        //spinwork(TDMSLOTS*WORDS);

        // next state    
        if (true) { 
          state++;
        }
        break;  
      }      
      default: {
          // no work, just "looping" until runcores == false is signaled from core 0
          break;
      }
    }
  
  }
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Exchange of state
///////////////////////////////////////////////////////////////////////////////
const unsigned int CPUFREQ = 80e6;
void corethreadeswork(void *cpuidptr) {
  int cpuid = *((int*)cpuidptr);
  
  int txcnt = 1;
  const int SENSORID0 = 0x112233;
  const unsigned int SENSORUPDATEHZ = 1;

  es_msg_t esmsg_out;
  es_msg_t esmsg_in[TDMSLOTS];
  
  int state = 0;
  while (runcores) {
    // NOC CONTROL SECTION //
    // overall "noc state" handled by poor core 0 as a sidejob 
    static int roundstate = 0;
    // max state before core 0 stops the mission
    const int MAXROUNDSTATE = 3;
    if (cpuid == 0){
      if(roundstate == MAXROUNDSTATE) {
        // signal to stop the slave cores
        runcores = false; 
        sync_printf(0, "core 0 is done, roundstate == false signalled\n");
      }  
      roundstate++;
    }
       
    // CORE WORK SECTION //  
    // individual core states incl 0
    switch (state) {
      // tx messages
      case 0: {
        // state work
        if(cpuid == 0){
          sync_printf(cpuid, "core %d to tx sensor state exchange\n", cpuid);
          sync_printf(cpuid, "sizeof(es_msg_t) = %d\n", sizeof(es_msg_t));
        }
        else
          sync_printf(cpuid, "core %d no work in state 0\n", cpuid);
        
        unsigned int sensestart = getcycles();
        while(getcycles()-sensestart < CPUFREQ/SENSORUPDATEHZ);
        
        // Prepare the sensor reading that is transmitted from core 0 to all the other cores
        if (cpuid == 0) {
          esmsg_out.txstamp   = -1;
          esmsg_out.sensorid  = SENSORID0;
          esmsg_out.sensorval = getcycles(); // the artificial "temperature" proxy
        }
         
        if (cpuid == 0) { 
          // tx the message
          for(int i=0; i<TDMSLOTS; i++) { 
            core[cpuid].tx[i][0] = cpuid*0x10000000 + i*0x1000000 + 0*10000 + txcnt;
            core[cpuid].tx[i][1] = esmsg_out.sensorid;          
            core[cpuid].tx[i][2] = esmsg_out.sensorval;
          }
          txcnt++;  
        }
                
        spinwork(TDMSLOTS*WORDS);
        
        // next state
        if (true) {
          state++;
        }
        break;
      }
      // rx sensor  
      case 1: {
        // state work
        // now slave cores receive the message
        if(cpuid != 0){
          sync_printf(cpuid, "core %d es msg rx in state 1\n", cpuid);
          int core0slot = -1;
          for(int i=0; i<TDMSLOTS; i++) 
            if (gettxcorefromrxcoreslot(cpuid, i) == 0)
              core0slot = i;
          
          esmsg_in[cpuid].txstamp   = core[cpuid].rx[core0slot][0];
          esmsg_in[cpuid].sensorid  = core[cpuid].rx[core0slot][1];          
          esmsg_in[cpuid].sensorval = core[cpuid].rx[core0slot][2];
          
          sync_printf(cpuid, "esmsg_in[%d](%d) 0x%08x 0x%08x 0x%08x\n",
              core0slot, 0, esmsg_in[cpuid].txstamp, esmsg_in[cpuid].sensorid, 
              esmsg_in[cpuid].sensorval);
        } else {
          sync_printf(cpuid, "core %d no work in this state\n", cpuid);
        }
        
        if(cpuid != 0){
          // check use case 3
          if (esmsg_in[cpuid].sensorid == SENSORID0)
            sync_printf(cpuid, "use case 3 ok (core 0 sensor state to core %d)!\n", cpuid);
          else
            sync_printf(cpuid, "use case 3 not ok (core 0 sensor state to core %d)!\n", cpuid);  
        }        
          
        spinwork(TDMSLOTS*WORDS);

        // next state    
        if (true) { 
          state++;
        }
        break;
      }
    
      default: {
          // no work, just "looping" until runcores == false is signaled from core 0
          break;
      }
    }
  
  }
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Streaming Double Buffer (sdb)
///////////////////////////////////////////////////////////////////////////////
// there is a number of buffers overlaid on the tx and rx memory for each core
// the size of each of these double buffers is DBUFSIZE 

void corethreadsdbwork(void *cpuidptr)
{
  int cpuid = *((int*)cpuidptr);
  sync_printf(cpuid, "Core %d started...DOUBLEBUFFERS=%d, TDMSLOTS=%d, DBUFSIZE=%d, WORDS=%d\n", 
              cpuid, DOUBLEBUFFERS, TDMSLOTS, DBUFSIZE, WORDS);
 
  // setup: each tx and rx tdm slot have two (or more) double buffers
  // each buffer_t represents one (of at least two) for each TDM slot (each 
  //   TDM slot is WORDS long)
  // so for a double buffer on TDM slot 0, there would be two buffer_t stuct objects
  // they look like arrays (of arrays) as written below, but 
  // inside each buffer_t is a pointer to an unsigned int, which is
  // is set up to point to the part of either the rx or tx memory buffer that 
  // a particular buffer_t "controls".

  // double buffers on tx
  buffer_t buf_out[TDMSLOTS][DOUBLEBUFFERS];
  // double buffers on rx
  buffer_t buf_in[TDMSLOTS][DOUBLEBUFFERS];

  // init the data pointer in all of the double buffers
  // it will print something like this:
  //   [cy592233042 id00 #01] buf_out[0][0].data address = 0xe8000000
  //   [cy592305786 id00 #02] buf_in[0][0].data address  = 0xe8000000
  //   as the tx and rx are memory mapped to the same address offset (ONEWAY_BASE)
  for(int i=0; i<TDMSLOTS; i++){
    for(int j=0; j < DOUBLEBUFFERS; j++){
      buf_out[i][j].data = (volatile _IODEV int *) (core[cpuid].tx[i] + (j*DBUFSIZE));  
      //sync_printf(cpuid, "&core[cpuid].tx[i][1] = %p, (j*DBUFSIZE) = %d\n",
      //                 &core[cpuid].tx[i][1], (j*DBUFSIZE));
      sync_printf(cpuid, "&buf_out[tdm=%d][dbuf=%d].data[1] address = %p\n", 
                       i, j, &buf_out[i][j].data[1]);     
      for(int k=0; k < DBUFSIZE; k++){
        buf_out[i][j].data[k] = 0;
      }
    }    
  }

  for(int i=0; i<TDMSLOTS; i++){
    for(int j=0; j < DOUBLEBUFFERS; j++){
      buf_in[i][j].data = (volatile _IODEV int *) (core[cpuid].rx[i] + (j*DBUFSIZE));   
      sync_printf(cpuid, "buf_in[%d][%d].data address  = %p\n", 
                  i, j, buf_in[i][j].data);   
    }
  }

  sync_printf(cpuid, "core %d mission start on cycle %d\n", cpuid, getcycles());
  if (cpuid !=0) 
    holdandgowork(cpuid);

  int txcnt = 1;
  int state = 0;
  // used by core 0
  int roundstate = 0;
  const int MAXROUNDSTATE = 1;
  if(cpuid == 0) state++;
  
  do {  
    // CORE WORK SECTION //  
    // individual core states incl 0
    
    switch (state) {
      // all tx their buffers
      case 0: {
        // UNCOMMENT THIS WHEN NOT DOING MEASUREMENTS
        //sync_printf(cpuid, "core %d to tx it's buffer in state 0\n", cpuid);
        
        //spinwork(TDMSLOTS*WORDS);
        //spinwork(TDMSLOTS*WORDS);  
        // first round of tx for the first buffer
        for(int i=0; i<TDMSLOTS; i++) {
          //buf_out[i][0].data[0] = cpuid*0x10000000 + i*0x1000000 + 0*10000 + txcnt; 
          for(int j=0; j < DBUFSIZE; j++){
            buf_out[i][0].data[j] = 2*cpuid*0x10000000 + i*0x1000000 + j*10000 + txcnt;
          }
          //sync_printf(cpuid, 
          //  "buf_out[%d](%d->).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
          //  i, getrxcorefromtxcoreslot(cpuid, i), buf_out[i][0].data[0], 
          //  buf_out[i][0].data[1], DBUFSIZE-1, buf_out[i][0].data[DBUFSIZE-1]);
        }
        txcnt++;  
        
        // second round of tx for the second buffer
        for(int i=0; i<TDMSLOTS; i++) {
          //buf_out[i][1].data[0] = cpuid*0x10000000 + i*0x1000000 + 0*10000 + txcnt; 
          for(int j=0; j < DBUFSIZE; j++){
            buf_out[i][1].data[j] = cpuid*0x10000000 + i*0x1000000 + j*10000 + txcnt;
          }
          //sync_printf(cpuid, 
          //  "buf_out[%d](%d->).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
          //  i, getrxcorefromtxcoreslot(cpuid, i), buf_out[i][1].data[0], 
          //  buf_out[i][1].data[1], DBUFSIZE-1, buf_out[i][1].data[DBUFSIZE-1]);
        }
        txcnt++;  
                
        // next state
        if (true) {
          //state++;
        }
        break;
      }
      // rx buffer core 0  
      case 1: {
        // state work
        sync_printf(cpuid, "core %d state 1 on cycle %d\n", cpuid, getcycles());
        if (cpuid == 0) holdandgowork(cpuid);
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
        aword = buf_in[rxslot][0].data[DBUFSIZE-1];
        for(int i=0; i < num; i++) {
          for(int j=0; j < DOUBLEBUFFERS; j++){
            while(aword == buf_in[rxslot][j].data[DBUFSIZE-1]);
            aword = buf_in[rxslot][j].data[DBUFSIZE-1];
            cyclestamp[j][i] = getcycles();
            // core 1 is in TDM slot 2
            lastword[j][i] = buf_in[rxslot][j].data[DBUFSIZE-1];
            // some actual work use the active buffer (summing it as an example)
            for(int k=0; k < DBUFSIZE; k++)
              tmpsum += buf_in[rxslot][j].data[k];
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

        if(cpuid == 0){
          sync_printf(cpuid, "core 0 in double buffer 0 rx state 1\n", cpuid);
          for(int i=0; i<TDMSLOTS; i++) {
            sync_printf(cpuid, 
              "buf_in[%d](->%d).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
              i, gettxcorefromrxcoreslot(cpuid, i), buf_in[i][0].data[0], 
              buf_in[i][0].data[1], DBUFSIZE-1, buf_in[i][0].data[DBUFSIZE-1]);
          }

          sync_printf(cpuid, "core 0 in double buffer 1 rx state 1\n", cpuid);
          for(int i=0; i<TDMSLOTS; i++) {
            sync_printf(cpuid, 
              "buf_in[%d](->%d).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
              i, gettxcorefromrxcoreslot(cpuid, i), buf_in[i][1].data[0], 
              buf_in[i][1].data[1], DBUFSIZE-1, buf_in[i][1].data[DBUFSIZE-1]);
          }

          // check rough timing
          if(totalcycles < 1e6)
            sync_printf(cpuid, "use case 4 ok!\n", cpuid);
          else
            sync_printf(cpuid, "use case 4 not ok!\n", cpuid);
            
        } else {
          sync_printf(cpuid, "core %d have no rx work in state 1\n", cpuid);
        }
          
        spinwork(TDMSLOTS*WORDS);

        // next state    
        if (true) { 
          //state++;
        }
        break;
      }
    
      default: {
          // no work, just "looping" until runcores == false is signaled from core 0
          break;
      }
    }
    // NOC CONTROL SECTION BY CORE 0//
    // max state before core 0 stops the mission
    if (cpuid == 0){
      roundstate++;
      if(roundstate >= MAXROUNDSTATE) {
        // signal to stop the slave cores (and core 0)
        runcores = false; 
        sync_printf(0, "core 0 is done, roundstate == false signalled\n");
      }  
    } 
  } while (runcores);
}

///////////////////////////////////////////////////////////////////////////////
// Control code for communication patterns
///////////////////////////////////////////////////////////////////////////////

#ifdef RUNONPATMOS
//static corethread_t corethread[CORES];
#else
static pthread_t corethread[CORES];
#endif

///////////////////////////////////////////////////////////////////
// utility stuff
///////////////////////////////////////////////////////////////////

// mappings
static const char *rstr = ROUTESSTRING;

static int tx_core_tdmslots_map[CORES][TDMSLOTS];
static int rx_core_tdmslots_map[CORES][TDMSLOTS];

// route strings
static char *routes[TDMSLOTS];

// get coreid from NoC grid position
int getcoreid(int row, int col, int n){
  return row * n + col;
}

// convert the routes string into separate routes 
void initroutestrings(){
  printf("initroutestrings:\n");
  int start = 0;
  int stop = 0;
  for(int i = 0; i < TDMSLOTS; i++){
    while(rstr[stop] != '|')
      stop++;
    routes[i] = calloc(stop - start + 1 , 1);
    strncpy(routes[i], &rstr[start], stop - start);
    stop++;
    start = stop;
    printf("%s\n", routes[i]);
  }
}

// init rxslot and txcoreid lookup tables
void txrxmapsinit()
{
  initroutestrings();
  // tx router grid:
  //   the idea here is that we follow the word/flit from its tx slot to its rx slot.
  //   this is done by tracking the grid index for each of the possible directins of
  //   'n', 'e', 's', and 'w'. From the destination grid position the core id is 
  //   calculated (using getcoreid(gridx, gridy, n). 
  for(int tx_i = 0; tx_i < GRIDN; tx_i++){
    for(int tx_j = 0; tx_j < GRIDN; tx_j++){
      // simulate each route for the given rx core
      for(int slot = 0; slot < TDMSLOTS; slot++){
	// the tx and rx tdm slot are known by now
        char *route = routes[slot];

        int txcoreid = getcoreid(tx_i, tx_j, GRIDN);
        int rxcoreid = -1;
	
	int txtdmslot = slot;
	int rxtdmslot = (strlen(route)) % 3; 
	
	// now for the rx core id
	int rx_i = tx_i;
	int rx_j = tx_j;
        for(int r = 0; r < strlen(route); r++){
          switch(route[r]){
	    case 'n':
              rx_i = (rx_i - 1 >= 0 ? rx_i - 1 : GRIDN - 1);
	      break;
	    case 's':
	      rx_i = (rx_i + 1 < GRIDN ? rx_i + 1 : 0);
	      break;
            case 'e':
              rx_j = (rx_j + 1 < GRIDN ? rx_j + 1 : 0);
	      break;
            case 'w':
	      rx_j = (rx_j - 1 >= 0 ? rx_j - 1 : GRIDN - 1);
	      break;
	  }
        }        

        rxcoreid = getcoreid(rx_i, rx_j, GRIDN);
      
        // fill in the rx core id in the tx slot map
        tx_core_tdmslots_map[txcoreid][txtdmslot] = rxcoreid;
	    // fill in the tx core id in the rx slot map
	    rx_core_tdmslots_map[rxcoreid][rxtdmslot] = txcoreid;
     }
    }
  }
}

// will print the TX and RX TDM slots for each core
void showmappings(){
  for(int i = 0; i < CORES; i++){
    printf("Core %d TX TDM slots:\n", i);
    // show them like in the paper
    for(int j = 0; j < TDMSLOTS; j++){
      printf("  TDM tx slot %d: to rx core %d\n", 
             j, tx_core_tdmslots_map[i][j]);//getrxcorefromtxcoreslot(i, j)); 
    }
  }
  for(int i = 0; i < CORES; i++){
    printf("Core %d RX TDM slots:\n", i);
    // show them like in the paper
    for(int j = 0; j < TDMSLOTS; j++){
      printf("  TDM rx slot %d: from tx core %d\n", 
             j, rx_core_tdmslots_map[i][j]);//gettxcorefromrxcoreslot(i, j)); 
    }
  }
}

// get the rx core based on tx core and tx (TDM) slot index
int getrxcorefromtxcoreslot(int txcore, int txslot){
  return tx_core_tdmslots_map[txcore][txslot];
}

// get the tx core based on rx core and rx (TDM) slot index
int gettxcorefromrxcoreslot(int rxcore, int rxslot){
  return rx_core_tdmslots_map[rxcore][rxslot];
}
