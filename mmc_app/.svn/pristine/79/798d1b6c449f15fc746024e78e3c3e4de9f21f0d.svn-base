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


void help_menu()
{
	printf("Usage: cmd32 -a arg\n\n");
}


int main(int argc, char * const argv[])
{
	int fd;
	int c;
	struct emmc_sndcmd_req mmc_cmd;
	uint32_t arg = 0;
	struct r1 r1_res;

	while(optind<argc){
		c = getopt(argc, argv,"a:h");
		switch(c){
		case 'a':                               //arg 
			arg = lib_valid_digit(optarg); 
			break; 
		default: 
			help_menu(); 
			return -1;
		}
	}
  
	fd=flashops->open();
	if(fd<0){
		printf("FLASHAPP_ERROR:open failure.\n"); 
		return -1;
	}


	lib_build_cmd(&mmc_cmd,32,arg,NULL,NULL,0,0, 0);


	if(flashops->cmd(&mmc_cmd)<0){ 
		printf("FLASHAPP_ERROR: cmd32 failure.\n"); 
		return -1;
	}

	r1_res=lib_r1_check(mmc_cmd.restoken);

	if(r1_res.error){ 
		printf("cmd32 failed. Error code: 0x%x\n",r1_res.error); 
		return -1;
	}

	printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));

	flashops->close();

	return 0;	
}
