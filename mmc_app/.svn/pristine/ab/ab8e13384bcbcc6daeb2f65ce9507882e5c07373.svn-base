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

#define SET_PWD_BIT 0x01
#define CLR_PWD_BIT 0x02

#define LOCK_BIT 	0x04
#define UNLOCK_BIT 	0x00

#define ERASE_BIT	0x08

extern char *optarg;
extern int optind;

void help_menu(void)
{
	printf("Usage: cmd42 -p PWD -[s|c|l|u]  \n");
	printf("       cmd42 -e \n");
	printf("Options:\n");
	printf("\t-p\tPWD\tpassword\n");
	printf("\t-s\t\tset password\n");
	printf("\t-c\t\tclear password\n");
	printf("\t-l\t\tlock\n");
	printf("\t-u\t\tunlock\n");
	printf("\t-e\t\tWarning: erase password and all usr data in SD\n");
}


int main(int argc, char * const argv[])
{
	int c;
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
	uint8_t *buf = NULL;
	char pwd[100];
	int flag = 0xdead;


	while(optind<argc){
		c = getopt(argc, argv,"p:sclueh");

		switch(c){
		case 'p':
			strcpy(pwd, optarg);
			break;

		case 's':
			flag = SET_PWD_BIT;
			break;

		case 'c':
			flag = CLR_PWD_BIT;
			break;

		case 'l':
			flag = LOCK_BIT;
			break;

		case 'u':
			flag = UNLOCK_BIT;
			break;
		
		case 'e':
			flag = ERASE_BIT;
			break;

		case 'h':
		default: 
			help_menu(); 
			return -1;
		}
	}

	if( flag == 0xdead ) {
		printf("no action provided.\n");
		return -1;
	}

	if( flag != ERASE_BIT ) {
		if (strlen(pwd) == 0) {
			printf("no password provided.\n");
			return -1;
		}
		else {
			printf("PWD is: %s\n", pwd);
		}
	}


	if( flashops->open() < 0)
		return -1;

	buf = malloc(SEC_SIZE);
	memset(buf, 0, SEC_SIZE);

	buf[0] = flag;

	buf[1] = strlen(pwd);
	strcpy((char *)(buf+2), pwd);

	dump_buf(buf, SEC_SIZE);

	lib_build_cmd(&mmc_cmd,42,0,NULL,buf,SEC_SIZE,0,0);

	if( flashops->cmd(&mmc_cmd) < 0)
		return -1;

	r1_res=lib_r1_check(mmc_cmd.restoken);
	if(r1_res.error){
		printf("cmd42 failed. Error code: 0x%08x\n",r1_res.error);
		return -1;
	}

	printf("cmd42 response: 0x%08x, status 0x%x %s\n",mmc_cmd.restoken[0], r1_res.sts,lib_decode_status(r1_res.sts));

	free(buf);

	flashops->close(); 

	return 0;	
}
