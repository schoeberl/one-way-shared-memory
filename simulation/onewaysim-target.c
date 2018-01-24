/*
  A software simulation of the One-Way Shared Memory
  Run on Patmos (the target) (and the PC)

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

//#include <machine/patmos.h>
#include "onewaysim.h"

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

  // todo: poll on the last word

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

  // copy what we got
  memcpy(&rxmsg, core[cid].rxmem, sizeof(rxmsg));

  // see handshaking protocol (which this is built upon)
  // todo: poll on the last word
  
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

  // todo: poll on the last word
  
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
}
