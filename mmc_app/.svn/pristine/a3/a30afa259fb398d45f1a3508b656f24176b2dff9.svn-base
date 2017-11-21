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
extern int optind ;

void help_menu()
{ 
	printf("Usage: cmd7 [-a ARG]\n\n"); 
	printf("Options:\n"); 
	printf("\t-a\tARG\targument (default 0x00010000)\n");
}


int main(int argc, char * const argv[])
{
	int c;
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
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

	lib_build_cmd(&mmc_cmd,7,argument,NULL,NULL,0,0,0);

	if( flashops->cmd(&mmc_cmd) < 0)
		return -1;
	
	if(mmc_cmd.restoken[0] == 0){
		printf("cmd7 deselect card\n");
		return 0;
	}
	
	r1_res=lib_r1_check(mmc_cmd.restoken);

	printf("cmd7 response: 0x%08x, status 0x%x %s\n",mmc_cmd.restoken[0], r1_res.sts,lib_decode_status(r1_res.sts));

	if(r1_res.error){
		printf("cmd7 failed. Error code: 0x%x\n",r1_res.error);
		return -1;
	}

	flashops->close();

	return 0;	
}
