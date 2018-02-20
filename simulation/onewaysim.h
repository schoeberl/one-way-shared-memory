#ifndef ONEWAYSIM_H
#define ONEWAYSIM_H
/*
  A software simulation of the One-Way Shared Memory
  Shared header file

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include <math.h>

#ifdef __patmos__
#define RUNONPATMOS
#endif

#ifdef RUNONPATMOS
#include "libcorethread/corethread.h"
#else
#include <pthread.h>
#endif

// how many messages the printfbuffer can store for each core
#define SYNCPRINTBUF 50

// NoC setup
// See:https://github.com/schoeberl/one-way-shared-memory/blob/master/src/main/scala/oneway/Network.scala

// One-way-memory NoC configuration:
//   CORES is defined from the NoC configuration (2x2, 3x3, etc.). It is derived 
//   from the speciffic configuration such as FOURNODES_N. 
//   Each core has one set (at least) of TX and RX TDM slots for communicating 
//   (i.e., sending and receiving one word/flit to/from each of the other cores) 
//   with the other cores.  
//   Additionally, each core has a number of buffers (MEMBUF) such that a 
//   message of several words/flits is transmitted in what is called
//   a TDMROUND.
//
//   Example TX setup for one core in a 2x2 NoC configuration:
//   CORES = 4
//   MEMBUF = 4
//                               TDMROUND 
//               MEMBUF[0] MEMBUF[1] MEMBUF[2] MEMBUF[3]  
//              +---------+---------+---------+---------+
//              |TX slot 2|TX slot 2|TX slot 2|TX slot 2|
//              +---------+---------+---------+---------+
//   TDM slots: |TX slot 1|TX slot 1|TX slot 1|TX slot 1|
//              +---------+---------+---------+---------+
//              |TX slot 0|TX slot 0|TX slot 0|TX slot 0|
//              +---------+---------+---------+---------+
//   
//   The routing in the one-way-mem NoC grid is defined by FOURNODES (for a 2x2 grid).
//   The NoC starts with the transmission of the word in TX slot 0 for MEMBUF[0] for
//   each core in parallel. The route for this very first word is the first line in 
//   FOURNODES; "nel". Two clock cycles later, the word in TX slot 1 (still MEMBUF[0])
//   is transmitted on the route "  nl". After transmitting TX slot 2 (one clock
//   cycle after transmitting TX slot 1), the second TDMROUND starts. This happens
//   one clock cycle after and hee TX slot 0 is transmitted from MEMBUF[1]. When
//   TX slot 2 is transmitted from MEMBUF[3], the TDMROUND is finished (and a new one 
//   starts). 
#define FOURNODES "nel|"\
                  "  nl|"\
		  "   el|"
#define FOURNODES_N 4

// do edit this to set up the NoC grid and buffer
#define ROUTESSTRING FOURNODES
#define CORES FOURNODES_N
#define MEMBUF 4 

// do not edit
// grid side size 
#define GRIDN ((int)sqrt(CORES))
// one core configuration
#define TDMSLOTS (CORES - 1)

// global memory
// a slot for loop-back testing included
extern unsigned long alltxmem[CORES][CORES - 1][MEMBUF]; 
extern unsigned long allrxmem[CORES][CORES - 1][MEMBUF];

// patmos hardware registers provided via Scala HDL
#ifdef RUNONPATMOS
volatile _UNCACHED bool runcores;
typedef volatile _UNCACHED unsigned long PATMOS_REGISTER;
#else
volatile bool runcores;
typedef volatile unsigned long PATMOS_REGISTER;
#endif

// defined patmos hardware registers (simulated)
// we do not have one for the SLOT counter in the simulator

// one word delivered from all to all
PATMOS_REGISTER TDMROUND_REGISTER;
// one memory block delivered (word for word) from all to all
PATMOS_REGISTER HYPERPERIOD_REGISTER;

// shared common simulation structs
typedef struct Core
{
  // each core's local memory use a particular row (the owner core) of alltxmem and allrxmem
  // txmem is an unsigned long array of [CORES-1][MEMBUF]
  //   txmem sends to each of the other cores (therefore it is [CORES-1])
  //   since txmem is [CORES-1], there is a mapping going on in txmem
  //   this mapping means that the sender core's id is skipped in the index
  //   for instance when core 0 does its second word, then the first index (txmem[0][1])
  //     will actually mean core 1. The '[1]' in means the second word and thre is no
  //     mapping going on there
  unsigned long **txmem;
  // rxmem is an unsigned long array of [CORES-1][MEMBUF]
  unsigned long **rxmem;
} Core;

// declare noc consisting of cores
extern Core core[CORES];

// a struct for the handshake push message
typedef struct handshakemsg_t
{
  unsigned long data0;
  unsigned long data1;
  unsigned long data2;
  unsigned long pushid; // 0 means no message
} handshakemsg_t;

// a struct for handshaking acknowledgement
typedef struct handshakeack_t
{
  unsigned long ackid;
} handshakeack_t;

// a struct for state exchange
typedef struct es_msg_t
{
  // sensor id
  unsigned long sensorid;
  //sensor value
  unsigned long sensorval;
} es_msg_t;

// a struct for one buffer
typedef struct buffer_t
{
  unsigned long data[MEMBUF * CORES];
} buffer_t;

void nocdone();
void noccontrol();
#ifndef RUNONPATMOS
void simcontrol();
#endif

// functions in the target and simulator
unsigned long getcycles();
void sync_printf(int, const char *format, ...);
void info_printf(const char *format, ...);
void sync_printall();
void corethreadtbs(void *coreid);
void corethreadhsp(void *coreid);
void corethreades(void *coreid);
void corethreadsdb(void *coreid);

void playandtest();

void memtxprint(int coreid);
void memrxprint(int coreid);
void memallprint();

void inittxrxmaps();
int getrxslot(int txcoreid, int rxcoreid, int txslot);
int gettxcoreid(int rxcoreid, int rxslot);

// the two new ones (todo: merge)
void initroutestrings();
void showmappings();

#endif
