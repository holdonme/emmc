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
	printf("Usage: cmd16 -a ARG\n\n"); 
	printf("Options:\n"); 
	printf("\t-a\tARG\tset block length (unit byte)\n");
}


int main(int argc, char * const argv[])
{
	int c;
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
	int blk_len = 0;

	while(optind<argc){
		c = getopt(argc, argv,"a:h");
		switch(c){
			case 'a': 
			  blk_len = lib_valid_digit(optarg);
			  break;
			case '?':
			default:
			  help_menu();
			  return -1;
		}
	}

	if(blk_len == 0){
		printf("-a option required!\n");
		return -1;
	}

	if( flashops->open() < 0)
		return -1;

	lib_build_cmd(&mmc_cmd,16,blk_len,NULL,NULL,0,0,0);

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;

	r1_res = lib_r1_check(mmc_cmd.restoken);

	if(r1_res.error){
		printf("Cmd16 failed. Error code: 0x%x\n",r1_res.error);
	}
	printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));

	flashops->close();

	return 0;	
}
