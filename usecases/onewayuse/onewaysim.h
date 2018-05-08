#ifndef ONEWAYSIM_H
#define ONEWAYSIM_H
/*
  A software simulation of the One-Way Shared Memory
  Shared header file

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
#ifndef RUNONPATMOS
#include <time.h>
#endif
#include <math.h>
#include <inttypes.h>
#include "syncprint.h"

#ifdef __patmos__
  #define RUNONPATMOS
#endif

#ifdef RUNONPATMOS
  #include "libcorethread/corethread.h"
#else
  // define as nothing when just simulating on the PC
  #define _SPM
  #define _IODEV
  #define _UNCACHED
  #include <pthread.h>
  #include <sched.h>
#endif

// NoC setup
// See:https://github.com/schoeberl/one-way-shared-memory/blob/master/src/main/scala/oneway/Network.scala

#define FOURNODES "nel|"\
"  nl|"\
"   el|"
#define FOURNODES_N 4

// do edit this to set up the NoC grid and buffer
#define ROUTESSTRING FOURNODES
#define CORES FOURNODES_N
//#define MEMBUF 256
//#define WORDS MEMBUF 
#define WORDS 256 

// do not edit
// grid side size 
#define GRIDN ((int)sqrt(CORES))
// one core configuration
#define TDMSLOTS (CORES - 1)

// patmos hardware registers provided via Scala HDL
volatile _UNCACHED bool runcores;
volatile _UNCACHED bool coreready[CORES];
typedef volatile _UNCACHED unsigned int PATMOS_REGISTER;

// one word delivered from all to all
PATMOS_REGISTER TDMROUND_REGISTER;
// one memory block delivered (word for word) from all to all
PATMOS_REGISTER HYPERPERIOD_REGISTER;

// shared common simulation structs
typedef struct Core
{
  // each core's local memory use a particular row (the owner core) of alltxmem and allrxmem
  // txmem is an unsigned int array of [CORES-1][MEMBUF]
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
void precoreloopwork(int loopcnt);

int getcpuidfromptr(void *acpuidptr);

// test case function declarations
void corethreadtestwork(void *cidptr);
void corethreadtbswork(void *cidptr);
void corethreadhswork(void *noarg);
void corethreadeswork(void *noarg);
void corethreadsdbwork(void *noarg);

// state that can be shared
typedef struct State {
  // State common to any use-case
  // the core id
  int cpuid;
  // states...
  int state;
  // loop counter for control loop
  int loopcount;
  // when the core is running (not in the final waiting state)
  bool corerunning;
  // when the core is just spinning in the last waiting state
  bool coredone;
  // the global flag (mirroed locally)
  bool runcore;
  // Local state per use-case as needed
#if USECASE==0

#elif USECASE==1
  // previous hyperperiod (used to detect/poll for new hyperperiod)
  unsigned int prevhyperperiod[TDMSLOTS];
  int startcycle;
  int stopcycle[TDMSLOTS];

#elif USECASE==2
  unsigned int step;
  unsigned int txcnt;
  unsigned int hyperperiod;
  int starttime;
  int endtime;
  unsigned int blockno;
  // set up the use case so all cores will send a handshake message to the other cores
  // and receive the appropriate acknowledgement
  handshakemsg_t hmsg_out[TDMSLOTS];
  handshakemsg_t hmsg_in[TDMSLOTS];
  handshakeack_t hmsg_ack_out[TDMSLOTS];
  handshakeack_t hmsg_ack_in[TDMSLOTS];
  unsigned int prevhyperperiod[TDMSLOTS];

#elif USECASE==3
  int txcnt;
  es_msg_t esmsg_out;
  es_msg_t esmsg_in[TDMSLOTS];

#elif USECASE==4
  int txcnt;
  int roundstate;
  // double buffers on tx
  buffer_t buf_out[TDMSLOTS][DOUBLEBUFFERS];
  // double buffers on rx
  buffer_t buf_in[TDMSLOTS][DOUBLEBUFFERS];
#endif
} State;

#ifndef RUNONPATMOS
  State states[CORES];
#endif

void playandtest();

// uses the router string (e.g., "nel, nl, el", ...)
// to generate the mappings that rxcorefromtxcoreslot and 
// txcorefromrxcoreslot use.
void txrxmapsinit();

// get tx slot from txcore, rxcore, and rxslot
int gettxslotfromtxcorerxcoreslot(int txcore, int rxcore, int rxslot);
// get rx slot from rxcore, txcore, and txslot
int getrxslotfromrxcoretxcoreslot(int rxcore, int txcore, int txslot);
// get the rx core based on tx core and tx (TDM) slot index
int getrxcorefromtxcoreslot(int txcore, int txslot);
// get the tx core based on tx core and rx (TDM) slot index
int gettxcorefromrxcoreslot(int rxcore, int rxslot);

#ifndef RUNONPATMOS
//extern pthread_t cpu_id_tid[CORES];
int get_cpuid();
#endif

// the two new ones (todo: merge)
void initroutestrings();
void showmappings();

void recordhyperperiodwork(int cpuid, unsigned int* hyperperiods);
void holdandgowork(int cpuid);
void spinwork(unsigned int waitcycles);

#ifdef RUNONPATMOS
#define ONEWAY_BASE ((volatile _IODEV int *) 0xE8000000)
extern volatile _SPM int *alltxmem;
extern volatile _SPM int *allrxmem;
#else
extern int alltxmem[CORES][CORES - 1][WORDS];
extern int allrxmem[CORES][CORES - 1][WORDS];
#endif

// get cycles (patmos) or time (pc)
int getcycles();

//unsigned int rdtsc();
#endif
