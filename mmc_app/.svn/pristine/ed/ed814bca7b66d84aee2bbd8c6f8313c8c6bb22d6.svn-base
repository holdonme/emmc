#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "flash_define.h"
#include "flash_lib.h"
#include "flash_ops.h"
#include "mmc-user.h"

#define CHUNK_SIZE   2*1024		// 2M
#define STORE_SECTOR 524288

uint8_t * do_flash_read(uint start_addr,uint total_size)
{
  int fd;
  uint8_t *ret = NULL;
  struct emmc_sndcmd_req mmc_cmd;
  struct config_req req;
  struct r1 r1_res;
  struct block_data bdata;
  
  fd=flashops->open();
  if(fd<0){
    printf("FLASHAPP_ERROR:open failure.\n");
	ret = NULL;
	goto err;
  }

  req.flag=REQ_ADDRESS_MODE; 
  if(flashops->req(&req)<0){
  	printf("FLASHAPP ERR: cmd18 req failure.\n");
	ret = NULL;
	goto err;
  }
  
  bdata.saddr=start_addr;
  bdata.len=total_size;
  bdata.len*=SEC_SIZE;
  if(req.data==REQ_ADDRESS_MODE_BYTE){
 	bdata.saddr*=SEC_SIZE;
  }
  
  bdata.pdata=(uint8_t*)malloc(sizeof(uint8_t)*bdata.len);
  memset(bdata.pdata, 0, sizeof(uint8_t)*bdata.len);
  
  lib_build_cmd(&mmc_cmd,18,bdata.saddr,NULL,bdata.pdata,bdata.len,0,(FLAG_AUTOCMD12|FLAG_NOCMDTIME));

  if(flashops->cmd(&mmc_cmd)<0){
  	printf("FLASHAPP ERR: cmd18 failure.\n");
  	if(bdata.pdata) 
		free(bdata.pdata);
	ret = NULL;
	goto err;
  }
  r1_res=lib_r1_check(mmc_cmd.restoken);
  
  if(r1_res.error){
  	printf("cmd18 failed. Error code: 0x%x\n",r1_res.error);
	ret = NULL;
	goto err;
  }

  ret = bdata.pdata;
 
err: 
  flashops->close();

  return ret;
}


/* 
 * start_addr ---- block address,  used as argument for CMD25
 * total_size ---- buffer length, unit block 
 * chunk_size ---- chunk size,    unit block 
 * value      ---- buffer value, uint byte
 */

int do_flash_write(int start_addr,int total_size,int chunk_size,char value)
{
	int fd;
	int i;
	int ret = 0;
	struct emmc_sndcmd_req mmc_cmd;
	struct config_req req;
	struct r1 r1_res;
	struct block_data bdata;

	bdata.pdata = NULL;

	if( total_size % chunk_size != 0 )
		return 1;

	fd=flashops->open();
	if(fd<0){
		printf("FLASHAPP_ERROR:open failure.\n");
		ret = 1;
		goto err;
	}

	req.flag=REQ_ADDRESS_MODE;
	if(flashops->req(&req)<0){
		printf("FLASHAPP ERR: cmd18 req failure.\n");
		ret = 1;
		goto err;
	}

	bdata.saddr=start_addr;
	bdata.len=chunk_size;
	bdata.len*=SEC_SIZE;
	if(req.data==REQ_ADDRESS_MODE_BYTE)
	   bdata.saddr*=SEC_SIZE;

	bdata.pdata=(uint8_t*)malloc(bdata.len);

	// init chunk buf
	for(i=0; i<bdata.len; i++){
		*(bdata.pdata + i) = value;
	}
	
	// write chunk buf to flash, one chunk each time 
	for(i=0;i<total_size/chunk_size;i++) { 
		lib_build_cmd(&mmc_cmd,25,bdata.saddr,NULL,bdata.pdata,bdata.len,0,(FLAG_AUTOCMD12|FLAG_NOCMDTIME)); 
		
		if(flashops->cmd(&mmc_cmd)<0) { 
			printf("FLASHAPP_ERROR: cmd25 failure.\n"); 
			ret = 1;
			goto err;
		} 
		
		r1_res=lib_r1_check(mmc_cmd.restoken); 
		if(r1_res.error) { 
			printf("Cmd25 failed. Error code: 0x%x\n",r1_res.error); 
			ret = 1;
			goto err;
		} 
		
		if(req.data==REQ_ADDRESS_MODE_BYTE) 
			bdata.saddr +=(chunk_size*SEC_SIZE); 
		else 
			bdata.saddr +=(chunk_size);
	}

err:
	flashops->close();

	if(bdata.pdata)
		free(bdata.pdata);

	return ret;
}

void powerloss_task(int count, int mode, int delay_us, int speed_us, float volt)
{
	int i=0, looper=0;
	int recheck=0;

	int ret = 0;
	FILE *fp = NULL;
	uint8_t pattern, pattern_2;
	uint8_t chunk_index, chunk_index_2;
	uint8_t *pdata,*tmp;
	char cmd_buf[100];

	printf("Loop for %d times\n", count);

	while(looper<count){ //main loop

		// choose a chunk (index 0-255) to test, chunk unit is 2M
		// and store index value into STORE_SECTOR block
		srandom(time(NULL) + getpid());
		chunk_index = (random() % 255);
		do_flash_write(STORE_SECTOR,1,1,chunk_index);
		printf("Chunk index: %d\n",chunk_index);
	
	
		// choose pattern for the chunk, 
		// and store pattern value into STORE_SECTOR+1 block
		// rewrite the chunk with differen pattern
		printf("Record data pattern and overwrite...\n");
		if(looper%2!=0)
			pattern = 0x55;
		else
			pattern = 0xaa;

		do_flash_write(STORE_SECTOR+1,1,1,pattern); // record the data pattern
		do_flash_write(chunk_index*CHUNK_SIZE,CHUNK_SIZE,CHUNK_SIZE,0xff - pattern); //override the chunk 

		if(mode == 2)
			sprintf(cmd_buf, "power_loss_config -d %d -s %d", delay_us, speed_us);
		else if(mode == 3)
			sprintf(cmd_buf, "power_loss_config -d %d -v %f", delay_us, volt);
		
		System(cmd_buf);	

		ret = do_flash_write(chunk_index*CHUNK_SIZE,CHUNK_SIZE,CHUNK_SIZE,pattern);
		if(ret)
			printf("In power loss test, write error\n");
		else
			printf("In power loss test, write over\n");

		sleep(2);
		run_power_up();
		System("mmcinit");
		System("mmcconfig -s");

	
		// read the chunk index and pattern from STORE_SECTOR / STORE_SECTOR+1 block
		pdata=do_flash_read(STORE_SECTOR,1);
		chunk_index_2 = (int)*pdata;
		free(pdata);

		pdata=do_flash_read(STORE_SECTOR+1,1);
		pattern_2 = (int)*pdata;
		free(pdata);
		printf("Last power loss occured at chunk index:%d, data pattern: 0x%x\n", chunk_index_2, pattern_2);

		printf("Checking data ...\n");
		
		for(recheck=0; recheck<20; recheck++){	
			pdata=do_flash_read(chunk_index_2*CHUNK_SIZE, CHUNK_SIZE);
			for(i=0,tmp=pdata;i<CHUNK_SIZE*512;i++,tmp++){
				if((*tmp != 0xaa) && (*tmp != 0x55) ){
					fp = fopen("/out.bin", "w+");
					if(!fp){
						printf("Fail open file\n");
						goto out;
					}
					fwrite(pdata,2048*512,1,fp);
					fclose(fp);
					printf("Data corrupt at offset 0x%x in chunk %d,recheck:%d,recod in out.bin\n", i, chunk_index_2, recheck);
					goto out;
				}
			}
			free(pdata);
		}
		printf("No corruption found\n");
		looper++;
		printf("\n\n############################################################## Loop: %d\n", looper);
		continue;

	out:	
		free(pdata);
		break;
	}
}


void help_menu(void)
{
	printf("Usage: mstar -d NUM -s NUM [-n NUM]\n");
	printf("       mstar -d NUM -v NUM [-n NUM]\n");
	printf("Options:\n"); 
	printf("-d : delay time after first CMD12 (unit us)\n");
	printf("-s : power loss speed time (unit us)\n");
	printf("-v : power loss volt, float num (unit  V)\n");
	printf("-n : count of the loop, default 1\n");
}


int main(int argc, char *argv[])
{
	int count=1;
	int c;
	int mode = 2;
	int delay_us = 0;
	int speed_us = 0;
	float volt = 0;

	while(optind<argc){
	   	c = getopt(argc, argv,"d:s:v:n:h");
	    switch(c){
        case 'd': 
            delay_us = lib_valid_digit(optarg);
            break;

        case 's': 
            speed_us = lib_valid_digit(optarg);
			mode = 2;
            break;

		case 'v': 
			if( sscanf(optarg, "%f",&volt) != 1){
				printf("input error [%s], should be a number\n", optarg);
				return -1;
			}

			mode = 3;
			break;

		case 'n':
			count = lib_valid_digit(optarg);
			break;

		case 'h':
		default:
			help_menu();
			return -1;
    	}
  	}

	System("mmcinit");
	System("mmcconfig -s");

	powerloss_task(count, mode, delay_us, speed_us, volt);

	return 0;
}
