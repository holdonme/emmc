#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#include "flash_define.h"
#include "flash_lib.h"
#include "flash_ops.h"
#include "mmc-user.h"

extern char *optarg;
extern int optind;

// use 128 Byte pattern data
uint32_t tuning_block_128[] = {
   0xff00ffff, 0x0000ffff, 0xccccffff, 0xcccc33cc,
   0xcc3333cc, 0xffffcccc, 0xffffeeff, 0xffeeeeff,
   0xffddffff, 0xddddffff, 0xbbffffff, 0xbbffffff,
   0xffffffbb, 0xffffff77, 0x77ff7777, 0xffeeddbb,
   0x00ffffff, 0x00ffffff, 0xccffff00, 0xcc33cccc,
   0x3333cccc, 0xffcccccc, 0xffeeffff, 0xeeeeffff,
   0xddffffff, 0xddffffff, 0xffffffdd, 0xffffffbb,
   0xffffbbbb, 0xffff77ff, 0xff7777ff, 0xeeddbb77
};

void help_menu(void)
{
	printf("Usage: cmd21 [-av]\n");
	printf("\t-a\targ (default 0)\n");
	printf("\t-v\tshow tuning buf from device\n");
}


int main(int argc, char * const argv[])
{
	int fd;
	int c;
	int verbose = 0;
	struct emmc_sndcmd_req cmd;
	uint8_t *buf = NULL;
	uint32_t argument = 0;

	while(optind<argc){
		c = getopt(argc, argv,"a:vh");
		switch(c){
		case 'a':
			argument = lib_valid_digit(optarg); 
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
		default: 
			help_menu(); 
			return -1;
		}
	}

	if(flashops->open()<0)
		return -1;

	buf = malloc(SEC_SIZE);	// FIXME FPGA need 512 Byte aligned buf, we use 512 Byte as DMA buffer
							// Buf we only use first 128 Byte

	lib_build_cmd(&cmd, 21, argument, NULL, buf, 128, 0, 0);
	
	if(flashops->cmd(&cmd)<0)
		return -1;

	if(verbose)
		dump_buf(buf, 128);

	if(memcmp(buf, tuning_block_128, sizeof(tuning_block_128))){
		printf("tuning fail...\n");
		return -1;
	}

	printf("tuning OK!\n");

	flashops->close();

	close(fd);

	return 0;	
}
