CC=gcc
PATMOSHOME=~/t-crest/patmos

HOSTSOURCEFILES = onewaymem-simulator.c onewaymem-usecases.c syncprint.c

help:
	#doit usecase=U0: run the simulator only on the host
	#onpatmos: run the test on patmos

doit: 
	@if test -z "$(usecase)"; then echo "usecase not defined; see README.md"; exit 1; fi
	rm -f ./a.out
	cd onewayuse && $(CC) -I$(PATMOSHOME)/../newlib/newlib/libc/machine/patmos -pthread -g $(HOSTSOURCEFILES) onewaymem-usecase$(usecase).c -D USECASE=$(usecase)
	cd onewayuse && ./a.out

#use this target if there is a segmentation fault from your code in the 'doit' target
#  after running the 'doit' target
#  then do this (gdb) run and (gdb) backtrace
debug:
	cd onewayuse && gdb ./a.out


onpatmos: 
	@if test -z "$(usecase)"; then echo "usecase not defined; see README.md"; exit 1; fi
	rm -rf $(PATMOSHOME)/c/apps/onewayuse
	cp -R onewayuse $(PATMOSHOME)/c/apps
	cd $(PATMOSHOME) && $(MAKE) app config download APP=onewayuse usecase=$(usecase)

testoneway:
	rm -rf $(PATMOSHOME)/c/apps/onewaytest
	cp -R onewaytest $(PATMOSHOME)/c/apps
	cd $(PATMOSHOME) && $(MAKE) app config download APP=onewaytest
