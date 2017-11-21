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
extern int optind, opterr, optopt;

struct user_arg arg={0,0,0,0,0,0xC0FF8080,0};

void help_menu()
{
	printf("Usage: cmd1 [-a ARG]\n\n");
	printf("Options:\n");
	printf("\t-a\tARG\targument (default 0xC0FF8080)\n");
}

int main(int argc, char * const argv[])
{
	int c;
	int ocr;
	struct emmc_sndcmd_req mmc_cmd;
	uint32_t argument = 0xC0FF8080;
	int fail_flag = 0;

	while(optind<argc){
		c = getopt(argc, argv,"a:h");

		switch(c){
			case 'a':
			  argument = lib_valid_digit(optarg);
			  break;

			case 'h':
			default:
			  help_menu();
			  return -1;
		}
	}

	if( flashops->open() < 0)
		return -1;

	lib_build_cmd(&mmc_cmd,1,argument,NULL,NULL,0,0,0);

	BEGINTIMER;

	while(1){
		if(flashops->cmd(&mmc_cmd)<0)
			return -1;

		GETTIMER;

		printf("Cmd1 response: 0x%08x\n",mmc_cmd.restoken[0]);
		ocr = mmc_cmd.restoken[0];

		if(ocr == 0xffffffff){
			fail_flag=1;
			printf("cmd1 no response 0x%x.\n",ocr);
			break;
		}

		if(ocr&0x80000000){
			break;
		}

		if(DIFFTIMERUS>CMD1_TIMEOUT*1000){
			fail_flag=1;
			printf("cmd1 timeout.\n");
			break;
		}
		usleep(1000);
	}

	PRINTTIMER;

	flashops->close();

	if(fail_flag){
		printf("cmd1 fail.\n");
		return -1;
	}  

	return 0;	
}
