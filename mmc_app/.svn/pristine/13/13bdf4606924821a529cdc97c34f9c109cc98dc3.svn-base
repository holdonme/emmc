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
	int i=0;
	printf("Usage: cmd13 [-a ARG] [-p STATUS] [-t TIME]\n\n");
	printf("Options:(check ECSD[503] before used as HPI)\n");
	printf("\t-a\tARG\targument (default 0x00010000)\n");
	printf("\t-p\tNUM\tspecify a status for polling until device goes to this status.\n");

	for(i=0;i<11;i++)  
		printf("\t\t\t%d:%s\n",i,lib_decode_status(i));

	printf("\t-t\tNUM\tspecify polling time out in ms (defualt 1000 ms)\n");
}

int main(int argc, char * const argv[])
{
	int c;
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
	int poll_status = -1;
	unsigned long timeout=1000;// defualt is 1s
	uint32_t argument = 0x00010000;

	while(optind<argc){
		c = getopt(argc, argv,"a:p:t:h");
		switch(c){
			case 'a':
				argument = lib_valid_digit(optarg); 
				break;

			case 'p':                               //polling status 
				poll_status = lib_valid_digit(optarg); 
				break;

			case 't':                               //polling time out 
				timeout=(unsigned long)lib_valid_digit(optarg); 
				break;

			case 'h':
			default:
			  help_menu();
			  return -1;
		}
	}

	timeout*=1000;//convert to us

	if(flashops->open()<0)
		return -1;

	lib_build_cmd(&mmc_cmd,13, argument,NULL,NULL,0,0,0);

	BEGINTIMER;

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;

	GETTIMER;

	r1_res=lib_r1_check(mmc_cmd.restoken);

	if(poll_status != -1){//need to polling for a specfied status

		while(r1_res.sts != poll_status){
			GETTIMER;
			if(DIFFTIMERUS>timeout){
				printf("Error: cmd13 polling timeout, status %s\n",lib_decode_status(poll_status));
				return -1;
			}
			
			if(flashops->cmd(&mmc_cmd)<0)
				return -1;
			
			r1_res=lib_r1_check(mmc_cmd.restoken); 
		}

		printf("Poll time is %ldus\n",DIFFTIMERUS);
	}

	if(r1_res.error){
		printf("Cmd13 failed. Error code: 0x%08x\n",r1_res.error);
		return -1;
	}

	printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));

	flashops->close();

	return 0;	
}
