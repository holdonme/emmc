/*
 * EMMC header shared by kernel and user space.
 *
 */
#ifndef __MMC_USER_H__
#define __MMC_USER_H__

#define EMMC_IOC_MAGIC      'E'
#define EMMC_IOC_SNDCMD     _IOWR(EMMC_IOC_MAGIC, 0, struct emmc_sndcmd_req)
#define EMMC_IOC_CONFIG_REQ _IOWR(EMMC_IOC_MAGIC, 2, struct config_req)
#define EMMC_IOC_DUMP_REG   _IOWR(EMMC_IOC_MAGIC, 3, void *)

#define EMMC_POWER_MAGIC    'P'
#define EMMC_POWER_ON       _IOWR(EMMC_POWER_MAGIC, 0, void* ) 
#define EMMC_POWER_OFF      _IOWR(EMMC_POWER_MAGIC, 1, void* ) 

#define EMMC_CMD_MAXNUM		63
#define EMMC_RESTOKEN_MAXLEN 4

#define MMC_BUS_WIDTH_1		0
#define MMC_BUS_WIDTH_4		2
#define MMC_BUS_WIDTH_8		3

#define FLAG_NO_DATA_COPY   0x800   	//add by Fred
#define FLAG_NOCMDTIME 		0x80000000	//add by jackwu
#define FLAG_CMD23			0x400		//add by leonwang for close-end 2014-09-11
#define FLAG_AUTOCMD12 		0x200		//modify by leonwang 2014-09-11


struct emmc_sndcmd_req {
	u_int32_t	 opcode;      // emmc command set
	u_int32_t  argument;      // op argument

	u_int32_t  restoken[EMMC_RESTOKEN_MAXLEN];              // response token data from EMMC to user
	
	unsigned char *data;      // the pointer of data
	u_int32_t  len;           // length of data 
	u_int32_t time;	      	  // time of the operation
  	u_int32_t flag;  			  // used for read/write auto cmd12
} __attribute__ ((packed));

struct config_req{
	u_int32_t flag; //indicate which data to be get
#define REQ_ADDRESS_MODE   0
#define REQ_DENSITY 	   1
#define CONFIG_CLK 		   2
#define CONFIG_IO_NUM 	   3
#define CONFIG_MODE_HS200  4	//leon add for emmc mode config 2014-07-28
#define CONFIG_MODE_HS400  5
#define CONFIG_MODE_HS_SDR 6	
#define CONFIG_MODE_HS_DDR 7
	u_int32_t data;
#define REQ_ADDRESS_MODE_SEC  1
#define REQ_ADDRESS_MODE_BYTE 2
	u_int32_t bus_width;
} __attribute__ ((packed));



struct misc {
	int option;
	int value[10];
};

#endif /* __MMC_USER_H__ */
