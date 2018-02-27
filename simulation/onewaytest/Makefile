MAIN?=hello_onewaytest
EXTRAFILES = syncprint.c

all:
	patmos-clang -O2 $(MAIN).c $(EXTRAFILES) -I ../.. -I ../../include ../../libcorethread/*.c -o $(APP).elf

clean:
	rm *.elf
