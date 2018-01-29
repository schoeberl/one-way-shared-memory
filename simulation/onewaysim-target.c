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
  int ackcoreidindexmap = getrxslot(pushcoreid, cid, 0);
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
