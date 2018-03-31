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
  #define _SPM
  #define _IODEV
  #define _UNCACHED
  #include <pthread.h>
#endif

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
//   CORES = 4, also called CNT
//   MEMBUF = 4 (up to 256), also called WORDS
//                                   HYPERPERIOD 
//   Dou. buf. ex.: |DOUBLEBUFFER[0]              |DOUBLEBUFFER[1]   ...                  
//                   MEMBUF[0] MEMBUF[1] MEMBUF[2] MEMBUF[3] ... WORDS - 1  
//                  +---------+---------+---------+---------+---
//                  |TX slot 0|TX slot 0|TX slot 0|TX slot 0|
//   TDMROUND:      +---------+---------+---------+---------+---
//     TDM slots    |TX slot 1|TX slot 1|TX slot 1|TX slot 1|
//     CORES -1     +---------+---------+---------+---------+---
//                  |TX slot 2|TX slot 2|TX slot 2|TX slot 2|
//                  +---------+---------+---------+---------+---
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
#define MEMBUF 256
#define WORDS MEMBUF 

// do not edit
// grid side size 
#define GRIDN ((int)sqrt(CORES))
// one core configuration
#define TDMSLOTS (CORES - 1)

// global memory
// a slot for loop-back testing included
//extern unsigned long alltxmem[CORES][CORES - 1][MEMBUF]; 
//extern unsigned long allrxmem[CORES][CORES - 1][MEMBUF];

// patmos hardware registers provided via Scala HDL
volatile _UNCACHED bool runcores;
volatile _UNCACHED bool coreready[CORES];
typedef volatile _UNCACHED unsigned int PATMOS_REGISTER;

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
  //     will actually mean core 1. The '[1]' in means the second word and there is no
  //     mapping going on there
  volatile _SPM int *tx[TDMSLOTS];
  // rxmem is an unsigned long array of [CORES-1][MEMBUF]
  volatile _SPM int *rx[TDMSLOTS];
} Core;

// declare noc consisting of cores
extern Core core[CORES];

// a struct for the handshake push message
#define HANDSHAKEMSGSIZE 8
typedef struct handshakemsg_t
{
  // first word captures some noc router information for possible extra checks on the receiver side
  // coreid*0x10000000 + tdmslot*0x1000000 + word*0x10000 + txcnt;
  unsigned int txstamp;
  // sender core id
  unsigned int fromcore;
  // receiver core id
  unsigned int tocore;
  // number of data words
  unsigned int length; 
  // data words (an array can also be used if a more complex/dynamic scheme is wanted)
  unsigned int data0;
  unsigned int data1;
  unsigned int data2;
  // block number: starts at 1 and is acknowledged in the handshake confirmation msg
  unsigned int blockno; 
} handshakemsg_t;

// a struct for handshaking acknowledgement
#define HANDSHAKEACKSIZE 4
typedef struct handshakeack_t
{
  // first word captures some noc router information for possible extra checks on the receiver side
  // coreid*0x10000000 + tdmslot*0x1000000 + word*0x10000 + msgid;
  unsigned int txstamp;
  // the receiver core that is acknowleding 
  unsigned int fromcore;
  // the original sender core waiting for the acknowledgement
  unsigned int tocore;
  // block number: from the just received handshakemsg 
  unsigned int blockno; 
} handshakeack_t;

// a struct for state exchange
typedef struct es_msg_t
{
  // first word captures some noc router information for possible extra checks on the receiver side
  // coreid*0x10000000 + tdmslot*0x1000000 + word*0x10000 + msgid;
  unsigned int txstamp;
  // timestamp
  unsigned int timestamp;
  // sensor id
  unsigned int sensorid;
  //sensor value
  unsigned int sensorval;
} es_msg_t;

// how many ("double") buffers overlaid on each core's tx/rx buffer (of size WORDS) 
// 2, 4, 8, ...
#define DOUBLEBUFFERS 2
#define DBUFSIZE WORDS/DOUBLEBUFFERS
// a struct for one buffer
typedef struct buffer_t
{
  //unsigned int txstamp;
  //data points to an array of DBUFSIZE words (like in "unsigned int data[DBUFSIZE]")
  volatile _SPM int *data;
} buffer_t;

// init patmos (simulated) internals
Core core[CORES];
// signal used to stop terminate the cores

int coreid[CORES];

void nocinit();
void nocstart();
void noccontrol();
void nocwaitdone();
#ifndef RUNONPATMOS
void simcontrol();
#endif

void corethreadtbswork(void *cidptr);
void corethreadhswork(void *noarg);
void corethreadeswork(void *noarg);
void corethreadsdbwork(void *noarg);

void playandtest();

// uses the router string (e.g., "nel, nl, el", ...)
// to generate the mappings that rxcorefromtxcoreslot and 
// txcorefromrxcoreslot use.
void txrxmapsinit();
// get the rx core based on tx core and tx (TDM) slot index
int getrxcorefromtxcoreslot(int txcore, int txslot);
// get the tx core based on tx core and rx (TDM) slot index
int gettxcorefromrxcoreslot(int rxcore, int rxslot);


#ifndef RUNONPATMOS
  extern pthread_t cpu_id_tid[CORES];
  int get_cpuid();
#endif

// the two new ones (todo: merge)
void initroutestrings();
void showmappings();

#ifdef RUNONPATMOS
#define ONEWAY_BASE ((volatile _IODEV int *) 0xE8000000)
extern volatile _SPM int *alltxmem;
extern volatile _SPM int *allrxmem;
#else
extern int alltxmem[CORES][CORES - 1][MEMBUF];
extern int allrxmem[CORES][CORES - 1][MEMBUF];
#endif

#endif
