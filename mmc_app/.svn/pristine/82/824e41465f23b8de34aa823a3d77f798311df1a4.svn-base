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
#include <errno.h>

#include "flash_define.h"
#include "flash_lib.h"
#include "flash_ops.h"
#include "mmc-user.h"

#define TEST_SIZE	(1024 * 1024 * 1024)	// 1GB
#define BUF_SIZE    (1024 * 1024 )			// 1MB,   2048 blocks
#define CHUNK_SIZE  (1024 *  256)			// 256KB,  512 blocks

// Init buf, block_buf 0 with pattern 0, block_buf 1 with pattern 1 ...
void init_buf_with_pattern(uint8_t *buf, int len)
{
	int block_addr = 0;
    uint32_t *block = NULL;
	int i = 0;
	
    for( block_addr = 0; block_addr < (len/SEC_SIZE); block_addr++){
        block = (uint32_t *)(buf + block_addr * SEC_SIZE);
        for(i = 0; i < SEC_SIZE/4; i++) {
            block[i] = block_addr;
        }
    }
}

// Init buf, block_buf 0 with pattern ~0, block_buf 1 with pattern ~1 ...
void init_buf_with_reverse_pattern(uint8_t *buf, int len)
{
	int block_addr = 0;
    uint32_t *block = NULL;
	int i = 0;
	
    for( block_addr = 0; block_addr < (len/SEC_SIZE); block_addr++){
        block = (uint32_t *)(buf + block_addr * SEC_SIZE);
        for(i = 0; i < SEC_SIZE/4; i++) {
            block[i] = ~block_addr;
        }
    }
}

// Check buf, block_buf 0 with pattern 0 or ~0, block_buf 1 with pattern 1 or ~1 ...
int check_buf_with_pattern(uint8_t *buf, int len, int *err_block_addr)
{
	int block_addr = 0;
    uint32_t *block = NULL;
	int i = 0;

	int block_pattern;	//  or ~0 , 1 or ~1 ...

	for( block_addr = 0; block_addr < len/SEC_SIZE; block_addr++){
		block = (uint32_t *)(buf + block_addr * SEC_SIZE);

		if( (block[0] != block_addr) && (block[0] != ~block_addr) )
			goto error;

		block_pattern = block[0];

		for( i = 0; i < SEC_SIZE/4; i++) {
			if(block[i] != block_pattern) 
				goto error;
		}
	}

	return 0;

error:
	
	printf("compare error @ block 0x%x offest 0x%x, expect 0x%08x or 0x%08x, actual 0x%08x\n", 
									block_addr, i*4, block_addr, ~block_addr, block[i]);
	*err_block_addr = block_addr;

	return -1;
}

int huawei_pl_task(int count, char *cmd_buf, uint8_t *write_buf, uint8_t *read_buf, int len)
{
	int ret = 0;
	int block_addr = 0;
	uint32_t *block = NULL;
	int m = 0;
	int i = 0;
	int delay = 0;

	pid_t pid;

	// Init buf, block_buf 0 with pattern ~0, block_buf ~1 with pattern ~1 ...
	init_buf_with_reverse_pattern(write_buf, len);

	printf("\n\n********* [index %d] STEP_1. power-loss when write flash ********\n\n", count);

	pid = Fork();
	if(pid == 0){	
		// In child thread
		printf("In child thread\n");
		while(1){
			ret = flash_write(0, write_buf, len, CHUNK_SIZE);
			if(ret)
				break;		
		}

		_exit(0);
	}

	// In parent thread		
	srandom(time(NULL) + getpid());
	delay = random() % 10;
	select_timer(delay*1000);	// N ms
	System(cmd_buf);
	printf("Parent thread delay %d ms to run power loss\n", delay);
	wait(NULL);

	sleep(1);

	System("power -a 1");

	printf("\n\n********* [index %d] STEP_2. power-loss when run ACMD41 ********\n\n", count);

	System("cmd0");
	System("sd_cmd8");

	pid = Fork();
	if( pid == 0){
		//In child thread
		printf("In child thread\n");
		ret = system("sd_acmd41");
		if(ret)
			printf("In child thread, sd_acmd41 error!\n");
		else
			printf("In child thread, sd_acmd41 over\n");

		_exit(0);
	}

	// In parent thread
	select_timer( 1000 );	// FIXME 1ms
	System("power -a 0");
	wait(NULL);

	sleep(1);

	System("power -a 1");

	printf("\n\n********* [index %d] STEP_3. power-up and check flash data ********\n\n", count);

	System("sd_init");

	// read 512KiB data and compare 
	memset(read_buf, 0, len);
	ret = flash_read(0, read_buf, len, CHUNK_SIZE);
	if(ret)
		return -1;

	ret = check_buf_with_pattern(read_buf, len, &block_addr);
	if(ret)
		goto compare_err;

	printf("compare ok\n");

	return 0;


compare_err: 
	printf("actual block data dump from BLOCK[%d]\n", block_addr); 
	block = (uint32_t *)(read_buf + block_addr * SEC_SIZE);
	for (i = 0; i < SEC_SIZE/4; i++) {
		if((i%4) == 0)
			printf("\n"); 
		printf(" 0x%08x,", block[i]); 
	}
	printf("\n"); 

	//repeat read 3 times after compare fail
	printf("repeat read 3 times after compare fail\n");
	for(m =0; m<3;m++) {   
		printf("repeat read %d start, block data dump from BLOCK[%d]\n",m, block_addr);

		memset(read_buf, 0, len);
		ret = flash_read(0, read_buf, len, CHUNK_SIZE);
		if(ret) 
			return -1;

		block = (uint32_t *)(read_buf + block_addr * SEC_SIZE);
		for (i = 0; i < SEC_SIZE/4; i++) {   
			if((i%4) == 0)  
				printf("\n");
			printf(" 0x%08x,", block[i]); 
		} 
		printf("\n");
	}

	return -1;
}

int init_flash_data(uint8_t *write_buf, int len)
{
    int ret = 0;
	int block_addr = 0;
	int i = 0;

	// Init buf, block_buf 0 with pattern 0, block_buf 1 with pattern 1 ...
	init_buf_with_pattern(write_buf, len); 

	// Init flash test area, block 0 with pattern 0, block 1 with pattern 1 ...
	for( block_addr = 0, i = 0; i < (TEST_SIZE/len); i++) {
    	ret = flash_write(block_addr, write_buf, len, CHUNK_SIZE);
		if(ret != 0)
			return ret;

		block_addr += (len/512);
	}

	return ret;
}


void help_menu(void)
{
	printf("Usage: sd_huawei -d NUM -s NUM [-n NUM]\n");
	printf("       sd_huawei -d NUM -v NUM [-n NUM]\n");
	printf("Options:\n"); 
	printf("-d : delay time after CMD12 (unit us)\n");
	printf("-s : power loss speed time  (unit us)\n");
	printf("-v : power loss volt, float num (unit V)\n");
	printf("-n : count of the loop, default 1\n");
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int c;

	int mode = 2;
	int delay_us = 0;
	int speed_us = 0;
	float volt = 0;
	char cmd_buf[100];

	uint8_t *write_buf = NULL;
	uint8_t *read_buf = NULL;

	int count = 1;
	int i = 0;

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

	System("power -a 1");
	System("sd_init");

	switch(mode){
	case 2:
		sprintf(cmd_buf, "power_loss_config -d %d -s %d", delay_us, speed_us);
		break;
	case 3:
		sprintf(cmd_buf, "power_loss_config -d %d -v %f", delay_us, volt);
		break;
	default:
		printf("error mode %d\n", mode);
		return -1;
	}

	write_buf = malloc(BUF_SIZE);
	read_buf = malloc(BUF_SIZE);
	
	ret = init_flash_data(write_buf, BUF_SIZE);
	if(ret) {
		printf("init flash data fail\n");
		goto out;
	}

	for(i = 0; i < count; i++) {
		ret = huawei_pl_task(i, cmd_buf, write_buf, read_buf, BUF_SIZE);	
		if(ret){
			printf("Powerloss test fail in count[%d]\n", i);
			goto out;
		}
	}

out:
	free(write_buf);
	free(read_buf);

	return ret;
}
