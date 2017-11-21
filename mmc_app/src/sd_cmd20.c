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
	printf("Usage: cmd20 [-s|d|c]\n\n");
	printf("\t-s\tStart recording\n");
	printf("\t-d\tUpdate DIR\n");
	printf("\t-c\tUpdate CI\n");
}

int main(int argc, char * const argv[])
{
	int c;
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
	uint32_t argument = 0xdead;

	while(optind<argc){
		c = getopt(argc, argv,"sdc");
		switch(c){
			case 's':
				argument = 0x0 << 28; 
				break;

			case 'd':
				argument = 0x1 << 28; 
				break;

			case 'c':
				argument = 0x4 << 28; 
				break;

			default:
			  help_menu();
			  return -1;
		}
	}

	if(argument == 0xdead){
		help_menu();
		return -1;
	} 

	if(flashops->open()<0)
		return -1;

	lib_build_cmd(&mmc_cmd,20, argument,NULL,NULL,0,0,0);

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;

	r1_res=lib_r1_check(mmc_cmd.restoken);

	if(r1_res.error){
		printf("Cmd20 failed. Error code: 0x%08x\n",r1_res.error);
		return -1;
	}

	flashops->close();

	return 0;	
}
