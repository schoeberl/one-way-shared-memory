CC=gcc
PATMOSHOME=~/t-crest/patmos

HOSTSOURCEFILES = onewaymem-simulator.c onewaymem-usecases.c onewaymem-util.c

help:
	#doit: run the simulator only on the host
	#onpatmos: run the test on patmos

doit: 
	rm -f ./a.out
	$(CC) -I$(PATMOSHOME)/../newlib/newlib/libc/machine/patmos -pthread -g $(HOSTSOURCEFILES)
	./a.out

#use this target if there is a segmentation fault from your code in the 'doit' target
#  after running the 'doit' target
#  then do this (gdb) run and (gdb) backtrace
debug:
	gdb ./a.out

onpatmos: 
	cd $(PATMOSHOME)/../aegean/ && $(MAKE) config
	rm -f ~/t-crest/patmos/c/onewaysim*
	cp onewaysim* ~/t-crest/patmos/c/
	#cd ~/t-crest/patmos/ && $(MAKE) BOOTAPP=bootable-bootloader APP=onewaymem tools comp gen synth config download
	cd ~/t-crest/patmos/ && $(MAKE) APP=onewaysim-target comp download
