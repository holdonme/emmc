/*******************************************************************************/
/*   File        : flash_define.h                                              */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This is the head file to define all the library macro for   */
/*                 both library code and user application.                     */
/*******************************************************************************/

#ifndef _FLASH_DEFINE_H_
#define _FLASH_DEFINE_H_
#include <sys/time.h>
#include "mmc-user.h"

/*flash device node path + name*/
#define FLASHNOD "/dev/flashdev0"
#define POWERNOD "/dev/mmcpower0" //add by jackwu

/*libaray fail return value*/
#define RFAIL 99999999

/*below define all the cmd code communicate with device driver*/
#define CMD_READ 0
#define CMD_WRITE 1
#define CMD_RSTHIGH 5
#define CMD_RSTLOW 6

/*timer macro control*/
#define BEGINTIMER gettimeofday(&g_begin,NULL)
#define GETTIMER gettimeofday(&g_end,NULL)
#define DIFFTIMERUS (unsigned long)(1000000*(g_end.tv_sec-g_begin.tv_sec)+(g_end.tv_usec-g_begin.tv_usec))
#define PRINTTIMER printf("\nTest time is %.2fs\n",(double)((1000000*(g_end.tv_sec-g_begin.tv_sec)+(g_end.tv_usec-g_begin.tv_usec))/1000000.0))

/*BIT MASK*/
#define BIT0 (0x1 << 0)
#define BIT1 (0x1 << 1)
#define BIT2 (0x1 << 2)
#define BIT3 (0x1 << 3)
#define BIT4 (0x1 << 4)
#define BIT5 (0x1 << 5)
#define BIT6 (0x1 << 6)
#define BIT7 (0x1 << 7)
#define BIT8 (0x1 << 8)
#define BIT9 (0x1 << 9)
#define BIT10 (0x1 << 10)
#define BIT11 (0x1 << 11)
#define BIT12 (0x1 << 12)
#define BIT13 (0x1 << 13)
#define BIT14 (0x1 << 14)
#define BIT15 (0x1 << 15)
#define BIT16 (0x1 << 16)
#define BIT17 (0x1 << 17)
#define BIT18 (0x1 << 18)
#define BIT19 (0x1 << 19)
#define BIT20 (0x1 << 20)
#define BIT21 (0x1 << 21)
#define BIT22 (0x1 << 22)
#define BIT23 (0x1 << 23)
#define BIT24 (0x1 << 24)
#define BIT25 (0x1 << 25)
#define BIT26 (0x1 << 26)
#define BIT27 (0x1 << 27)
#define BIT28 (0x1 << 28)
#define BIT29 (0x1 << 29)
#define BIT30 (0x1 << 30)
#define BIT31 (0x1 << 31)

/*R1 response bit mask*/
#define R1_ERROR (BIT7|BIT13|BIT14|BIT15|BIT16|BIT17|BIT18|BIT19|BIT20|BIT21|BIT22|BIT23|BIT24|BIT26|BIT27|BIT28|BIT29|BIT30|BIT31)
#define R1_STS (BIT9|BIT10|BIT11|BIT12)


/*bit mask*/
#define DQ0 BIT0
#define DQ1 BIT1
#define DQ2 BIT2
#define DQ3 BIT3
#define DQ4 BIT4
#define DQ5 BIT5
#define DQ6 BIT6
#define DQ7 BIT7

/*MMC define*/
#define SEC_SIZE 512


/*cmd defination*/
#define CMD_1ST_ADDR   0x555
#define CMD_1ST_DATA   0xAA
#define CMD_2ND_ADDR   0x2AA
#define CMD_2ND_DATA   0x55
#define CMD_AUTO_DATA  0x90
#define CMD_RESET_DATA 0xF0
#define CMD_CFI_ADDR   0x55
#define CMD_CFI_DATA   0x98
#define CMD_ERASE_DATA_80   0x80
#define CMD_ERASE_DATA_30   0x30

#define GO_IDLE_STATE 0
#define GO_PRE_IDLE_STATE 0xf0f0f0f0
#define BOOT_INITIATION 0xfffffffa


/*unit for block_data*/
#define UNIT_NORMAL 1
#define UNIT_BLK 2
#define AREA_MAIN 1
#define AREA_CFI 2
#define AREA_AUTOSEL 3

/*array size defination, different from akaishi and h33*/ 
#define MAXBLK  512
#define BLKSIZE 0x20000
#define BLK2ADDR(x) (x*BLKSIZE)

#define MAXCE  1

/*convert the addr to blk number*/
#define ADDRTOBLK(addr) (unsigned int)(addr/BLKSIZE)

/*time out value defination, value need to be checked*/
#define ERS_TIMEOUT 4000
#define PGM_TIMEOUT 50
#define BC_TIMEOUT 50
#define CMD1_TIMEOUT 10000
//#define CRC_TIMEOUT 17000

/*max buffer size*/
#define MAXBUFSIZE 512

/*read max fail count for print*/
#define MAXFAILCNT 32

/*input data pattern path + name defination*/
#define DPATPATH "/bin/mmc_test/pat/pat.bin"
#define DPATH "/bin/mmc_test/pat/"

/*output data pattern path + name defination*/
#define DOUTPATPATH "/bin/mmc_test/pat/pat_out.bin"

/*PATEND for struct blkPattern*/
#define PATEND 0xffffffff

/*crc64 gen*/
#define cnCRC_64_H  0x42F0E1EB    
#define cnCRC_64_L  0xA9EA3693


/*define the level for basic pattern*/
#define LEVEL0 101
#define LEVEL1 102
#define LEVEL2 103
#define LEVEL3 104
#define L0L1CHK 0x11111111
#define L0L2CHK 0x22222222
#define L0L3CHK 0x33333333
#define L1L2CHK 0x44444444
#define L1L3CHK 0x55555555
#define L2L3CHK 0x66666666
#define L0L1CHKINV 0xeeeeeeee
#define L0L2CHKINV 0xdddddddd
#define L0L3CHKINV 0xcccccccc
#define L1L2CHKINV 0xbbbbbbbb
#define L1L3CHKINV 0xaaaaaaaa
#define L2L3CHKINV 0x99999999
#define LEVELCKB 0x1000000//used to check if the pattern is check board


#define MODE_PRINT    0
#define MODE_CONSTCMP 1
#define MODE_PATCMP   2
#define MODE_PATWRITE 3
#define MODE_QUIET    4

#define MODE_CONSTWT  5
#define MODE_PATWT    6
#define MODE_FILEWT   7 


typedef unsigned int uint;
typedef unsigned char uint8_t;

/*structure defination used for communication with device driver*/
struct strData{
  uint data;
  uint addr;
  uint ce;
};

/*blk pattern index for crc calculation*/
struct blkPattern{
  uint startBlk;
  uint stopBlk;
  uint patIndex;//1-pbi normal, 2-pbi 32blk, 3-pbi last32blk
};

//flash cfi item info
struct flash_cfi_item{
  char name[8];
  uint saddr; //start addr
  uint eaddr; //end addr
  uint* value;
};

//store the flash part level info
struct flash_info{
  uint flash_size;//flash array size
  uint ublk_size;//normal blk size
  uint ublk_number;//normal blk 
  uint pblk_size;
  struct flash_cfi_item *cfi_first;
};

//block data definition
struct block_data{
  uint saddr;
  uint len;
  uint8_t* pdata;
  uint8_t cdata;
  uint chunk;
};

struct block_region{
  uint sblk;
  uint eblk;
};

//flash operation defination
struct flash_ops{
  int (*open) (void);
  int (*close) (void);
  int (*pu) (void);
  int (*req)(struct config_req* req);
  int (*cmd) (struct emmc_sndcmd_req* cmd);
  int (*read) (struct block_data* bdata, int open_end, int time_flag, u_int32_t sec_mode);
  int (*randread) (uint8_t* data, uint* addr, int size, uint chunk, u_int32_t sec_mode);
  int (*write) (struct block_data* bdata, int open_end, int time_flag, u_int32_t sec_mode);
  int (*randwrite) (uint8_t* data, uint* addr, int size, uint chunk, u_int32_t sec_mode);
  int (*dump_reg) (void *);
};

struct power_ops{
  int (*open) (void);
  int (*close) (void);
  int (*set_power_on) (int volt, int which);
  int (*set_power_off) (int speed_us, int which);
};

//user struct for input arg
struct user_arg{
  uint sdata; //start 
  uint edata; //stop
  uint data_unit; //blk address or normal address
  uint area; //data area
  uint mode; //operation mode
  uint special; //data input
  uint flag; //flag data
};

/*struct for r1 response*/
struct r1{
  uint sts;
  uint error;
};


#endif                      
