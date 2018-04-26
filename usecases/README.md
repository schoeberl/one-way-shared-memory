# onewaymem Use-Case Testing

The code in the `onewayuse` can do two things. First, it can simulate multicore usecases on
the PC. Second, it can use patmos to run the same usecases directy on hardware. 

There are four use-cases at this point. They are as follows:
* U0: Time-based synchronization use-case
* U1: Handshaking protocol use-case
* U2: State exchange use-case
* U3: Double buffer use-case

In the make script below, each use-case is identified by the identifier U0, U1, U2, or U3.

## Source code organiztion

The source code is split in three parts. One part that runs only on the PC (the simulator), one part that runs only on patmos (setting up the usecases for execution), and a common part (the actual usecases and some utilities). The files used for simulation or running the code on a HW platform are listed below. 

## Simulating on PC

Files in use: onemem-simulator.c, onewaymem-usecases.c, onewaysim.h, syncprint.c, and syncprint.h.

`RUNONPATMOS` is defined in onewaysim.h and determines if the code shall run on patmos or not. It is automatically set by the build system when running on patmos:
```
#ifdef __patmos__
  #define RUNONPATMOS
#endif
```

The `doit` target is for PC-based simulation. `RUNONPATMOS` must not be defined.

A speciffic use-case is run by supplying the use-case identifier to the respective make target. The following would execute use-case 0 on the PC:

```
make usecase=U0 doit
```

## Simulating on HW Platform

Files in use: onemem-patmos_onewayuse.c, onewaymem-usecases.c, onewaysim.h, syncprint.c, syncprint.h

The `onpatmos` target is for running on patmos. 