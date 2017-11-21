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
	printf("Usage: power_loss_config -d NUM [ -s NUM | -v NUM]\n\n");
	printf("Options:\n");
	printf("-d : delay time (unit us)\n");
	printf("-s : speed time (unit us)\n");
	printf("-v : volt, float num (unit V)\n");
}

int main(int argc, char * const argv[])
{
	int c; 
	int fd; 
	float volt = 0;
	struct misc misc ={
		.option = 6,
		.value[0] = 0,
		.value[1] = 0,
		.value[2] = 0,
	}; 


	while(optind<argc){ 
		c = getopt(argc, argv,"d:s:v:h"); 
		switch(c){
		case 'd': 
			misc.value[0] = lib_valid_digit(optarg);
			break;

		case 's': 
			misc.value[1] = lib_valid_digit(optarg);
			break;

		case 'v': 
			if( sscanf(optarg, "%f",&volt) != 1){
				printf("input error [%s], should be a number\n", optarg);
				return -1;
			}
			misc.value[2] = (int)(volt*10);
			
			break;

		case 'h':
    	default:
    	  help_menu();
    	  return -1; 
		}
	}

	if( (fd = open(FLASHNOD,O_RDWR)) < 0) {
		printf("FLASHAPP_ERROR:open failure.\n"); 
		exit(1); 
	}

	if(ioctl(fd,EMMC_IOC_DUMP_REG,&misc) < 0) {
		printf("FLASHLIB ERROR: in ops_dump_reg(), ioctl flash dev fail\n");
		exit(1);
	}

	close(fd);
	return 0;	
}
