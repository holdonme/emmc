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
	printf("Usage: cmd24 -a ADDR [-d PATTERN | -u FILE_NAME]  [-t]\n\n");
	printf("Options:\n");
	printf("\t-a\tNUM \tstart address\n");
	printf("\t-d\tNUM \tuse pattern to write\n");
	printf("\t-u\tFILE\tuse file to write\n");
	printf("\t-t\t\tdisplay the time of cmd\n");
}


int main(int argc, char * const argv[])
{
	int c;
	struct block_data bdata;
	uint32_t pattern = 0;
	int fd = -1;

	char file_path[50];

	int saddr_block = 0;

	int mode_flag = 0;
	int time_flag = 0;

	while(optind<argc){
		c = getopt(argc, argv,"a:d:u:th");

		switch(c){
		case 'a':                               //arg 
			saddr_block = lib_valid_digit(optarg); 
			break;

		case 'd':                               //const data 
			pattern = lib_valid_digit(optarg); 
			mode_flag = MODE_CONSTWT;
		  	break;

		case 'u':                               //pat data 
			strcpy(file_path,optarg); 
			mode_flag = MODE_PATWT; 

			//check file
			if(access(file_path, O_RDONLY) < 0){
				printf("access [%s] error\n", file_path);
				return -1;
			}
			break;

		case 't':                               //time flag
		  time_flag=1;
		  break;

		case 'h':
		default:
		  help_menu();
		  return -1;
		}
	}

	if( (mode_flag != MODE_CONSTWT) && (mode_flag != MODE_PATWT) ){
		help_menu();
		return -1;
	}

	if( mode_flag ==  MODE_PATWT ){
		fd = open(file_path, O_RDWR);
		if(fd < 0){
			printf("open file [%s] error\n", file_path);
			return -1;
		}
	}

	if( flashops->open() < 0)
		return -1;

	// all argument unit is block
	bdata.saddr = saddr_block;
	bdata.len   = 1;
	bdata.chunk = 1 ;

	if(do_write_task(&bdata, 0, time_flag, mode_flag, fd, pattern) != 0)
		return -1;

	flashops->close(); 

	if( mode_flag ==  MODE_PATWT )
		close(fd);

	return 0;	
}
