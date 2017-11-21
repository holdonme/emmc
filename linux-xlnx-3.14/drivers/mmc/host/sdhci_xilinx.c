/*
 *  linux/drivers/mmc/host/sdhci_xilinx.c - Xilinx FPGA eMMC host driver
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/jiffies.h>

#include <linux/blkdev.h>
#include <../drivers/mmc/card/queue.h>

/**********************************************************************
 * Register address mapping
 **********************************************************************/

/*
 * AXI_DMA			Base addr 0x4040_0000
 */
#define XILINX_MM2S_DMACR			0x00
#define XILINX_MM2S_DMASR			0x04
#define XILINX_MM2S_CURDESC			0x08
#define XILINX_MM2S_TAILDESC		0x10
#define XILINX_MM2S_SA				0x18
#define XILINX_MM2S_LENGTH			0x28

#define XILINX_S2MM_DMACR			0x30
#define XILINX_S2MM_DMASR			0x34
#define XILINX_S2MM_CURDESC			0x38
#define XILINX_S2MM_TAILDESC		0x40
#define XILINX_S2MM_DA				0x48
#define XILINX_S2MM_LENGTH			0x58

/*
 * AXI_SLAVE_MISC	Base addr 0x7AA0_0000
 */

#define EMMC_CLOCK					0x00
#define EMMC_CFG					0x04
#define EMMC_BLK_LEN				0x08
#define	EMMC_VOLTAGE_CTRL			0x0C
#define EMMC_POWER_CTRL				0x10
#define EMMC_RESISTOR				0x14
#define EMMC_TIME					0x18
#define EMMC_MISC_SR                0x1C
#define EMMC_SUB_VERSION			0x78
#define EMMC_VERSION				0x7C

/*
 * AXI_SLAVE_CMD	Base addr 0x7AB0_0000
 */

#define	EMMC_CMD_ARGUMENT			0x00
#define EMMC_CMD_INDEX				0x04
#define EMMC_CMD_RESP0				0x08
#define EMMC_CMD_RESP1				0x0c
#define EMMC_CMD_RESP2				0x10
#define EMMC_CMD_RESP3				0x14
#define EMMC_CMD_RESP4				0x18
#define EMMC_CMD_SR					0x1C
#define EMMC_CMD_RESP_TIMEOUT		0x20

/*
 * AXI_SLAVE_WR		Base addr 0x7AC0_0000
 */

#define EMMC_WR_BLK_CNT				0x00
#define	EMMC_WR_SR					0x04
#define EMMC_WR_RISE_FIFO_COUNT		0x08
#define EMMC_WR_FALL_FIFO_COUNT		0x0C
#define EMMC_WR_TIMEOUT_VALUE		0x10
#define EMMC_WR_CRC_TIMEOUT_VALUE	0x14

/*
 * AXI_SLAVE_RD		Base addr 0x7AD0_0000
 */

#define EMMC_RD_BLK_CNT				0x00
#define EMMC_RD_SR					0x04
#define EMMC_RD_RISE_FIFO_COUNT		0x08
#define EMMC_RD_FALL_FIFO_COUNT		0x0C
#define EMMC_RD_TIMEOUT_VALUE		0x10
#define	EMMC_RD_DMA_LEN				0x14
#define EMMC_RD_IO_DELAY			0x18


#define CLOCK_PLL_LOCKED		0x80000000

//XXX "EMMC_CFG" field

#define CFG_DDR_ENABLE			0x00000001		// 0--SDR  		1--DDR
#define CFG_DATA_CLK			0x00000002		// 0--400k		1--data transfer clock
#define CFG_HOST_RESET		    0x00000004
#define CFG_BUS_WIDTH	        0x00000018		// 0--x8 bit    1--x1 bit  2--x4 bit
#define CFG_FORCE_CLK			0x00000020

//XXX "EMMC_CMD_INDEX" field

#define RESPONSE_R1			(0 << 8) 
#define	RESPONSE_R1B		(1 << 8)
#define RESPONSE_R2			(2 << 8)
#define RESPONSE_R3			(3 << 8)
#define RESPONSE_NONE		(4 << 8)


//XXX "EMMC_CMD_SR" field

#define	RESP_DONE			0x01
#define	RESP_TIMEOUT		0x02
#define	RESP_CRC_ERROR		0x04


//XXX "EMMC_WR_SR" field

#define	WR_DONE				0x01
#define	WR_TIMEOUT			0x02
#define	WR_CRC_ERROR		0x04
#define	WR_CRC_TIMEOUT		0x08


//XXX "EMMC_RD_SR" field

#define	RD_DONE				0x01
#define	RD_TIMEOUT			0x02
#define	RD_CRC_ERROR		0x04

//XXX "EMMC_MISC_SR" field

#define RESIS_VCC_DONE      0x01
#define RESIS_VCCQ_DONE     0x02

#define XILINX_RETRY_MAX 500
#define XILINX_EMMC_DRV_VERSION "20170710"

#define XILINX_BOOT_x8	0

// SD use 64 Byte pattern data
uint8_t tuning_block_64[] = {
	0xFF, 0x0F, 0xFF, 0x00, 0xFF, 0xCC, 0xC3, 0xCC, 0xC3, 0x3C, 0xCC, 0xFF, 0xFE, 0xFF, 0xFE, 0xEF,
	0xFF, 0xDF, 0xFF, 0xDD, 0xFF, 0xFB, 0xFF, 0xFB, 0xBF, 0xFF, 0x7F, 0xFF, 0x77, 0xF7, 0xBD, 0xEF,
	0xFF, 0xF0, 0xFF, 0xF0, 0x0F, 0xFC, 0xCC, 0x3C, 0xCC, 0x33, 0xCC, 0xCF, 0xFF, 0xEF, 0xFF, 0xEE,
	0xFF, 0xFD, 0xFF, 0xFD, 0xDF, 0xFF, 0xBF, 0xFF, 0xBB, 0xFF, 0xF7, 0xFF, 0xF7, 0x7F, 0x7B, 0xDE
};


// eMMC HS200 use 128 Byte pattern data
uint8_t tuning_block_128[] = {
   0xff, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xcc, 0xcc, 0xff, 0xff, 0xcc, 0xcc, 0x33, 0xcc,
   0xcc, 0x33, 0x33, 0xcc, 0xff, 0xff, 0xcc, 0xcc, 0xff, 0xff, 0xee, 0xff, 0xff, 0xee, 0xee, 0xff,
   0xff, 0xdd, 0xff, 0xff, 0xdd, 0xdd, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0x77, 0x77, 0xff, 0x77, 0x77, 0xff, 0xee, 0xdd, 0xbb,
   0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xcc, 0xff, 0xff, 0x00, 0xcc, 0x33, 0xcc, 0xcc,
   0x33, 0x33, 0xcc, 0xcc, 0xff, 0xcc, 0xcc, 0xcc, 0xff, 0xee, 0xff, 0xff, 0xee, 0xee, 0xff, 0xff,
   0xdd, 0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xbb,
   0xff, 0xff, 0xbb, 0xbb, 0xff, 0xff, 0x77, 0xff, 0xff, 0x77, 0x77, 0xff, 0xee, 0xdd, 0xbb, 0x77
};

struct xilinx_emmc_regs {
	uint32_t reg_XILINX_MM2S_DMACR;
	uint32_t reg_XILINX_MM2S_DMASR;
	uint32_t reg_XILINX_MM2S_CURDESC;
	uint32_t reg_XILINX_MM2S_TAILDESC;
	uint32_t reg_XILINX_MM2S_SA;
	uint32_t reg_XILINX_MM2S_LENGTH;

	uint32_t reg_XILINX_S2MM_DMACR;
	uint32_t reg_XILINX_S2MM_DMASR;
	uint32_t reg_XILINX_S2MM_CURDESC;
	uint32_t reg_XILINX_S2MM_TAILDESC;
	uint32_t reg_XILINX_S2MM_DA;
	uint32_t reg_XILINX_S2MM_LENGTH;

	uint32_t reg_EMMC_CLOCK; 
	uint32_t reg_EMMC_CFG;
	uint32_t reg_EMMC_BLK_LEN; 
	uint32_t reg_EMMC_VOLTAGE_CTRL;
	uint32_t reg_EMMC_POWER_CTRL;
	uint32_t reg_EMMC_RESISTOR;
	uint32_t reg_EMMC_TIME;
	uint32_t reg_EMMC_SUB_VERSION;
	uint32_t reg_EMMC_VERSION;

	uint32_t reg_EMMC_CMD_ARGUMENT;
	uint32_t reg_EMMC_CMD_INDEX; 
	uint32_t reg_EMMC_CMD_RESP0;	
	uint32_t reg_EMMC_CMD_RESP1;	
	uint32_t reg_EMMC_CMD_RESP2;	
	uint32_t reg_EMMC_CMD_RESP3;	
	uint32_t reg_EMMC_CMD_RESP4;	
	uint32_t reg_EMMC_CMD_SR;	

	uint32_t reg_EMMC_WR_BLK_CNT;
	uint32_t reg_EMMC_WR_SR;
	uint32_t reg_EMMC_WR_RISE_FIFO_COUNT;
	uint32_t reg_EMMC_WR_FALL_FIFO_COUNT;
	uint32_t reg_EMMC_WR_TIMEOUT_VALUE;
	uint32_t reg_EMMC_WR_CRC_TIMEOUT_VALUE;

	uint32_t reg_EMMC_RD_BLK_CNT;
	uint32_t reg_EMMC_RD_SR;
	uint32_t reg_EMMC_RD_RISE_FIFO_COUNT;
	uint32_t reg_EMMC_RD_FALL_FIFO_COUNT;
	uint32_t reg_EMMC_RD_TIMEOUT_VALUE;
	uint32_t reg_EMMC_RD_DMA_LEN;	
	uint32_t reg_EMMC_RD_IO_DELAY;
};

struct xilinx_emmc_host {
	struct mmc_host 		*mmc;
	struct mmc_command 		*cmd;
	struct mmc_data 		*data;
	struct mmc_request 		*mrq;

	void __iomem *dma_reg;
	void __iomem *misc_reg;
	void __iomem *cmd_reg;
	void __iomem *wr_reg;
	void __iomem *rd_reg;
	int irq;

	spinlock_t lock;	/* Mutex */

	struct sg_mapping_iter sg_miter;

	struct timer_list timer;
	struct xilinx_emmc_regs regs;
};

/* Rewrite some kernel API to avoid use 'mmc_card' */
int xilinx_mmc_send_status(struct mmc_host *mmc, u32 *status, bool ignore_crc);
int xilinx_mmc_switch(struct mmc_host *mmc, u8 set, u8 index, u8 value, unsigned int timeout_ms);
int xilinx_mmc_send_cxd_data(struct mmc_host *host, u32 opcode, void *buf, unsigned len);
int xilinx_mmc_get_ext_csd(struct mmc_host *host, u8 **new_ext_csd);

/* eMMC Host Debug function */
void dump_buf(uint8_t *buf, int len);
void xilinx_get_time_dbg(	struct timeval *p_sbc_start, 
					struct timeval *p_sbc_end, 
					struct timeval *p_cmd_start,
					struct timeval *p_cmd_end, 
					struct timeval *p_stop_start,
					struct timeval *p_stop_end, 
					uint32_t *p_tran_time,
					uint32_t *p_stop_time);
int xilinx_dump_reg(struct xilinx_emmc_host *host, void *arg);
void save_regs_to_mem(struct xilinx_emmc_host *host);
void dump_mem_of_regs(struct xilinx_emmc_host *host);

/* eMMC Host misc config function */
int xilinx_set_bus_width(struct mmc_host *mmc, int bus_width);
int xilinx_set_timing(struct mmc_host *mmc, int ddr_enable);
int xilinx_reset_host(struct mmc_host *mmc);
static void xilinx_set_ios(struct mmc_host *mmc, struct mmc_ios *ios);
int	xilinx_start_signal_voltage_switch(struct mmc_host *mmc, struct mmc_ios *ios);

/* eMMC Host power config function */
void xilinx_set_card_removed( struct mmc_card *card, bool remove);
void xilinx_emmc_power_on(struct mmc_host *mmc, int voltage_x_10, int which);
void xilinx_emmc_set_resistor(struct mmc_host *mmc, int value, int which);
void xilinx_emmc_power_off(struct mmc_host *mmc, uint32_t speed_us, int which);

/* eMMC Timing config function  clk/cmd/rd/wr */
int xilinx_set_clk(struct mmc_host *mmc, int khz);
int xilinx_set_wr_phase(struct mmc_host *mmc, int phase);
int xilinx_set_cmd_phase(struct mmc_host *mmc, int phase);
int xilinx_set_rd_phase(struct mmc_host *mmc, int phase);
int xilinx_set_rd_delay(struct mmc_host *mmc, int phase);

/* eMMC Tuning function  cmd/rd */
int xilinx_cmd_tuning(struct mmc_host *mmc);
int xilinx_rd_tuning(struct mmc_host *mmc, int opcode, uint8_t *pattern, int len);
int read_ecsd_and_compare(struct mmc_host *mmc, uint8_t *pattern, int len);
int read_tuning_and_compare(struct mmc_host *mmc, int opcode, uint8_t *pattern, int len);

/* eMMC Mode Switch SDR/DDR/HS200/HS400 */
int xilinx_prepare_hs_sdr(struct mmc_host *mmc, int mhz, int bus_width);
int xilinx_prepare_hs_ddr(struct mmc_host *mmc, int mhz, int bus_width);
int xilinx_prepare_hs200(struct mmc_host *mmc, int mhz);
int xilinx_prepare_hs400(struct mmc_host *mmc, int mhz);

/* Handle eMMC request */
static void xilinx_timeout_timer(unsigned long data);
void xilinx_send_command(struct xilinx_emmc_host *host, struct mmc_command *cmd);
int xilinx_sdma_prepare(struct xilinx_emmc_host *host, struct mmc_request *mrq);
static void xilinx_request(struct mmc_host *mmc, struct mmc_request *mrq);

/* Handle eMMC device Interrupt */
void xilinx_handle_cmd_irq(struct xilinx_emmc_host *host, uint32_t sr );
void xilinx_handle_dma_irq(struct xilinx_emmc_host *host, uint32_t sr );
static irqreturn_t xilinx_irq(int irq, void *dev);

/* External kernal API */
extern struct bus_type *find_bus(char *name);
extern struct block_device *blkdev_get_by_path(const char *path, fmode_t mode, void *holder);

/*
 * Global paramter
 */
int xilinx_global_access = MMC_STATE_BLOCKADDR;
#if XILINX_BOOT_x8 // workaroud code when FPGA doesn't support x1 bus width 
int xilinx_global_x8_ready = 0;
#endif
struct mmc_host *xilinx_global_mmc = NULL;

struct timeval sbc_start,sbc_end;
struct timeval cmd_start,cmd_end;
struct timeval stop_start,stop_end;

uint32_t fpga_tran_time = 0;
uint32_t fpga_stop_time = 0;

static char resp_string[5][10] = {"R1", "R1B", "R2", "R3", "NO RESP"};

int global_rca = 1;             // default RCA is 1
int global_in_tuning = 0;
int global_in_vendor = 0;
int global_debug = 0;
int global_fake_wp = 0;

int do_power_loss_flag = 0;
int do_power_loss_delay_us = 0;
int do_power_loss_speed_us = 0;
int do_power_loss_volt = 0;
void xilinx_op_request_queue(struct xilinx_emmc_host *host, bool stop);

#define my_debug(fmt, ...) 	\
	do { if(global_debug) printk( fmt, ##__VA_ARGS__); } while(0)

/*
 * Rewrite some kernel API to avoid use 'mmc_card'
 * mmc_send_status() / mmc_switch() / mmc_send_cxd_data() / mmc_get_ext_csd()
 */

#define MMC_OPS_TIMEOUT_MS	(10 * 60 * 1000) /* 10 minute timeout */
#define MMC_CMD_RETRIES     3

int xilinx_mmc_send_status(struct mmc_host *mmc, u32 *status, bool ignore_crc)
{
	int err;
	struct mmc_command cmd = {0};

	cmd.opcode = MMC_SEND_STATUS;
	cmd.arg = global_rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;
	if (ignore_crc)
		cmd.flags &= ~MMC_RSP_CRC;

	err = mmc_wait_for_cmd(mmc, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	/* NOTE: callers are required to understand the difference
	 * between "native" and SPI format status words!
	 */
	if (status)
		*status = cmd.resp[0];

	return 0;
}


int xilinx_mmc_switch(struct mmc_host *mmc, u8 set, u8 index, u8 value, unsigned int timeout_ms)
{
	int err;
	struct mmc_command cmd = {0};
	unsigned long timeout;
	u32 status = 0;
	bool ignore_crc = false;

	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
		  (index << 16) |
		  (value << 8) |
		  set;
	cmd.flags = MMC_CMD_AC | MMC_RSP_SPI_R1B | MMC_RSP_R1B;

	cmd.cmd_timeout_ms = timeout_ms;
	if (index == EXT_CSD_SANITIZE_START)
		cmd.sanitize_busy = true;

	err = mmc_wait_for_cmd(mmc, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	/*
	 * Must check status to be sure of no errors
	 * If CMD13 is to check the busy completion of the timing change,
	 * disable the check of CRC error.
	 */
	if (index == EXT_CSD_HS_TIMING &&
	    !(mmc->caps & MMC_CAP_WAIT_WHILE_BUSY))
		ignore_crc = true;

	timeout = jiffies + msecs_to_jiffies(MMC_OPS_TIMEOUT_MS);
	do {
		err = xilinx_mmc_send_status(mmc, &status, ignore_crc);
		if (err)
			return err;
		if (mmc->caps & MMC_CAP_WAIT_WHILE_BUSY)
			break;

		/* Timeout if the device never leaves the program state. */
		if (time_after(jiffies, timeout)) {
			pr_err("%s: Card stuck in programming state! %s\n",
				mmc_hostname(mmc), __func__);
			return -ETIMEDOUT;
		}
	} while (R1_CURRENT_STATE(status) == R1_STATE_PRG);

	if (status & 0xFDFFA000)
		pr_warning("%s: unexpected status %#x after "
			   "switch", mmc_hostname(mmc), status);
	if (status & R1_SWITCH_ERROR)
		return -EBADMSG;

	return 0;
}

int xilinx_mmc_send_cxd_data(struct mmc_host *host, u32 opcode, void *buf, unsigned len)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;
	void *data_buf;
	int is_on_stack;

	is_on_stack = object_is_on_stack(buf);
	if (is_on_stack) {
		/*
		 * dma onto stack is unsafe/nonportable, but callers to this
		 * routine normally provide temporary on-stack buffers ...
		 */
		data_buf = kmalloc(len, GFP_KERNEL);
		if (!data_buf)
			return -ENOMEM;
	} else
		data_buf = buf;

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = opcode;
	cmd.arg = 0;

	/* NOTE HACK:  the MMC_RSP_SPI_R1 is always correct here, but we
	 * rely on callers to never use this with "native" calls for reading
	 * CSD or CID.  Native versions of those commands use the R2 type,
	 * not R1 plus a data block.
	 */
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = len;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, data_buf, len);

	if (opcode == MMC_SEND_CSD || opcode == MMC_SEND_CID) {
		/*
		 * The spec states that CSR and CID accesses have a timeout
		 * of 64 clock cycles.
		 */
		data.timeout_ns = 0;
		data.timeout_clks = 64;
	} /*else
		mmc_set_data_timeout(&data, card);
	*/

	mmc_wait_for_req(host, &mrq);

	if (is_on_stack) {
		memcpy(buf, data_buf, len);
		kfree(data_buf);
	}

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return 0;
}

int xilinx_mmc_get_ext_csd(struct mmc_host *host, u8 **new_ext_csd)
{
	int err;
	u8 *ext_csd;

	BUG_ON(!new_ext_csd);

	*new_ext_csd = NULL;

	/*
	 * As the ext_csd is so large and mostly unused, we don't store the
	 * raw block in mmc_card.
	 */
	ext_csd = kmalloc(512, GFP_KERNEL);
	if (!ext_csd) {
		printk("could not allocate a buffer to receive the ext_csd.\n");
		return -ENOMEM;
	}

	err = xilinx_mmc_send_cxd_data(host, MMC_SEND_EXT_CSD, ext_csd, 512);
	if (err) {
		kfree(ext_csd);
		*new_ext_csd = NULL;

		/* If the host or the card can't do the switch,
		 * fail more gracefully. */
		if ((err != -EINVAL)
		 && (err != -ENOSYS)
		 && (err != -EFAULT))
			return err;
	} else
		*new_ext_csd = ext_csd;

	return err;
}

/*
 * eMMC Host Debug function
 */

void dump_buf(uint8_t *buf , int len)	// Here must be 'uint8_t';  if 'char', output prefix may be 'ffff'
{
	int count =0 ;
	while( count < len) {
		if(count % 16 == 0)
			printk("\n%04d:    ", count);
		printk("%02x ", *buf);
		count++;
		buf++;
	}
	printk("\n");
	return; 
}

void xilinx_get_time_dbg(	struct timeval *p_sbc_start, 
					struct timeval *p_sbc_end, 
					struct timeval *p_cmd_start,
					struct timeval *p_cmd_end, 
					struct timeval *p_stop_start,
					struct timeval *p_stop_end, 
					uint32_t *p_tran_time,
					uint32_t *p_stop_time)
{
	if(p_sbc_start)
		*p_sbc_start = sbc_start;
	if(p_sbc_end)
		*p_sbc_end = sbc_end;

	if(p_cmd_start)
		*p_cmd_start = cmd_start;
	if(p_cmd_end)
		*p_cmd_end = cmd_end;

	if(p_stop_start)
		*p_stop_start = stop_start;
	if(p_stop_end)
		*p_stop_end = stop_end;

	if(p_tran_time)
		*p_tran_time = fpga_tran_time;	//unit ns

	if(p_stop_time)
		*p_stop_time = fpga_stop_time;	//unit ns

	return ;
}

struct misc {
	int option;
	int value[10];
};

int xilinx_dump_reg(struct xilinx_emmc_host *host, void *arg)
{
	struct mmc_card *card = NULL;
	struct bus_type *bus = NULL;

	struct device_driver *drv = NULL;
	struct mmc_driver *mmc_drv = NULL;

	struct misc *p_misc = (struct misc *)arg;
	int option = p_misc->option;
	int *value = p_misc->value;

	card = host->mmc->card;
	bus = find_bus("mmc");
	if(!bus){
		printk("no mmc bus\n");
		return -1;
	}

	drv = driver_find("mmcblk", bus);
	mmc_drv = container_of(drv, struct mmc_driver, drv);
	
	if(option == 0){
		printk(" EMMC_CMD_ARGUMENT          %08x\n" , readl( host->cmd_reg + EMMC_CMD_ARGUMENT));   
		printk(" EMMC_CMD_INDEX             %08x\n" , readl( host->cmd_reg + EMMC_CMD_INDEX)   );
		printk(" EMMC_CMD_RESP0             %08x\n" , readl( host->cmd_reg + EMMC_CMD_RESP0) 	);
		printk(" EMMC_CMD_RESP1             %08x\n" , readl( host->cmd_reg + EMMC_CMD_RESP1) 	);
		printk(" EMMC_CMD_RESP2             %08x\n" , readl( host->cmd_reg + EMMC_CMD_RESP2) 	);
		printk(" EMMC_CMD_RESP3             %08x\n" , readl( host->cmd_reg + EMMC_CMD_RESP3) 	);
		printk(" EMMC_CMD_RESP4             %08x\n" , readl( host->cmd_reg + EMMC_CMD_RESP4) 	);
		printk(" EMMC_CMD_SR                %08x\n" , readl( host->cmd_reg + EMMC_CMD_SR)   	);
		printk("\n");
		printk(" EMMC_CLOCK                 %08x\n" , readl( host->misc_reg + EMMC_CLOCK )      );  
		printk(" EMMC_CFG                   %08x\n" , readl( host->misc_reg + EMMC_CFG)		    );  
		printk(" EMMC_BLK_LEN               %08x\n" , readl( host->misc_reg + EMMC_BLK_LEN ) 	);  
		printk(" EMMC_VOLTAGE_CTRL          %08x\n" , readl( host->misc_reg + EMMC_VOLTAGE_CTRL));  
		printk(" EMMC_POWER_CTRL            %08x\n" , readl( host->misc_reg + EMMC_POWER_CTRL)  );  
		printk(" EMMC_RESISTOR              %08x\n" , readl( host->misc_reg + EMMC_RESISTOR)    );  
		printk(" EMMC_TIME                  %08x\n" , readl( host->misc_reg + EMMC_TIME)        );  
		printk(" EMMC_MISC_SR               %08x\n" , readl( host->misc_reg + EMMC_MISC_SR)     );  
		printk("\n");
		printk(" EMMC_WR_BLK_CNT            %08x\n" , readl( host->wr_reg + EMMC_WR_BLK_CNT)    );
		printk(" EMMC_WR_SR                 %08x\n" , readl( host->wr_reg + EMMC_WR_SR     )    );
		printk(" EMMC_WR_RISE_FIFO_COUNT    %08x\n" , readl( host->wr_reg + EMMC_WR_RISE_FIFO_COUNT));
		printk(" EMMC_WR_FALL_FIFO_COUNT    %08x\n" , readl( host->wr_reg + EMMC_WR_FALL_FIFO_COUNT));
		printk(" EMMC_WR_TIMEOUT_VALUE      %08x\n" , readl( host->wr_reg + EMMC_WR_TIMEOUT_VALUE));
		printk(" EMMC_WR_CRC_TIMEOUT_VALUE  %08x\n" , readl( host->wr_reg + EMMC_WR_CRC_TIMEOUT_VALUE));
		printk("\n");
		printk(" EMMC_RD_BLK_CNT            %08x\n" , readl( host->rd_reg + EMMC_RD_BLK_CNT)    );
		printk(" EMMC_RD_SR                 %08x\n" , readl( host->rd_reg + EMMC_RD_SR)         );
		printk(" EMMC_RD_RISE_FIFO_COUNT    %08x\n" , readl( host->rd_reg + EMMC_RD_RISE_FIFO_COUNT));
		printk(" EMMC_RD_FALL_FIFO_COUNT    %08x\n" , readl( host->rd_reg + EMMC_RD_FALL_FIFO_COUNT));
		printk(" EMMC_RD_TIMEOUT_VALUE      %08x\n" , readl( host->rd_reg + EMMC_RD_TIMEOUT_VALUE));
		printk(" EMMC_RD_DMA_LEN            %08x\n" , readl( host->rd_reg + EMMC_RD_DMA_LEN)    );
		printk(" EMMC_RD_IO_DELAY           %08x\n" , readl( host->rd_reg + EMMC_RD_IO_DELAY)   );
		printk("\n");
		printk(" XILINX_MM2S_DMACR          %08x\n" , readl( host->dma_reg + XILINX_MM2S_DMACR  )	  );
		printk(" XILINX_MM2S_DMASR          %08x\n" , readl( host->dma_reg + XILINX_MM2S_DMASR  )     );
		printk(" XILINX_MM2S_CURDESC        %08x\n" , readl( host->dma_reg + XILINX_MM2S_CURDESC)     );
		printk(" XILINX_MM2S_TAILDESC       %08x\n" , readl( host->dma_reg + XILINX_MM2S_TAILDESC)    );
		printk(" XILINX_MM2S_SA             %08x\n" , readl( host->dma_reg + XILINX_MM2S_SA )         );
		printk(" XILINX_MM2S_LENGTH         %08x\n" , readl( host->dma_reg + XILINX_MM2S_LENGTH)      );
		printk("\n");
		printk(" XILINX_S2MM_DMACR          %08x\n" , readl( host->dma_reg + XILINX_S2MM_DMACR)       );
		printk(" XILINX_S2MM_DMASR          %08x\n" , readl( host->dma_reg + XILINX_S2MM_DMASR)       );
		printk(" XILINX_S2MM_CURDESC        %08x\n" , readl( host->dma_reg + XILINX_S2MM_CURDESC)     );
		printk(" XILINX_S2MM_TAILDESC       %08x\n" , readl( host->dma_reg + XILINX_S2MM_TAILDESC)    );
		printk(" XILINX_S2MM_DA             %08x\n" , readl( host->dma_reg + XILINX_S2MM_DA)          );
		printk(" XILINX_S2MM_LENGTH         %08x\n" , readl( host->dma_reg + XILINX_S2MM_LENGTH)      );

		return 0;
	}
	else if(option == 1){

		struct block_device *bdev = blkdev_get_by_path("/dev/mmcblk1p1", FMODE_READ|FMODE_WRITE, NULL);
		struct request_queue *q = bdev_get_queue(bdev);

		blk_cleanup_queue(q);	// mark request queue DEAD, all future req will return fail immediately
		printk("request queue cleanup\n");
	}
	else if(option == 2){
		if(value[0] == 1)
			writel(readl(host->misc_reg + EMMC_CFG) | CFG_FORCE_CLK, host->misc_reg + EMMC_CFG);
		else
			writel(readl(host->misc_reg + EMMC_CFG) & ~CFG_FORCE_CLK, host->misc_reg + EMMC_CFG);
	}
	else if(option == 3) { 
		//mmc_blk_suspend()	--> blk_stop_queue()  
		//mmc_blk_resume()	--> blk_start_queue()
		value[0] ?  mmc_drv->suspend(card) : mmc_drv->resume(card) ;		
	}
	else if(option == 4){
		value[0] ? xilinx_set_card_removed( card, true) : xilinx_set_card_removed( card, false);
	}
	else if(option == 5){
		value[0] ? (global_debug = 1) : (global_debug = 0);
	}
	else if(option == 6){
		do_power_loss_flag = 1;
		do_power_loss_delay_us = value[0];
		do_power_loss_speed_us = value[1];
		do_power_loss_volt = value[2];
		if(do_power_loss_volt == 0){
			printk("==========> Delay %d us after first CMD12, then do power loss speed %d us\n", 
					do_power_loss_delay_us, 
					do_power_loss_speed_us);
		}
		else{
			printk("==========> Delay %d us after first CMD12, then do power loss volt %d/10 V\n", 
					do_power_loss_delay_us, 
					do_power_loss_volt);
		}
	}
	else if(option == 7){
		xilinx_op_request_queue(host, true);
	}
	else if(option == 8){
		if(value[0]) {
			global_fake_wp = 1;
			printk("==========> enable fake Write Protect err for debug\n");
		}
		else {
			global_fake_wp = 0;
			printk("==========> disable fake Write Protect\n");
		}
	}
	else if(option == 9){
		xilinx_set_wr_phase(host->mmc, value[0]);
		xilinx_set_cmd_phase(host->mmc, value[1]);
		xilinx_set_rd_phase(host->mmc, value[2]);
		xilinx_set_rd_delay(host->mmc, value[3]);
	}
	else if(option == 10){
		xilinx_set_clk(host->mmc, value[0]);
	}
	else if(option == 11){
		xilinx_set_bus_width(host->mmc, value[0]);
	}
	
	return 0;
}

void save_regs_to_mem(struct xilinx_emmc_host *host)
{
	host->regs.reg_XILINX_MM2S_DMACR			= readl(host->dma_reg + XILINX_MM2S_DMACR	  );
	host->regs.reg_XILINX_MM2S_DMASR            = readl(host->dma_reg + XILINX_MM2S_DMASR	  );
	host->regs.reg_XILINX_MM2S_CURDESC          = readl(host->dma_reg + XILINX_MM2S_CURDESC	  );
	host->regs.reg_XILINX_MM2S_TAILDESC         = readl(host->dma_reg + XILINX_MM2S_TAILDESC  );
	host->regs.reg_XILINX_MM2S_SA               = readl(host->dma_reg + XILINX_MM2S_SA        );
	host->regs.reg_XILINX_MM2S_LENGTH           = readl(host->dma_reg + XILINX_MM2S_LENGTH    );
                                            
	host->regs.reg_XILINX_S2MM_DMACR            = readl(host->dma_reg + XILINX_S2MM_DMACR     );
	host->regs.reg_XILINX_S2MM_DMASR            = readl(host->dma_reg + XILINX_S2MM_DMASR     );
	host->regs.reg_XILINX_S2MM_CURDESC          = readl(host->dma_reg + XILINX_S2MM_CURDESC   );
	host->regs.reg_XILINX_S2MM_TAILDESC         = readl(host->dma_reg + XILINX_S2MM_TAILDESC  );
	host->regs.reg_XILINX_S2MM_DA               = readl(host->dma_reg + XILINX_S2MM_DA        );
	host->regs.reg_XILINX_S2MM_LENGTH           = readl(host->dma_reg + XILINX_S2MM_LENGTH    );

	host->regs.reg_EMMC_CLOCK 	                = readl(host->misc_reg + EMMC_CLOCK 	  );
	host->regs.reg_EMMC_CFG			            = readl(host->misc_reg + EMMC_CFG		  );  
	host->regs.reg_EMMC_BLK_LEN 		        = readl(host->misc_reg + EMMC_BLK_LEN 	  );
	host->regs.reg_EMMC_VOLTAGE_CTRL		    = readl(host->misc_reg + EMMC_VOLTAGE_CTRL);  
	host->regs.reg_EMMC_POWER_CTRL	            = readl(host->misc_reg + EMMC_POWER_CTRL  );
	host->regs.reg_EMMC_RESISTOR	            = readl(host->misc_reg + EMMC_RESISTOR	  );
	host->regs.reg_EMMC_TIME	                = readl(host->misc_reg + EMMC_TIME	      );
	                                                                                           
	host->regs.reg_EMMC_CMD_ARGUMENT        	= readl(host->cmd_reg + EMMC_CMD_ARGUMENT );
	host->regs.reg_EMMC_CMD_INDEX               = readl(host->cmd_reg + EMMC_CMD_INDEX    );
	host->regs.reg_EMMC_CMD_RESP0 		        = readl(host->cmd_reg + EMMC_CMD_RESP0 	  );
	host->regs.reg_EMMC_CMD_RESP1 		        = readl(host->cmd_reg + EMMC_CMD_RESP1 	  );
	host->regs.reg_EMMC_CMD_RESP2 		        = readl(host->cmd_reg + EMMC_CMD_RESP2 	  );
	host->regs.reg_EMMC_CMD_RESP3 		        = readl(host->cmd_reg + EMMC_CMD_RESP3 	  );
	host->regs.reg_EMMC_CMD_RESP4 		        = readl(host->cmd_reg + EMMC_CMD_RESP4 	  );
	host->regs.reg_EMMC_CMD_SR 		            = readl(host->cmd_reg + EMMC_CMD_SR 	  );
	                                                                                         
	host->regs.reg_EMMC_WR_BLK_CNT              = readl( host->wr_reg + EMMC_WR_BLK_CNT   );
	host->regs.reg_EMMC_WR_SR                   = readl( host->wr_reg + EMMC_WR_SR        );
	host->regs.reg_EMMC_WR_RISE_FIFO_COUNT      = readl( host->wr_reg + EMMC_WR_RISE_FIFO_COUNT);
	host->regs.reg_EMMC_WR_FALL_FIFO_COUNT      = readl( host->wr_reg + EMMC_WR_FALL_FIFO_COUNT);
	host->regs.reg_EMMC_WR_TIMEOUT_VALUE        = readl( host->wr_reg + EMMC_WR_TIMEOUT_VALUE);
	host->regs.reg_EMMC_WR_CRC_TIMEOUT_VALUE    = readl( host->wr_reg + EMMC_WR_CRC_TIMEOUT_VALUE);

	host->regs.reg_EMMC_RD_BLK_CNT              = readl( host->rd_reg + EMMC_RD_BLK_CNT    );
	host->regs.reg_EMMC_RD_SR                   = readl( host->rd_reg + EMMC_RD_SR         );
	host->regs.reg_EMMC_RD_RISE_FIFO_COUNT      = readl( host->rd_reg + EMMC_RD_RISE_FIFO_COUNT);
	host->regs.reg_EMMC_RD_FALL_FIFO_COUNT      = readl( host->rd_reg + EMMC_RD_FALL_FIFO_COUNT);
	host->regs.reg_EMMC_RD_TIMEOUT_VALUE        = readl( host->rd_reg + EMMC_RD_TIMEOUT_VALUE);
	host->regs.reg_EMMC_RD_DMA_LEN	            = readl( host->rd_reg + EMMC_RD_DMA_LEN    );
	host->regs.reg_EMMC_RD_IO_DELAY             = readl( host->rd_reg + EMMC_RD_IO_DELAY   );
}

void dump_mem_of_regs(struct xilinx_emmc_host *host)
{
	printk(" EMMC_CMD_ARGUMENT          %08x\n" , host->regs.reg_EMMC_CMD_ARGUMENT   );   
    printk(" EMMC_CMD_INDEX             %08x\n" , host->regs.reg_EMMC_CMD_INDEX      );
    printk(" EMMC_CMD_RESP0             %08x\n" , host->regs.reg_EMMC_CMD_RESP0 	  );
    printk(" EMMC_CMD_RESP1             %08x\n" , host->regs.reg_EMMC_CMD_RESP1 	  );
    printk(" EMMC_CMD_RESP2             %08x\n" , host->regs.reg_EMMC_CMD_RESP2 	  );
    printk(" EMMC_CMD_RESP3             %08x\n" , host->regs.reg_EMMC_CMD_RESP3 	  );
    printk(" EMMC_CMD_RESP4             %08x\n" , host->regs.reg_EMMC_CMD_RESP4 	  );
    printk(" EMMC_CMD_SR                %08x\n" , host->regs.reg_EMMC_CMD_SR    	  );
   
    printk(" EMMC_CLOCK                 %08x\n" , host->regs.reg_EMMC_CLOCK 	      );  
    printk(" EMMC_CFG                   %08x\n" , host->regs.reg_EMMC_CFG		      );  
    printk(" EMMC_BLK_LEN               %08x\n" , host->regs.reg_EMMC_BLK_LEN 		  );  
    printk(" EMMC_VOLTAGE_CTRL          %08x\n" , host->regs.reg_EMMC_VOLTAGE_CTRL	  );  
    printk(" EMMC_POWER_CTRL            %08x\n" , host->regs.reg_EMMC_POWER_CTRL	  );
    printk(" EMMC_RESISTOR              %08x\n" , host->regs.reg_EMMC_RESISTOR	      );  
    printk(" EMMC_TIME                  %08x\n" , host->regs.reg_EMMC_TIME	          );  

	printk(" EMMC_WR_BLK_CNT            %08x\n" , host->regs.reg_EMMC_WR_BLK_CNT    );
	printk(" EMMC_WR_SR                 %08x\n" , host->regs.reg_EMMC_WR_SR         );
	printk(" EMMC_WR_RISE_FIFO_COUNT    %08x\n" , host->regs.reg_EMMC_WR_RISE_FIFO_COUNT);
	printk(" EMMC_WR_FALL_FIFO_COUNT    %08x\n" , host->regs.reg_EMMC_WR_FALL_FIFO_COUNT);
	printk(" EMMC_WR_TIMEOUT_VALUE      %08x\n" , host->regs.reg_EMMC_WR_TIMEOUT_VALUE);
	printk(" EMMC_WR_CRC_TIMEOUT_VALUE  %08x\n" , host->regs.reg_EMMC_WR_CRC_TIMEOUT_VALUE);

	printk(" EMMC_RD_BLK_CNT            %08x\n" , host->regs.reg_EMMC_RD_BLK_CNT    );
	printk(" EMMC_RD_SR                 %08x\n" , host->regs.reg_EMMC_RD_SR         );
	printk(" EMMC_RD_RISE_FIFO_COUNT    %08x\n" , host->regs.reg_EMMC_RD_RISE_FIFO_COUNT);
	printk(" EMMC_RD_FALL_FIFO_COUNT    %08x\n" , host->regs.reg_EMMC_RD_FALL_FIFO_COUNT);
	printk(" EMMC_RD_TIMEOUT_VALUE      %08x\n" , host->regs.reg_EMMC_RD_TIMEOUT_VALUE);
	printk(" EMMC_RD_DMA_LEN            %08x\n" , host->regs.reg_EMMC_RD_DMA_LEN    );
	printk(" EMMC_RD_IO_DELAY           %08x\n" , host->regs.reg_EMMC_RD_IO_DELAY   );

    printk(" XILINX_MM2S_DMACR          %08x\n" , host->regs.reg_XILINX_MM2S_DMACR		  );
    printk(" XILINX_MM2S_DMASR          %08x\n" , host->regs.reg_XILINX_MM2S_DMASR        );
    printk(" XILINX_MM2S_CURDESC        %08x\n" , host->regs.reg_XILINX_MM2S_CURDESC      );
    printk(" XILINX_MM2S_TAILDESC       %08x\n" , host->regs.reg_XILINX_MM2S_TAILDESC     );
    printk(" XILINX_MM2S_SA             %08x\n" , host->regs.reg_XILINX_MM2S_SA           );
    printk(" XILINX_MM2S_LENGTH         %08x\n" , host->regs.reg_XILINX_MM2S_LENGTH       );

    printk(" XILINX_S2MM_DMACR          %08x\n" , host->regs.reg_XILINX_S2MM_DMACR        );
    printk(" XILINX_S2MM_DMASR          %08x\n" , host->regs.reg_XILINX_S2MM_DMASR        );
    printk(" XILINX_S2MM_CURDESC        %08x\n" , host->regs.reg_XILINX_S2MM_CURDESC      );
    printk(" XILINX_S2MM_TAILDESC       %08x\n" , host->regs.reg_XILINX_S2MM_TAILDESC     );
    printk(" XILINX_S2MM_DA             %08x\n" , host->regs.reg_XILINX_S2MM_DA           );
    printk(" XILINX_S2MM_LENGTH         %08x\n" , host->regs.reg_XILINX_S2MM_LENGTH       );
}

/*
 * Host misc config function
 */

int xilinx_set_bus_width(struct mmc_host *mmc, int bus_width)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	uint8_t *misc_reg = host->misc_reg;
	uint32_t tmp = readl(misc_reg + EMMC_CFG);

	tmp &= (~CFG_BUS_WIDTH);

	switch(bus_width){
		case 8:
			tmp |= (0x0 << 3);
			break;
		case 4:
			tmp |= (0x2 << 3);
			break;
		case 1:
			tmp |= (0x1 << 3);
			break;

		default:
			return -1;
	}

	printk("==========> set x%d\n", bus_width);
	writel( tmp, misc_reg + EMMC_CFG);

	return 0;
}

int xilinx_set_timing(struct mmc_host *mmc, int ddr_enable)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	uint8_t *misc_reg = host->misc_reg;
	uint32_t tmp = readl(misc_reg + EMMC_CFG);

	if(ddr_enable) {
		tmp |= CFG_DDR_ENABLE;
		printk("==========> set DDR\n");
	}
	else {
		tmp &= (~CFG_DDR_ENABLE);
		printk("==========> set SDR\n");
	}
	
	writel( tmp, misc_reg + EMMC_CFG);

	return 0;
}

int xilinx_reset_host(struct mmc_host *mmc)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	uint8_t *misc_reg = host->misc_reg;
	int i = 0;

	//step 1.1  soft reset read dma	

	writel(0x04 | 0x7000, host->dma_reg + XILINX_S2MM_DMACR); 
	for(i = 0; i < XILINX_RETRY_MAX; i++){ 					// wait soft reset 
		if( ! (readl(host->dma_reg + XILINX_S2MM_DMACR) & 0x04) )
			break;
	}
	if( i == XILINX_RETRY_MAX)
		printk("==========> dma engine hang\n");

	//step 1.2  soft reset write dma	

	writel(0x04, host->dma_reg + XILINX_MM2S_DMACR); 		
	for(i = 0; i < XILINX_RETRY_MAX; i++){ 					// wait soft reset 
		if(!(readl(host->dma_reg + XILINX_MM2S_DMACR) & 0x04))
			break;
	}
	if( i == XILINX_RETRY_MAX)
		printk("==========> dma engine hang\n");

	//step 2 reset host and config x1 bit, SDR

	writel( CFG_HOST_RESET, misc_reg + EMMC_CFG); // reset and disable data clk
	mdelay(1);
	writel( 0, misc_reg + EMMC_CFG); 

#if XILINX_BOOT_x8 // workaroud code when FPGA doesn't support x1 bus width 
	xilinx_global_x8_ready = 0;
	xilinx_set_bus_width(mmc, 8); // x8
#else 
	xilinx_set_bus_width(mmc, 1); // x1
#endif

	xilinx_set_timing(mmc, 0);	   // SDR


	//step 4 reset wr/rd/cmd clk phase config and dq delay config

	xilinx_set_clk(mmc, 400);	  // 400 KHz clk
	
	xilinx_set_wr_phase(mmc, 0);
	xilinx_set_cmd_phase(mmc,1);
	xilinx_set_rd_phase(mmc, 20);

	xilinx_set_rd_delay(mmc, 0);


#if XILINX_BOOT_x8 // workaroud code when FPGA doesn't support x1 bus width 
	printk("==========> reset host over. x8 bus width.\n");
#else
	printk("==========> reset host over. x1 bus width.\n");
#endif

	return 0;
}

static void xilinx_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	int bus_width = 0;

	switch(ios->bus_width){
	case MMC_BUS_WIDTH_1:
		bus_width = 1;
		break;
	case MMC_BUS_WIDTH_4:
		bus_width = 4;
		break;
	case MMC_BUS_WIDTH_8:
		bus_width = 8;
		break;
	default:
		return;
	}

	xilinx_set_bus_width(mmc, bus_width);

	xilinx_set_clk(mmc, ios->clock / 1000);

	return ;
}


int	xilinx_start_signal_voltage_switch(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);

	// add force clock after CMD11
	writel(readl(host->misc_reg + EMMC_CFG) | CFG_FORCE_CLK, host->misc_reg + EMMC_CFG);
	mdelay(5);
	writel(readl(host->misc_reg + EMMC_CFG) & ~CFG_FORCE_CLK, host->misc_reg + EMMC_CFG);
	return 0;
}

/*
 * Xilinx MMC host power config function
 */

#define VCC_ON          0x00000001
#define VCC_100K_ohm    0x00000002
#define VCC_35_ohm      0x00000004
#define VCC_17_ohm      0x00000008
#define VCC_08_ohm      0x00000010
#define VCC_04_ohm      0x00000020

#define VCC_MASK_ohm    0x0000003f

#define VCCq_ON         0x00000100
#define VCCq_100K_ohm   0x00000200
#define VCCq_35_ohm     0x00000400
#define VCCq_17_ohm     0x00000800
#define VCCq_08_ohm     0x00001000
#define VCCq_04_ohm     0x00002000

#define VCCq_MASK_ohm   0x00003f00

#define OP_VCC   0
#define OP_VCCq	 1
#define OP_ALL   2


// Defined in drivers/mmc/card/block.c 
// Faint.... not in .h file, so redefine here

struct mmc_blk_data {
	spinlock_t	lock;
	struct gendisk	*disk;
	struct mmc_queue queue;
	struct list_head part;

	unsigned int	flags;
#define MMC_BLK_CMD23	(1 << 0)	/* Can do SET_BLOCK_COUNT for multiblock */
#define MMC_BLK_REL_WR	(1 << 1)	/* MMC Reliable write support */
#define MMC_BLK_PACKED_CMD	(1 << 2)	/* MMC packed command support */

	unsigned int	usage;
	unsigned int	read_only;
	unsigned int	part_type;
	unsigned int	name_idx;
	unsigned int	reset_done;
#define MMC_BLK_READ		BIT(0)
#define MMC_BLK_WRITE		BIT(1)
#define MMC_BLK_DISCARD		BIT(2)
#define MMC_BLK_SECDISCARD	BIT(3)

	/*
	 * Only set in main mmc_blk_data associated
	 * with mmc_card with mmc_set_drvdata, and keeps
	 * track of the current selected device partition.
	 */
	unsigned int	part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	int	area_type;
};


// FIXME debug
void xilinx_set_card_removed( struct mmc_card *card, bool remove)
{
	struct mmc_blk_data *md = NULL;

	struct mmc_card *part_card = NULL;
	struct mmc_blk_data *part_md = NULL;

	if(remove)
		mmc_card_set_removed(card);
	else
		card->state &= ~MMC_CARD_REMOVED;

	// handle sub partitions 
	md = mmc_get_drvdata(card);

	list_for_each_entry(part_md, &md->part, part) {
		part_card = md->queue.card;

		if(remove) 
			mmc_card_set_removed(part_card);
		else
			part_card->state &= ~MMC_CARD_REMOVED;
	}
}

void xilinx_op_request_queue(struct xilinx_emmc_host *host, bool stop)
{
	struct mmc_card *card = NULL;
	struct bus_type *bus = NULL;

	struct device_driver *drv = NULL;
	struct mmc_driver *mmc_drv = NULL;

	card = host->mmc->card;
	if(!card)
		return ;

	bus = find_bus("mmc");
	if(!bus){
		printk("no mmc bus\n");
		return ;
	}

	drv = driver_find("mmcblk", bus);
	mmc_drv = container_of(drv, struct mmc_driver, drv);

	if(stop){
		mmc_drv->suspend(card);		//mmc_blk_suspend()	--> blk_stop_queue()  
		xilinx_set_card_removed( card, true);
	}
	else{
		mmc_drv->resume(card);		//mmc_blk_resume()	--> blk_start_queue()
		xilinx_set_card_removed( card, false);
	}
}


void xilinx_emmc_power_on(struct mmc_host *mmc, int voltage_x_10, int which)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	uint8_t *misc_reg = host->misc_reg;
	uint32_t value;
	uint32_t data;

	switch(which){
	case OP_VCC:

		//1. set voltage 
		value = voltage_x_10  * 255 / 33 ;
		writel(value & 0xff, misc_reg + EMMC_VOLTAGE_CTRL);
		mdelay(1);

		//2. turn on 
		data = readl(misc_reg + EMMC_POWER_CTRL);
		data &= ~VCC_MASK_ohm;
		writel( data | VCC_ON, misc_reg + EMMC_POWER_CTRL);

#if 0
		if(voltage_x_10 != 33)
			xilinx_op_request_queue(host, true);
		else
			xilinx_op_request_queue(host, false);
#endif
	
		break;

	case OP_VCCq:

		//1. set voltage 
		value = voltage_x_10  * 255 / 33;
		writel( (value & 0xff) | 0x100, misc_reg + EMMC_VOLTAGE_CTRL);
		mdelay(1);

		//2. turn on 
		data = readl(misc_reg + EMMC_POWER_CTRL);
		data &= ~VCCq_MASK_ohm;
		writel( data | VCCq_ON, misc_reg + EMMC_POWER_CTRL);

#if 0
		if(voltage_x_10 != 18)
			xilinx_op_request_queue(host, true);
		else
			xilinx_op_request_queue(host, false);
#endif

		break;

	case OP_ALL:

		//1. set voltage 
		value = voltage_x_10  * 255 / 33 ;
		writel(value & 0xff, misc_reg + EMMC_VOLTAGE_CTRL);
		mdelay(1);

		value = voltage_x_10  * 255 / 33;
		writel( (value & 0xff) | 0x100, misc_reg + EMMC_VOLTAGE_CTRL);
		mdelay(1);

		//2. turn on 
		data = readl(misc_reg + EMMC_POWER_CTRL);
		data &= ~(VCC_MASK_ohm | VCCq_MASK_ohm) ;
		writel( data | VCC_ON | VCCq_ON, misc_reg + EMMC_POWER_CTRL);

#if 0
		xilinx_op_request_queue(host, true);
#endif
		
		break;

	default:
		printk("==========> %s(), has wrong argument %d\n", __FUNCTION__, which);
		return ;
	}
}


void xilinx_emmc_set_resistor(struct mmc_host *mmc, int value, int which)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	uint8_t *misc_reg = host->misc_reg;
	uint32_t data = 0;
	int count = 10000000;
	struct timeval start,end;

	data = readl(misc_reg + EMMC_RESISTOR);

	switch(which){
	case OP_VCC:
		data &= ~0x0000ffff;
		data = (value * 1024 / 100000) | 0x400;
		break;
	case OP_VCCq:
		data &= ~0xffff0000;
		data |= ((value * 1024 / 100000) | 0x400) << 16;
		break;
	case OP_ALL:
		data = (value * 1024 / 100000) | 0x400;
		data |= ((value * 1024 / 100000) | 0x400) << 16;
		break;
	default:
		printk("==========> %s(), has wrong argument %d\n", __FUNCTION__, which);
		return ;
	}

	writel( data, misc_reg + EMMC_RESISTOR);
	//printk("==========> set resistor reg %08x\n", data);

	do_gettimeofday(&start);
	data = readl(misc_reg + EMMC_MISC_SR);
	while( !(data & RESIS_VCC_DONE) || !(data & RESIS_VCCQ_DONE)  ) {
		count--;
		if(!count) {
			printk("==========> take so long time to wait for resistor configuration\n");
			break;
		}
		data = readl(misc_reg + EMMC_MISC_SR);
	}

	do_gettimeofday(&end);

	my_debug("==========> FPGA takes %ld s, %ld us to set resistor \n", end.tv_sec - start.tv_sec, end.tv_usec - start.tv_usec);
}

void xilinx_emmc_set_power_off_speed(struct mmc_host *mmc, uint32_t speed_us, int which)
{
	uint32_t temp;
	uint32_t resistor_value;

	if(speed_us > 120) {
		resistor_value = speed_us / 4;
		xilinx_emmc_set_resistor(mmc, resistor_value, which);
	}
	else if(speed_us > 70) {
		temp = speed_us / 4;
		resistor_value = (temp * 35) / ( 35 - temp);
		xilinx_emmc_set_resistor(mmc, resistor_value, which);
	}
	else if(speed_us > 32) {
		temp = speed_us / 4;
		resistor_value = (temp * 12) / ( 12 - temp);
		xilinx_emmc_set_resistor(mmc, resistor_value, which);
	}
	else if(speed_us > 16) {
		temp = speed_us / 4;
		resistor_value = (temp * 5) / ( 5 - temp);
		xilinx_emmc_set_resistor(mmc, resistor_value, which);
	}
	else {
		xilinx_emmc_set_resistor(mmc, 0, which);
	}
}

void xilinx_emmc_power_off(struct mmc_host *mmc, uint32_t speed_us, int which)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	uint8_t *misc_reg = host->misc_reg;
	uint32_t data;
	uint32_t vcc_data;
	uint32_t vccq_data;

	data = readl(misc_reg + EMMC_POWER_CTRL);	

	if(speed_us > 120) {
		vcc_data = VCC_100K_ohm ;
		vccq_data = VCCq_100K_ohm;
	}
	else if(speed_us > 70) {
		vcc_data = VCC_100K_ohm | VCC_35_ohm;
		vccq_data = VCCq_100K_ohm | VCCq_35_ohm;
	}
	else if(speed_us > 32) {
		vcc_data = VCC_100K_ohm | VCC_35_ohm | VCC_17_ohm;
		vccq_data = VCCq_100K_ohm | VCCq_35_ohm | VCCq_17_ohm;
	}
	else if(speed_us > 16) {
		vcc_data = VCC_100K_ohm | VCC_35_ohm | VCC_17_ohm | VCC_08_ohm;
		vccq_data = VCCq_100K_ohm | VCCq_35_ohm | VCCq_17_ohm | VCCq_08_ohm;
	}
	else {
		vcc_data = VCC_100K_ohm | VCC_35_ohm | VCC_17_ohm | VCC_08_ohm | VCC_04_ohm;
		vccq_data = VCCq_100K_ohm | VCCq_35_ohm | VCCq_17_ohm | VCCq_08_ohm | VCCq_04_ohm;
	}

	switch(which){
	case OP_VCC:
		data &= ~VCC_MASK_ohm;
		writel( data | vcc_data, misc_reg + EMMC_POWER_CTRL);
		break;
	case OP_VCCq:
		data &= ~VCCq_MASK_ohm;
		writel( data | vccq_data, misc_reg + EMMC_POWER_CTRL);
		break;
	case OP_ALL:
		writel( vcc_data | vccq_data, misc_reg + EMMC_POWER_CTRL);
		break;
	default:
		printk("==========> %s(), has wrong argument %d\n", __FUNCTION__, which);
		return; 
	}
	
	printk("==========> %s(), speed %d us\n", __FUNCTION__, speed_us);

	//xilinx_op_request_queue(host, true);
	//if(host->mmc->card)
	//	xilinx_set_card_removed(host->mmc->card, true);
} 


/*
 *  eMMC Timing config function  clk/cmd/rd/wr
 */
int xilinx_set_clk(struct mmc_host *mmc, int khz)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	uint8_t *misc_reg = host->misc_reg;
	uint32_t value = readl(misc_reg + EMMC_CLOCK);

	uint32_t clkm;	// bit [5:0]
	uint32_t clkd;	// bit [11:6]

	value &= ~0xfff;// mask[11:0]
/*
 *		f/100  = clkm/clkd
 * 		CLKM [6,12], default 10
 * 		CLKM, CLKD should even
 */

	if(khz == 0)
		return 0;	// do nothing

	if( khz <= 50) {
		printk("==========> set to 50 KHz\n");
		clkm = 10;
		clkd = 40;
	}
	else if( (50<khz) && (khz<=100) ) {
		printk("==========> set to 100 KHz\n");
		clkm = 10;
		clkd = 20;
	}
	else if( (100<khz) && (khz<=200) ) {
		printk("==========> set to 200 KHz\n");
		clkm = 10;
		clkd = 10;
	}
	else if( (200<khz) && (khz<=400) ) {
		printk("==========> set to 400 KHz\n");
		clkm = 12;
		clkd = 6;
	}
	else if( (400<khz) && (khz<=25000) ) {
		printk("==========> set to 25 MHz\n");
		clkm = 10;
		clkd = 40;
		writel(readl(host->misc_reg + EMMC_CFG) | CFG_DATA_CLK, host->misc_reg + EMMC_CFG);
	}
	else if( (25000<khz) && (khz<=50000) ) {
		printk("==========> set to 50 MHz\n");
		clkm = 10;
		clkd = 20;
		writel(readl(host->misc_reg + EMMC_CFG) | CFG_DATA_CLK, host->misc_reg + EMMC_CFG);
	}
	else if( (50000<khz) && (khz<=100000) ) {
		printk("==========> set to 100 MHz\n");
		clkm = 10;
		clkd = 10;
		writel(readl(host->misc_reg + EMMC_CFG) | CFG_DATA_CLK, host->misc_reg + EMMC_CFG);
	}
	else if( (100000<khz) && (khz<=200000) ) {
		printk("==========> set to 200 MHz\n");
		clkm = 12;
		clkd = 6;
		writel(readl(host->misc_reg + EMMC_CFG) | CFG_DATA_CLK, host->misc_reg + EMMC_CFG);
	}
	else{
		printk("==========> clk %d KHz out of range, only support 50/100/200/400/25000/50000/100000/200000 KHz\n", khz);
		return -1;
	}
	
	writel( value | (clkd << 6) | clkm, misc_reg + EMMC_CLOCK);

	while( ! (readl(misc_reg + EMMC_CLOCK) & CLOCK_PLL_LOCKED) ) ;	//

	return 0;
}

int xilinx_set_cmd_phase(struct mmc_host *mmc, int phase)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	uint8_t *misc_reg = host->misc_reg;
	uint32_t value = readl(misc_reg + EMMC_CLOCK);

	my_debug("set cmd phase=%d\n", phase);

	value &= ~0x3f000000; //mask[29:24]

	writel( value | (phase << 24), misc_reg + EMMC_CLOCK);

	while( ! (readl(misc_reg + EMMC_CLOCK) & CLOCK_PLL_LOCKED) ) ;	//

	return 0;
}

int xilinx_set_rd_phase(struct mmc_host *mmc, int phase)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	uint8_t *misc_reg = host->misc_reg;
	uint32_t value = readl(misc_reg + EMMC_CLOCK);

	my_debug("set rd phase=%d\n", phase);

	value &= ~0x00fc0000; //maks[23:18]

	writel( value | (phase << 18), misc_reg + EMMC_CLOCK);

	while( ! (readl(misc_reg + EMMC_CLOCK) & CLOCK_PLL_LOCKED) ) ;	//

	return 0;
}

int xilinx_set_wr_phase(struct mmc_host *mmc, int phase)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	uint8_t *misc_reg = host->misc_reg;
	uint32_t value = readl(misc_reg + EMMC_CLOCK);

	my_debug("set wr phase=%d\n", phase);

	value &= ~0x0003f000; //mask[17:12]

	writel( value | (phase << 12), misc_reg + EMMC_CLOCK);

	while( ! (readl(misc_reg + EMMC_CLOCK) & CLOCK_PLL_LOCKED) ) ;	//

	return 0;
}

int xilinx_set_rd_delay(struct mmc_host *mmc, int phase)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	uint8_t *rd_reg = host->rd_reg;
	int i;

	my_debug("set dq phase=%d\n", phase);

	for(i=0; i<8; i++){	// total 8 IO-pins
		writel( i | (phase<<8), rd_reg + EMMC_RD_IO_DELAY);
		mdelay(10);
	}

	return 0;
}

/*
 * eMMC Tuning function  cmd/rd
 */

int xilinx_cmd_tuning(struct mmc_host *mmc)
{
	uint32_t err = 0;
	uint32_t status = 0;
	int count = 0;
	int phase = 0;

	for( phase = 0; phase < 30; phase++){
		my_debug("set cmd phase [%d] count %d\n", phase, count);

		xilinx_set_cmd_phase(mmc, phase);
		err = xilinx_mmc_send_status(mmc, &status, false);
		if(err || (status & R1_COM_CRC_ERROR) ) {
			count = 0;
			continue;
		}
		else{
			count++;
		}

		if(count == 2) {	// FIXME Toshiba eMMC need 2
							//		 Micon eMMC need 3
							//       SD need 2 or 3 ??
			xilinx_set_cmd_phase(mmc, phase - 1);
			printk("==========> cmd phase is [%d]\n",phase-1 );
			return 0;
		}
	}

	my_debug("cmd tuning fail, count [%d]\n", count);
	return -1;
}

int xilinx_rd_tuning(struct mmc_host *mmc, int opcode, uint8_t *pattern, int len)
{
	uint32_t err = 0;
	int count = 0;
	int rd_phase = 0;
	int dq_phase = 0;

	for( rd_phase = 0; rd_phase < 30; rd_phase++){
		my_debug("set rd phase [%d] count %d\n", rd_phase, count);
		count = 0;
		xilinx_set_rd_phase(mmc, rd_phase);

		for( dq_phase = 0; dq_phase < 32; dq_phase++){
			my_debug("set dq phase [%d]\n", dq_phase);
			xilinx_set_rd_delay(mmc, dq_phase);

			if(opcode == MMC_SEND_EXT_CSD)
				err = read_ecsd_and_compare(mmc, pattern, len);
			else
				err = read_tuning_and_compare(mmc, opcode, pattern, len);

			if(err){
				count = 0;
				continue;
			}
			else{
				count++;
			}

			if(count == 11){
				xilinx_set_rd_delay( mmc, dq_phase-5 );
				printk("==========> rd phase is [%d]\n", rd_phase);	
				printk("==========> dq phase is [%d]\n", dq_phase-5);	
				return 0;
			}
		}

	}

	my_debug("rd tuning fail.\n");
	return -1;
}


int read_ecsd_and_compare(struct mmc_host *mmc, uint8_t *pattern, int len)
{
	uint8_t *p_ecsd = NULL;	
	int ok = 0;

	xilinx_mmc_get_ext_csd(mmc, &p_ecsd);

	if(!p_ecsd)
		return -1;

	ok =( (pattern[EXT_CSD_PARTITION_SUPPORT] == p_ecsd[EXT_CSD_PARTITION_SUPPORT]) &&
		(pattern[EXT_CSD_ERASED_MEM_CONT] 		== p_ecsd[EXT_CSD_ERASED_MEM_CONT]) &&
		(pattern[EXT_CSD_REV] 					== p_ecsd[EXT_CSD_REV]) &&
		(pattern[EXT_CSD_STRUCTURE] 			== p_ecsd[EXT_CSD_STRUCTURE]) &&
		(pattern[EXT_CSD_CARD_TYPE] 			== p_ecsd[EXT_CSD_CARD_TYPE]) &&
		(pattern[EXT_CSD_S_A_TIMEOUT] 			== p_ecsd[EXT_CSD_S_A_TIMEOUT]) &&
		(pattern[EXT_CSD_HC_WP_GRP_SIZE] 		== p_ecsd[EXT_CSD_HC_WP_GRP_SIZE]) &&
		(pattern[EXT_CSD_ERASE_TIMEOUT_MULT] 	== p_ecsd[EXT_CSD_ERASE_TIMEOUT_MULT]) &&
		(pattern[EXT_CSD_HC_ERASE_GRP_SIZE] 	== p_ecsd[EXT_CSD_HC_ERASE_GRP_SIZE]) &&
		(pattern[EXT_CSD_SEC_TRIM_MULT] 		== p_ecsd[EXT_CSD_SEC_TRIM_MULT]) &&
		(pattern[EXT_CSD_SEC_ERASE_MULT] 		== p_ecsd[EXT_CSD_SEC_ERASE_MULT]) &&
		(pattern[EXT_CSD_SEC_FEATURE_SUPPORT] 	== p_ecsd[EXT_CSD_SEC_FEATURE_SUPPORT]) &&
		(pattern[EXT_CSD_TRIM_MULT] 			== p_ecsd[EXT_CSD_TRIM_MULT]) &&
		(pattern[EXT_CSD_SEC_CNT + 0] 			== p_ecsd[EXT_CSD_SEC_CNT + 0]) &&
		(pattern[EXT_CSD_SEC_CNT + 1] 			== p_ecsd[EXT_CSD_SEC_CNT + 1]) &&
		(pattern[EXT_CSD_SEC_CNT + 2] 			== p_ecsd[EXT_CSD_SEC_CNT + 2]) &&
		(pattern[EXT_CSD_SEC_CNT + 3] 			== p_ecsd[EXT_CSD_SEC_CNT + 3]));

	kfree(p_ecsd);

	if(ok)
		return 0;
	else
		return -1;
}

int read_tuning_and_compare(struct mmc_host *mmc, int opcode, uint8_t *pattern, int len)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;
	void *data_buf;
	int ret = 0;

	data_buf = kmalloc(512/*len*/, GFP_KERNEL);	// Must use 512-aligned
	if (!data_buf)
		return -ENOMEM;

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = opcode;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = len;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, data_buf, len);

	memset(data_buf, 0, 512);

	mmc_wait_for_req(mmc, &mrq);

	ret = memcmp(data_buf, pattern, len);

	kfree(data_buf);

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return ret;
}

/*
 * eMMC Mode Switch SDR/DDR/HS200/HS400 
 */

int xilinx_prepare_hs_sdr(struct mmc_host *mmc, int mhz, int bus_width)
{
	uint8_t *ext_csd = NULL;
	uint32_t err = 0;

	int clk_cmd_phase, clk_rd_phase, clk_wr_phase ;

	// Step 1: read ecsd 
	err = xilinx_mmc_get_ext_csd(mmc, &ext_csd);
	if (err || ext_csd == NULL){
		printk("==========> before SDR tuning, get ext csd err\n");
		return err;
	}

	// Step 2: config eMMC device, SDR / bus_width
	xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 1, 0);

	switch(bus_width){
		case 8:
			xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, 2, 0);
			break;
		case 4:
			xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, 1, 0);
			break;
		case 1:
			xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, 0, 0);
			break;
		default:
			return -1;
	}

	// Step 3: config eMMC host, SDR / bus_width, CLK
	clk_wr_phase = 0;	// default value
	clk_rd_phase = 0;	// default value, need to tuning later
	clk_cmd_phase = 1;  // default value, need to tuning later

	xilinx_set_wr_phase(mmc, clk_wr_phase);
	xilinx_set_cmd_phase(mmc, clk_cmd_phase);
	xilinx_set_rd_phase(mmc,  clk_rd_phase);

	xilinx_set_clk(mmc, mhz*1000);

	xilinx_set_bus_width(mmc, bus_width); 	// x1/4/8
	xilinx_set_timing(mmc,0); 				// SDR

	printk("==========> writeclk is [%d]\n", clk_wr_phase);	

	// Step 4: Tuning cmd
	global_in_tuning = 1;
	
	err = xilinx_cmd_tuning(mmc);
	if(err){
		printk("==========> SDR cmd tuning fail\n");
		goto err_out;
	}

	// Step 5: Tuning read 
	err = xilinx_rd_tuning(mmc, MMC_SEND_EXT_CSD, ext_csd, 512);
	if(err){
		printk("==========> SDR rd tuning fail\n");
		goto err_out;
	}

	printk("==========> SDR tuning ok\n");

err_out:
	kfree(ext_csd);
	
	global_in_tuning = 0;

	return err;
}

int xilinx_prepare_hs_ddr(struct mmc_host *mmc, int mhz, int bus_width)
{
	uint8_t *ext_csd = NULL;
	uint32_t err = 0;

	int clk_cmd_phase, clk_rd_phase, clk_wr_phase;

	// Step 1: read ecsd 
	err = xilinx_mmc_get_ext_csd(mmc, &ext_csd);
	if (err || ext_csd == NULL){
		printk("==========> before DDR tuning, get ext csd err\n");
		return err;
	}

	// Step 2: set eMMC device to DDR mode 
	xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 1, 0);

	switch(bus_width){
		case 8:
			xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, 6, 0);
			break;
		case 4:
			xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, 5, 0);
			break;
		default:
			return -1;
	}

	// Step 3: set eMMC host DDR mode, x8 ddr default
	clk_wr_phase = 250/mhz;	// v ns = 1/4 * Tperiod
	clk_rd_phase = 20;		// default value, need to tuning later
	clk_cmd_phase = 1;  	// default value, need to tuning later

	xilinx_set_wr_phase(mmc, clk_wr_phase);
	xilinx_set_cmd_phase(mmc, clk_cmd_phase);
	xilinx_set_rd_phase(mmc,  clk_rd_phase);

	xilinx_set_clk(mmc, mhz*1000);

	xilinx_set_bus_width(mmc, bus_width);	// x4/8
	xilinx_set_timing(mmc,1); 				// DDR

	printk("==========> writeclk is [%d]\n", clk_wr_phase);	

	// Step 4: Tuning cmd
	global_in_tuning = 1;

	err = xilinx_cmd_tuning(mmc);
	if(err){
		printk("==========> DDR cmd tuning fail\n");
		goto err_out;
	}

	// Step 5: Tuning read 
	err = xilinx_rd_tuning(mmc, MMC_SEND_EXT_CSD, ext_csd, 512);
	if(err){
		printk("==========> DDR rd tuning fail\n");
		goto err_out;
	}

	printk("==========> DDR tuning ok\n");

err_out:
	kfree(ext_csd);
	global_in_tuning = 0;
	return err;
}


int hs200_mhz = 200; //FIXME default HS200 clock is 200 MHz

int xilinx_execute_hs200_tuning(struct mmc_host *mmc, u32 opcode)
{
	uint8_t *ext_csd = NULL;
	int err = 0;

	int clk_cmd_phase, clk_wr_phase, clk_rd_phase;

	// Step 1: read ecsd 

	#if 1	
	// when kernel boot, mmc_init_card() will set freq to 200MHz without tuning
	// Here should restore freq to 25MHz, otherwise, some eMMC will read ECSD error.
	static int restore_low_freq = 1;
	if(restore_low_freq){
		xilinx_set_clk(mmc, 25000);	// 25MHz 
		restore_low_freq = 0;
	}
	#endif

	err = xilinx_mmc_get_ext_csd(mmc, &ext_csd);
	if (err || ext_csd == NULL){
		printk("==========> before HS200 tuning, get ext csd err\n");
		return err;
	}

	if(ext_csd[EXT_CSD_CARD_TYPE] & 0x30 ){
		// Step 2: Set emmc device to HS200 mode
		xilinx_set_clk(mmc, 25000);	// 25MHz 
		xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, 2, 0); 
		xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 2, 0);

		// Step 3: Set emmc host to HS200 mode
		clk_wr_phase = 0;	// default value
		clk_cmd_phase = 1;	// default value, need to tuning
		clk_rd_phase = 0;	// default value, need to tuning
 
		xilinx_set_wr_phase(mmc, clk_wr_phase);
		xilinx_set_cmd_phase(mmc, clk_cmd_phase);
		xilinx_set_rd_phase(mmc, clk_rd_phase);
		xilinx_set_clk(mmc, hs200_mhz*1000);

		xilinx_set_bus_width(mmc, 8);			// x8
		xilinx_set_timing(mmc,0); 				// SDR
	
		printk("==========> wr phase is [%d]\n", clk_wr_phase);	

		// Step 4: Tuning cmd
		global_in_tuning = 1;
	
		err = xilinx_cmd_tuning(mmc);
		if(err){
			printk("==========> HS200 cmd tuning fail\n");
			goto err_out;
		}

		// Step 5: Tuning read 
		err = xilinx_rd_tuning(mmc, MMC_SEND_EXT_CSD, ext_csd, 512);
		if(err){
			printk("==========> HS200 rd tuning fail\n");
			goto err_out;
		}

		printk("==========> HS200 tuning ok\n");
	}
	else{
		printk("==========> Doesn't support HS200, device type value [0x%02x]\n", ext_csd[EXT_CSD_CARD_TYPE]);
	}

err_out:
	kfree(ext_csd);
	global_in_tuning = 0;
	return err;
}


int xilinx_execute_sd_tuning(struct mmc_host *mmc, u32 opcode)
{
	int err = 0;
	int clk_cmd_phase, clk_wr_phase, clk_rd_phase;

	// Step 1: Init host configuration 
	clk_wr_phase = 0;	// default value
	clk_cmd_phase = 1;	// default value, need to tuning
	clk_rd_phase = 0;	// default value, need to tuning

	xilinx_set_wr_phase(mmc, clk_wr_phase);
	xilinx_set_cmd_phase(mmc, clk_cmd_phase);
	xilinx_set_rd_phase(mmc, clk_rd_phase);
	xilinx_set_clk(mmc, 200000);			// 200000 KHz = 200 MHz

	xilinx_set_bus_width(mmc, 4);			// x4
	xilinx_set_timing(mmc,0); 				// SDR

	printk("==========> wr phase is [%d]\n", clk_wr_phase);	

	// Step 1: Tuning cmd
	global_in_tuning = 1;

	err = xilinx_cmd_tuning(mmc);
	if(err){
		printk("==========> SD cmd tuning fail\n");
		goto err_out;
	}

	// Step 5: Tuning read 
	err = xilinx_rd_tuning(mmc, MMC_SEND_TUNING_BLOCK, tuning_block_64, 64);
	if(err){
		printk("==========> SD rd tuning fail\n");
		goto err_out;
	}

	printk("==========> SD tuning ok\n");

err_out:
	global_in_tuning = 0;
	return err;
		
}


int xilinx_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	if(opcode == MMC_SEND_TUNING_BLOCK_HS200)	// for eMMC
		return 	xilinx_execute_hs200_tuning(mmc, opcode);
	else if( opcode == MMC_SEND_TUNING_BLOCK)	// for SD
		return xilinx_execute_sd_tuning(mmc, opcode);
	else
		return -1;
}

int xilinx_prepare_hs200(struct mmc_host *mmc, int mhz)
{
	hs200_mhz = mhz;	// default 200 MHz
	return xilinx_execute_hs200_tuning(mmc, 8);
}

int xilinx_prepare_hs400(struct mmc_host *mmc, int mhz)
{
	uint8_t *ext_csd = NULL;
	int err = 0;

	int clk_cmd_phase, clk_wr_phase, clk_rd_phase;

	// Step 1: read ecsd 
	err = xilinx_mmc_get_ext_csd(mmc, &ext_csd);
	if (err || ext_csd == NULL){
		printk("==========> before HS400 tuning, get ext csd err\n");
		return err;
	}

	if(ext_csd[EXT_CSD_CARD_TYPE] & 0xc0 ){
		// Step 2: Set emmc device to HS400 mode
		xilinx_set_clk(mmc, 25000);	// 25MHz
		xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 1, 0);
		xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, 6, 0);
		xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 3, 0);

		// Step 3: Set emmc host to HS400 mode
		clk_wr_phase = 250/mhz;	// v ns = 1/4 * Tperiod
		clk_rd_phase = 0;		// default value, need to tuning
		clk_cmd_phase = 1;		// default value, need to tuning
 
		xilinx_set_wr_phase(mmc, clk_wr_phase);
		xilinx_set_cmd_phase(mmc, clk_cmd_phase);
		xilinx_set_rd_phase(mmc, clk_rd_phase);
		xilinx_set_clk(mmc, mhz*1000);	// default 200MHz
	
		xilinx_set_bus_width(mmc, 8);			// x8
		xilinx_set_timing(mmc,1); 				// DDR

		printk("==========> writeclk is [%d]\n", clk_wr_phase);	

		// Step 4: Tuning cmd
		global_in_tuning = 1;

		err = xilinx_cmd_tuning(mmc);
		if(err){
			printk("==========> HS400 cmd tuning fail\n");
			goto err_out;
		}

		// Step 5: Tuning read 
		err = xilinx_rd_tuning(mmc, MMC_SEND_EXT_CSD, ext_csd, 512);
		if(err){
			printk("==========> HS400 rd tuning fail\n");
			goto err_out;
		}

		printk("==========> HS400 tuning ok\n");
	}
	else{
		printk("==========> Doesn't support HS400\n");
	}

err_out:
	kfree(ext_csd);
	global_in_tuning = 0;
	return err;
}


int detect_bus_width_change(struct xilinx_emmc_host *host, uint32_t opcode, uint32_t arg)
{
	struct mmc_host *mmc = host->mmc;
	int access = (arg>>24) & 0x3;
	int index = (arg>>16) & 0xff;
	int value = (arg>>8) & 0xff;

	if( (opcode == MMC_SWITCH) 
		&& (access == MMC_SWITCH_MODE_WRITE_BYTE) 
		&& (index == EXT_CSD_BUS_WIDTH)) 
	{
		switch(value){
		case 0:		// x1, SDR
			xilinx_set_bus_width(mmc, 1);
			xilinx_set_timing(mmc,0);
			break;
		case 1:		// x4, SDR
			xilinx_set_bus_width(mmc, 4);
			xilinx_set_timing(mmc,0);
			break;
		case 5:		// x4, DDR
			xilinx_set_bus_width(mmc, 4);
			xilinx_set_timing(mmc,1);
			break;
		case 2:		// x8, SDR
			xilinx_set_bus_width(mmc, 8);
			xilinx_set_timing(mmc,0);
			break;
		case 6:		// x8, DDR
			xilinx_set_bus_width(mmc, 8);
			xilinx_set_timing(mmc,1);
			break;
		default:
			printk("==========> cmd [%d, %08x], argument err\n", opcode, arg);
			return -EINVAL;
		}
	}

	return 0;
}


/*
 * Handle eMMC request 
 */

static void xilinx_timeout_timer(unsigned long data)
{
	struct xilinx_emmc_host *host = (struct xilinx_emmc_host *)data;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);

	printk("==========> before err, reg list:\n\n");
	dump_mem_of_regs(host);

	printk("\n\n==========> after err, reg list:\n\n");
	save_regs_to_mem(host);
	dump_mem_of_regs(host);

    if (host->mrq) {
        printk("==========> Timeout waiting for hardware interrupt.\n");

        if (host->data) {
            host->data->error = -ETIMEDOUT;
        } else {
            if (host->cmd)
                host->cmd->error = -ETIMEDOUT;
            else 
                host->mrq->cmd->error = -ETIMEDOUT;
        }    
		mmc_request_done(host->mmc, host->mrq);
		host->mrq = NULL;
		host->cmd = NULL;
		host->data = NULL;
    } 

	spin_unlock_irqrestore(&host->lock, flags);
}

void xilinx_send_command(struct xilinx_emmc_host *host, struct mmc_command *cmd)
{
	uint8_t *cmd_reg = host->cmd_reg;
	uint32_t arg = cmd->arg;
	uint32_t opcode = cmd->opcode;
	uint32_t tmp = 0;

	host->cmd = cmd;			//
	host->data = cmd->data;		//

	tmp = opcode;

	switch(mmc_resp_type(cmd)){
		case MMC_RSP_NONE:
			tmp |= RESPONSE_NONE;
			break;	

		case MMC_RSP_R1:
			tmp |= RESPONSE_R1;
			break;	

		case MMC_RSP_R2:
			tmp |= RESPONSE_R2;
			break;	

		case MMC_RSP_R3:
			tmp |= RESPONSE_R3;
			break;	

		case MMC_RSP_R1B:
			tmp |= RESPONSE_R1B;
			break;	

		default:
			printk("==========> unsupport response type [%d], R1 default\n", mmc_resp_type(cmd));
			tmp |= RESPONSE_R1;
			break;
	}

	switch(opcode){

	case MMC_GO_IDLE_STATE:
		global_in_vendor = 0;
		if( (arg == 0) || (arg == 0xf0f0f0f0) )
			xilinx_reset_host(host->mmc);
		break;

	case MMC_SET_RELATIVE_ADDR:
		if( mmc_cmd_type(cmd) == MMC_CMD_AC )
			global_rca = arg >> 16;
		break;

	case MMC_SWITCH:
		detect_bus_width_change(host, opcode, arg);
		if( 165 == ( (arg>>16) & 0xff ) )
			tmp = opcode | RESPONSE_R1;			// FIXME  when sanitize, change it to R1
		break;

	case MMC_SELECT_CARD:
		if(arg >> 16 != global_rca)				// if use cmd7 to deselect
			tmp = opcode | RESPONSE_NONE;		// no reponse when deselect
		break;

	case 60:
	case 61:
	case 62:
	case 63:
		global_in_vendor = 1;
		break;
	default:
		break;
	}

	writel(arg, cmd_reg + EMMC_CMD_ARGUMENT);
	writel(tmp, cmd_reg + EMMC_CMD_INDEX);

	my_debug("\n==========> cmd [%d, %08x] expect %s\n", opcode, arg, resp_string[(tmp&0xff00)>>8] );
}


int xilinx_sdma_prepare(struct xilinx_emmc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;
	int i = 0;

	writel(data->blksz, host->misc_reg + EMMC_BLK_LEN);	// config block length
	my_debug("\n==========> config block size %d bytes\n", data->blksz);

	if( data->flags & MMC_DATA_READ) {
		// soft reset read dma	
		writel(0x04 | 0x7000, host->dma_reg + XILINX_S2MM_DMACR);

		// wait soft reset 
		for(i = 0; i < XILINX_RETRY_MAX; i++){
			if( ! (readl(host->dma_reg + XILINX_S2MM_DMACR) & 0x04) )
				break;
		}
		if( i == XILINX_RETRY_MAX)
			printk("==========> dma engine hang\n");

		// start read 
		writel(0x01, host->dma_reg + XILINX_S2MM_DMACR);

		// wait read dma channel
		for(i = 0; i < XILINX_RETRY_MAX; i++){
			if(!(readl(host->dma_reg + XILINX_S2MM_DMASR) & 0x01))
				break;
		}
		if( i == XILINX_RETRY_MAX)
			printk("==========> dma engine hang\n");

		// set dest dma addr
		writel(sg_dma_address(data->sg), host->dma_reg + XILINX_S2MM_DA);

	}
	else{
		// software reset write dma	
		writel(0x04, host->dma_reg + XILINX_MM2S_DMACR);

		// wait software reset 
		for(i = 0; i < XILINX_RETRY_MAX; i++){
			if(!(readl(host->dma_reg + XILINX_MM2S_DMACR) & 0x04))
				break;
		}
		if( i == XILINX_RETRY_MAX)
			printk("==========> dma engine hang\n");

		// start write dma
		writel(0x01, host->dma_reg + XILINX_MM2S_DMACR);

		// wait read dma channel
		for(i = 0; i < XILINX_RETRY_MAX; i++){
			if(!(readl(host->dma_reg + XILINX_MM2S_DMASR) & 0x01))
				break;
		}
		if( i == XILINX_RETRY_MAX)
			printk("==========> dma engine hang\n");

		// set src dma addr
		writel(sg_dma_address(data->sg), host->dma_reg + XILINX_MM2S_SA);

	}

	return 0;
}

int xilinx_sdr_x8_25mhz(struct mmc_host *mmc)
{
	uint32_t err = 0;

	int clk_cmd_phase, clk_rd_phase, clk_wr_phase ;

	// Step 2: set eMMC device to x8, SDR mode 
	xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 1, 0);
	xilinx_mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, 2, 0);

	// Step 3: set eMMC host SDR mode, x8 sdr default
	clk_wr_phase = 0;
	clk_rd_phase = 20;
	clk_cmd_phase = 1;

	xilinx_set_wr_phase(mmc, clk_wr_phase);
	xilinx_set_cmd_phase(mmc, clk_cmd_phase);
	xilinx_set_rd_phase(mmc,  clk_rd_phase);

	xilinx_set_clk(mmc, 25000);

	xilinx_set_bus_width(mmc, 8);				// x8
	xilinx_set_timing(mmc, 0);					// SDR

	return err;
}

static void xilinx_pre_request(struct mmc_host *mmc, struct mmc_request *mrq, bool is_first_req)
{
	struct mmc_data *data = mrq->data;

	if (!data)
		return;

	BUG_ON(data->host_cookie);

	dma_map_sg(mmc_dev(mmc), 
			data->sg, data->sg_len,
			(data->flags & MMC_DATA_READ) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);

	data->host_cookie = 1;
}

static void xilinx_post_request(struct mmc_host *mmc, struct mmc_request *mrq, int err)
{
	struct mmc_data *data = mrq->data;

	if (!data)
		return;

	if(data->host_cookie == 0)
		return; 

	dma_unmap_sg(mmc_dev(mmc),mrq->data->sg, mrq->data->sg_len, 
		(mrq->data->flags & MMC_DATA_READ) ? DMA_FROM_DEVICE : DMA_TO_DEVICE );

	data->host_cookie = 0;
}

static void xilinx_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct xilinx_emmc_host *host = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;
	int err = 0;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);

	mod_timer(&host->timer, jiffies + 200*HZ);
#if 1
	if( mmc_card_removed(mmc->card) ) {
		printk("==========> card has been removed, done request\n");
		goto finish_request;
	}
#endif

	save_regs_to_mem(host);

	host->mrq = mrq;

	if(data){
#if XILINX_BOOT_x8 // workaroud code when FPGA doesn't support x1 bus width 
		/************************************************
		 * force to x8 bit bus width, 
		 * here will call xilinx_request() again
		 * FIXME could change mrq, could relock
		 */
		if( xilinx_global_x8_ready == 0) {

			spin_unlock_irqrestore(&host->lock, flags);	// workaround issue of "relock"
			host->mrq = NULL;							// workaround issue of "change mrq"	

			err = xilinx_sdr_x8_25mhz(mmc);				// default is 25MHz
			
			spin_lock_irqsave(&host->lock, flags);
			host->mrq = mrq;

			if(err)
				goto finish_request; 
			else
				xilinx_global_x8_ready = 1;
		}
		/**********************************************/
#endif

		if(data->host_cookie == 0) {
			dma_map_sg(mmc_dev(mmc), 
					data->sg, data->sg_len,
					(data->flags & MMC_DATA_READ) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
		}

		err = xilinx_sdma_prepare(host, mrq);
		if(err){
			mrq->cmd->error = -EIO;
			goto finish_request;
		}
	}

	if(mrq->sbc){	// close-end multiblock transfer
		do_gettimeofday(&sbc_start);
		xilinx_send_command(host, mrq->sbc);
	}
	else{			// open-end multiblock transfer or non-r/w command or boot_mode
		do_gettimeofday(&cmd_start);
		xilinx_send_command(host, mrq->cmd);
	}

	spin_unlock_irqrestore(&host->lock, flags);
	return ;

finish_request:
	del_timer(&host->timer);
	mmc_request_done(mmc, mrq);
	host->mrq = NULL;
	host->cmd = NULL;
	host->data = NULL;

	spin_unlock_irqrestore(&host->lock, flags);
	return ;
}



/*
 * Handle eMMC device Interrupt
 */

#define CMD_ERRORS                          \
    (R1_OUT_OF_RANGE    | /* Command argument out of range */\
     R1_ADDRESS_ERROR   | /* Misaligned address */        \
     R1_BLOCK_LEN_ERROR | /* Transferred block length incorrect */\
	 R1_ERASE_SEQ_ERROR | /* er, c */\
	 R1_ERASE_PARAM		| /* ex, c */\
     R1_WP_VIOLATION    | /* Tried to write to protected block */\
     R1_CARD_IS_LOCKED  | /* sx, a */\
	 R1_LOCK_UNLOCK_FAILED| /* erx, c */\
	 R1_COM_CRC_ERROR	| /* er, b */\
	 R1_ILLEGAL_COMMAND	| /* er, b */\
	 R1_CARD_ECC_FAILED	| /* ex, c */\
     R1_CC_ERROR        | /* Card controller error */\
     R1_ERROR           | /* General/unknown error */\
	 R1_CID_CSD_OVERWRITE |	/* erx, c, CID/CSD overwrite */\
	 R1_WP_ERASE_SKIP)	  /* sx, c */



#define printk_in_irq(fmt, ...) 	\
	do { if(global_in_tuning == 0) printk(fmt, ##__VA_ARGS__); } while(0)

// R1 -->  32 bit, device status
// R2 --> 128 bit, CID, CSD
// R3 -->  32 bit, OCR

void xilinx_handle_cmd_irq(struct xilinx_emmc_host *host, uint32_t sr )
{
	uint8_t *cmd_reg = host->cmd_reg;
	uint32_t tmp0 = readl(cmd_reg+ EMMC_CMD_RESP0);
	uint32_t tmp1 = readl(cmd_reg+ EMMC_CMD_RESP1);
	uint32_t tmp2 = readl(cmd_reg+ EMMC_CMD_RESP2);
	uint32_t tmp3 = readl(cmd_reg+ EMMC_CMD_RESP3);

	host->cmd->error = 0;

	// 1. handle response interrupt fail 
	if( sr & RESP_TIMEOUT){
		host->cmd->error = -ETIMEDOUT;
		switch(host->cmd->opcode){
		case 5:
		case 55:
		case 52:
			break;
		default:
			printk_in_irq("==========> [%08x] RESP_TIMEOUT for cmd [%d, %08x]\n", sr, host->cmd->opcode, host->cmd->arg);
		}
		goto finish_request;
	}

	if( sr & RESP_CRC_ERROR){
		host->cmd->error = -EIO;
		printk_in_irq("==========> [%08x] RESP_CRC_ERROR for cmd [%d, %08x]\n", sr, host->cmd->opcode, host->cmd->arg);
		goto finish_request;
	}

	// 2. handle response status fail
	switch(mmc_resp_type(host->cmd)) {
	case MMC_RSP_R1:
	case MMC_RSP_R1B:

		host->cmd->resp[0] = tmp1<<24 | tmp0>>8;
		my_debug("re0 is [%08x]\n", host->cmd->resp[0]);

		if(global_fake_wp){
			host->cmd->resp[0] |= R1_WP_VIOLATION;
			printk("==========> fake Write Protect err has been set\n");
		}

		// FIXME when cmd7 deslect, no resp
		if((host->cmd->opcode == MMC_SELECT_CARD) && (host->cmd->arg >> 16) != global_rca)
			goto finish_request;

		// FIXME when cmd42 unlock, host will send a block to device, ignore R1_CARD_IS_LOCKED bit in response
		if((host->cmd->opcode == MMC_LOCK_UNLOCK) && (host->cmd->resp[0] & R1_CARD_IS_LOCKED) ) 
			break;	

		if( (mmc_cmd_type(host->cmd) != MMC_CMD_BCR) &&  (!global_in_vendor) && (host->cmd->resp[0] & CMD_ERRORS) ) {
			// ignore fake error of R1_OUT_OF_RANGE
			// refer to JEDEC 5.0 Sector 6.8.3 <<Read ahead in multiple block read operation>>
			if( (host->cmd->resp[0] & R1_OUT_OF_RANGE) &&
				(host->cmd->opcode == MMC_STOP_TRANSMISSION) &&
				(host->mrq->cmd->opcode == MMC_READ_MULTIPLE_BLOCK) 
			  ) 
			{
				if( host->mrq->cmd->arg + host->mrq->data->blocks == host->mmc->card->ext_csd.sectors) { 
					printk("==========> ignore R1_OUT_OF_RANGE because of read ahead feature when open-end access user-data partition\n");
					host->cmd->resp[0] &= ~(R1_OUT_OF_RANGE);
					break;
				}
				else if( host->mrq->cmd->arg + host->mrq->data->blocks == host->mmc->card->part[0].size >> 9) { 
					printk("==========> ignore R1_OUT_OF_RANGE because of read ahead feature when open-end access boot partition\n");
					host->cmd->resp[0] &= ~(R1_OUT_OF_RANGE);
					break;
				}
			}

			printk_in_irq("==========> resp err[%08x] for cmd[%d, %08x]\n",host->cmd->resp[0], host->cmd->opcode, host->cmd->arg);
			host->cmd->error = -EIO;
			goto finish_request;
		}

		break;

	case MMC_RSP_R2:
		host->cmd->resp[0] = tmp3;
		host->cmd->resp[1] = tmp2;
		host->cmd->resp[2] = tmp1;
		host->cmd->resp[3] = tmp0;

		my_debug("re0 is [%08x]\n", host->cmd->resp[0]);
		my_debug("re1 is [%08x]\n", host->cmd->resp[1]);
		my_debug("re2 is [%08x]\n", host->cmd->resp[2]);
		my_debug("re3 is [%08x]\n", host->cmd->resp[3]);
		break;

	case MMC_RSP_R3:
		host->cmd->resp[0] = tmp1<<24 | tmp0>>8;
		#if 1 // leonwang add 2014-12-17 to avoid using mmc_card->state
			if(host->cmd->opcode == MMC_SEND_OP_COND){
				if(host->cmd->resp[0] & (1<<30))
					xilinx_global_access = MMC_STATE_BLOCKADDR;
				else
					xilinx_global_access = 0;
			}
		#endif
		my_debug("re0 is [%08x]\n\n", host->cmd->resp[0]);
		break;

	case MMC_RSP_NONE:
		if( (host->cmd->opcode == MMC_GO_IDLE_STATE) && (host->cmd->arg == 0xfffffffa) ) {	// add for boot_mode
			// remove host config to application mmap()
			//writel( readl(host->misc_reg + EMMC_CFG) | CFG_DATA_CLK, host->misc_reg + EMMC_CFG);
			break;
		}

		goto finish_request;

	default:
		host->cmd->resp[0] = tmp1<<24 | tmp0>>8;
		my_debug("re0 is [%08x]\n\n", host->cmd->resp[0]);
		break;
	}

	// FIXME word around FPGA bug that always get 0x0 resp after power down eMMC unit
	if( (host->cmd->resp[0] == 0) 
			&& (mmc_resp_type(host->cmd) != MMC_RSP_R2)
			&& (mmc_resp_type(host->cmd) != MMC_RSP_R6)
			&& (mmc_resp_type(host->cmd) != MMC_RSP_NONE) )
	{
		printk_in_irq("==========> unit power supply has been cut off, resp is [%08x]\n", host->cmd->resp[0]);
		host->cmd->error = -EIO;
		goto finish_request;
	}


	//3. close-end multiblock transfer, finish CMD23, now send actual rw CMD18/25
	if(host->cmd == host->mrq->sbc){
		do_gettimeofday(&sbc_end);
		do_gettimeofday(&cmd_start);

		host->cmd = NULL;
		xilinx_send_command(host, host->mrq->cmd);
		return ;
	}
	else{
		//4. complete non-rw cmd and finish here
		if(host->cmd->data == NULL)
			goto finish_request;

		//5. complete rw cmd CMD18/25, trigger DMA transfer 
		if(host->cmd->data){
			if(host->cmd->data->flags & MMC_DATA_READ) {
				// set len (fifo ===> main ram)
				writel( host->cmd->data->blocks * host->cmd->data->blksz , host->dma_reg + XILINX_S2MM_LENGTH);

				writel( host->cmd->data->blocks,       host->rd_reg + EMMC_RD_BLK_CNT);
				writel( host->cmd->data->blocks * host->cmd->data->blksz , host->rd_reg + EMMC_RD_DMA_LEN);
				my_debug("\n==========> start read %d blocks\n", host->cmd->data->blocks);
			}
			else{

				// set len (main ram ===> fifo)
				writel(sg_dma_len(host->cmd->data->sg), host->dma_reg + XILINX_MM2S_LENGTH);

				writel( host->cmd->data->blocks, host->wr_reg + EMMC_WR_BLK_CNT);
				my_debug("\n==========> start write %d blocks\n", host->cmd->data->blocks);

				
			}
		}
	}

	return ;

finish_request:

	if((host->cmd->opcode == MMC_SET_RELATIVE_ADDR) && (mmc_cmd_type(host->cmd) == MMC_CMD_BCR ) )
		global_rca = (host->cmd->resp[0]) >> 16;

	if(host->cmd->opcode == MMC_SET_RELATIVE_ADDR) {
		// After CMD3, enable data tranfser clock (min 25MHz)
		host->mmc->ios.clock = 25000000;
		xilinx_set_clk(host->mmc, 25000);
	}

	if(host->cmd->opcode == MMC_STOP_TRANSMISSION) {
		do_gettimeofday(&stop_end);

		fpga_stop_time = readl(host->misc_reg + EMMC_TIME) * 10; 	//unit ns
		writel(0x00000000, host->misc_reg + EMMC_TIME);
	}

#if 0
#define UNSTUFF_BITS(resp,start,size)					\
	({								\
		const int __size = size;				\
		const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);			\
		const int __shft = (start) & 31;			\
		u32 __res;						\
									\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})
	
	if(host->cmd->opcode == MMC_ALL_SEND_CID){
		unsigned int *resp = host->cmd->resp;
		unsigned int mid = UNSTUFF_BITS(resp, 120, 8);
		printk("==========> Now eMMC cid.mid is [%08x]\n", mid);
		if( (mid != 0xFE) && (mid != 0x13) ) {
			printk("==========> Not Micron eMMC unit, stop init.\n");
			host->cmd->error = -EIO;
		}
		else{
			printk("==========> Micron eMMC unit, let's go on.\n");
		}
	}
#endif

	del_timer(&host->timer);
	mmc_request_done(host->mmc, host->mrq);

	host->mrq = NULL;
	host->cmd = NULL;
	host->data = NULL;

	return ;
}

void xilinx_handle_dma_irq(struct xilinx_emmc_host *host, uint32_t sr )
{
	host->data->error = 0;

	// 1. handle response interrupt fail 
	if(host->data->flags & MMC_DATA_WRITE) {
		if( sr & WR_TIMEOUT){
			printk_in_irq("==========> [%08x] DMA write timeout\n", sr);
			host->data->error = -EIO;
		}

		if( sr & WR_CRC_ERROR){
			printk_in_irq("==========> [%08x] DMA write CRC error\n", sr);
			host->data->error = -EIO;
		}

		if( sr & WR_CRC_TIMEOUT){
			printk_in_irq("==========> [%08x] DMA write CRC token timeout\n", sr);
			host->data->error = -EIO;
		}
	}
	else{
		if( sr & RD_TIMEOUT){
			printk_in_irq("==========> [%08x] DMA read timeout\n", sr);
			host->data->error = -EIO;
		}

		if( sr & RD_CRC_ERROR){
			printk_in_irq("==========> [%08x] DMA read CRC error\n", sr);
			host->data->error = -EIO;
		}
	}


	if( host->data->host_cookie == 0) {
		dma_unmap_sg(mmc_dev(host->mmc),host->data->sg, host->data->sg_len, 
			(host->data->flags & MMC_DATA_READ) ? DMA_FROM_DEVICE : DMA_TO_DEVICE );
	}

	if(host->data->error)
		goto finish_request;
		

	host->data->bytes_xfered += host->data->blksz * host->data->blocks;

#if 0
	uint8_t *data_buf = kmalloc(8192, GFP_KERNEL);	

	sg_copy_to_buffer(host->data->sg, host->data->sg_len, data_buf, 8192);

	printk("debug in %s()  %s %d Bytes\n", 
		__FUNCTION__, 
		(host->data->flags & MMC_DATA_READ) ?"read":"write", 
		host->data->bytes_xfered
	);

	dump_buf(data_buf, host->data->bytes_xfered );
#endif

	fpga_tran_time = readl(host->misc_reg+ EMMC_TIME) * 10; 	//unit ns
	writel(0x00000000, host->misc_reg+ EMMC_TIME);

	// open-end multiblock transfer, need to send stop cmd
	if( (host->cmd->data->stop) && (!host->mrq->sbc) ) {

		do_gettimeofday(&cmd_end);
		do_gettimeofday(&stop_start);

		if(do_power_loss_flag){
			if(do_power_loss_volt == 0)
				xilinx_emmc_set_power_off_speed(host->mmc, 0, OP_ALL);
			else
				;	// do nothing
		}

		xilinx_send_command(host, host->cmd->data->stop);

		if(do_power_loss_flag){

			udelay(do_power_loss_delay_us);

			if(do_power_loss_volt == 0)
				xilinx_emmc_power_off(host->mmc, do_power_loss_speed_us, OP_ALL);
			else
				xilinx_emmc_power_on(host->mmc, do_power_loss_volt, OP_ALL);
					
			do_power_loss_flag = 0;

			host->cmd->error = -EIO;
		
			writel( CFG_HOST_RESET, host->misc_reg + EMMC_CFG); // clear the pending irq 
			goto finish_request;
		}

		return ;
	}


	// close-end data cmd or one-block rw command, wake up here
finish_request:

	do_gettimeofday(&cmd_end);
	del_timer(&host->timer);
	mmc_request_done(host->mmc, host->mrq);
	host->mrq = NULL;
	host->cmd = NULL;
	host->data = NULL;
	
	return ;
}

static irqreturn_t xilinx_irq(int irq, void *dev)
{
	struct xilinx_emmc_host *host = (struct xilinx_emmc_host *)dev;

	uint8_t *cmd_reg = host->cmd_reg;
	uint8_t *wr_reg = host->wr_reg;
	uint8_t *rd_reg = host->rd_reg;

	uint32_t cmd_sr = 0;
	uint32_t wr_sr = 0;
	uint32_t rd_sr = 0;

	unsigned long flags;

	my_debug(">>>>>>>>>>>>> ISR IN  <<<<<<<<<<<\n");

	spin_lock_irqsave(&host->lock, flags);

	cmd_sr = readl(cmd_reg+ EMMC_CMD_SR);
	wr_sr = readl(wr_reg+ EMMC_WR_SR);
	rd_sr = readl(rd_reg+ EMMC_RD_SR);

	if(cmd_sr & RESP_DONE){
		// Handle cmd irq
		writel(0x0, cmd_reg+ EMMC_CMD_SR);

		my_debug("%s() in cmd irq %08x\n", __FUNCTION__, cmd_sr);
		if( host->cmd == NULL){
			printk("==========> host->cmd is NULL in RESP interrupt! cmd_sr %08x\n", cmd_sr);
			goto irq_out;
		}
		xilinx_handle_cmd_irq(host, cmd_sr);
	}
	else if(wr_sr & WR_DONE){

		// Handle DMA write irq
		writel(0x0, wr_reg+ EMMC_WR_SR);

		my_debug("%s() in dma write irq %08x\n", __FUNCTION__, wr_sr);
		if( host->data == NULL){
			printk("==========> host->data is NULL in DMA write interrupt! wr_sr %08x\n", wr_sr);
			goto irq_out;
		}
		xilinx_handle_dma_irq(host, wr_sr);
	}
	else if(rd_sr & RD_DONE){
		// Handle DMA read irq
		writel(0x0, rd_reg+ EMMC_RD_SR);

		my_debug("%s() in dma read irq %08x\n", __FUNCTION__, rd_sr);
		if( host->data == NULL){
			printk("==========> host->data is NULL in DMA read interrupt! rd_sr %08x\n", rd_sr);
			goto irq_out;
		}
		xilinx_handle_dma_irq(host, rd_sr);
	}

irq_out:
	my_debug(">>>>>>>>>>>>> ISR OUT <<<<<<<<<<<\n");
	spin_unlock_irqrestore(&host->lock, flags);
	return IRQ_HANDLED;
}



static const struct mmc_host_ops xilinx_ops = {
    .request    = xilinx_request,
    .set_ios    = xilinx_set_ios,
	.execute_tuning	= xilinx_execute_tuning,
	.pre_req    = xilinx_pre_request,
	.post_req   = xilinx_post_request,
	.start_signal_voltage_switch = xilinx_start_signal_voltage_switch,
};


static int __init xilinx_emmc_probe(struct platform_device *pdev)
{
	struct device *dev = &(pdev->dev);
	struct resource *res0;
	struct resource *res1;
	struct resource *res2;
	struct resource *res3;
	struct resource *res4;

	struct mmc_host *mmc;
	struct xilinx_emmc_host *host;

	res0 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	res2 = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	res3 = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	res4 = platform_get_resource(pdev, IORESOURCE_MEM, 4);

	if (!res0 || !res1 || !res2 || !res3 || !res4 ) {
		dev_err(dev, "no memory specified\n");
		return -ENOENT;
	}

    mmc = mmc_alloc_host(sizeof(struct xilinx_emmc_host), dev);
	if (!mmc){
		dev_err(dev, "mmc_alloc_host() fail\n");
		return -ENOMEM;
	}

	/*
	 * init emmc_host 
	 */
	mmc->ops = &xilinx_ops;
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->caps = 0;
	mmc->caps |= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA;			// support x4/x8
	mmc->caps |= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED;		// support SD/MMC High Speed
	mmc->caps |= MMC_CAP_CMD23;				// support close-end multiblock transfer
	mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY;	// when set, mmc_cmd has MMC_RSP_CRC flag and mmc_resp_type() need it.
	mmc->caps |= MMC_CAP_ERASE;				// when set, mmc_can_erase() return ture, mmc_init_queue()-->mmc_queue_setup_discard()

	mmc->caps |= MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 | 
				MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR104 | 	// Need tuning
				MMC_CAP_UHS_DDR50;	// Add for SD card

	mmc->caps2 = 0;
	mmc->caps2 |= MMC_CAP2_HS200;			// support HS200
	mmc->caps2 |= MMC_CAP2_CACHE_CTRL;		// when set, mmc_init_card() will enable Cache Feature
	mmc->caps2 |= MMC_CAP2_SANITIZE;		// when set, mmc_blk_ioctl() interface will support sanitize operation

	mmc->max_blk_size  = 512;
	mmc->max_blk_count = 2 * 1024 * 10;
	mmc->max_req_size  = 1024 * 1024 * 10;	// maximum number of bytes in one req
	mmc->max_seg_size  = 1024 * 1024 * 10;  // maximum number of bytes in one segment
	mmc->max_segs      = 1; 				// use sdma	

	mmc->f_min =     50000;			//  50 KHz
	mmc->f_max = 200000000;			// 200 MHz

	platform_set_drvdata(pdev, mmc);
	xilinx_global_mmc = mmc;

	/*
	 * init xilinx_emmc_host 
	 */
	host = mmc_priv(mmc);
	host->mmc = mmc;

	spin_lock_init(&host->lock);
	setup_timer(&host->timer, xilinx_timeout_timer, (unsigned long)host);

	host->dma_reg  = ioremap(res0->start, resource_size(res0));
	host->misc_reg = ioremap(res1->start, resource_size(res1));
	host->cmd_reg  = ioremap(res2->start, resource_size(res2));
	host->wr_reg   = ioremap(res3->start, resource_size(res3));
	host->rd_reg   = ioremap(res4->start, resource_size(res4));

	if (!host->dma_reg || !host->misc_reg || !host->cmd_reg || !host->wr_reg || !host->rd_reg) {
		dev_err(dev, "memory map error!\n");
		return -ENOMEM;
	}

	printk("==========> fpga_version [%08x]\n", readl(host->misc_reg + EMMC_VERSION) );
	printk("==========> driv_version [%s]\n",   XILINX_EMMC_DRV_VERSION );

	host->irq = platform_get_irq(pdev, 0);
	request_irq(host->irq, xilinx_irq, IRQF_DISABLED, mmc_hostname(mmc), host);
	printk("==========> Xilinx IRQ[%s] is %d\n", mmc_hostname(mmc), host->irq);

	/*
	 * reset host config 
	 */

	xilinx_reset_host(mmc);
	writel(0x2fffffff, host->rd_reg + EMMC_RD_TIMEOUT_VALUE);	// FIXME config timeout to max
	writel(0xffffffff, host->wr_reg + EMMC_WR_TIMEOUT_VALUE);	// FIXME config timeout to max
	writel(512, host->misc_reg + EMMC_BLK_LEN);					// FIXME config block length default 512

	// these 2 funcions only work when power-loss board attached
	xilinx_emmc_power_on(mmc, 33, OP_VCC);		// eMMC_VCC / SD_VCC
	xilinx_emmc_power_on(mmc, 18, OP_VCCq);		// eMMC_VCCq

	mmc_add_host(mmc); 	// add host to MMC layer

	return 0;
}

struct of_device_id xilinx_emmc_of_match[] = {
	{ .compatible = "xilinx-emmc", },
	{},
};

static struct platform_driver xilinx_emmc_driver = { 
	.probe		= xilinx_emmc_probe,
	.remove     = __exit_p(xilinx_emmc_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "xilinx-emmc",
		.of_match_table = xilinx_emmc_of_match,
	},
};

static int __init xilinx_emmc_init(void)
{
    return platform_driver_register(&xilinx_emmc_driver);
}

static void __exit xilinx_emmc_remove(void)
{
    platform_driver_unregister(&xilinx_emmc_driver);
}

EXPORT_SYMBOL_GPL(xilinx_global_access);
EXPORT_SYMBOL_GPL(xilinx_global_mmc);

EXPORT_SYMBOL_GPL(xilinx_mmc_send_status);
EXPORT_SYMBOL_GPL(xilinx_mmc_switch);

EXPORT_SYMBOL_GPL(xilinx_dump_reg);
EXPORT_SYMBOL_GPL(xilinx_get_time_dbg);

EXPORT_SYMBOL_GPL(xilinx_prepare_hs_sdr);
EXPORT_SYMBOL_GPL(xilinx_prepare_hs_ddr);
EXPORT_SYMBOL_GPL(xilinx_prepare_hs200);
EXPORT_SYMBOL_GPL(xilinx_prepare_hs400);

EXPORT_SYMBOL_GPL(xilinx_emmc_power_on);
EXPORT_SYMBOL_GPL(xilinx_emmc_power_off);

module_init(xilinx_emmc_init);
module_exit(xilinx_emmc_remove);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leon Wang");
MODULE_DESCRIPTION("Xilinx FPGA eMMC Host driver");