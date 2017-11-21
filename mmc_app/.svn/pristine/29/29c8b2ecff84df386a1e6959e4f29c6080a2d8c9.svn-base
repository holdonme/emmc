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
	printf("Usage: sd_acmd6 -r RCA -a ARG\n\n");
	printf("Options:\n");
	printf("\t-r\tRCA\targument\n");
	printf("\t-a\tARG\targument\n");
}

int main(int argc, char * const argv[])
{
	int c;
	struct emmc_sndcmd_req mmc_app;
	struct emmc_sndcmd_req mmc_cmd;
	uint32_t rca = 0;
	uint32_t argument = 0x0;
	struct r1 r1_res;

	while(optind<argc){
		c = getopt(argc, argv,"r:a:h");

		switch(c){
			case 'r': 
				rca = lib_valid_digit(optarg); 
				break;

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

	lib_build_cmd(&mmc_app,55, rca, NULL,NULL,0,0,0);
	lib_build_cmd(&mmc_cmd, 6, argument,   NULL,NULL,0,0,0);

	if(flashops->cmd(&mmc_app)<0)
		return -1;

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;


	r1_res=lib_r1_check(mmc_cmd.restoken);

	if(r1_res.error) {
		printf("sd_acmd6 failed. Error code: 0x%x\n",r1_res.error);
		return -1;
	}

	printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));

	flashops->close();

	return 0;	
}
