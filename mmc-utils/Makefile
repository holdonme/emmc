CROSS_COMPILE=/home/mike/CodeSourcery/Sourcery_CodeBench_Lite_for_Xilinx_GNU_Linux/bin/arm-xilinx-linux-gnueabi-
CC=gcc
CFLAGS=-Wall -Wundef -Wuninitialized -O4 -g
objects=mmc.o mmc_cmds.o

all:mmc

.c.o:
	$(CROSS_COMPILE)$(CC) -c $(CFLAGS) -o $@ $<
mmc:$(objects)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) --static -o mmc $(objects)

.PHONY clean:
	rm -rf *.o mmc
