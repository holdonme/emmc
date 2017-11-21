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
	printf("Usage: cmd2\n\n");
}


int main(int argc, char * const argv[])
{
	int c;
	int i;
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
  
	if(flashops->open() < 0)
		return -1;

	lib_build_cmd(&mmc_cmd,2,0,NULL,NULL,0,0,0);

	if(flashops->cmd(&mmc_cmd) <0 )
		return -1;

	for(i=0;i<4;i++)
		printf("CID response[%d] 0x%08x\n",i,mmc_cmd.restoken[i]);

	lib_decode_cid(mmc_cmd.restoken);

	flashops->close();

	return 0;	
}
