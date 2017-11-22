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
	printf("Usage: dump_reg\n\n");
	printf("-a 0 ------- dump all register values\n\n");
	printf("-a 12 ------- pull down RST_n\n\n");
	printf("-a 13 ------- pull up RST_n\n\n");
	
}

int main(int argc, char * const argv[])
{
	char c;
	int i = 0;
	struct misc misc ={
		.option = 0,
		.value[0] = 0,
	}; 
  
	while(optind<argc){
		c = getopt(argc, argv,"a:h"); 
		switch(c){
		case 'a': 
			misc.option = lib_valid_digit(optarg); 

			i = 0;
			while(optind < argc) {
				misc.value[i] = lib_valid_digit(argv[optind]);
				i++;
				optind++;
			}

			break;

		default:
			help_menu();
			return -1;
		}
	}

	if(flashops->open()< 0)
		return -1;

	if(flashops->dump_reg((void *)&misc)<0)
		return -1;

	flashops->close();

	return 0;
}
