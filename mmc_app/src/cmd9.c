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

struct user_arg arg={0,0,0,0,0,1,0};

void help_menu()
{ 
	printf("Usage: cmd9 [-a ARG]]\n\n");
	printf("Options:\n");
	printf("\t-a\tARG\targment (defualt 0x00010000)\n");
}

int main(int argc, char * const argv[])
{
	int c;
	int i;
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

	lib_build_cmd(&mmc_cmd,9,argument,NULL,NULL,0,0,0);

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;

	for(i=0;i<4;i++)
		printf("CSD response[%d] 0x%x\n",i,mmc_cmd.restoken[i]);

	lib_decode_csd(mmc_cmd.restoken);

	flashops->close();

	return 0;	
}
