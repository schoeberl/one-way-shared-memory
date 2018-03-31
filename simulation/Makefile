CC=gcc
PATMOSHOME=~/t-crest/patmos

HOSTSOURCEFILES = onewaymem-simulator.c onewaymem-usecases.c syncprint.c

help:
	#doit: run the simulator only on the host
	#onpatmos: run the test on patmos

doit: 
	rm -f ./a.out
	cd onewaysim && $(CC) -I$(PATMOSHOME)/../newlib/newlib/libc/machine/patmos -pthread -g $(HOSTSOURCEFILES)
	cd onewaysim && ./a.out

#use this target if there is a segmentation fault from your code in the 'doit' target
#  after running the 'doit' target
#  then do this (gdb) run and (gdb) backtrace
debug:
	cd onewaysim && gdb ./a.out

onpatmos: 
	rm -rf $(PATMOSHOME)/c/apps/onewaysim
	cp -R onewaysim $(PATMOSHOME)/c/apps
	cd $(PATMOSHOME) && $(MAKE) app config download APP=onewaysim

testoneway:
	rm -rf $(PATMOSHOME)/c/apps/onewaytest
	cp -R onewaytest $(PATMOSHOME)/c/apps
	cd $(PATMOSHOME) && $(MAKE) app config download APP=onewaytest
