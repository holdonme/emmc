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
	printf("Usage: sd_config [-f CLK | -i WIDTH]\n");
	printf("Options:\n");
	printf("\t-f\tCLK frequency (unit MHz)\n");
	printf("\t-i\tbus width, support x1/x4\n");
	printf("\n");
}

int main(int argc, char * const argv[])
{ 
	struct config_req req; 
	int c; 
	int clk=0; 
	int bus_width = 1;

	while(optind<argc){ 
		c = getopt(argc, argv,"f:i:h"); 

		switch(c){ 
			case 'f':                          //CLK
			  	clk = atoi(optarg);
				req.flag = CONFIG_CLK;
				req.data = clk;
				break;

			case 'i':                          //WIDTH
			  	bus_width = atoi(optarg); 

				if( bus_width == 1) {
					bus_width = MMC_BUS_WIDTH_1;
				}
				else if(bus_width == 4) {
					bus_width = MMC_BUS_WIDTH_4;
				}
				else {
					printf("bus width %d is illegal\n", bus_width);
					return -1;
				}
				
				req.flag = CONFIG_IO_NUM;
				req.data = bus_width;
				break;

			case 'h':
			case '?':
			default:
			  	help_menu();
			  	return -1;
		} 
	} 


	if( flashops->open() < 0)
		return -1;

	if( flashops->req(&req) < 0)
		return -1;

	flashops->close(); 

	return 0;	
}
