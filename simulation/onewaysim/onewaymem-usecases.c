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
#include "libcorethread/corethread.h"
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
void holdandgowork(){
  coreready[get_cpuid()] = true;
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

int tbstriggerwork(int txcid) {
  unsigned int cyclecnt = getcycles();
  sync_printf(get_cpuid(), "Usecase: Time-synced (on hyperperiod) trigger on rx core %d from tx core %d!!!\n", 
              get_cpuid(), txcid);
  return cyclecnt;
}

void txwork(int id) {
  volatile _SPM int *txMem = (volatile _IODEV int *) ONEWAY_BASE;
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
      txMem[i*WORDS + j] = id*0x10000000 + i*0x1000000 + jj*0x10000 + msgid;
    }
  }
}	

void rxwork(int id) {
  volatile _SPM int *rxMem = (volatile _IODEV int *) ONEWAY_BASE;
  volatile int tmp = 0;
      
  for (int j=0; j<WORDS; ++j) {
    for (int i=0; i<TDMSLOTS; ++i) {
      volatile unsigned int val = core[id].rx[i][j];
      if(rxMem[i*WORDS + j] != val)
        sync_printf(id, "Error!\n");
      if(j < 4){
        sync_printf(id, "RX: TDM slot %d, TDM slot word %d, rxMem[0x%04x] 0x%04x_%04x\n", 
                         i, j, i*WORDS + j, val>>16, val&0xFFFF);
        sync_printf(id, "      rx unit test core[id=%02d].rx[i=%02d][j=%03d] 0x%04x_%04x (tx'ed from core %d)\n", 
                         id, i, j, val>>16, val&0xFFFF, gettxcorefromrxcoreslot(id, i));
      }               
    }
  }
}

//  saves the current hyperperiod for each TDMSLOT in prevhyperperiod[TDMSLOTS]
//  as seen from each rx core
void recordhyperperiodwork(unsigned int* hyperperiods){
  for(int i = 0; i < TDMSLOTS; i++){
    hyperperiods[i] = core[get_cpuid()].rx[i][0];
    //sync_printf(get_cpuid(), "prevhyperperiod[i=%d] = 0x%08x (from core %d)\n", 
     //           i, hyperperiods[i], gettxcorefromrxcoreslot(get_cpuid(), i));        
  }
}

// the cores
// detect changes in HYPERPERIOD_REGISTER and TDMROUND_REGISTER
void corethreadtbswork(void *noarg) {
  int state = 0;

  int cid = get_cpuid();
  sync_printf(cid, "in corethreadtbs(%d)...\n", cid);
  unsigned int tdmround[TDMSLOTS];
  // previous hyperperiod (used to detect/poll for new hyperperiod)
  unsigned int prevhyperperiod[TDMSLOTS];
  // point to first word in each TDM message
  volatile _SPM int* hyperperiodptr[TDMSLOTS];
  
  for(int i = 0; i < TDMSLOTS; i++)
    hyperperiodptr[i] = &core[get_cpuid()].rx[i][0];
  
  holdandgowork();
  sync_printf(cid, "core %d mission start on cycle %d\n", cid, getcycles());


  // test harness stuff
  // instruction: comment out sync_printf in the measured section/path etc.
  // use getcycles() to record startcycle and then stopcycle
  // sync_print them after the measurement
  int startcycle = -1;
  int stopcycle[TDMSLOTS] = {-1};

  while (runcores) {
    // overall "noc state" handled by poor core 0 as a sidejob 
    static int roundstate = 0;
    // the statemachine will reach this state below
    const int MAXROUNDSTATE = 4;
    if (cid == 0){
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
        sync_printf(cid, "core %d tx state 0\n", cid);
        
        txwork(get_cpuid());
        
        for(int i = 0; i < TDMSLOTS; i++)
            prevhyperperiod[i] = core[get_cpuid()].rx[i][0];

        //recordhyperperiodwork(&prevhyperperiod[0]);
        //spinwork(TDMSLOTS*WORDS);

        
        // next state
        if (true) {
          state++;
        }
        
        if(cid == 0)
          state = 3;
        else 
          state = 2;
        
        
        break;
      }
        
      case 1: {
        // state work
        //sync_printf(cid, "core %d rx state 1\n", cid);        
        //rxwork(get_cpuid());
        
        
        //for(int i = 0; i < TDMSLOTS; i++)
         // hyperperiodptr[i] = &core[get_cpuid()].rx[i][0];
        //recordhyperperiodwork(&prevhyperperiod[0]);
        
        //spinwork(TDMSLOTS*WORDS);
        // next state    
        if (true) { 
          state++;
        }
        
        if(cid == 0)
          state = 3;

        break;
      }
        
      case 2: {
        // state work
        //if(cid != 0)
          //sync_printf(cid, "core %d tx state 2\n", cid);        
        
          txwork(get_cpuid());
        
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
        //sync_printf(cid, "core %d rx state 3\n", cid);  
        bool nextstatetbstrigger = false;
        
        bool trigger[TDMSLOTS] = {false};
        
        if(cid == 0) {
          while(!nextstatetbstrigger) {
            nextstatetbstrigger = true;
            for(int i = 0; i < TDMSLOTS; i++){
              if((!trigger[i]) && core[0].rx[i][0] != prevhyperperiod[i]){
                // use case 1 tbs trigger
                stopcycle[i] = getcycles();
                trigger[i] = true;
                //tbstriggerwork(gettxcorefromrxcoreslot(get_cpuid(), i));
              }
              else {
                nextstatetbstrigger = false;              
              }
            }    
          }
          
          for(int i = 0; i < TDMSLOTS; i++)
            sync_printf(cid, "core %d(%d) stopcycle %d - startcycle %d = %d\n", 
                   cid, gettxcorefromrxcoreslot(cid, i), stopcycle[i], 
                   startcycle, (stopcycle[i]-startcycle));

          
          rxwork(get_cpuid());
          recordhyperperiodwork(&prevhyperperiod[0]);
          spinwork(TDMSLOTS*WORDS);
        } else {
          // for the slave tx cores
          nextstatetbstrigger = true;  
        }
        
        sync_printf(cid, "core %d rx state 3 done\n", cid);  
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
  sync_printf(cid, "leaving corethreadtbswork(%d)...\n", cid);
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Handshaking Protocol
///////////////////////////////////////////////////////////////////////////////

void triggerhandshakework(int cid)
{
  sync_printf(cid, "handshaketriggger in core %d...\n", cid);
}

void corethreadhswork(void *noarg)
{
  int cid = get_cpuid();
  int step = 0;
  int txcnt = 1;
  unsigned int hyperperiod = 0xFFFFFFFF;

  // set up the use case so all cores will send a handshake message to the other cores
  // and receive the appropriate acknowledgement
  handshakemsg_t hmsg_out[TDMSLOTS];
  handshakemsg_t hmsg_in[TDMSLOTS];
  handshakeack_t hmsg_ack_out[TDMSLOTS];
  handshakeack_t hmsg_ack_in[TDMSLOTS];

  sync_printf(cid, "in corethreadhsp(%d)...\n", cid);
  
  //unsigned int tdmround[TDMSLOTS];
  // previous hyperperiod (used to detect/poll for new hyperperiod)
  unsigned int prevhyperperiod[TDMSLOTS];
  volatile _SPM int* hyperperiodptr[TDMSLOTS];
  
  for(int i = 0; i < TDMSLOTS; i++)
    hyperperiodptr[i] = &core[get_cpuid()].rx[i][0];

  int state = 0;
  const int donestate = 100;
  while (runcores) {
    // NOC CONTROL SECTION //
    // overall "noc state" handled by poor core 0 as a sidejob 
    static int roundstate = 0;
    // max state before core 0 stops the mission
    const int MAXROUNDSTATE = 4;
    if (cid == 0){
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
        sync_printf(cid, "core %d tx state 0\n", cid);
        
        // prepare the handshaked messages
        for(int i=0; i < TDMSLOTS; i++){ 
          // txstamp in first word
          hmsg_out[i].txstamp =  cid*0x10000000 + i*0x1000000 + 0*10000 + txcnt;
          hmsg_out[i].fromcore = cid;          
          hmsg_out[i].tocore   = gettxcorefromrxcoreslot(cid, i);
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
          core[cid].tx[i][0] = hmsg_out[i].txstamp;
          core[cid].tx[i][1] = hmsg_out[i].fromcore;          
          core[cid].tx[i][2] = hmsg_out[i].tocore;
          // fixed words ad 4 data words
          core[cid].tx[i][3] = hmsg_out[i].length; 
          // some test data
          core[cid].tx[i][4] = hmsg_out[i].data0;
          core[cid].tx[i][5] = hmsg_out[i].data1;
          core[cid].tx[i][6] = hmsg_out[i].data2;
          // block identifier
          core[cid].tx[i][7] = hmsg_out[i].blockno;
        }
        
        txcnt++;  
        spinwork(TDMSLOTS*WORDS);
        
        // next state
        if (true) {
          state++;
        }
        break;
      }
      // rx messages, check them and ack them 
      case 1: {
        // state work
        sync_printf(cid, "core %d msg rx state 1\n", cid);        
        
        for(int i=0; i<TDMSLOTS; i++) { 
          hmsg_in[i].txstamp =  core[cid].rx[i][0];
          hmsg_in[i].fromcore = core[cid].rx[i][1];          
          hmsg_in[i].tocore =   core[cid].rx[i][2];
          // fixed words a3d 4 data words
          hmsg_in[i].length =   core[cid].rx[i][3]; 
          // some test data
          hmsg_in[i].data0 =    core[cid].rx[i][4];
          hmsg_in[i].data1 =    core[cid].rx[i][5];
          hmsg_in[i].data2 =    core[cid].rx[i][6];
          // block identifier
          hmsg_in[i].blockno =  core[cid].rx[i][7];
          sync_printf(cid, "hmsg_in[%d](%d) 0x%08x 0x%08x 0x%08x 0x%08x\n",
            i, getrxcorefromtxcoreslot(cid,i),
            core[cid].rx[i][0], core[cid].rx[i][1], core[cid].rx[i][2], core[cid].rx[i][3]);
          sync_printf(cid, "              0x%08x 0x%08x 0x%08x 0x%08x\n",
            core[cid].rx[i][4], core[cid].rx[i][5], core[cid].rx[i][6], core[cid].rx[i][7]);
        }
        recordhyperperiodwork(&prevhyperperiod[0]);
        spinwork(TDMSLOTS*WORDS);
        
       
        
        txcnt++;    

        // next state    
        if (true) { 
          state++;
        }
        break;
      }
        
      case 2: {
        // state work
        sync_printf(cid, "core %d ack tx state 2\n", cid);        
        spinwork(TDMSLOTS*WORDS);
        // prepare the acks (if the msg was ok)
        for(int i=0; i < TDMSLOTS; i++){ 
          bool msgok = (hmsg_in[i].tocore == cid) && (hmsg_in[i].length == HANDSHAKEMSGSIZE);
          if (msgok) {
            // txstamp in first word
            hmsg_ack_out[i].txstamp  = cid*0x10000000 + i*0x1000000 + 0*10000 + txcnt;
            hmsg_ack_out[i].fromcore = cid;          
            hmsg_ack_out[i].tocore   = getrxcorefromtxcoreslot(cid, i);
            // block identifier (acknowledged)
            hmsg_ack_out[i].blockno  = hmsg_in[i].blockno;
          }
          
          if(msgok){
            core[cid].tx[i][0] = hmsg_ack_out[i].txstamp;
            core[cid].tx[i][1] = hmsg_ack_out[i].fromcore;          
            core[cid].tx[i][2] = hmsg_ack_out[i].tocore;
            core[cid].tx[i][3] = hmsg_ack_out[i].blockno;
          }
          
          if(msgok){
            sync_printf(cid, "hmsg_ack[%d] ack (blockno %d) sent to core %d\n",
                        i, hmsg_ack_out[i].blockno, hmsg_ack_out[i].tocore);
          }
        }       
        recordhyperperiodwork(&prevhyperperiod[0]);
        spinwork(TDMSLOTS*WORDS);
        //next state
        if (true) {
          state++;
        }
        break;
      }
      
      case 3: {
        // state work
        sync_printf(cid, "core %d ack rx state 3\n", cid);    
        for(int i=0; i<TDMSLOTS; i++) { 
          hmsg_ack_in[i].txstamp =  core[cid].rx[i][0];
          hmsg_ack_in[i].fromcore = core[cid].rx[i][1];          
          hmsg_ack_in[i].tocore =   core[cid].rx[i][2];
          // block identifier that is acknowledged
          hmsg_ack_in[i].blockno =  core[cid].rx[i][3];
          sync_printf(cid, "hmsg_ack_in[%d](%d) 0x%08x 0x%08x 0x%08x 0x%08x\n",
            i, getrxcorefromtxcoreslot(cid,i),
            hmsg_ack_in[i].txstamp, hmsg_ack_in[i].fromcore, 
            hmsg_ack_in[i].tocore, hmsg_ack_in[i].blockno);
        }
        
        // check use 2
        for(int i=0; i<TDMSLOTS; i++) { 
          int ackok = (hmsg_out[i].blockno == hmsg_ack_in[i].blockno) && 
                      (hmsg_ack_in[i].fromcore == gettxcorefromrxcoreslot(cid, i)) &&
                      (hmsg_ack_in[i].tocore == cid);
          if(ackok)
            sync_printf(cid, "use case 2 ok (ack from tx core %d ok)!\n", hmsg_ack_in[i].fromcore);
          else
            sync_printf(cid, "Error: use case 2 not ok (from %d)!\n", hmsg_ack_in[i].fromcore);
            
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
//COMMUNICATION PATTERN: Exchange of state
///////////////////////////////////////////////////////////////////////////////
const unsigned int CPUFREQ = 80e6;
void corethreadeswork(void *noarg) {
  int cid = get_cpuid();
  
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
    if (cid == 0){
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
        if(cid == 0)
          sync_printf(cid, "core %d to tx sensor state exchange\n", cid);
        else
          sync_printf(cid, "core %d no work in state 0\n", cid);
        
        unsigned int sensestart = getcycles();
        while(getcycles()-sensestart < CPUFREQ/SENSORUPDATEHZ);
        
        // Prepare the sensor reading that is transmitted from core 0 to all the other cores
        if (cid == 0) {
          esmsg_out.txstamp   = -1;
          esmsg_out.sensorid  = SENSORID0;
          esmsg_out.sensorval = getcycles(); // the artificial "temperature" proxy
        }
         
        if (cid == 0) { 
          // tx the message
          for(int i=0; i<TDMSLOTS; i++) { 
            core[cid].tx[i][0] = cid*0x10000000 + i*0x1000000 + 0*10000 + txcnt;
            core[cid].tx[i][1] = esmsg_out.sensorid;          
            core[cid].tx[i][2] = esmsg_out.sensorval;
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
        if(cid != 0){
          sync_printf(cid, "core %d es msg rx in state 1\n", cid);
          int core0slot = -1;
          for(int i=0; i<TDMSLOTS; i++) 
            if (gettxcorefromrxcoreslot(cid, i) == 0)
              core0slot = i;
          
          esmsg_in[cid].txstamp   = core[cid].rx[core0slot][0];
          esmsg_in[cid].sensorid  = core[cid].rx[core0slot][1];          
          esmsg_in[cid].sensorval = core[cid].rx[core0slot][2];
          
          sync_printf(cid, "esmsg_in[%d](%d) 0x%08x 0x%08x 0x%08x\n",
              core0slot, 0, esmsg_in[cid].txstamp, esmsg_in[cid].sensorid, esmsg_in[cid].sensorval);
        } else {
          sync_printf(cid, "core %d no work in this state\n", cid);
        }
        
        if(cid != 0){
          // check use case 3
          if (esmsg_in[cid].sensorid == SENSORID0)
            sync_printf(cid, "use case 3 ok (core 0 sensor state to core %d)!\n", cid);
          else
            sync_printf(cid, "use case 3 not ok (core 0 sensor state to core %d)!\n", cid);  
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

void corethreadsdbwork(void *noarg)
{
  int cid = get_cpuid();
  sync_printf(cid, "Core %d started...DOUBLEBUFFERS=%d, DBUFSIZE=%d, TDMSLOTS=%d, WORDS=%d\n", 
              cid, DOUBLEBUFFERS, DBUFSIZE, TDMSLOTS, WORDS);
 
  buffer_t buf_out[DBUFSIZE][TDMSLOTS];
  buffer_t buf_in[DBUFSIZE][TDMSLOTS];


  int txcnt = 1;
  int state = 0;
  while (runcores) {
    // NOC CONTROL SECTION //
    // overall "noc state" handled by poor core 0 as a sidejob 
    static int roundstate = 0;
    // max state before core 0 stops the mission
    const int MAXROUNDSTATE = 2;
    if (cid == 0){
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
      // all tx their buffers
      case 0: {
        sync_printf(cid, "core %d to tx it's buffer in state 0\n", cid);
        
        // first round of tx
        for(int i=0; i<TDMSLOTS; i++) {
          buf_out[0][i].data[0] = cid*0x10000000 + i*0x1000000 + 0*10000 + txcnt; 
          for(int j=1; j < DBUFSIZE; j++){
            buf_out[0][i].data[j] = cid + j;
          }
        }

        for(int i=0; i<TDMSLOTS; i++) {
          core[cid].tx[i][0] = buf_out[0][i].data[0];
          for(int j=1; j < DBUFSIZE; j++){
            core[cid].tx[i][j] = buf_out[0][i].data[j];          
          }
        }
        txcnt++;  
        
        // second round of tx
        for(int i=0; i<TDMSLOTS; i++) {
          buf_out[0][i].data[0] = cid*0x10000000 + i*0x1000000 + 0*10000 + txcnt; 
          for(int j=1; j < DBUFSIZE; j++){
            buf_out[0][i].data[j] = cid + j;
          }
        }

        for(int i=0; i<TDMSLOTS; i++) {
          core[cid].tx[i][0] = buf_out[0][i].data[0];
          for(int j=1; j < DBUFSIZE; j++){
            core[cid].tx[i][j] = buf_out[0][i].data[j];          
          }
        }
        txcnt++;  
                
        //spinwork(TDMSLOTS*WORDS);
        
        // next state
        if (true) {
          state++;
        }
        break;
      }
      // rx buffer core 0  
      case 1: {
        // state work
        //spinwork(TDMSLOTS*WORDS);
        
        if(cid == 0){
          sync_printf(cid, "core 0 in buffer rx state 1\n", cid);
          for(int i=0; i<TDMSLOTS; i++) {
            buf_in[0][i].data[0] = core[cid].rx[i][0];
            for(int j=1; j < DBUFSIZE; j++) {
              buf_in[0][i].data[j] = core[cid].rx[i][j];          
            }
            sync_printf(cid, "buf_in[%d](%d) 0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
              i, gettxcorefromrxcoreslot(cid, i), buf_in[0][i].data[0], 
              buf_in[0][i].data[1], DBUFSIZE-2, buf_in[0][i].data[(DBUFSIZE)-2]);
          }
          
          if((buf_in[0][0].data[DBUFSIZE-2]-buf_in[0][0].data[0]) == DBUFSIZE - 2)
            sync_printf(cid, "use case 4 ok!\n", cid);
          else
            sync_printf(cid, "use case 4 not ok!\n", cid);
            
        } else {
          sync_printf(cid, "core %d have no rx work in state 1\n", cid);
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
