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

void help_menu(void)
{
	printf("Switch to 1.8V bus singling level\n");	
	printf("Usage: sd_cmd11\n\n");
}

int main(int argc, char * const argv[])
{
	int c;
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;

	while(optind<argc){
		c = getopt(argc, argv,"h");
		switch(c){
			case 'h':
			default:
			  help_menu();
			  return -1;
		}
	}

	if(flashops->open()<0)
		return -1;

	lib_build_cmd(&mmc_cmd,11, 0,NULL,NULL,0,0,0);

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;

	r1_res=lib_r1_check(mmc_cmd.restoken);

	if(r1_res.error){
		printf("Cmd11 failed. Error code: 0x%08x\n",r1_res.error);
		return -1;
	}

	printf("Response status 0x%08x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));

	flashops->close();

	return 0;	
}
