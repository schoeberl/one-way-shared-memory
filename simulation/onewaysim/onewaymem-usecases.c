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

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Time-Based Synchronization (tbs)
///////////////////////////////////////////////////////////////////////////////

void core0control()
{
  sync_printf(0, "core 0 is done\n");
  // signal to stop the slave cores
  runcores = false; 
}

void txwork(int id){
  volatile _SPM int *txMem = (volatile _IODEV int *) ONEWAY_BASE;
  int msg = 0;
  for (int j=0; j<WORDS; ++j) {
    for (int i=0; i<TDMSLOTS; ++i) {
      //                   tx core        TDM slot     TDM slot word  "Message"
      //                                  TDMSLOTS     WORDS               
      txMem[i*WORDS + j] = id*0x1000000 + i*0x100000 + j*0x10000      + msg;
      msg++;
    }
  }
}	

void rxwork(int id){
  volatile _SPM int *rxMem = (volatile _IODEV int *) ONEWAY_BASE;
  volatile int tmp = 0;
  for (int j=0; j<WORDS; ++j)
    for (int i=0; i<TDMSLOTS; ++i)
      tmp = rxMem[i*WORDS + j];
      
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

int tbstriggerwork(int cid)
{
  unsigned int cyclecnt = getcycles();
  sync_printf(cid, "Time-synced trigger on core %d: Cycles = %lu\n", cid, cyclecnt);
  sync_printf(cid, "HYPERPERIOD_REGISTER = %lu, TDMROUND_REGISTER = %lu\n",
              HYPERPERIOD_REGISTER, TDMROUND_REGISTER);
  return cyclecnt;
}

// the cores
// detect changes in HYPERPERIOD_REGISTER and TDMROUND_REGISTER
void corethreadtbswork(void *noarg)
{
  int cid = get_cpuid();
  sync_printf(cid, "in corethreadtbs(%d)...\n", cid);
  unsigned long tdmround = 0xFFFFFFFF;
  unsigned long hyperperiod = 0xFFFFFFFF;

  txwork(get_cpuid());
  spinwork(TDMSLOTS*WORDS);
  rxwork(get_cpuid());

  // all cores except core 0
  //while (cid > 0 && runcores)
  while (runcores)
  {
    //sync_printf(cid, "while: in corethreadtbs(%d)...\n", cid);
    // the cores are aware of the global cycle times for time-based-synchronization
    // the cores print their output after the cycle count changes are (simultaneously) detected
    if (tdmround != TDMROUND_REGISTER)
    {
      tbstriggerwork(cid);
      //sync_printf(cid, "Core #%ld: HYPERPERIOD_REGISTER = %lu, TDMROUND_REGISTER(*) = %lu\n", cid, HYPERPERIOD_REGISTER, TDMROUND_REGISTER);
      tdmround = TDMROUND_REGISTER;
    }

    if (hyperperiod != HYPERPERIOD_REGISTER)
    {
      tbstriggerwork(cid);
      //sync_printf(cid, "Core #%ld:HYPERPERIOD_REGISTER(*) = %lu, TDMROUND_REGISTER = %lu\n", cid, HYPERPERIOD_REGISTER, TDMROUND_REGISTER);
      hyperperiod = HYPERPERIOD_REGISTER;
    }

    // the other cores will sleep
    if (cid != 0)
      usleep(10);

    if (cid == 0)
      core0control();
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

void corethreadhsp(void *coreid)
{
  int cid = *((int *)coreid);
  int step = 0;
  unsigned long tdmround = 0xFFFFFFFF;
  unsigned long hyperperiod = 0xFFFFFFFF;
  handshakemsg_t pushmsg;
  memset(&pushmsg, 0, sizeof(pushmsg));
  handshakeack_t ackmsg;
  memset(&ackmsg, 0, sizeof(ackmsg));

  sync_printf(cid, "in corethreadhsp(%d)...\n", cid);

  //  push a message to another core
  //    (and wait for ack)
  //  doing this for core 1 to core 2 to keep it transparent
  int pushcoreid = 1;
  int ackcoreid = 2;
  // rx slot from txcoreid, rxcoreid, and txslot
  int ackcoreidindexmap = 0;//getrxslot(pushcoreid, cid, 0);
  int lastpushid = cid + 10;
  pushmsg.pushid = lastpushid; // so we are expecting a ack id of 10 + our core id

  printf("\n");
  // only copy to the tx buffer if this is the pushcoreid
  if (cid == pushcoreid)
  {
    //memcpy(core[cid].txmem[0], &pushmsg, sizeof(pushmsg));
  }
  //memtxprint(cid);

  int lastackid = 0; // 0 means no message

  while (runcores)
  {
    int monitorlastword = core[cid].rx[ackcoreidindexmap][0];
    if (monitorlastword != lastackid)
    {
      // new message id
      //   copy rx buffer
      //memcpy(&ackmsg, core[cid].rxmem[0], sizeof(ackmsg));
      // ack it (if we are the test core set up to do so)
      ackmsg.ackid = monitorlastword;
      lastackid = monitorlastword;
      // tx if we are the core set up to do so
      if (cid == ackcoreid)
      {
        //memcpy(core[cid].txmem[0], &pushmsg, sizeof(pushmsg));
      }
    }
    //sync_printf(cid, "Core %ld: TDMROUND_REGISTER   =%lu, HYPERPERIOD_REGISTER(*)=%lu,\n  ackmsg.reqid=%lu,\n  ackmsg.respid=%lu,\n  ackmsg.data1=%lu,\n  ackmsg.data2=%lu\n", cid, TDMROUND_REGISTER, HYPERPERIOD_REGISTER, ackmsg.reqid, ackmsg.respid, ackmsg.data1, ackmsg.data2);
  }
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Exchange of state
///////////////////////////////////////////////////////////////////////////////

// called when there is change of state
void triggerexchangeofstatework(int cid)
{
  sync_printf(cid, "Core %d change of state...\n", cid);
}

void corethreadeswork(void *coreid)
{
  int cid = *((int *)coreid);
  int step = 0;
  es_msg_t esmsg;
  memset(&esmsg, 0, sizeof(esmsg));

  int delay = 1000000;
  while (runcores)
  {
    if (step % delay == 0)
    {
      //sync_printf(cid, "Core %ld sensor request: TDMROUND_REGISTER   =%lu, HYPERPERIOD_REGISTER(*)=%lu,\n  ackmsg.ackid=%lu,\n  ackmsg.ackid=%lu,\n", cid, TDMROUND_REGISTER, HYPERPERIOD_REGISTER, ackmsg.ackid, ackmsg.ackid);
      esmsg.sensorid = 10 + cid;     // to be able to recognize it easily on the tx side
      esmsg.sensorval = getcycles(); // the artificial "temperature" proxy
      memcpy(core[cid].tx, &esmsg, sizeof(esmsg));
    }

    // sensor read
    if (core[cid].tx[0][0] > 0)
    {
      //sync_printf(cid, "Core %ld sensor reply: TDMCYCLE_REGISTER   =%lu, TDMROUND_REGISTER(*)=%lu,\n  ackmsg.reqid=%lu,\n  ackmsg.respid=%lu,\n  ackmsg.sensorid=%lu,\n  ackmsg.sensorval=%lu\n", cid, TDMROUND_REGISTER, TDMROUND_REGISTER, ackmsg.reqid, ackmsg.respid, ackmsg.data1, ackmsg.data2);
    }
    step++;
  }
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Streaming Double Buffer (sdb)
///////////////////////////////////////////////////////////////////////////////

//called when there is a new buffer
void triggeronbufferwork(int cid)
{
  sync_printf(cid, "Core %d new buffer...\n", cid);
}

void corethreadsdbwork(void *noarg)
{
  int cid = *((int *)coreid);
  sync_printf(cid, "Core %d started...\n", cid);
  int step = 0;
  buffer_t buffer_in;
  memset(&buffer_in, 0, sizeof(buffer_in));

  int lastword = 0;
  while (runcores)
  {
    int monitorlastword = core[cid].rx[0][0];
    if (monitorlastword != lastword)
    {
      memcpy(&buffer_in, core[cid].rx, sizeof(buffer_t));
      triggeronbufferwork(cid);
    }
    // tx new data for other buffers at another core
    // step data used here
    core[cid].tx[0][0] = 10000 + step * 100 + cid;
    core[cid].tx[1][0] = 10000 + step * 100 + cid;
    core[cid].tx[2][0] = 10000 + step * 100 + cid;
    core[cid].tx[3][0] = 10000 + step * 100 + cid;
    step++;
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
