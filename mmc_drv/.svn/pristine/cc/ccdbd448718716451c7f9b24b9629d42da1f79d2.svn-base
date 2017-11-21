/*
 * Character-device access to EMMC devices.
 */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/host.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/slab.h>	
#include <linux/dma-mapping.h>	

#include "mmc_ioctl.h"

#define MMCDEV_MAJOR       227
#define MMCPOWER_DEV_MAJOR 228  

struct misc {
	int option;
	int value[10];
};

int sd_mode = 0;

static int mmc_major = MMCDEV_MAJOR;
static int mmc_power_major = MMCPOWER_DEV_MAJOR; 

static struct cdev cdev; 
static struct cdev power_cdev; 

static int blksz = 512;
static char *buf = NULL;
#define BUF_SIZE (8*1024*1024)
dma_addr_t dma_addr;

extern int xilinx_global_access;
extern struct mmc_host *xilinx_global_mmc;

extern int xilinx_dump_reg(void *host, void *misc);
extern int xilinx_mmc_switch(struct mmc_host *mmc, u8 set, u8 index, u8 value, unsigned int timeout_ms);

extern int xilinx_prepare_hs_sdr(struct mmc_host *mmc, int mhz, int bus_width);
extern int xilinx_prepare_hs_ddr(struct mmc_host *mmc, int mhz, int bus_width);
extern int xilinx_prepare_hs200(struct mmc_host *mmc, int mhz);
extern int xilinx_prepare_hs400(struct mmc_host *mmc, int mhz);

extern void xilinx_emmc_power_on(struct mmc_host *mmc, int voltage_x_10, int which);
extern void xilinx_emmc_power_off(struct mmc_host *mmc, int speed_us, int which);

extern int xilinx_get_time_dbg(struct timeval *p_sbc_start, 	// software time
                    struct timeval *p_sbc_end, 					// s
                    struct timeval *p_cmd_start,				// s 
                    struct timeval *p_cmd_end, 					// s
                    struct timeval *p_stop_start,				// s
                    struct timeval *p_stop_end, 				// s
                    uint32_t *p_cmd_time,						// hardware time
                    uint32_t *p_stop_time);						// h

/*
 * eMMC char driver
 */

static unsigned int mmc_fill_cmd_flag(u32 opcode,u32 arg,unsigned int *opflag,int *datadir)
{
	unsigned int flag=0;

	switch(opcode)
	{
		case MMC_GO_IDLE_STATE:  /* bc     */
			flag = MMC_RSP_SPI_R1 | MMC_CMD_BC | MMC_RSP_NONE;
			if(arg == 0xFFFFFFFA)	
				*datadir = MMC_DATA_READ;
			break;

		case MMC_SEND_OP_COND:
			flag = MMC_RSP_SPI_R1 | MMC_CMD_BCR |MMC_RSP_R3;
			break;

		case MMC_ALL_SEND_CID:
			flag = MMC_CMD_BCR |MMC_RSP_R2;
			break;

		case MMC_SET_RELATIVE_ADDR:	// D_SEND_RELATIVE_ADDR
			if(sd_mode)
				flag = MMC_RSP_R6 | MMC_CMD_BCR;
			else 
				flag = MMC_RSP_R1 | MMC_CMD_AC;
			break;

		case MMC_SET_DSR:
			flag = MMC_CMD_BC | MMC_RSP_NONE;
			break;
		
		case MMC_SLEEP_AWAKE:
			flag = MMC_RSP_R1B | MMC_CMD_AC; //only for select one specify device
			break;

		case MMC_SWITCH:		  /* ac   [31:0] See below   R1b */
			flag = MMC_CMD_AC |MMC_RSP_R1B;
			break;

		case MMC_SELECT_CARD:
			flag = MMC_RSP_R1B | MMC_CMD_AC; //only for select one specify device
			break;

		case MMC_SEND_EXT_CSD:	// SD_SEND_IF_COND
			if(sd_mode){
				flag = MMC_RSP_SPI_R7 | MMC_RSP_R7 | MMC_CMD_BCR;
			}
			else{
				flag = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
				*datadir = MMC_DATA_READ;
			}
			break;

		case MMC_SEND_CSD:
			flag = MMC_RSP_R2 | MMC_CMD_AC;
			*datadir = MMC_DATA_READ;
			break;

		case MMC_SEND_CID:
			flag = MMC_RSP_R2 | MMC_CMD_AC;
			*datadir = MMC_DATA_READ;
			break;

		case MMC_READ_DAT_UNTIL_STOP:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC; //need to double check this one, never used
			*datadir = MMC_DATA_READ;
			break;

		case MMC_STOP_TRANSMISSION:
			flag = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
			break;

		case MMC_SEND_STATUS:
			flag = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;
			break;

		case MMC_GO_INACTIVE_STATE:
			flag = MMC_CMD_AC;//need to double check this one
			break;

		case MMC_SPI_READ_OCR:
			flag = MMC_RSP_SPI_R3;
			break;

		case MMC_SPI_CRC_ON_OFF:
			flag = MMC_RSP_SPI_R1;
			break;

		case MMC_SET_BLOCKLEN:
			flag = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;
			break;

		case MMC_READ_SINGLE_BLOCK:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			*datadir = MMC_DATA_READ;
			break;
		
		case MMC_READ_MULTIPLE_BLOCK:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			*datadir = MMC_DATA_READ;
			break;
		
		case MMC_BUS_TEST_R:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			*datadir = MMC_DATA_READ;
			break;

		case MMC_SEND_TUNING_BLOCK_HS200:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			*datadir = MMC_DATA_READ;		
			break;

		case MMC_WRITE_DAT_UNTIL_STOP:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC; //need to double check this one, never used
			*datadir = MMC_DATA_WRITE;
			break;

		case MMC_SET_BLOCK_COUNT:
			flag = MMC_RSP_R1 | MMC_CMD_AC;//need to double check this one
			break;

		case MMC_WRITE_BLOCK:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			*datadir = MMC_DATA_WRITE;
			break;

		case MMC_WRITE_MULTIPLE_BLOCK:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			*datadir = MMC_DATA_WRITE;
			break;
	
		case MMC_BUS_TEST_W:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			*datadir = MMC_DATA_WRITE;
			break;
		
		case MMC_PROGRAM_CID:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;//need to double check this one
			break;

		case MMC_PROGRAM_CSD:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			break;

		case MMC_SET_WRITE_PROT:
			flag = MMC_RSP_R1B | MMC_CMD_AC;//need to double check this one
			break;

		case MMC_CLR_WRITE_PROT:
			flag = MMC_RSP_R1B | MMC_CMD_AC;//need to double check this one
			break;

		case MMC_SEND_WRITE_PROT:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			break;

		case MMC_ERASE_GROUP_START:
			flag = MMC_RSP_R1 | MMC_CMD_AC;
			break;

		case MMC_ERASE_GROUP_END:
			flag = MMC_RSP_R1 | MMC_CMD_AC;
			break;

		case MMC_ERASE:
			flag = MMC_RSP_R1B | MMC_CMD_AC;
			break;

		case MMC_FAST_IO:
			flag = MMC_RSP_R4 | MMC_CMD_AC;
			break;

		case MMC_GO_IRQ_STATE:
			flag = MMC_RSP_R5 | MMC_CMD_BCR;
			break;

		case MMC_LOCK_UNLOCK:
			flag = MMC_RSP_R1B | MMC_CMD_ADTC;
			*datadir = MMC_DATA_WRITE;
			break;

		case MMC_APP_CMD:
			flag = MMC_RSP_R1 | MMC_CMD_AC;
			break;

		case MMC_GEN_CMD:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			*datadir = MMC_DATA_READ;
			break;
		
		case MMC_VEN_CMD60:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			*datadir = MMC_DATA_WRITE;
			break;

		case MMC_VEN_CMD61:
			flag = MMC_RSP_R1 | MMC_CMD_ADTC;
			*datadir = MMC_DATA_READ;
			break;

		case MMC_VEN_CMD62:
			flag = MMC_RSP_SPI_R1 | MMC_CMD_BC | MMC_RSP_NONE;
			break;

		case MMC_VEN_CMD63:
			flag = MMC_RSP_SPI_R1 | MMC_CMD_BC | MMC_RSP_NONE;
			break;

		case SD_APP_OP_COND:
			flag = MMC_RSP_SPI_R1 | MMC_RSP_R3 | MMC_CMD_BCR;
			break;
		
		case SD_ERASE_WR_BLK_START:
			flag = MMC_RSP_R1 | MMC_CMD_AC;
			break;
			
		case SD_ERASE_WR_BLK_END:
			flag = MMC_RSP_R1 | MMC_CMD_AC;
			break;
	}

	*opflag = flag;
	return 0;
}

static int mmc_send_cmd_data(struct mmc_host *mmc,struct emmc_sndcmd_req *req)
{
	int err = 0;
	struct mmc_request mrq;
	struct mmc_command sbc;
	struct mmc_command cmd;
	struct mmc_data data;
	struct mmc_command stop;

	struct scatterlist sg;
	uint32_t time = 0;

	uint32_t fpga_tran_time_ns;
	uint32_t fpga_stop_time_ns;

	memset(&mrq, 0, sizeof(struct mmc_request));
  	memset(&sbc, 0, sizeof(struct mmc_command));
  	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));
	memset(&stop, 0, sizeof(struct mmc_command));

	cmd.opcode = req->opcode;
  	cmd.arg= req->argument;
	cmd.retries = 0;
	mmc_fill_cmd_flag(cmd.opcode, cmd.arg, &(cmd.flags), &(data.flags) );
	mrq.cmd = &cmd;

	if ( (cmd.opcode == MMC_READ_MULTIPLE_BLOCK) || (cmd.opcode == MMC_WRITE_MULTIPLE_BLOCK)) {
		if ((req->flag & FLAG_AUTOCMD12) == FLAG_AUTOCMD12 ) {	// open-end
			stop.opcode = MMC_STOP_TRANSMISSION;
			stop.arg = 0;
			stop.flags = MMC_RSP_R1B | MMC_CMD_AC;
			mrq.stop = &stop;
		} 
		else{													// close-end
			sbc.opcode = MMC_SET_BLOCK_COUNT;
			sbc.arg = req->len/blksz;
			sbc.flags = MMC_RSP_R1 | MMC_CMD_AC;
			mrq.sbc = &sbc;
		}
	}

	if(req->len != 0){
		if( (data.flags != MMC_DATA_READ) && (data.flags != MMC_DATA_WRITE) ){
			printk("MMC: data.flags error[%08x]\n", data.flags);
			err = -EINVAL;
			goto err_out;
		}

		if( (data.flags == MMC_DATA_WRITE) 
			&& ((req->flag & FLAG_NO_DATA_COPY) != FLAG_NO_DATA_COPY) )
		{ 
			err = copy_from_user(buf, req->data, req->len);
			if (err) 
				goto err_out;
		}

		mrq.data = &data;

		data.blksz = blksz;
		data.blocks = req->len/blksz;
		data.sg = &sg;
		data.sg_len = 1;

		#if 1	// FIXME FPGA need 512 Byte aligned buf, although we only need 128 when HS200 tuning
				// We need extra buf to meet align requirement.
				// Here our drv bonce buf is 8MB, it should be ok.
		if(cmd.opcode == MMC_SEND_TUNING_BLOCK_HS200){
			data.blksz = 128;
			data.blocks = 1;
		}
		#endif

		sg_init_one(&sg, buf, req->len);
	}
  
	mmc_wait_for_req(mmc, &mrq);

#if 0	// debug
	if(req->len != 0) {
		if ((req->flag & FLAG_AUTOCMD12) == FLAG_AUTOCMD12 ) {	// open-end
			printk("resp[%08x] for cmd[%d] arg[%08x]\n", cmd.resp[0], cmd.opcode, cmd.arg);
			printk("resp[%08x] for cmd[%d] arg[%08x]\n", stop.resp[0], stop.opcode, stop.arg);
		}
		else{
			printk("resp[%08x] for cmd[%d] arg[%08x]\n", sbc.resp[0], sbc.opcode, sbc.arg);
			printk("resp[%08x] for cmd[%d] arg[%08x]\n", cmd.resp[0], cmd.opcode, cmd.arg);
		}
	}
#endif

	req->resp[0]=cmd.resp[0];
	req->resp[1]=cmd.resp[1];
	req->resp[2]=cmd.resp[2];
	req->resp[3]=cmd.resp[3];

	if (sbc.error || cmd.error || data.error || stop.error) {
		err = -EIO;
		goto err_out;
	}

	// hardware time
	if( (req->len != 0) && (req->opcode != MMC_SEND_EXT_CSD) ) {
		if ((req->flag & FLAG_AUTOCMD12) == FLAG_AUTOCMD12 ) {	// open-end
			xilinx_get_time_dbg(NULL,NULL,NULL,NULL,NULL,NULL,
								&fpga_tran_time_ns,
								&fpga_stop_time_ns);	

			time = (fpga_tran_time_ns + fpga_stop_time_ns) / 1000;
		}
		else{													// close-end
			xilinx_get_time_dbg(NULL,NULL,NULL,NULL,NULL,NULL,
								&fpga_tran_time_ns,
								NULL);	

			time = (fpga_tran_time_ns) / 1000;
		}
	}
	
	if( req->len != 0) { 
		if( (data.flags == MMC_DATA_READ)
				&& ( (req->flag & FLAG_NO_DATA_COPY) != FLAG_NO_DATA_COPY) ) 
		{
	  		sg_copy_to_buffer(&sg, 1,buf,req->len);
			err = copy_to_user(req->data,buf,req->len);
			if(err)
				goto err_out;
		}
	}

	req->time = time;

err_out:
	return err;
}

long emmc_test_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct emmc_sndcmd_req req;
	struct config_req req_flag;
	struct mmc_host *mmc;
	void __user *argp = (void __user *)arg;
	int err = 0;
	struct misc *p_misc = NULL;
  	mmc = xilinx_global_mmc;

	switch (cmd) {
		case EMMC_IOC_SNDCMD:
			if(( err = copy_from_user(&req, argp, sizeof(struct emmc_sndcmd_req))) ) {
				printk(KERN_ERR "MMC: copy_from_user() err %d\n", err);
				return -EFAULT;
			}

			if(req.len > BUF_SIZE) {
				printk(KERN_ERR "MMC: chunck size limit %08x Byte\n", BUF_SIZE);
				return -EINVAL;
			}

			if(( err = mmc_send_cmd_data(mmc, &req) )){
  				printk(KERN_ERR "MMC: mmc_send_cmd_data() err %d\n", err);
				return -EFAULT;
  			}

			if(( err = copy_to_user(argp, &req, sizeof(struct emmc_sndcmd_req))) ) {
				printk(KERN_ERR "MMC: copy_to_user() err %d\n", err);
				return -EFAULT;
			}
			
			break;

		case EMMC_IOC_CONFIG_REQ:
			if(( err = copy_from_user(&req_flag, argp, sizeof(struct config_req)) )) {
				printk(KERN_ERR "MMC: copy_from_user() err %d\n", err);
				return -EFAULT;
			}

			if(req_flag.flag == REQ_ADDRESS_MODE){
				// leonwang add 2014-12-17 to avoid using mmc_card->state
				req_flag.data = ( xilinx_global_access == MMC_STATE_BLOCKADDR ) ?
									REQ_ADDRESS_MODE_SEC : REQ_ADDRESS_MODE_BYTE ;

				if(( err = copy_to_user(argp,&req_flag,sizeof(struct config_req)))){
					printk(KERN_ERR "MMC: copy_to_user() err %d\n", err);
					return -EFAULT;
				}
			}
			else if(req_flag.flag == CONFIG_CLK){
				mmc->ios.clock = (req_flag.data) * 1000000;
				mmc->ops->set_ios(mmc, &(mmc->ios) );
			}
			else if(req_flag.flag == CONFIG_IO_NUM){
				mmc->ios.bus_width = req_flag.data;
				mmc->ops->set_ios(mmc, &(mmc->ios) );
			}
			else if(req_flag.flag == CONFIG_MODE_HS_SDR){
				err = xilinx_prepare_hs_sdr(mmc, req_flag.data, req_flag.bus_width);
			}
			else if(req_flag.flag == CONFIG_MODE_HS_DDR){
				err = xilinx_prepare_hs_ddr(mmc, req_flag.data, req_flag.bus_width);
			}
			else if(req_flag.flag == CONFIG_MODE_HS200){
				err = xilinx_prepare_hs200(mmc, req_flag.data);
			}
			else if(req_flag.flag == CONFIG_MODE_HS400){
				err= xilinx_prepare_hs400(mmc, req_flag.data);
			}
			else{
				printk(KERN_ERR "MMC: req_flag.flag[%d] is undefined\n", req_flag.flag);
				return -ENOTTY;
			}

			break;

		case EMMC_IOC_DUMP_REG:
			p_misc = (struct misc *)argp;
			err = xilinx_dump_reg(mmc_priv(mmc), p_misc);
			break;
		default:
			printk(KERN_ERR "MMC: unknown ioctl cmd %08x\n", cmd);
			return -ENOTTY;
	}

	return err;
}


static int mmc_open(struct inode *inode, struct file *file)
{
	if(!xilinx_global_mmc){
		printk("MMC: initalization has failed..\n");
		return -EINVAL;
	}

	mmc_claim_host(xilinx_global_mmc);
	return 0;
}


static int mmc_close(struct inode *inode, struct file *file)
{
	if(!xilinx_global_mmc){
		printk("MMC: initalization has failed..\n");
		return -EINVAL;
	}

	mmc_release_host(xilinx_global_mmc);
	return 0;
}


struct file_operations emmc_test_operations = {
	.unlocked_ioctl = emmc_test_ioctl,
	.open = mmc_open,
	.release = mmc_close,
	.owner = THIS_MODULE,
};


/*
 * Power control char driver
 */

long mmc_power_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct mmc_host *mmc;

	struct {
		int value;
		int which;
	} *p_arg;
	
	p_arg = (void *)arg;
  	mmc = xilinx_global_mmc;

	switch (cmd)
	{
		case EMMC_POWER_ON:
			xilinx_emmc_power_on(mmc, p_arg->value, p_arg->which);
			break;

		case EMMC_POWER_OFF:
			xilinx_emmc_power_off(mmc, p_arg->value, p_arg->which);
			break;
			
		default: 
			printk(KERN_ERR "MMC power: ioctl error\n");
			err = -ENOTTY;
			break;
	}

	return err;
}

static int mmc_power_open(struct inode *inode, struct file *file)
{
	if(!xilinx_global_mmc){
		printk("MMC: initalization has failed..\n");
		return -EINVAL;
	}

	return 0;
}


static int mmc_power_close(struct inode *inode, struct file *file)
{
	if(!xilinx_global_mmc){
		printk("MMC: initalization has failed..\n");
		return -EINVAL;
	}
	return 0;
}

struct file_operations emmc_power_operations = {
	.unlocked_ioctl = mmc_power_ioctl,
	.open = mmc_power_open,
	.release = mmc_power_close,
	.owner = THIS_MODULE,
};

static int __init init_mmcchar(void)
{
	int result;
	dev_t mmc_devno; 
	dev_t mmc_power_devno;

	//-----------	
	mmc_devno = MKDEV(mmc_major, 0);
	result = register_chrdev_region(mmc_devno, 1, "mmcdev");
	if (result < 0) {
		printk("register_chrdev_region() error %d\n", result);
		return result;
	}

	cdev_init(&cdev, &emmc_test_operations);
	cdev.owner = THIS_MODULE;
	result = cdev_add(&cdev, mmc_devno, 1);
	if (result < 0) {
		printk("cdev_add() error %d\n", result);
		return result;
	}

	//-----------	
	mmc_power_devno = MKDEV(mmc_power_major,0);
	result = register_chrdev_region(mmc_power_devno, 1, "mmcpowerdev");
	if (result < 0) {
		printk("register_chrdev_region() error %d\n", result);
		return result;
	}

	cdev_init(&power_cdev, &emmc_power_operations);
	power_cdev.owner = THIS_MODULE;
	cdev_add(&power_cdev, mmc_power_devno, 1);
	if (result < 0) {
		printk("cdev_add() error %d\n", result);
		return result;
	}

#if 0
	buf = kmalloc(BUF_SIZE, GFP_KERNEL);
#else
	buf = dma_alloc_coherent(xilinx_global_mmc->parent, BUF_SIZE, &dma_addr, GFP_KERNEL );
#endif
	if (!buf)
		return -ENOMEM;

	printk("mmc char driver init finish.\n");

	return 0;
}

static void __exit cleanup_mmcchar(void)
{
	if(buf)	
#if 0
		kfree(buf);
#else
		dma_free_coherent(xilinx_global_mmc->parent, BUF_SIZE, buf, dma_addr);
#endif

	cdev_del(&cdev);
	unregister_chrdev_region(MKDEV(mmc_major, 0), 1); 

	cdev_del(&power_cdev);
	unregister_chrdev_region(MKDEV(mmc_power_major, 0), 1); 


	printk("mmc char driver exit finish.\n");
}

module_param(sd_mode, int, S_IRUGO | S_IWUSR);

module_init(init_mmcchar);
module_exit(cleanup_mmcchar);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jack/Youxin");
