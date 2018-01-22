CC=gcc
PATMOSHOME=/home/ulslevpedersen/t-crest/patmos

doit:
	rm -f ./a.out
	$(CC) -I$(PATMOSHOME)/../newlib/newlib/libc/machine/patmos -pthread -g onewaysim-common.c onewaysim-target.c onewaysim-host.c
	./a.out

#use this target if there is a segmentation fault from your code in the 'doit' target
#  after running the 'doit' target
debug:
	gdb ./a.out
	#then do this
	#(gdb) run
	#<segfault happens here>
	#(gdb) backtrace
	#<offending code is shown here>

onpatmos: 
	cd $(PATMOSHOME)/../aegean/ && $(MAKE) config
	rm -f ~/t-crest/patmos/c/onewaysim-target.c	
	rm -f ~/t-crest/patmos/c/onewaysim-common.c	
	rm -f ~/t-crest/patmos/c/onewaysim.h	
	cp onewaysim-target.c ~/t-crest/patmos/c/
	cp onewaysim-common.c ~/t-crest/patmos/c/
	cp onewaysim.h ~/t-crest/patmos/c/
	#cd ~/t-crest/patmos/ && $(MAKE) BOOTAPP=bootable-bootloader APP=onewaymem tools comp gen synth config download
	cd ~/t-crest/patmos/ && $(MAKE) APP=onewaysim-target comp download
	rm -f ~/t-crest/patmos/c/onewaysim-target.c	
	rm -f ~/t-crest/patmos/c/onewaysim-common.c	
	rm -f ~/t-crest/patmos/c/onewaysim.h
