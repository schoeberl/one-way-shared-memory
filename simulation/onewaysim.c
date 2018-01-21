/*
  A software simulation of the One-Way Shared Memory
  Some utility functions

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

#include <machine/patmos.h>
#include "onewaysim.h"

//target tables
//information on where to send tx slots
//example: rxslot[1][3][2] = 1 means that core 1 will tx a word to core 3
//           from tx slot 2 into rx slot 1 respectively
//useful when working on code in the transmitting end
int rxslots[CORES][CORES][CORES - 1];

//find the tx core that filled a certain rxslot.
//useful when working in code running in the receiving end
int txcoreids[CORES][CORES - 1];

// init rxslot and txcoreid lookup tables
void inittxrxmaps()
{
  for (int txcoreid = 0; txcoreid < CORES; txcoreid++) // sender core
  {
    int txslot = 0;
    for (int rxcoreid = 0; rxcoreid < CORES; rxcoreid++) // receiver core
    {
      if (rxcoreid != txcoreid) // cannot tx to itself
      {
        int rxslot = 0;
        if (txcoreid > rxcoreid)
          rxslot = txcoreid - 1;
        else
          rxslot = txcoreid;
        rxslots[txcoreid][rxcoreid][txslot] = rxslot;
        txcoreids[rxcoreid][rxslot] = txcoreid;
        //sync_printf("rxslots[txcoreid:%d][rxcoreid:%d][txslot:%d] = %d\n", txcoreid, rxcoreid, txslot, rxslot);
        //sync_printf("txcoreids[rxcoreid:%d][rxslot:%d] = %d\n", rxcoreid, rxslot, txcoreid);
        txslot++;
      }
    }
    //sync_printf("\n");
  }
}

// rx slot from txcoreid, rxcoreid, and txslot
int getrxslot(int txcoreid, int rxcoreid, int txslot)
{
  //sync_printf("rxslots[txcoreid:%d][rxcoreid:%d][txslot:%d] = %d\n", txcoreid, rxcoreid, txslot, rxslots[txcoreid][rxcoreid][txslot]);
  return rxslots[txcoreid][rxcoreid][txslot];
}

// txcoreid from rxcoreid and rxslot
int gettxcoreid(int rxcoreid, int rxslot)
{
  //sync_printf("txcoreids[rxcoreid:%d][rxslot:%d] = %d\n", rxcoreid, rxslot, txcoreids[rxcoreid][rxslot]);
  return txcoreids[rxcoreid][rxslot];
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
    volatile _IODEV int *io_ptr = (volatile _IODEV int *)0xf0020004; // cycles 0xf002000c; // timer low word
    int val = *io_ptr;
    timestamps[cid][mi[cid]] = (unsigned long)val;
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
    volatile _IODEV int *io_ptr = (volatile _IODEV int *)0xf0020004; // cycles 0xf002000c; // timer low word
    int val = *io_ptr;
    timestamps[CORES][mi[CORES]] = (unsigned long)val;
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
      printf("[cycle=%10lu, info=%d, msg#=%2d] %s", timestamps[closestcoreid][minmark[closestcoreid]], 0,
             minmark[closestcoreid], strings[closestcoreid][minmark[closestcoreid]]);
    else // other cores
      printf("[cycle=%10lu, core=%d, msg#=%2d] %s", timestamps[closestcoreid][minmark[closestcoreid]], closestcoreid,
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

  printf("leaving sync_printall()...\n");
}
