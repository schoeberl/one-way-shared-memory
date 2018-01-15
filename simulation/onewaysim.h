#ifndef ONEWAYSIM_H
#define ONEWAYSIM_H
/*
  A software simulation of the One-Way Shared Memory
  Shared header file

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

// NoC configuration
#define CORES 4
// one core configuration
#define MEMBUF 4 // 32-bit words

// global memory
extern unsigned long alltxmem[CORES][MEMBUF*CORES]; // a slot for loop-back testing included
extern unsigned long allrxmem[CORES][MEMBUF*CORES]; // a slot for loop-back testing included

// patmos hardware registers provided via Scala HDL
typedef volatile unsigned long PATMOS_REGISTER;

// defined patmos hardware registers (simulated)
// we do not have one for the SLOT counter in the simulator

// one word delivered from all to all
PATMOS_REGISTER TDMROUND_REGISTER; 
// one memory block delivered (word for word) from all to all
PATMOS_REGISTER HYPERPERIOD_REGISTER;

// shared common simulation structs
typedef struct Core
{
  // each core's local memory use a particular row of ms's global memory
  // global memory: unsigned long alltxmem[CORES][TXRXMEMSIZE]
  //                unsigned long allrxmem[CORES][TXRXMEMSIZE]
  unsigned long* txmem; //tx[TXRXMEMSIZE];
  unsigned long* rxmem; //rx[TXRXMEMSIZE];
} Core;

// declare noc consisting of cores
extern Core core[CORES];

// a struct for handshaking
typedef struct handshakemsg_t{
  // if non-zero then valid msg
  // if respid = 0 then it is a request
  unsigned long reqid; 
  // if non-zero then it is a response
  unsigned long respid;
  unsigned long data1;
  unsigned long data2;
} handshakemsg_t;

// a struct for state exchange
typedef struct es_msg_t{
  // if non-zero then valid msg
  // if respid = 0 then it is a request
  unsigned long reqid; 
  // if non-zero then it is a response
  unsigned long respid;
  //sensor id
  unsigned long sensorid;
  //sensor value
  unsigned long sensorval;
} es_msg_t;

// a struct for one buffer
typedef struct buffer_t{
  unsigned long data[MEMBUF*CORES]; 
} buffer_t;

// shared (will not work on target yet)
extern bool runnoc;

// functions in the target and simulator
int sync_printf(const char *format, ...);
void *corethreadtbs(void *coreid);
void *corethreadhsp(void *coreid);
void *corethreades(void *coreid);
void *corethreadsdb(void *coreid);

void initsim();

#endif