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
	printf("Usage: mmcconfig -a|b|s|d  [-f CLK] [-i WIDTH]\n");
	printf("Options:\n");
	printf("\t-a\tSDR   mode, x1/x4/x8, support 25/50(default) MHz, with option \"-f -i\"\n");
	printf("\t-b\tDDR   mode, x4/x8,    support 25/50(default) MHz, with option \"-f -i\"\n");
	printf("\t-s\tHS200 mode, x8,       support 25/50/100/200(default) MHz, with option \"-f\" \n");
	printf("\t-d\tHS400 mode, x8,       support 25/50/100/200(defualt) MHZ, with option \"-f\"\n");
	printf("\t-f\tCLK frequency (unit MHz)\n");
	printf("\t-i\tbus width, support x1/x4/x8(default)\n");
	printf("\n");
}

int main(int argc, char * const argv[])
{ 
	struct config_req req; 
	int c; 
	int clk=0; 
	int bus_width = 8;

	while(optind<argc){ 
		c = getopt(argc, argv,"absdf:i:h"); 

		switch(c){ 
			case 'a':                          //SDR 
				req.flag = CONFIG_MODE_HS_SDR;
				clk = 50;
				break;
			case 'b':                          //DDR
			  	req.flag = CONFIG_MODE_HS_DDR;
				clk = 50;
			  	break;
			case 's':                          //HS200 
			  	req.flag = CONFIG_MODE_HS200;
				clk = 200;
			  	break;
			case 'd':                          //HS400
			  	req.flag = CONFIG_MODE_HS400;
				clk = 200;
			  	break;
			case 'f':                          //CLK
				req.flag = CONFIG_CLK;
			  	clk = atoi(optarg); 
				break;
			case 'i':                          //IO_NUM
				req.flag = CONFIG_IO_NUM;
			  	bus_width = atoi(optarg); 
				break;
			case 'h':
			case '?':
			default:
			  	help_menu();
			  	return -1;
		} 
	} 

	// check bus width
	switch(bus_width){
	case 1:
	case 4:
	case 8:
		req.bus_width = bus_width;
		break;
	default:
		printf("bus width %d is illegal\n", bus_width);
		return -1;
	}
	

	// check clock frequency
	switch(req.flag){
		case CONFIG_MODE_HS_SDR:
		case CONFIG_MODE_HS_DDR:

			if( (clk != 25) && (clk != 50) ) {
				printf("FLASHAPP_ERROR: clk arg [%d] error\n", clk);
				return -1;	
			}
			break;

		case CONFIG_MODE_HS200:
		case CONFIG_MODE_HS400:
			if((clk != 25) && (clk != 50) && (clk != 100) && (clk != 200) ) {
				printf("FLASHAPP_ERROR: clk arg [%d] error\n", clk);
				return -1;	
			}
			break;
		default:
			// Just modify CLK or IO_NUM ??
			// Then user should issue tuning cmd in follwing sequence...
			break;
	}

	req.data = clk;


	if( flashops->open() < 0)
		return -1;

	if( flashops->req(&req) < 0)
		return -1;

	flashops->close(); 

	return 0;	
}
