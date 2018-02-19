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
#include <math.h>

#ifndef RUNONPATMOS
#include <time.h>
#endif

#include "onewaysim.h"

unsigned long alltxmem[CORES][CORES - 1][MEMBUF];
unsigned long allrxmem[CORES][CORES - 1][MEMBUF];

///////////////////////////////////////////////////////////////////////////////
// Control code for communication patterns
///////////////////////////////////////////////////////////////////////////////

// init patmos (simulated) internals
Core core[CORES];
// signal used to stop terminate the cores

static int coreid[CORES];

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
  inittxrxmaps();

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

///////////////////////////////////////////////////////////////////////////////
//shared main
///////////////////////////////////////////////////////////////////////////////
// we are core 0
int main(int argc, char *argv[])
{
  runcores = true;
  printf("***********************************************************\n");
  printf("onewaysim-target main(): **********************************\n");
  printf("***********************************************************\n");
#ifdef RUNONPATMOS
  printf("RUNONPATMOS is defined...\n");
  // play and test
  // comment out if not needed or used
  //playandtest(); return 0;
#else
  printf("RUNONPATMOS is not defined...\n");
#endif
  //start the other cores
  runcores = true;
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
  printf("TX AND RX MAPPING TESTING:\n");
  initroutestrings();
  showmappings();
  return 0;
}

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
// utility stuff
///////////////////////////////////////////////////////////////////

// new mappings (still not perfect)
// todo: merge / delete with present mappings (see further down)
#define FOURNODES "nel|"\
                  "  nl|"\
                  "   el|"

#define FOURNODES_N 4

static const char *rstr = FOURNODES;

// TX slot in rows, Cores in cols
static int tx_tdm_slot[FOURNODES_N-1][FOURNODES_N] = {0};

// route strings
static char *routes[FOURNODES_N-1];

// get coreid from position in grid
int getcoreid(int row, int col, int n){
  return row * n + col;
}

void initroutestrings(){
  int start = 0;
  int stop = 0;
  for(int i = 0; i < FOURNODES_N-1; i++){
    while(rstr[stop] != '|')
      stop++;
    routes[i] = calloc(stop - start + 1 , 1);
    strncpy(routes[i], &rstr[start], stop - start);
    stop++;
    start = stop;
    //printf("%s\n", routes[i]);
  }
}

void showmappings(){
  int n = (int)sqrt(FOURNODES_N);
  int rows = n;
  int cols = n;
  int routecnt = FOURNODES_N - 1;

  // tx router grid
  for(int tx_i = 0; tx_i < rows; tx_i++){
    for(int tx_j = 0; tx_j < cols; tx_j++){
      // simulate each route for the given rx core
      for(int r = 0; r < routecnt; r++){
	// the tx and rx tdm slot are known by now
        char *route = routes[r];
	int txtdmslot = r;
	int rxtdmslot = (strlen(route)) % 3; 
	// find the rx core id
	int rx_i = tx_i;
	int rx_j = tx_j;
        for(int s = 0; s < strlen(route); s++){
          switch(route[s]){
	    case 'n':
              rx_i = (rx_i - 1 >= 0 ? rx_i - 1 : n - 1);
	      break;
	    case 's':
	      rx_i = (rx_i + 1 < n ? rx_i + 1 : 0);
	      break;
            case 'e':
              rx_j = (rx_j + 1 < n ? rx_j + 1 : 0);
	      break;
            case 'w':
	      rx_j = (rx_j - 1 >= 0 ? rx_j - 1 : n - 1);
	      break;
	  }
        }
	if (getcoreid(rx_i, rx_j, n) == 2) {
	  printf("\"%s\":\n", route); 
          printf("  Core %d@(%d,%d)  to Core %d@(%d,%d)\n",  getcoreid(tx_i, tx_j, n), tx_i, tx_j, getcoreid(rx_i, rx_j, n), rx_i, rx_j); 
          printf("  TX TDM slot %d to RX TDM slot %d\n", txtdmslot, rxtdmslot); 
	}
     }
    }
  }
}







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
