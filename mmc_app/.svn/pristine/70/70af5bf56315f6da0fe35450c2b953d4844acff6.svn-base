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
	printf("Usage: power -a MODE -d ARG [-w POWER_SRC]\n\n");
	printf("Options:\n");
	printf("-a : set power loss mode\n");
	printf("     0-- power off\n");
	printf("     1-- power on to 3.3/1.8 v\n");
	printf("     2-- power off in ARG us, with option '-d/w'\n");
	printf("     3-- set voltage to ARG v, with option '-d/w'\n");
	printf("-d : NUM time (unit us) or\n");
	printf("     NUM volt (unit v)\n");
	printf("-w : select which to operate\n");
	printf("     0-- vcc\n");
	printf("     1-- vccq\n");
	printf("     2-- all(default)\n");
}

#define OP_VCC  0
#define OP_VCCq 1
#define OP_ALL  2

int main(int argc, char * const argv[])
{
	int c; 
	float value = 0; 
	int mode = 0;
	int which = OP_ALL;
	char *str[3] = {"VCC", "VCCq", "VCC/VCCq"};

	while(optind<argc){ 
		c = getopt(argc, argv,"a:d:w:h"); 
		switch(c){
		case 'a': 
			mode = lib_valid_digit(optarg); 
			break;

		case 'd': 
			if( sscanf(optarg, "%f",&value) != 1){
				printf("input error [%s], should be a number\n", optarg);
				return -1;
			}

			break;

		case 'w':
			which = lib_valid_digit(optarg);
			break;

		case 'h':
    	default:
    	  help_menu();
    	  return -1; 
		}
	}

	if(power->open() < 0){ 
		printf("FLASHAPP_ERROR:open failure.\n"); 
		exit(1); 
	}

	switch(mode){
	case 0:		// power off in 0 us
		power->set_power_off(0, OP_ALL);
		printf("power down VCC/VCCq to 0\n");
		break;

	case 1:		// power on to 3.3/1.8 v
		power->set_power_on(33, OP_VCC);
		power->set_power_on(18, OP_VCCq); 
		printf("power up VCC to 3.3v, VCCq to 1.8v\n");
		break;

	case 2:		// power off in ARG us
		if(value == 0) {
			printf("'-d' option should > 0\n");
			goto err;
		}
		power->set_power_off(value, which);
		printf("power down %s in %.0f us\n", str[which], value);

		break;

	case 3:		// power on to ARG v
		if( (value>3.3) || (value<0) ){
			printf("valid voltage range 0~3.3v, [%.1f] is out of range\n", value);
			goto err;
		}
		power->set_power_on(value*10, which);
		printf("power set %s to %.1f v\n", str[which], value);
		break;

	default:
		printf("invalid argument with option '-a'\n");
		goto err;
	}	
 
err: 
	power->close(); 

	return 0;	
}
