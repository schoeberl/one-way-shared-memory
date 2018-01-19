CC=gcc
PATMOSHOME=/home/ulslevpedersen/t-crest/patmos

doit:
	rm -f ./a.out
	$(CC) -pthread -g onewaysim-target.c onewaysim.c
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
	@echo "put this in the patmos makefile: BUILDDIR/onewaysim-target.elf: onewaysim.c onewaysim.h"
	rm -f ~/t-crest/patmos/c/onewaysim-target.c	
	rm -f ~/t-crest/patmos/c/onewaysim.h	
	rm -f ~/t-crest/patmos/c/onewaysim.c	
	cp onewaysim-target.c ~/t-crest/patmos/c/
	cp onewaysim.h ~/t-crest/patmos/c/
	cp onewaysim.c ~/t-crest/patmos/c/
	#cd ~/t-crest/patmos/ && $(MAKE) BOOTAPP=bootable-bootloader APP=onewaymem tools comp gen synth config download
	cd ~/t-crest/patmos/ && $(MAKE) APP=onewaysim-target comp download
	rm -f ~/t-crest/patmos/c/onewaysim-target.c	
	rm -f ~/t-crest/patmos/c/onewaysim.h	
	rm -f ~/t-crest/patmos/c/onewaysim.c
