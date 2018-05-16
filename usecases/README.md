# One-Way Memory Use-Cases

This repository contains the use-cases that have been used in the evaluation
section of following paper:

Martin Schoeberl and Rasmus Ulslev Pedersen, One-Way Memory, submitted to the
Journal of Systems Architecture.

## Abstract

The number of cores (CORES) is defined from the NoC configuration (2x2, 3x3, etc.). It is derived from the specific configuration such as FOURNODES_N. Each core has one set (at least) of TX and RX TDM slots for communicating (i.e., sending and receiving one word/flit to/from each of the other cores) 
with the other cores. Additionally, each core has a number of buffers (MEMBUF) such that a 
message of several words/flits is transmitted in what is called a TDMROUND.

Example TX setup for one core in a 2x2 NoC configuration:
CORES = 4, also called CNT
MEMBUF = 4 (up to 256), also called WORDS
```
                                    HYPERPERIOD 
   Dou. buf. ex.: |DOUBLEBUFFER[0]              |DOUBLEBUFFER[1]   ...
                   MEMBUF[0] MEMBUF[1] MEMBUF[2] MEMBUF[3] ... WORDS - 1 
                  +---------+---------+---------+---------+---
                  |TX slot 0|TX slot 0|TX slot 0|TX slot 0|
   TDMROUND:      +---------+---------+---------+---------+---
     TDM slots    |TX slot 1|TX slot 1|TX slot 1|TX slot 1|
     CORES -1     +---------+---------+---------+---------+---
                  |TX slot 2|TX slot 2|TX slot 2|TX slot 2|
                  +---------+---------+---------+---------+---
```

The routing in the one-way memory NoC grid is defined by FOURNODES (for a 2x2 grid). The NoC starts with the transmission of the word in TX slot 0 for MEMBUF[0] for each core in parallel. The route for this very first word is the first line in FOURNODES; "nel". Two clock cycles later, the word in TX slot 1 (still MEMBUF[0])
is transmitted on the route "  nl". After transmitting TX slot 2 (one clock cycle after transmitting TX slot 1), the second TDMROUND starts. This happens one clock cycle after and the TX slot 0 is transmitted from MEMBUF[1]. When
TX slot 2 is transmitted from MEMBUF[3], the TDMROUND is finished (and a new one starts). 

## Source Code and Use-cases

The source code is split into three parts. One part that runs only on the PC (the simulator), one part that runs only on Patmos (setting up the use-cases for execution), and a common part (the actual use-cases and some utilities). The files used for simulation or running the code on a HW platform are listed below. 

There are the use-cases:
* 0: Simple use-case for verifying NoC routing 
* 1: Time-based synchronization use-case
* 2: Handshaking protocol use-case
* 3: State exchange use-case
* 4: Double buffer use-case

The code in the `onewayuse` directory can do two things. First, it can simulate multicore use-cases on the PC. Second, it can use Patmos to run the same use-cases directly on hardware. In the make script below, each use-case is identified by the identifier USECASE as either 0, 1, ..., etc.

`RUNONPATMOS` is defined in onewaysim.h and determines if the code shall run on Patmos or not. It is automatically set by the build system when running on Patmos:
```
#ifdef __patmos__
  #define RUNONPATMOS
#endif
```

## Simulating on a PC

The `onpc` target is for PC-based simulation. `RUNONPATMOS` is not defined.

Files in use: onemem-simulator.c, onewaymem-usecases.c, onewaymem-usecase$(usecase).c, onewaysim.h, syncprint.c, and syncprint.h.

A specific use-case is run by supplying the use-case identifier to the respective make target. The following would execute use-case 0 on the PC:

```
make usecase=0 onpc
```

## Executing on Hardware Platform

The `onpatmos` target is for running code directly on Patmos. 

Files in use: onemem-patmos_onewayuse.c, onewaymem-usecases.c, onewaymem-usecase$(usecase).c, onewaysim.h, syncprint.c, syncprint.h

The following would execute use-case 1 on Patmos HW:

```
make usecase=1 onpatmos
```