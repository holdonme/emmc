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
	printf("Usage: cmd15 [-a ARG]\n\n"); 
	printf("Options:\n"); 
	printf("\t-a\tARG\targument (default 0x00010000)\n");
}

int main(int argc, char * const argv[])
{
	int c;
	struct emmc_sndcmd_req mmc_cmd;
	uint32_t argument = 0x00010000;

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

	lib_build_cmd(&mmc_cmd,15,argument,NULL,NULL,0,0,0);

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;

	flashops->close();

	return 0;	
}
