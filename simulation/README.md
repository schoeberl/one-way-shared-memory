onewaymem usecase simulator and patmos usecase tester
=====================================================

The code in this directory can do two things. First, it can simulate multicore usecases on
the PC. Second, it can use patmos to run the same usecases directy on hardware. 

The source code is split in three parts. One part that runs only on the PC (the simulator), one part that runs only on patmos (setting up the usecases for execution), and a common part (the actual usecases and some utilities). 

`RUNONPATMOS` is defined in onewaysim.h and determines if the code shall run on patmos or not.

The `doit` target is for PC-based simulation. `RUNONPATMOS` must not be defined.

The `onpatmos` target is for running on patmos. In the file `onewaymem-patmos_onewaysim.c` you must comment in the use-case to be executed. The following would execute use-case 3:
```
// use case 3, state exchange:  corethreadeswork
void (*corefuncptr)(void *) = &corethreadeswork;
```
