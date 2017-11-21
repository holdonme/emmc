#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>

#include "flash_define.h"
#include "flash_lib.h"
#include "flash_ops.h"
#include "mmc-user.h"

extern char *optarg;
extern int optind;

int mmap_config_host(int mode, int width)
{
	uint8_t *reg_base = NULL;
	uint32_t value = 0;
	int fd;

	fd = open("/dev/mem", O_RDWR); 
	if(fd < 0){
		printf("open /dev/mem error\n");
		return -1;
	}

	reg_base = mmap(NULL, 0x80, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x7aa00000);
	//printf("fpga version %08x\n", *(uint32_t *)(reg_base+0x7c) );

	// bit[0]  0-SDR  1-DDR
	if(mode == 0)
		value |= 0x00;
	else
		value |= 0x01;

	// bit[1]  0-400KHz  1-data clock
	value |= 0x02;
	
	// bit[2]  not touch 

	// bit[4:3]  00b->x8  01b -->x1 10b -->x4
	switch(width){
		case 1:
			value |= 0x08;
			break;
		case 4:
			value |= 0x10;
			break;
		case 8:
			value |= 0x00;
			break;
		default:
			return -1;	
	}

	*(uint32_t *)(reg_base + 0x04) = value;

	munmap(reg_base, 0x80);
	
	close(fd);
	return 0;
}

void help_menu()
{
	printf("Usage: cmd0 [-a ARG] [-n CNT] [-m MODE] [-i WIDTH] [-f FILE]\n\n"); 
	printf("Options:\n"); 
	printf("\t-a\tARG\targument (default 0)\n");
	printf("\n");
	printf("\tWhen argument is 0xfffffffa, options below are valid.\n");
	printf("\tPlease confirm ECSD 177, 178, 179 are correct before use them\n");
	printf("\n");
	printf("\t-n\tCNT\tread block count\n");
	printf("\t-m\tMODE\t0-SDR 1-DDR\n");
	printf("\t-i\tWIDTH\t1 or 4 or 8\n");
	printf("\t-f\tFILE\twrite data to file (print by default)\n");
}

int main(int argc, char * const argv[])
{
	struct emmc_sndcmd_req mmc_cmd;
	int arg = 0;
	int c;
	uint8_t *buf = NULL;
	int err = 0;

	char file_path[50];
	int fd = -1;

	int count = 0;
	int mode = -1;	
	int width = -1;

	while(optind<argc){
		c = getopt(argc, argv,"a:n:m:i:f:h");
		switch(c){
			case 'a': 
				arg = lib_valid_digit(optarg); 
				break;
			
			case 'n':
				count = lib_valid_digit(optarg); 
				if(count < 0) {
					printf("Error: option -n invalid\n");
					return -1;
				}

				break;

			case 'm':
				mode = lib_valid_digit(optarg); 
				break;

			case 'i':
				width = lib_valid_digit(optarg); 
				break;

			case 'f':                               //write data to file 
				strcpy(file_path,optarg);

				//create file
				fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC,S_IRWXU);
				if(fd < 0){
					printf("create file error\n");
					return -1;
				}

				break;

			case 'h':
			default: 
				help_menu(); 
				return -1;
		}
	}

	switch(arg){
		case BOOT_INITIATION:
			if( count == 0 ) {
				goto send_cmd;
			}

			if((mode != 0) && (mode != 1)){
				printf("Error: option -m invalid\n");
				return -1;
			}

			if((width != 1)  && (width != 4) && (width != 8)){
				printf("Error: option -i invalid\n");
				return -1;
			}

			if(mmap_config_host(mode, width) < 0)
				return -1;

			buf = malloc(count*512);
			// fall through

		case GO_IDLE_STATE:
		case GO_PRE_IDLE_STATE:
			break;

		default: 
			printf("Error: option -a invalid\n");
			return -1;
	}

send_cmd:
	if((err = flashops->open()) <0)
		goto out;

	lib_build_cmd(&mmc_cmd,0,arg,NULL,buf,count*512,0,0);

	if((err = flashops->cmd(&mmc_cmd)) < 0)
		goto out;

	flashops->close();

	if(arg == BOOT_INITIATION) {
		if( count == 0 ) 
			return 0;

		if( fd < 0) {
			dump_block(buf, count*512, 0);
		}
		else{
			write(fd, buf, count * 512);
			close(fd);
		}
	}

out:
	if(buf)
		free(buf);
	
	return err;	
}
