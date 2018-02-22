/*
  A software simulation of the One-Way Shared Memory
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

#ifndef RUNONPATMOS
#include <time.h>
#endif

#include "onewaysim.h"

// TODO see
// https://github.com/t-crest/patmos/blob/master/c/apps/oneway/hello_oneway.c
// Change the memory to a pointer base
// #define ONEWAY_BASE *((volatile _SPM int *) 0xE8000000)
// For the shared fields we need to use _UNCACHED: 
// volatile _UNCACHED static int field;



unsigned long alltxmem[CORES][CORES - 1][MEMBUF];
unsigned long allrxmem[CORES][CORES - 1][MEMBUF];

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Time-Based Synchronization (tbs)
///////////////////////////////////////////////////////////////////////////////

int tbstrigger(int cid)
{
  unsigned long cyclecnt = getcycles();
  sync_printf(cid, "-->Time-synced trigger on core %d: Cycles = %lu\n", cid, cyclecnt);
  sync_printf(cid, "-->HYPERPERIOD_REGISTER = %lu, TDMROUND_REGISTER = %lu\n", HYPERPERIOD_REGISTER, TDMROUND_REGISTER);
  return cyclecnt;
}

// the cores
// detect changes in HYPERPERIOD_REGISTER and TDMROUND_REGISTER
void corethreadtbs(void *coreid)
{
  int cid = *((int *)coreid);
  sync_printf(cid, "in corethreadtbs(%d)...\n", cid);
  unsigned long tdmround = 0xFFFFFFFF;
  unsigned long hyperperiod = 0xFFFFFFFF;
  // all cores except core 0
  //while (cid > 0 && runcores)
  while (runcores)
  {
    //sync_printf(cid, "while: in corethreadtbs(%d)...\n", cid);
    // the cores are aware of the global cycle times for time-based-synchronization
    // the cores print their output after the cycle count changes are (simultaneously) detected
    if (tdmround != TDMROUND_REGISTER)
    {
      tbstrigger(cid);
      //sync_printf(cid, "Core #%ld: HYPERPERIOD_REGISTER = %lu, TDMROUND_REGISTER(*) = %lu\n", cid, HYPERPERIOD_REGISTER, TDMROUND_REGISTER);
      tdmround = TDMROUND_REGISTER;
    }

    if (hyperperiod != HYPERPERIOD_REGISTER)
    {
      tbstrigger(cid);
      //sync_printf(cid, "Core #%ld:HYPERPERIOD_REGISTER(*) = %lu, TDMROUND_REGISTER = %lu\n", cid, HYPERPERIOD_REGISTER, TDMROUND_REGISTER);
      hyperperiod = HYPERPERIOD_REGISTER;
    }

    if (cid > 0)
      usleep(10);

    if (cid == 0)
      noccontrol();
  }

  sync_printf(cid, "leaving corethreadtbs(%d)...\n", cid);
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Handshaking Protocol
///////////////////////////////////////////////////////////////////////////////

void triggerhandshake(int cid)
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
  memtxprint(cid);
  printf("\n");
  // only copy to the tx buffer if this is the pushcoreid
  if (cid == pushcoreid)
  {
    memcpy(core[cid].txmem[0], &pushmsg, sizeof(pushmsg));
  }
  memtxprint(cid);

  int lastackid = 0; // 0 means no message

  while (runcores)
  {
    int monitorlastword = core[cid].rxmem[ackcoreidindexmap][0];
    if (monitorlastword != lastackid)
    {
      // new message id
      //   copy rx buffer
      memcpy(&ackmsg, core[cid].rxmem[0], sizeof(ackmsg));
      // ack it (if we are the test core set up to do so)
      ackmsg.ackid = monitorlastword;
      lastackid = monitorlastword;
      // tx if we are the core set up to do so
      if (cid == ackcoreid)
      {
        memcpy(core[cid].txmem[0], &pushmsg, sizeof(pushmsg));
      }
    }
    //sync_printf(cid, "Core %ld: TDMROUND_REGISTER   =%lu, HYPERPERIOD_REGISTER(*)=%lu,\n  ackmsg.reqid=%lu,\n  ackmsg.respid=%lu,\n  ackmsg.data1=%lu,\n  ackmsg.data2=%lu\n", cid, TDMROUND_REGISTER, HYPERPERIOD_REGISTER, ackmsg.reqid, ackmsg.respid, ackmsg.data1, ackmsg.data2);
  }
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Exchange of state
///////////////////////////////////////////////////////////////////////////////

// called when there is change of state
void triggerexchangeofstate(int cid)
{
  sync_printf(cid, "Core %d change of state...\n", cid);
}

void corethreades(void *coreid)
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
      memcpy(core[cid].txmem, &esmsg, sizeof(esmsg));
    }

    // sensor read
    if (core[cid].txmem[0][0] > 0)
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
void triggeronbuffer(int cid)
{
  sync_printf(cid, "Core %d new buffer...\n", cid);
}

void corethreadsdb(void *coreid)
{
  int cid = *((int *)coreid);
  sync_printf(cid, "Core %d started...\n", cid);
  int step = 0;
  buffer_t buffer_in;
  memset(&buffer_in, 0, sizeof(buffer_in));

  int lastword = 0;
  while (runcores)
  {
    int monitorlastword = core[cid].rxmem[0][0];
    if (monitorlastword != lastword)
    {
      memcpy(&buffer_in, core[cid].rxmem, sizeof(buffer_t));
      triggeronbuffer(cid);
    }
    // tx new data for other buffers at another core
    // step data used here
    core[cid].txmem[0][0] = 10000 + step * 100 + cid;
    core[cid].txmem[1][0] = 10000 + step * 100 + cid;
    core[cid].txmem[2][0] = 10000 + step * 100 + cid;
    core[cid].txmem[3][0] = 10000 + step * 100 + cid;
    step++;
  }
}


///////////////////////////////////////////////////////////////////////////////
// Control code for communication patterns
///////////////////////////////////////////////////////////////////////////////

#ifdef RUNONPATMOS
static corethread_t corethread[CORES];
#else
static pthread_t corethread[CORES];
#endif

void nocmem()
{
  // map ms's alltxmem and allrxmem to each core's local memory
  for (int i = 0; i < CORES; i++)
  { // allocate pointers to each core's membuf array
    core[i].txmem = (unsigned long **)malloc((CORES - 1) * sizeof(unsigned long **));
    core[i].rxmem = (unsigned long **)malloc((CORES - 1) * sizeof(unsigned long **));
    for (int j = 0; j < CORES - 1; j++)
    { // assign membuf addresses from allrx and alltx to rxmem and txmem
      core[i].txmem[j] = alltxmem[i][j];
      core[i].rxmem[j] = allrxmem[i][j];
      for (int m = 0; m < MEMBUF; m++)
      { // zero the tx and rx membuffer slots
        // see the comment on 'struct Core'
        core[i].txmem[j][m] = 0;
        core[i].rxmem[j][m] = 0;
      }
    }
  }
}

// patmos memory initialization
// function pointer to one of the test cases
void nocinit()
{
  info_printf("in nocinit()...\n");
  //inittxrxmaps();

  nocmem();

  // init signals and counters
  HYPERPERIOD_REGISTER = 0;
  TDMROUND_REGISTER = 0;
  for (int c = 0; c < CORES; c++)
  {
    coreid[c] = c;
#ifdef RUNONPATMOS
    corethread[c] = c; // which core it will run on
#endif
  }

  // start the "slave" cores
  for (int c = 1; c < CORES; c++)
  {
#ifdef RUNONPATMOS
    corethread_create(&corethread[c], &corethreadtbs, (void *)&coreid[c]);
#else
    // the typecast 'void * (*)(void *)' is because pthread expects a function that returns a void *
    pthread_create(&corethread[c], NULL, (void *(*)(void *)) & corethreadtbs, (void *)&coreid[c]);
#endif
  }
}

void noccontrol()
{
#ifdef RUNONPATMOS
  info_printf("noc control is done in HW when running on patmos\n");
  runcores = false; // stop the cores here or somewhere else...
#else
  info_printf("simulation control when just running on host PC\n");
  simcontrol();
#endif
}

void nocdone()
{
  info_printf("waiting for cores to join ...\n");
  int *retval;
  // now let the others join
  for (int c = 1; c < CORES; c++)
  {
    info_printf("core thread %d to join...\n", c);
// the cores *should* join here...
#ifdef RUNONPATMOS
    corethread_join(corethread[c], (void **)&retval);
#else
    pthread_join(corethread[c], NULL);
#endif
    info_printf("core thread %d joined\n", c);
  }
}

///////////////////////////////////////////////////////////////////
// utility stuff
///////////////////////////////////////////////////////////////////

// new mappings (still not perfect)
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
void inittxrxmaps()
{
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
    for(int j = TDMSLOTS - 1; j >= 0; j--){
      printf("  TDM slot %d: to core %d\n", j, tx_core_tdmslots_map[i][j]); 
    }
  }
  for(int i = 0; i < CORES; i++){
    printf("Core %d RX TDM slots:\n", i);
    // show them like in the paper
    for(int j = TDMSLOTS - 1; j >= 0; j--){
      printf("  TDM slot %d: from core %d\n", j, rx_core_tdmslots_map[i][j]); 
    }
  }
}

void memtxprint(int coreid)
{
  for (int m = 0; m < MEMBUF; m++)
    for (int i = 0; i < CORES - 1; i++)
    {
      sync_printf(0, "core[coreid:%d].txmem[i:%d][m:%d] = %lu\n", coreid, i, m, core[coreid].txmem[i][m]);
    }
}

void memrxprint(int coreid)
{
  for (int m = 0; m < MEMBUF; m++)
    for (int i = 0; i < CORES - 1; i++)
    {
      sync_printf(0, "core[coreid:%d].rxmem[i:%d][m:%d] = %lu\n", coreid, i, m, core[coreid].rxmem[i][m]);
    }
}

void memallprint()
{
  for (int c = 0; c < CORES; c++)
  {
    memtxprint(c);
    memrxprint(c);
  }
}

void printpatmoscounters()
{
  // ms should probably name the counters PATMOS_IO_HYPERPERIOD or similar
  sync_printf(0, "HYPERPERIOD_REGISTER = %d\n", HYPERPERIOD_REGISTER);
  sync_printf(0, "TDMROUND_REGISTER    = %d\n", TDMROUND_REGISTER);
}

//print support so the threads on all cores can use printf
//  call sync_printall to print them after the most
//  important code has run.
//  don't not use sync_printf when running cycle accurate code (e.g. wcet)
static char strings[CORES + 1][SYNCPRINTBUF][80];
// clock cycles
static unsigned long timestamps[CORES + 1][SYNCPRINTBUF];
// message counter per core
static int mi[CORES + 1];
// call it with the core id so there are no race conditions
void sync_printf(int cid, const char *format, ...)
{
  if (mi[cid] < SYNCPRINTBUF)
  {
    //volatile _IODEV int *io_ptr = (volatile _IODEV int *)0xf0020004; // cycles 0xf002000c; // timer low word
    //int val = *io_ptr;
    int val = 0;
    timestamps[cid][mi[cid]] = getcycles(); //(unsigned long)val;
    va_list args;
    va_start(args, format);
    vsprintf(&strings[cid][mi[cid]][0], format, args);
    va_end(args);
    mi[cid]++;
  }
}

// an extra print buffer for info and end of sim stuff
void info_printf(const char *format, ...)
{
  // cid is equal to the number of CORES (i.e., the top buffer)
  if (mi[CORES] < SYNCPRINTBUF)
  {
    timestamps[CORES][mi[CORES]] = getcycles(); //(unsigned long)val;
    va_list args;
    va_start(args, format);
    vsprintf(&strings[CORES][mi[CORES]][0], format, args);
    va_end(args);
    mi[CORES]++;
  }
}

// print minimum timestamp. go over the [core][msg] matrix and find the lowest timestamp
// then print that message. keep a current msg index for each counter. increase it
//   for the message that has just been printed.

// call this from 0 when done
void sync_printall()
{
  printf("in sync_printall()...\n");
  bool print = true;
  int closestcoreid = 0;
  int closestmsgid = 0;
  int minmark[CORES + 1]; // keep track of messages printed for each core

  for (int c = 0; c < CORES + 1; c++)
    minmark[c] = 0;

  while (print)
  {
    unsigned long smallest = 0xffffffff;
    for (int c = 0; c < CORES + 1; c++)
    {
      if (minmark[c] < mi[c])
      {
        if (timestamps[c][minmark[c]] <= smallest)
        {
          closestcoreid = c;                    // candidate
          smallest = timestamps[c][minmark[c]]; // new smallest
        }
      }
    }
    if (closestcoreid == CORES) // info core?
      printf("[cycle=%lu, info=%d, msg#=%2d] %s", timestamps[closestcoreid][minmark[closestcoreid]], 0,
             minmark[closestcoreid], strings[closestcoreid][minmark[closestcoreid]]);
    else // other cores
      printf("[cycle=%lu, core=%d, msg#=%2d] %s", timestamps[closestcoreid][minmark[closestcoreid]], closestcoreid,
             minmark[closestcoreid], strings[closestcoreid][minmark[closestcoreid]]);

    usleep(1000); // perhaps not needed

    minmark[closestcoreid]++;
    print = false;
    for (int c = 0; c < CORES + 1; c++)
    {
      // are we done?
      if (minmark[c] < mi[c])
      {
        print = true;
        break;
      }
    }
  }

  //printf("leaving sync_printall()...\n");
}

// used for synchronizing printf from the different cores
unsigned long getcycles()
{
#ifdef RUNONPATMOS
  volatile _IODEV int *io_ptr = (volatile _IODEV int *)0xf0020004; 
  return (unsigned long)*io_ptr;
#else
  clock_t now_t;
  now_t = clock();
  return (unsigned long)now_t;
#endif
}
