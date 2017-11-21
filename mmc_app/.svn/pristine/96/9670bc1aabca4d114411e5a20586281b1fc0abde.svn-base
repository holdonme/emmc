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

void help_menu()
{ 
	printf("Usage: cmd12 [-a ARG]\n\n"); 
	printf("Options: (check ECSD[503] before used as HPI)\n"); 
	printf("\t-a\tARG\targument (default 0x00010000)\n");
}

int main(int argc, char * const argv[])
{
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
	int c;
	uint32_t argument = 0x00010000;

	while(optind < argc){
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

	lib_build_cmd(&mmc_cmd, 12, argument, NULL,NULL,0,0,0);

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;

	r1_res = lib_r1_check(mmc_cmd.restoken);

	if(r1_res.error){
		printf("Cmd12 failed. Error code: 0x%x\n",r1_res.error);
		return -1;
	}

	printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));

	flashops->close();

	return 0;	
}
