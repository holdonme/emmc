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

#define SD_OCR_S18R     (1 << 24)    /* 1.8V switching request */
#define SD_OCR_CCS      (1 << 30)    /* Card Capacity Status */

#define R1_CARD_IS_LOCKED   (1 << 25)   /* sx, a */

extern char *optarg;
extern int optind;

void help_menu()
{
	printf("Usage: sd_acmd41 [-a|c|v ARG]\n\n");
	printf("Options:\n");
	printf("\t-a\tARG\targument\n");
	printf("\t-c\t0 -- host only support SDSC\n");
	printf("\t  \t1 -- host support SDHC/SDXC (default)\n");
	printf("\t-v\t0 -- use current voltage (default)\n");
	printf("\t  \t1 -- switch to 1.8v\n");
	printf("\t\n");
	printf("\t-g\t     ignore card lock error\n");
}

int main(int argc, char * const argv[])
{
	int c;
	int ocr;
	int err = 0;
	struct emmc_sndcmd_req mmc_app;
	struct emmc_sndcmd_req mmc_cmd;
	uint32_t argument = 0x40300000;	// FIXME set bit SD_OCR_CCS 
	int ignore_card_lock = 0;
	
	while(optind<argc){
		c = getopt(argc, argv,"a:c:v:gh");

		switch(c){
		case 'a': 
			argument = lib_valid_digit(optarg); 
			break;

		case 'c': 
			if(lib_valid_digit(optarg)) 
				argument |= SD_OCR_CCS;
			else
				argument &= ~SD_OCR_CCS;
			break;

		case 'v': 
			if(lib_valid_digit(optarg)) 
				argument |= SD_OCR_S18R;
			else
				argument &= ~SD_OCR_S18R;
			break;

		case 'g':
			ignore_card_lock = 1;
			break;
		
		case 'h': 
		default: 
			help_menu(); 
			return -1;
		}
	}

	if( flashops->open() < 0)
		return -1;

	lib_build_cmd(&mmc_app,55, 0,       NULL,NULL,0,0,0);
	lib_build_cmd(&mmc_cmd,41, argument,NULL,NULL,0,0,0);

	BEGINTIMER;

	while(1){

		if(flashops->cmd(&mmc_app)<0) {
			if( ignore_card_lock && (mmc_app.restoken[0] & R1_CARD_IS_LOCKED) )
				;	// ignore card lock status whan acmd41
			else
				return -1;
		}

		if(flashops->cmd(&mmc_cmd)<0)
			return -1;

		GETTIMER;

		printf("acmd41 response: 0x%08x\n",mmc_cmd.restoken[0]);
		ocr = mmc_cmd.restoken[0];

		if(ocr&0x80000000)
			break;

		if(DIFFTIMERUS>CMD1_TIMEOUT*1000){
			err = -1;
			printf("acmd41 timeout.\n");
			break;
		}
		usleep(1000);
	}

	PRINTTIMER;

	flashops->close();

	return err;	
}
