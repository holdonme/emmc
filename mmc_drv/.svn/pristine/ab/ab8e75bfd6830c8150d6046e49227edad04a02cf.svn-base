/*
 * EMMC header shared by kernel and user space.
 *
 */
#ifndef __MMC_USER_H__
#define __MMC_USER_H__

#define EMMC_IOC_MAGIC		'E'
#define EMMC_IOC_SNDCMD 	_IOWR(EMMC_IOC_MAGIC, 0, struct emmc_sndcmd_req)
#define EMMC_IOC_CONFIG_REQ _IOWR(EMMC_IOC_MAGIC, 2, struct config_req)
#define EMMC_IOC_DUMP_REG   _IOWR(EMMC_IOC_MAGIC, 3, void *)

#define EMMC_POWER_MAGIC	'P'
#define EMMC_POWER_ON       _IOWR(EMMC_POWER_MAGIC, 0, void *) 
#define EMMC_POWER_OFF      _IOWR(EMMC_POWER_MAGIC, 1, void *)

#define FLAG_NO_DATA_COPY	0x800 //ADD BY FRED FOR NO DATA COPY AT 2014-08-12
#define FLAG_CMD23			0x400 //add by leonwang for close-end at 2014-09-11
#define FLAG_AUTOCMD12      0x200 //modify by leonwang 2014-09-11

#define MAX_NUM_LEN_SG 		0x10000

#define MMC_VEN_CMD60 60
#define	MMC_VEN_CMD61 61
#define	MMC_VEN_CMD62 62
#define	MMC_VEN_CMD63 63

struct emmc_sndcmd_req {
	uint32_t opcode;          // emmc command set	
	uint32_t argument;        // op argument	
	uint32_t resp[4];         //response token data from EMMC
	uint8_t  *data;           // the pointer of data	
	uint32_t len;             // length of data 	
	uint32_t time;            // time of the operation
	uint32_t flag;            // used for read/write auto cmd12
} __attribute__ ((packed));

struct config_req{
	uint32_t flag;
#define REQ_ADDRESS_MODE   0
#define REQ_DENSITY        1
#define CONFIG_CLK         2
#define CONFIG_IO_NUM      3

#define CONFIG_MODE_HS200  4
#define CONFIG_MODE_HS400  5
#define CONFIG_MODE_HS_SDR 6
#define CONFIG_MODE_HS_DDR 7

	uint32_t data;
#define REQ_ADDRESS_MODE_SEC  1
#define REQ_ADDRESS_MODE_BYTE 2
	uint32_t bus_width;
} __attribute__ ((packed));

#endif
