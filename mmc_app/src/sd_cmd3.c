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
	printf("Usage: sd_cmd3\n\n"); 
}


int main(int argc, char * const argv[])
{
	int c;
	struct emmc_sndcmd_req mmc_cmd;

	while(optind<argc){
		c = getopt(argc, argv,"h");
		switch(c){ 
			case 'h':
			default: 
				help_menu(); 
				return -1; 
		} 
	}

	if( flashops->open() < 0)
		return -1;

	lib_build_cmd(&mmc_cmd,3,0,NULL,NULL,0,0,0);

	if( flashops->cmd(&mmc_cmd) < 0)
		return -1;

	printf("sd_cmd3 response: 0x%08x\n", mmc_cmd.restoken[0]);

	flashops->close();

	return 0;	
}
