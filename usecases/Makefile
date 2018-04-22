CC=gcc
PATMOSHOME=~/t-crest/patmos

HOSTSOURCEFILES = onewaymem-simulator.c onewaymem-usecases.c syncprint.c

help:
	#doit usecase=U0: run the simulator only on the host
	#onpatmos: run the test on patmos

doit: 
	rm -f ./a.out
	cd onewayuse && $(CC) -I$(PATMOSHOME)/../newlib/newlib/libc/machine/patmos -pthread -g $(HOSTSOURCEFILES)
	cd onewayuse && ./a.out $(usecase)

#use this target if there is a segmentation fault from your code in the 'doit' target
#  after running the 'doit' target
#  then do this (gdb) run and (gdb) backtrace
debug:
	cd onewayuse && gdb ./a.out

onpatmos: 
	rm -rf $(PATMOSHOME)/c/apps/onewayuse
	cp -R onewayuse $(PATMOSHOME)/c/apps
	cd $(PATMOSHOME) && $(MAKE) app config download APP=onewayuse

testoneway:
	rm -rf $(PATMOSHOME)/c/apps/onewaytest
	cp -R onewaytest $(PATMOSHOME)/c/apps
	cd $(PATMOSHOME) && $(MAKE) app config download APP=onewaytest
