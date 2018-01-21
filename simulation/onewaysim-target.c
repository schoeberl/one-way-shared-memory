/*
  A software simulation of the One-Way Shared Memory
  Run on Patmos (the target) (and the PC)

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

//#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include <machine/patmos.h>
#include "libcorethread/corethread.h"
#include "onewaysim.h"

unsigned long alltxmem[CORES][CORES - 1][MEMBUF];
unsigned long allrxmem[CORES][CORES - 1][MEMBUF];

///////////////////////////////////////////////////////////////////////////////
// Control code for communication patterns
///////////////////////////////////////////////////////////////////////////////

// init patmos (simulated) internals
Core core[CORES];
// signal used to stop terminate the cores
// core0 is special as it runs on the main thread
static const int corecntmax = 10000;
static int coreid[CORES];
static corethread_t corethread[CORES];

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
  inittxrxmaps();

  nocmem();

  // init signals and counters
  HYPERPERIOD_REGISTER = 0;
  TDMROUND_REGISTER = 0;
  for (int c = 0; c < CORES; c++)
  {
    coreid[c] = c;
    corethread[c] = c; // which core it will run on
  }

  // start the "slave" cores
  for (int c = 1; c < CORES; c++)
  {
    corethread_create(&corethread[c], &corethreadtbs, (void *)&coreid[c]);
  }
}

// called repeatedly from core 0
// this can (and will) sometimes be done by real HW instead
void noccontrol()
{
  sync_printf(0, "in noccontrol()...\n");

  // change this to the number of runs you want
  const int hyperperiodstorun = 5;

  // hyperperiod starts
  static int hyperperiod = 0;
  for (int m = 0; m < MEMBUF; m++)
  {                                                      // one word from each to each
    for (int txcoreid = 0; txcoreid < CORES; txcoreid++) // sender core
    {
      int txslot = 0;
      for (int rxcoreid = 0; rxcoreid < CORES; rxcoreid++) // receiver core
      {
        if (txcoreid != rxcoreid) // no tx to itself
        {
          // sync_printf(0,"rxcoreid = %d\n", rxcoreid);
          // sync_printf(0,"txcoreid = %d\n", txcoreid);
          // sync_printf(0,"txslot   = %d\n", txslot);
          // sync_printf(0,"getrxslot(txcoreid, rxcoreid, txslot)=%d\n", getrxslot(txcoreid, rxcoreid, txslot));
          // sync_printf(0,"m        = %d\n", m);
          core[rxcoreid].rxmem[getrxslot(txcoreid, rxcoreid, txslot)][m] = core[txcoreid].txmem[txslot][m];
          txslot++;
        }
      }
    } // tdmround ends
    TDMROUND_REGISTER++;
    sync_printf(0, "TDMROUND_REGISTER = %lu\n", TDMROUND_REGISTER);
    //usleep(100000); //much slower than the cores on purpose, so they can poll
  }
  HYPERPERIOD_REGISTER++;
  sync_printf(0, "HYPERPERIOD_REGISTER = %lu\n", HYPERPERIOD_REGISTER);
  //usleep(100000);
  // current hyperperiod ends

  TDMROUND_REGISTER = 0;

  hyperperiod++;
  sync_printf(0, "leaving noccontrol()...\n");
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
    corethread_join(corethread[c], (void **)&retval);
    info_printf("core thread %d joined: coreid=%d\n", c);
  }
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Time-Based Synchronization (tbs)
///////////////////////////////////////////////////////////////////////////////

int tbstrigger(int cid)
{
  // table 3.3 in the patmos manual
  volatile _IODEV int *io_ptr = (volatile _IODEV int *)(PATMOS_IO_TIMER + 4);
  int val = *io_ptr;
  sync_printf(cid, "-->Time-synced trigger on core %d: Cycles = %d\n", cid, val);
  sync_printf(cid, "-->HYPERPERIOD_REGISTER = %lu, TDMROUND_REGISTER = %lu\n", HYPERPERIOD_REGISTER, TDMROUND_REGISTER);
  return val;
}

// the cores
// detect changes in HYPERPERIOD_REGISTER and TDMROUND_REGISTER
void corethreadtbs(void *coreid)
{
  int cid = *((int *)coreid);
  sync_printf(cid, "in corethreadtbs(%d)...\n", cid);
  unsigned long tdmround = 0xFFFFFFFF;
  unsigned long hyperperiod = 0xFFFFFFFF;
  int corecnt = corecntmax;
  // all cores except core 0
  while (corecnt != 0)
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

    corecnt--;
  }

  // while ((cid == 0) && runcore0)
  // {
  //   if (core0cnt == 1000)
  //     runcore0 = false;

  //   core0cnt++;
  // }

  sync_printf(cid, "leaving corethreadtbs(%d)...core0cnt %d\n", cid, corecnt);
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Handshaking Protocol
///////////////////////////////////////////////////////////////////////////////

// the cores
void corethreadhsp(void *coreid)
{
  int cid = *((int *)coreid);
  int step = 0;
  unsigned long tdmround = 0xFFFFFFFF;
  unsigned long hyperperiod = 0xFFFFFFFF;
  handshakemsg_t txmsg;
  memset(&txmsg, 0, sizeof(txmsg));
  handshakemsg_t rxmsg;
  memset(&rxmsg, 0, sizeof(rxmsg));

  sync_printf(cid, "in corethreadhsp(%d)...\n", cid);
  sync_printf(cid, "sizeof(txmsg)=%d\n", sizeof(txmsg));
  //kickstart with sending a request to each of the other cores
  txmsg.reqid = 1; // want to get message 1 from another core (and no more)
  memtxprint(cid);
  printf("\n");
  memcpy(core[cid].txmem[0], &txmsg, sizeof(txmsg));
  memtxprint(cid);

  // the cores are aware of the global cycle times for time-based-synchronization
  // the cores print their output after the cycle count changes are detected
  if (tdmround != TDMROUND_REGISTER)
  {
    //sync_printf("Core %ld: TDMCYCLE_REGISTER(*)=%lu, TDMROUND_REGISTER   =%lu\n", cid, TDMCYCLE_REGISTER, TDMROUND_REGISTER);
    tdmround = TDMROUND_REGISTER;
  }
  if (hyperperiod != HYPERPERIOD_REGISTER)
  {
    // copy what we got
    memcpy(&rxmsg, core[cid].rxmem[0], sizeof(rxmsg));
    // first we will see the request for message id 1 arriving at each core
    // then we will see that each core responds with message 1 and some data
    // it keeps repeating the same message until it gets another request for a new
    // message id
    sync_printf(cid, "Core %ld: TDMROUND_REGISTER   =%lu, HYPERPERIOD_REGISTER(*)=%lu,\n  rxmsg.reqid=%lu,\n  rxmsg.respid=%lu,\n  rxmsg.data1=%lu,\n  rxmsg.data2=%lu\n", cid, TDMROUND_REGISTER, HYPERPERIOD_REGISTER, rxmsg.reqid, rxmsg.respid, rxmsg.data1, rxmsg.data2);
    // is it a request?
    if (rxmsg.reqid > 0 && rxmsg.respid == 0)
    {
      memset(&txmsg, 0, sizeof(txmsg));
      txmsg.reqid = rxmsg.reqid;
      txmsg.respid = rxmsg.reqid;
      txmsg.data1 = step;
      txmsg.data2 = cid;
      // schedule for sending (tx)
      memcpy(core[cid].txmem[0], &txmsg, sizeof(txmsg));
    }

    hyperperiod = HYPERPERIOD_REGISTER;
  }
  step++;
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Exchange of state
///////////////////////////////////////////////////////////////////////////////

// the cores
void corethreades(void *coreid)
{
  int cid = *((int *)coreid);
  int step = 0;
  unsigned long tdmround = 0xFFFFFFFF;
  unsigned long hyperperiod = 0xFFFFFFFF;
  handshakemsg_t txmsg;
  memset(&txmsg, 0, sizeof(txmsg));
  handshakemsg_t rxmsg;
  memset(&rxmsg, 0, sizeof(rxmsg));

  //kickstart with sending a request to each of the other cores
  txmsg.reqid = 1; // want to get message 1 from another core (and no more)
  memcpy(&core[cid].txmem[0], &txmsg, sizeof(txmsg));

  // the cores are aware of the global cycle times for time-based-synchronization
  // the cores print their output after the cycle count changes are detected
  if (tdmround != TDMROUND_REGISTER)
  {
    //sync_printf("Core #%ld: TDMCYCLE_REGISTER(*)=%lu, TDMROUND_REGISTER   =%lu\n", cid, TDMCYCLE_REGISTER, TDMROUND_REGISTER);
    tdmround = TDMROUND_REGISTER;
  }
  if (hyperperiod != HYPERPERIOD_REGISTER)
  {
    // copy what we got
    memcpy(&rxmsg, core[cid].rxmem, sizeof(rxmsg));

    // see handshaking protocol (which this is built upon)

    // is it a request?
    if (rxmsg.reqid > 0 && rxmsg.respid == 0)
    {
      sync_printf(cid, "Core %ld sensor request: TDMROUND_REGISTER   =%lu, HYPERPERIOD_REGISTER(*)=%lu,\n  rxmsg.reqid=%lu,\n  rxmsg.respid=%lu,\n  rxmsg.data1=%lu,\n  rxmsg.data2=%lu\n", cid, TDMROUND_REGISTER, HYPERPERIOD_REGISTER, rxmsg.reqid, rxmsg.respid, rxmsg.data1, rxmsg.data2);
      memset(&txmsg, 0, sizeof(txmsg));
      txmsg.reqid = rxmsg.reqid;
      txmsg.respid = rxmsg.reqid;
      txmsg.data1 = cid;
      txmsg.data2 = rxmsg.reqid + rxmsg.reqid + cid; // not real
      // schedule for sending (tx)
      memcpy(core[cid].txmem, &txmsg, sizeof(txmsg));
    }

    // is it a response?
    if (rxmsg.reqid > 0 && rxmsg.respid == rxmsg.reqid)
    {
      sync_printf(cid, "Core %ld sensor reply: TDMCYCLE_REGISTER   =%lu, TDMROUND_REGISTER(*)=%lu,\n  rxmsg.reqid=%lu,\n  rxmsg.respid=%lu,\n  rxmsg.sensorid=%lu,\n  rxmsg.sensorval=%lu\n", cid, TDMROUND_REGISTER, TDMROUND_REGISTER, rxmsg.reqid, rxmsg.respid, rxmsg.data1, rxmsg.data2);
    }

    hyperperiod = HYPERPERIOD_REGISTER;
  }
  step++;
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Streaming Double Buffer (sdb)
///////////////////////////////////////////////////////////////////////////////

// the cores
void corethreadsdb(void *coreid)
{
  int cid = *((int *)coreid);
  sync_printf(cid, "Core %d started...\n", cid);
  if (cid == 0)
  {
  }
  int step = 0;
  unsigned long tdmround = 0xFFFFFFFF;
  unsigned long hyperperiod = 0xFFFFFFFF;
  buffer_t buffer_in_a;
  buffer_t buffer_in_b;
  buffer_t *active_buffer_in = &buffer_in_a;
  bool a_in_active = true;
  //memset(&buffer_in_a, 0, sizeof(buffer_in_a));
  //memset(&buffer_in_b, 0, sizeof(buffer_in_b));

  // the cores are aware of the global cycle times for time-based-synchronization
  // the cores print their output after the cycle count changes are detected
  if (tdmround != TDMROUND_REGISTER)
  {
    //sync_printf("Core #%ld: TDMCYCLE_REGISTER(*)=%lu, TDMROUND_REGISTER   =%lu\n", cid, TDMCYCLE_REGISTER, TDMROUND_REGISTER);
    tdmround = TDMROUND_REGISTER;
  }
  if (hyperperiod != HYPERPERIOD_REGISTER)
  {
    //print the active buffer to see what is in it
    sync_printf(cid, "Core %ld active buffer: TDMROUND_REGISTER   =%lu, HYPERPERIOD_REGISTER(*)=%lu,\n  active_buffer_in->data[0]=%lu,\n  active_buffer_in->data[1]=%lu,\n  active_buffer_in->data[2]=%lu,\n  active_buffer_in->data[3]=%lu\n", cid, TDMROUND_REGISTER, HYPERPERIOD_REGISTER, active_buffer_in->data[0], active_buffer_in->data[1], active_buffer_in->data[2], active_buffer_in->data[3]);

    // copy what we got into the passive buffer
    if (a_in_active)
      memcpy(&buffer_in_b, core[cid].rxmem, sizeof(buffer_t));
    else
      memcpy(&buffer_in_a, core[cid].rxmem, sizeof(buffer_t));

    if (a_in_active)
    {
      active_buffer_in = &buffer_in_b;
      a_in_active = false;
    }
    else
    {
      active_buffer_in = &buffer_in_a;
      a_in_active = true;
    }

    // tx new data for other buffers at another core
    // just step data, which is only related to thread scheduling (artificial data)
    core[cid].txmem[0][0] = step;
    core[cid].txmem[1][0] = step + 1;
    core[cid].txmem[2][0] = step + 2;
    core[cid].txmem[3][0] = step + 3;

    tdmround = TDMROUND_REGISTER;
  }
  step++;
}

///////////////////////////////////////////////////////////////////////////////
//shared main
///////////////////////////////////////////////////////////////////////////////

// we are core 0
int main(int argc, char *argv[])
{
  printf("***********************************************************\n");
  printf("onewaysim-target main(): ******************************************\n");
  printf("***********************************************************\n");
  //start the other cores
  nocinit();
  // "start" ourself (core 0)
  corethreadtbs(&coreid[0]);
  // core 0 is done, wait for the others
  nocdone();
  info_printf("done...\n");
  sync_printall();

  printf("leaving main...\n");
  printf("***********************************************************\n");
  printf("***********************************************************\n");
  return 0;
}

//////////////////////////////////////////////////////////////////
//should be updated to use patmos threads?
//////////////////////////////////////////////////////////////////
// MAIN1 begin
void *coredo(void *vargp)
{
  int id = *((int *)vargp);
  unsigned long *txmem;
  txmem = alltxmem[id][0];
  unsigned long *rxmem;
  rxmem = allrxmem[id][0];

  printf("I am core %d\n", id);
  // this should be the work done
  for (int i = 0; i < 10; ++i)
  {
    printf("%d: %lu, ", id, allrxmem[id][0][0]);
    fflush(stdout);
    txmem[id] = id * 10 + id;
    usleep(10000);
  }
  return NULL; // it gave a warning to have it commented out
  //return NULL;
}

int main1()
{
  // Just to make it clear that those are accessed by threads
  //static pthread_t tid[4];
  static int id[4];

  printf("\nmain1(): Simulation starts\n");
  for (int i = 0; i < 4; ++i)
  {
    id[i] = i;
    //int returncode = pthread_create(&tid[i], NULL, coredo, &id[i]);
  }

  // main is simulating the data movement by the NIs through the NoC
  for (int i = 0; i < 10; ++i)
  {
    // This is VERY incomplete and does not reflect the actual indexing
    // And that it is more than one word in the memory blocks
    allrxmem[0][0][0] = alltxmem[1][3][0];
    allrxmem[1][0][0] = alltxmem[3][0][0];
    allrxmem[2][0][0] = alltxmem[2][0][0];
    allrxmem[3][0][0] = alltxmem[1][0][0];
    usleep(100000);
    // or maybe better ues yield() to give
    // TODO: find the buffer mapping from the real hardware for a more
    // realistic setup
  }

  usleep(1000);

  for (int i = 0; i < 4; ++i)
  {
    //pthread_join(tid[i], NULL);
  }

  printf("\n");
  for (int i = 0; i < 4; ++i)
  {
    printf("alltxmem[%d][0]: 0x%08lx\n", i, alltxmem[i][0][0]);
  }

  for (int i = 0; i < 4; ++i)
  {
    printf("allrxmem[%d][0]: 0x%08lx\n", i, allrxmem[i][0][0]);
  }

  printf("End of simulation\n");
  return 0;
}

// MAIN1 end
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////