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
#include "onewaysim.h"

unsigned long alltxmem[CORES][CORES - 1][MEMBUF];
unsigned long allrxmem[CORES][CORES - 1][MEMBUF];

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Time-Based Synchronization (tbs)
///////////////////////////////////////////////////////////////////////////////

// the cores
void *corethreadtbs(void *coreid)
{
  int cid = *((int *)coreid);
  int step = 0;
  unsigned long tdmround = 0xFFFFFFFF;
  unsigned long hyperperiod = 0xFFFFFFFF;
  while (runnoc)
  {
    // the cores are aware of the global cycle times for time-based-synchronization
    // the cores print their output after the cycle count changes are detected
    if (tdmround != TDMROUND_REGISTER)
    {
      sync_printf("Core #%ld: TDMROUND_REGISTER(*)=%lu, HYPERPERIOD_REGISTER   =%lu\n", cid, TDMROUND_REGISTER, HYPERPERIOD_REGISTER);
      tdmround = TDMROUND_REGISTER;
    }
    if (hyperperiod != HYPERPERIOD_REGISTER)
    {
      sync_printf("Core #%ld: TDMROUND_REGISTER   =%lu, HYPERPERIOD_REGISTER(*)=%lu\n", cid, TDMROUND_REGISTER, HYPERPERIOD_REGISTER);
      hyperperiod = HYPERPERIOD_REGISTER;
    }
    step++;
  }
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Handshaking Protocol
///////////////////////////////////////////////////////////////////////////////

// the cores
void *corethreadhsp(void *coreid)
{
  int cid = *((int *)coreid);
  static int step = 0;
  static unsigned long tdmround = 0xFFFFFFFF;
  static unsigned long hyperperiod = 0xFFFFFFFF;
  static handshakemsg_t txmsg;
  memset(&txmsg, 0, sizeof(txmsg));
  static handshakemsg_t rxmsg;
  memset(&rxmsg, 0, sizeof(rxmsg));

  sync_printf("in corethreadhsp(%d)...\n", cid);
sync_printf("sizeof(txmsg)=%d\n", sizeof(txmsg));
  //kickstart with sending a request to each of the other cores
  txmsg.reqid = 1; // want to get message 1 from another core (and no more)
memtxprint(cid);
printf("\n");
  memcpy(core[cid].txmem[0], &txmsg, sizeof(txmsg));
memtxprint(cid);
  //while (runnoc)
  //{
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
    sync_printf("Core %ld: TDMROUND_REGISTER   =%lu, HYPERPERIOD_REGISTER(*)=%lu,\n  rxmsg.reqid=%lu,\n  rxmsg.respid=%lu,\n  rxmsg.data1=%lu,\n  rxmsg.data2=%lu\n", cid, TDMROUND_REGISTER, HYPERPERIOD_REGISTER, rxmsg.reqid, rxmsg.respid, rxmsg.data1, rxmsg.data2);
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
  //}
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Exchange of state
///////////////////////////////////////////////////////////////////////////////

// the cores
void *corethreades(void *coreid)
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

  while (runnoc)
  {
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
        sync_printf("Core %ld sensor request: TDMROUND_REGISTER   =%lu, HYPERPERIOD_REGISTER(*)=%lu,\n  rxmsg.reqid=%lu,\n  rxmsg.respid=%lu,\n  rxmsg.data1=%lu,\n  rxmsg.data2=%lu\n", cid, TDMROUND_REGISTER, HYPERPERIOD_REGISTER, rxmsg.reqid, rxmsg.respid, rxmsg.data1, rxmsg.data2);
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
        sync_printf("Core %ld sensor reply: TDMCYCLE_REGISTER   =%lu, TDMROUND_REGISTER(*)=%lu,\n  rxmsg.reqid=%lu,\n  rxmsg.respid=%lu,\n  rxmsg.sensorid=%lu,\n  rxmsg.sensorval=%lu\n", cid, TDMROUND_REGISTER, TDMROUND_REGISTER, rxmsg.reqid, rxmsg.respid, rxmsg.data1, rxmsg.data2);
      }

      hyperperiod = HYPERPERIOD_REGISTER;
    }
    step++;
  }
}

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Streaming Double Buffer (sdb)
///////////////////////////////////////////////////////////////////////////////

// the cores
void *corethreadsdb(void *coreid)
{
  int cid = *((int *)coreid);
  sync_printf("Core %d started...\n", cid);
  int step = 0;
  unsigned long tdmround = 0xFFFFFFFF;
  unsigned long hyperperiod = 0xFFFFFFFF;
  buffer_t buffer_in_a;
  buffer_t buffer_in_b;
  buffer_t *active_buffer_in = &buffer_in_a;
  bool a_in_active = true;
  memset(&buffer_in_a, 0, sizeof(buffer_in_a));
  memset(&buffer_in_b, 0, sizeof(buffer_in_b));

  while (runnoc)
  {
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
      sync_printf("Core %ld active buffer: TDMROUND_REGISTER   =%lu, HYPERPERIOD_REGISTER(*)=%lu,\n  active_buffer_in->data[0]=%lu,\n  active_buffer_in->data[1]=%lu,\n  active_buffer_in->data[2]=%lu,\n  active_buffer_in->data[3]=%lu\n", cid, TDMROUND_REGISTER, HYPERPERIOD_REGISTER, active_buffer_in->data[0], active_buffer_in->data[1], active_buffer_in->data[2], active_buffer_in->data[3]);

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
}

///////////////////////////////////////////////////////////////////////////////
//shared main
///////////////////////////////////////////////////////////////////////////////
// MS: yes, we are missing Java with the option on having more than one main entry
// into a project :-(

int main(int argc, char *argv[])
{
  printf("\n");
  printf("*********************************************************************\n");
  printf("onewaysim main(): ***************************************************\n");
  printf("*********************************************************************\n");
  //main1();
  initsim();
  printf("\n");
}