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

#define BUF_SIZE (512 * 1024)
#define CHUNK_SIZE (256 * 1024)

void help_menu(void)
{
	printf("Usage: samsung -n CNT\n");
	printf("Options:\n"); 
	printf("-n : the times of the loop\n");
}

int main(int argc, char *argv[])
{
	int loop = 0;
	int sec_addr = 0;
	int offset = 0;
	uint32_t *block = NULL;
	uint8_t *buf = NULL;
	uint8_t *buf2 = NULL;
	int ret = 0;
	int delay = 0;
	char c;
	int count = 1;

	pid_t pid;

	while(optind<argc){
	   	c = getopt(argc, argv,"n:h");
	    switch(c){
	        case 'n':                              
	   		count = lib_valid_digit(optarg);
	    	break;

			case 'h':
	    	default:
	    	  	help_menu();
	    	  	return -1;
		}
	}

	buf = malloc(BUF_SIZE);
	buf2 = malloc(BUF_SIZE);

	// 0 init 512KiB data buf 
	for( sec_addr = 0; sec_addr < BUF_SIZE/SEC_SIZE; sec_addr ++){
		block = (uint32_t *)(buf + sec_addr * SEC_SIZE);
		for( offset = 0; offset < SEC_SIZE/4; offset++) {
			block[offset] = sec_addr;
		}
	}

	System("cmd6 -m 3 -a 179 -d 1");
	ret = flash_write(0, buf, BUF_SIZE, CHUNK_SIZE);
	

	//STEP_1. random wirte to get a dirty eMMC card  TODO
	printf("Assume the card is dirty.\n");

	while(loop<count){ //main loop

		//STEP_2. power loss when write user-data partition 
		System("cmd6 -m 3 -a 179 -d 0");
		
		if((pid=fork()) < 0){
			printf("fork error!\n");
		}

		if(pid == 0){
			// In child thread
			printf("In child thread\n");

			ret = flash_write(0, buf, BUF_SIZE, CHUNK_SIZE);
			if(ret)
				printf("In child thread, write error!\n");
			else
				printf("In child thread, write over\n");

			_exit(0);
		}

		// In parent thread
		srandom(time(NULL) + getpid());
		delay = random() % 1000;
		select_timer(delay*100 );

		System("power -a 0");
		printf("In parent thread\n");

		wait(NULL);

		//STEP_3. reset eMMC
		System("power -a 1");

		System("mmcinit");
		
		//STEP_3. switch boot partition and read 
		printf("switch to boot partition\n");
		System("cmd6 -m 3 -a 179 -d 1");
		
		// 05. read 512KiB data and compare 
		ret = flash_read(0, buf2, BUF_SIZE, CHUNK_SIZE);
		if(ret){
			printf("flash read error!\n");
			return -1;
		}

		loop++;
		printf("####################################################################### Loop: %d\n", loop);
	}

	free(buf);
	free(buf2);

	return 0;
}
