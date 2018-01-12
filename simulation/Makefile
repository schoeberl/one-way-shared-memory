CC=gcc

doit:
	$(CC) -pthread -g onewaysim.c
	./a.out

#use this target if there is a segmentation fault
debug:
	gdb ./a.out
	#then do this
	#(gdb) run
	#<segfault happens here>
	#(gdb) backtrace
	#<offending code is shown here>
