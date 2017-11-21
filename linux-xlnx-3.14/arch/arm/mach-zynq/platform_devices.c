/*
 * Copyright (C) 2011 Xilinx
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/mtd/physmap.h>
#include <linux/spi/flash.h>
#include <linux/module.h>
#include <linux/amba/xilinx_dma.h>
#include <linux/xilinx_devices.h>
//#include <mach/dma.h>
#include <asm/pmu.h>

#define DMAC0_BASE		(0xF8003000)
#define IRQ_DMAC0_ABORT		45
#define IRQ_DMAC0		46
#define IRQ_DMAC3		72


#if 0	//leonwang add
		//leonwang 2014-12-12 remove definition to device tree file 'zynq-zed.dts'
static u64 dma_mask = 0xFFFFFFFFUL;

#include <linux/mmc/host.h>

#define XILINX_EMMC_BASE	0x7AA00000
#define XILINX_DMA_BASE		0x40400000
#define XILINX_PWR_BASE		0x7AB00000

#define XILINX_IRQ			89

static struct resource xilinx_emmc_res[] = {
	{
		.start = XILINX_EMMC_BASE,
		.end = XILINX_EMMC_BASE + 0x80,
		.flags = IORESOURCE_MEM,
	}, {
		.start = XILINX_DMA_BASE,
		.end = XILINX_DMA_BASE + 0x80,
		.flags = IORESOURCE_MEM,
	}, {
		.start = XILINX_PWR_BASE,
		.end = XILINX_PWR_BASE + 0x80,
		.flags = IORESOURCE_MEM,
	}, {
		.start = XILINX_IRQ,
		.end = XILINX_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device xilinx_emmc = {
        .name           = "xilinx-emmc",
        .id             = 0,
		.num_resources  = ARRAY_SIZE(xilinx_emmc_res),
		.resource		= xilinx_emmc_res,
		.dev = {
			.dma_mask = &dma_mask,
			.coherent_dma_mask = 0xFFFFFFFF,
		},
};

#endif

/* add all platform devices to the following table so they
 * will be registered
 */
struct platform_device *xilinx_pdevices[] __initdata = {

#if 0	//leonwang add
		//leonwang 2014-12-12 remove definition to device tree file 'zynq-zed.dts'
	&xilinx_emmc,
#endif
};


/**
 * platform_device_init - Initialize all the platform devices.
 *
 **/
void __init platform_device_init(void)
{
	int ret, i;
	struct platform_device **devptr;
	int size;

	devptr = &xilinx_pdevices[0];
	size = ARRAY_SIZE(xilinx_pdevices);

	ret = 0;

	/* Initialize all the platform devices */

	for (i = 0; i < size; i++, devptr++) {
		pr_info("registering platform device '%s' id %d\n",
			(*devptr)->name,
			(*devptr)->id);
			ret = platform_device_register(*devptr);
		if (ret)
			pr_info("Unable to register platform device '%s': %d\n",
				(*devptr)->name, ret);
	}

//#if defined CONFIG_SPI_SPIDEV || defined CONFIG_MTD_M25P80
//	spi_register_board_info(&xilinx_qspipss_0_boardinfo, 1);
//#endif

}

