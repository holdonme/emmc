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

#define MODE_PRINT    0
#define MODE_ONE_REG  1

extern char *optarg;
extern int optind;

void help_menu()
{ 
	printf("Usage: cmd8 [-r NUM]\n\n"); 
	printf("Options:\n"); 
	printf("\t-r\tNUM \tspecify reg addr (0~511)\n"); 
	printf("\t-a\tARG\targument (default 0)\n");
}


int main(int argc, char * const argv[])
{
	int c;
	int reg_addr = 0;
	int mode = MODE_PRINT;

	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
	uint32_t argument = 0;

	uint8_t *buf = NULL;

	while(optind<argc){
		c = getopt(argc, argv,"a:r:h");
		switch(c){
		case 'a':
			argument = lib_valid_digit(optarg); 
			break;

		case 'r':                           //address
			mode = MODE_ONE_REG;
			reg_addr = lib_valid_digit(optarg);
			if( reg_addr < 0 || reg_addr > 511){
				printf("argument for `-r` should be 0~511\n");
				return -1;
			}
			break;

		case 'h':
		default:
			help_menu();
			return -1;
		}
	}

  
	if(flashops->open()<0)
		return -1;

	buf = malloc(SEC_SIZE);
	memset(buf, 0, SEC_SIZE);

	lib_build_cmd(&mmc_cmd, 8, argument,NULL, buf, SEC_SIZE,0,0);

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;

	r1_res=lib_r1_check(mmc_cmd.restoken);

	if(r1_res.error){
		printf("Cmd8 failed. Error code: 0x%x\n",r1_res.error);
		return -1;
	}

	printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));

	flashops->close();

	switch(mode){
		case MODE_PRINT:
			dump_buf(buf, SEC_SIZE);
			printf("\n");
			lib_decode_xcsd(buf);
			break;

		case MODE_ONE_REG:
			printf("\nADDR     VALUE\n");
			printf("%04d     0x%02x\n", reg_addr, buf[reg_addr] );
			break;

		default:
			break;
	}

	free(buf);

	return 0;	
}
