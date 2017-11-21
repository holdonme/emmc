/*******************************************************************************/
/*   File        : flash_ops.c                                                 */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This file provide the all the flash function for the user   */
/*                 application code.                                           */
/*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <malloc.h>


#include "flash_define.h"
#include "flash_lib.h"
#include "mmc-user.h"

/*subs*/
static int ops_open(void);
static int ops_close(void);
static int ops_cmd(struct emmc_sndcmd_req* cmd);
static int ops_read(struct block_data* bdata, int open_end, int time_flag, u_int32_t sec_mode);
static int ops_read_random(uint8_t* data, uint* addr, int size, uint chunk, u_int32_t sec_mode);
static int ops_write(struct block_data* bdata, int open_end, int time_flag, u_int32_t sec_mode);
static int ops_write_random(uint8_t* data, uint* addr, int size, uint chunk, u_int32_t sec_mode);
static int ops_config_req(struct config_req* req);
static int ops_dump_reg(void *);


/*emmcops is the struct for user application code to use, define all the flash operation*/
const struct flash_ops emmcops = {
	.open=ops_open,
	.close=ops_close,
	.req=ops_config_req,
	.read=ops_read,
	.randread=ops_read_random,
	.write=ops_write,
	.randwrite=ops_write_random,
	.cmd=ops_cmd,
	.dump_reg=ops_dump_reg,
};

const struct flash_ops *flashops = &emmcops;

static int ops_dump_reg(void *misc)
{
	if(ioctl(g_fd,EMMC_IOC_DUMP_REG,misc) < 0) {
		lib_error("FLASHLIB ERROR: in ops_dump_reg(), open flash dev fail",__FILE__,__LINE__);
		return -1;
	}
	return 0;
}

/*******************************************************************************/
/*   Function    : ops_open                                                    */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used for open the flash device.            */
/*******************************************************************************/
static int ops_open()
{
	g_fd = open(FLASHNOD,O_RDWR);

	if(g_fd<0){
		lib_error("FLASHLIB ERROR: in ops_open(), open flash dev fail",__FILE__,__LINE__);
		return -RFAIL;
	}
	return 0;
}

/*******************************************************************************/
/*   Function    : ops_close                                                   */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used for close the flash device.           */
/*******************************************************************************/
static int ops_close()
{
	return close(g_fd);
}

/*******************************************************************************/
/*   Function    : ops_cmd                                                     */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used to send general cmd to driver.        */
/*******************************************************************************/
static int ops_cmd(struct emmc_sndcmd_req* cmd)
{
	if(ioctl(g_fd,EMMC_IOC_SNDCMD,cmd) < 0) {
		return -RFAIL;
	}

	return 0;
}

/*******************************************************************************/
/*   Function    : ops_read                                                    */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used to read the data.                     */
/*   Note:         bdata len, chunk and saddr is in byte address.              */
/*******************************************************************************/
static int ops_read(struct block_data* bdata, int open_end, int time_flag, u_int32_t sec_mode)
{
	struct emmc_sndcmd_req cmd;
	struct block_data t_bdata;
	uint s_addr=bdata->saddr;
	uint offset=0;
	uint left=bdata->len;//len is in byte mode

	while(left){
		t_bdata.saddr=s_addr;
		if(left>=bdata->chunk){
			t_bdata.len=bdata->chunk;
			left-=bdata->chunk;
		}
		else{
			t_bdata.len=left;
			left=0;
		}

		t_bdata.pdata=&(bdata->pdata[offset]);

		if(sec_mode == REQ_ADDRESS_MODE_SEC)
			lib_build_cmd(&cmd,18,t_bdata.saddr/SEC_SIZE,NULL,t_bdata.pdata,t_bdata.len,0,open_end);
		else
			lib_build_cmd(&cmd,18,t_bdata.saddr,NULL,t_bdata.pdata,t_bdata.len,0,open_end);
		
		if(flashops->cmd(&cmd)<0){
			lib_error("FLASHLIB ERROR: in ops_read(), Call cmd failed",__FILE__,__LINE__);
			return -RFAIL;
 		}

		if(time_flag)
	  		printf("read time 0x%x-0x%x: %d us\n",s_addr/SEC_SIZE,(s_addr+(bdata->chunk)-1)/SEC_SIZE,cmd.time);

		s_addr+=bdata->chunk;
		offset+=bdata->chunk;
	}
	
	return 0;
}

/*******************************************************************************/
/*   Function    : ops_read_random                                             */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2011-08-31                                                  */
/*   Description : This function is used to random read the data with          */
/*                 a predefined the sequence, the addr array should be init    */
/*                 by user.                                                    */
/*   Note:         bdata len, chunk and saddr is in byte address.              */
/*******************************************************************************/
static int ops_read_random(uint8_t* data, uint* addr, int size, uint chunk, u_int32_t sec_mode)
{
	struct emmc_sndcmd_req cmd;
	uint s_addr;
	int i;
	if(!data){
		lib_error("FLASHLIB ERROR: in ops_read_random(), data array is NULL",__FILE__,__LINE__);
  		return -RFAIL;
	}
	
	if(!addr){
		lib_error("FLASHLIB ERROR: in ops_read_random(), addr array is NULL",__FILE__,__LINE__);
  		return -RFAIL;
	}
	
	for(i=0;i<size;i++){
		s_addr=addr[i];

		if(sec_mode == REQ_ADDRESS_MODE_SEC)
			lib_build_cmd(&cmd,18,s_addr/SEC_SIZE,NULL,data,chunk,0,FLAG_AUTOCMD12);
		else
			lib_build_cmd(&cmd,18,s_addr,NULL,data,chunk,0,FLAG_AUTOCMD12);
		
		if(flashops->cmd(&cmd)<0){
  			lib_error("FLASHLIB ERROR: in ops_read_random(), Call cmd failed",__FILE__,__LINE__);
  			return -RFAIL;
 		}
		printf("read time 0x%x-0x%x: %d us\n",s_addr/SEC_SIZE,(s_addr+chunk-1)/SEC_SIZE,cmd.time);
	} 
	
	return 0;
}

/*******************************************************************************/
/*   Function    : ops_write                                                   */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2011-08-31                                                  */
/*   Description : This function is used to write the data.                    */
/*   Note:         bdata len, chunk and saddr is in byte address.              */
/*******************************************************************************/
static int ops_write(struct block_data* bdata, int open_end, int time_flag, u_int32_t sec_mode){
	struct emmc_sndcmd_req cmd;
	struct block_data t_bdata;
	uint s_addr=bdata->saddr;
	uint offset=0;
	uint left=bdata->len;//len is in byte mode

	while(left){
		t_bdata.saddr=s_addr;
		if(left>=bdata->chunk){
			t_bdata.len=bdata->chunk;
			left-=bdata->chunk;
		}
		else{
			t_bdata.len=left;
			left=0;
		}
		t_bdata.pdata=&(bdata->pdata[offset]);

		if(sec_mode == REQ_ADDRESS_MODE_SEC)
			lib_build_cmd(&cmd,25,t_bdata.saddr/SEC_SIZE,NULL,t_bdata.pdata,t_bdata.len,0,open_end);
	  	else
			lib_build_cmd(&cmd,25,t_bdata.saddr,NULL,t_bdata.pdata,t_bdata.len,0,open_end);
		
		if(flashops->cmd(&cmd)<0){
			lib_error("FLASHLIB ERROR: in ops_write(), Call cmd failed",__FILE__,__LINE__);
			return -RFAIL;
 		}
		if(time_flag)
	  		printf("write time 0x%x-0x%x: %d us\n",s_addr/SEC_SIZE,(s_addr+(bdata->chunk)-1)/SEC_SIZE,cmd.time); 
		
		s_addr+=bdata->chunk;
		offset+=bdata->chunk; 
	} 
	
	return 0;
}


/*******************************************************************************/
/*   Function    : ops_write_random                                            */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2011-08-31                                                  */
/*   Description : This function is used to random write the data with         */
/*                 a predefined the sequence, the addr array should be init    */
/*                 by user.                                                    */
/*   Note:         bdata len, chunk and saddr is in byte address.              */
/*******************************************************************************/
static int ops_write_random(uint8_t* data, uint* addr, int size, uint chunk, u_int32_t sec_mode)
{
	struct emmc_sndcmd_req cmd;
	uint s_addr;
	int i;
	if(!data){
		lib_error("FLASHLIB ERROR: in ops_write_random(), data array is NULL",__FILE__,__LINE__);
  		return -RFAIL;
	}
	
	if(!addr){
		lib_error("FLASHLIB ERROR: in ops_write_random(), addr array is NULL",__FILE__,__LINE__);
  		return -RFAIL;
	}
	
	for(i=0;i<size;i++){
		s_addr=addr[i];

		if(sec_mode == REQ_ADDRESS_MODE_SEC)
			lib_build_cmd(&cmd,25,s_addr/SEC_SIZE,NULL,data,chunk,0,FLAG_AUTOCMD12);
	  	else
			lib_build_cmd(&cmd,25,s_addr,NULL,data,chunk,0,FLAG_AUTOCMD12);
		
		if(flashops->cmd(&cmd)<0){
			lib_error("FLASHLIB ERROR: in ops_write_random(), Call cmd failed",__FILE__,__LINE__);
			return -RFAIL;
 		} 
		printf("write time 0x%x-0x%x: %d us\n",s_addr/SEC_SIZE,(s_addr+chunk-1)/SEC_SIZE,cmd.time);
  	} 
	
	return 0;
}

/*******************************************************************************/
/*   Function    : ops_config_req                                              */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2011-08-01                                                  */
/*   Description : This function is used to get mmc card info from kernal.     */
/*******************************************************************************/
static int ops_config_req(struct config_req* req)
{
	if(ioctl(g_fd,EMMC_IOC_CONFIG_REQ,req) < 0) {
		lib_error("FLASHLIB ERROR: in ops_config_req(), Call ioctl failed",__FILE__,__LINE__);
		return -RFAIL;
	}

	return 0;
}


/*
 * Power Loss control function
 */


int g_pd;

static int power_open()
{
	g_pd = open(POWERNOD,O_RDWR);

	if(g_pd<0){
		lib_error("FLASHLIB ERROR: in ops_open(), open flash dev fail",__FILE__,__LINE__);
		return -RFAIL;
	}
	return 0;
}

static int power_close()
{
	return close(g_pd);
}

static int power_on(int voltage_x_10, int which)
{ 
	struct {
		int value;
		int which;
	} arg;

	arg.value = voltage_x_10;
	arg.which = which;

	return ioctl(g_pd,EMMC_POWER_ON, &arg); 
}

static int power_off(int speed_us, int which)
{ 
	struct {
		int value;
		int which;
	} arg;

	arg.value = speed_us;
	arg.which = which;

	return ioctl(g_pd,EMMC_POWER_OFF, &arg); 
}

const struct power_ops powerops = 
{
	.open      		= power_open,
	.close     		= power_close,
	.set_power_on  	= power_on,
	.set_power_off 	= power_off,
};

const struct power_ops *power = &powerops;
