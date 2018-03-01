MAIN?=onewaymem-patmos_onewaysim
EXTRAFILES = onewaymem-usecases.c syncprint.c

all:
	patmos-clang -O2 $(MAIN).c $(EXTRAFILES) -I ../.. -I ../../include ../../libcorethread/*.c -o $(APP).elf

clean:
	rm *.elf
