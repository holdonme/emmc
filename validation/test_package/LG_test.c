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
#include <pthread.h>

#include "flash_define.h"
#include "flash_lib.h"
#include "flash_ops.h"
#include "mmc-user.h"


int WRITE_PL_ADDR = 0;
int WRITE_PL_SIZE = 0;
unsigned int POWER_FLAG = 1;


extern char *optarg;
extern int optind;

static uint32_t getRandNum(){
	srand((unsigned)time(NULL));
	return(rand());
}

void help_menu(char *name)
{
	printf("Usage: %s -n Loop_Count [-h]\n", name);
	printf("-n : Test Loop count\n");
	printf("-h : help list\n\n");
}

void * pattern1_write(void * arg) {
	unsigned int i = 0;
	unsigned int ZoneCnt = 8;
	char CmdBuf[128];
	unsigned int pattern = (int)(*(int*)arg);

	/* 64MB zone = 128K sectors*/
	unsigned int ZoneSize = 0x20000; 

	unsigned int StartAddr = 0;
	unsigned int EndAddr = 0;
	unsigned int start = 0;
	unsigned int end = 0;
	
	while(1) {
		fflush(stdout);
		EndAddr = StartAddr + 7;
		if(EndAddr <= ZoneSize) {
			for (i = 0; i != ZoneCnt; i++) {
				start = StartAddr + ZoneSize * i;
				end = start + 7;
				sprintf(CmdBuf, "cmd25 -s %d -e %d -d %d", start, end, pattern);
				system(CmdBuf);
				if(!POWER_FLAG) {
					printf("write failure @ 0x%x!\n", start);
					WRITE_PL_ADDR = start;
					WRITE_PL_SIZE = 8;
					fflush(stdout);
					return;
				}
				else
					printf("8 sectors write to addr: 0x%x\n", start);
			}
		}
		else
			goto LEFTSPACE_WRITE;
		
		StartAddr += 8;
		EndAddr = StartAddr + 63;
		if(EndAddr <= ZoneSize) {
			for (i = 0; i != ZoneCnt; i++) {
				start = StartAddr + ZoneSize * i;
				end = start + 63;
				sprintf(CmdBuf, "cmd25 -s %d -e %d -d %d", start, end, pattern);
				system(CmdBuf);
				if(!POWER_FLAG) {
					printf("write failure @ 0x%x!\n", start);
					WRITE_PL_ADDR = start;
					WRITE_PL_SIZE = 64;
					fflush(stdout);
					return;
				}
				else
					printf("64 sectors write to addr: 0x%x\n", start);
			}
		}
		else
			goto LEFTSPACE_WRITE;

		StartAddr += 64;
		EndAddr = StartAddr + 511;
		if(EndAddr <= ZoneSize) {
			for (i = 0; i != ZoneCnt; i++) {
				start = StartAddr + ZoneSize * i;
				end = start + 511;
				sprintf(CmdBuf, "cmd25 -s %d -e %d -d %d", start, end, pattern);
				system(CmdBuf);
				if(!POWER_FLAG) {
					printf("write failure @ 0x%x!\n", start);
					WRITE_PL_ADDR = start;
					WRITE_PL_SIZE = 512;
					fflush(stdout);
					return;
				}
				else
					printf("512 sectors write to addr: 0x%x\n", start);
			}
		}
		else
			goto LEFTSPACE_WRITE;

		StartAddr += 512;
	}

LEFTSPACE_WRITE:
	if(StartAddr < ZoneSize) {
		for (i = 0; i != ZoneCnt; i++) {
			start = StartAddr + ZoneSize * i;
			end = ZoneSize*(i+1)-1;
			sprintf(CmdBuf, "cmd25 -s %d -e %d -d %d", start, end, pattern);
			system(CmdBuf);
			if(!POWER_FLAG) {
				printf("write failure @ 0x%x!\n", start);
				WRITE_PL_ADDR = start;
				WRITE_PL_SIZE = end - start + 1;
				fflush(stdout);
				return;
			}
			else
				printf("%d sectors write to addr: 0x%x\n", end - start + 1);
		}
	}
}

void pattern2_write(){
	int StartAddr = 0x100000;
	int EndAddr = 0x200000;
	int start = StartAddr;
	int end = 0;
	int pattern = 0;
	int chunk = 0;
	char CmdBuf[128];
	
	while(start < EndAddr){
		// chunk size is randomly in 4K to 256K
		chunk = (getRandNum() % 506 + 8)>>3;
		chunk = chunk<<3;
		end = start + chunk - 1;
		if (end < EndAddr){
			sprintf(CmdBuf, "cmd25 -s %d -e %d -d %d", start, end, pattern);
			system(CmdBuf);
			pattern = (pattern + 1) & 0xFF;
		}
		else{
			end = EndAddr - 1;
			sprintf(CmdBuf, "cmd25 -s %d -e %d -d %d", start, end, pattern);
			system(CmdBuf);
			break;
		}	
		start = start + chunk;
	}
	return;
}

int main(int argc, char *argv[])
{
	char cmdbuf[128];
	char c;

	uint32_t CurLoop = 0;
	uint32_t LoopCnt = 0;	
	int ret = 0;
	pthread_t thread;
	int delay_us = 0;
	uint32_t PrePattern = 0, CurPattern = 0;

	while(optind < argc) {
		c = getopt(argc, argv, "t:n:ph");
		switch(c) {
		case 'n':
			LoopCnt = lib_valid_digit(optarg);
			break;
		case 'h':
		default:
			help_menu(argv[0]);
			return -1;
		}
	}

	POWER_FLAG = 1;
	System("mmcinit");
	System("mmcconfig -b -f 50 -i 8");
	sleep(5);

	for (CurLoop = 1; CurLoop <= LoopCnt; CurLoop++)
	{
		PrePattern = (CurLoop - 1) & 0xFF;
		CurPattern = CurLoop & 0xFF;
		sprintf(cmdbuf, "cmd25 -s 0 -e 0xfffff -c 0x800 -d 0x%x", PrePattern);
		System(cmdbuf);
		pthread_create(&thread, NULL, pattern1_write, &CurPattern);
		delay_us = (CurLoop % 80 + 1) * 100000;
		select_timer(delay_us);
		POWER_FLAG = 0;
		system("power -a 0");
		system("dump_reg -a 12");
		pthread_join(thread, NULL);

		sleep(2);
		System("power -a 1");
		POWER_FLAG = 1;
		System("mmcinit");
		System("mmcconfig -b -f 50 -i 8");
		
		sprintf(cmdbuf, "./LGPLDataValidate.sh %d %d %d 1", CurPattern, WRITE_PL_ADDR, WRITE_PL_SIZE);
		ret=System(cmdbuf);
		fflush(stdout);
		if(ret)
			return ret;
		
		pattern2_write();
		sprintf(cmdbuf, "./LGPLDataValidate.sh %d %d %d 2", CurPattern, WRITE_PL_ADDR, WRITE_PL_SIZE);
		ret=System(cmdbuf);
		fflush(stdout);
		if(ret)
			return ret;
	}
	return ret;
}
