# onewaymem Use-Case Testing

The code in the `onewayuse` can do two things. First, it can simulate multicore usecases on
the PC. Second, it can use patmos to run the same usecases directy on hardware. 

## Source code organiztion

The source code is split in three parts. One part that runs only on the PC (the simulator), one part that runs only on patmos (setting up the usecases for execution), and a common part (the actual usecases and some utilities). 

## Simulating on PC

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

## Simulating on HW

The `onpatmos` target is for running on patmos. 