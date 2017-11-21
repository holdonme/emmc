#KERNELDIR ?= /media/mike/OS/All/software/linux-xlnx/
KERNELDIR ?= ../linux-xlnx-3.14
PWD := $(shell pwd)

obj-m := mmc_drv.o
mmc_drv-objs := mmc_char.o 

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	cp mmc_drv.ko /home/mike/nfs/

clean:
	rm -rf *.ko *.o *.mod.o *.mod.c *.symvers modules.order .*.cmd .tmp_versions
