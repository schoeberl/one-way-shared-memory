CC=gcc

doit:
	rm -f ./a.out
	$(CC) -pthread -g onewaysim-target.c onewaysim.c
	./a.out

#use this target if there is a segmentation fault
#  after running the 'doit' target
debug:
	gdb ./a.out
	#then do this
	#(gdb) run
	#<segfault happens here>
	#(gdb) backtrace
	#<offending code is shown here>
