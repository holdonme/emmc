obj-m := flash_lib.o flash_ops.o
CROSS := /home/mike/CodeSourcery/Sourcery_CodeBench_Lite_for_Xilinx_GNU_Linux/bin/arm-xilinx-linux-gnueabi-
COMPILE := gcc
CFLAG := -Wall -O4 

all:$(obj-m)
	
flash_ops.o : flash_ops.c flash_define.h flash_lib.h mmc-user.h
	$(CROSS)$(COMPILE) $(CFLAG) -c $< -o $@
flash_lib.o : flash_lib.c flash_define.h mmc-user.h
	$(CROSS)$(COMPILE) $(CFLAG) -c $< -o $@

clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers
	rm -f flash_app
