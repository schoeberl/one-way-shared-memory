onewaysim
=========

`RUNONPATMOS` is defined in onewaysim.h and determines if the code shall run on patmos or not.

The `doit` target is for PC-based simulation. `RUNONPATMOS` must not be defined.

The `onpatmos` target is for running on patmos. You need to insert one line in the main patmos make file:
  $(BUILDDIR)/onewaysim-target.elf: onewaysim-common.c onewaysim.h
`RUNONPATMOS` must be defined.

