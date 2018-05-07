MAIN?=onewaymem-patmos_onewayuse
EXTRAFILES = onewaymem-usecases.c onewaymem-usecase$(usecase).c syncprint.c

all:
	test $(usecase)
	patmos-clang -O2 $(MAIN).c $(EXTRAFILES) -I ../.. -I ../../include ../../libcorethread/*.c -o $(APP).elf -D USECASE=$(usecase)

clean:
	rm *.elf
