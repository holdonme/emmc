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

void help_menu(char *name)
{
	printf("Usage: %s -s START_ADDR -e END_ADDR [-c CHUNK_SIZE]\n", name);
	printf("              [-d PATTERN | -u FILE_NAME]  [-n|t]\n\n");
	printf("Options:\n");
	printf("\t-s\tNUM \tstart address\n");
	printf("\t-e\tNUM \tend address\n");
	printf("\t-c\tNUM \tchunk size(unit block)\n");
	printf("\t-d\tNUM \tuse pattern to write\n");
	printf("\t-u\tFILE\tuse file to write\n");
  	printf("\t-n\t\tclose-end transfer(default open-end)\n");
	printf("\t-t\t\tdisplay the time of cmd\n");
}

int main(int argc, char * const argv[])
{
	int c;
	struct block_data bdata;
	uint8_t pattern = 0;
	int fd = -1;
	
	char file_path[100];

	int saddr_block = 0;
	int eaddr_block = 0; 
	int chunk_block = 0;

	int mode_flag = MODE_PRINT;
	int open_flag = FLAG_AUTOCMD12;
	int time_flag = 0;
  
	while(optind<argc){
    	c = getopt(argc, argv,"s:e:c:d:u:nth");
    	switch(c){
    		case 's':                               //start address 
				saddr_block = lib_valid_digit(optarg); 
				break;
    		case 'e':                               //end address 
				eaddr_block = lib_valid_digit(optarg); 
				break;
    		case 'c':                               //chunk size in number of 512B 
				chunk_block = lib_valid_digit(optarg); 
				break;
    		case 'd':                               //const data 
				pattern =lib_valid_digit(optarg); 
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
    		case 'n':                               //not issue cmd12 
				open_flag=0; 
				break;
    		case 't':                               //time flag 
				time_flag=1; 
				break;
    		case 'h':
    		default: 
				help_menu(argv[0]); 
				return -1;
		}
	}
  
	if(saddr_block > eaddr_block){
		printf("Error: start addr should be larger than end addr.\n");
		return -1;
	}

	if( (mode_flag != MODE_CONSTWT) && (mode_flag != MODE_PATWT) ){
		printf("Error: -d or -u should be used.\n");
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
	bdata.len   = eaddr_block - saddr_block + 1;
	bdata.chunk = (chunk_block == 0) ? bdata.len : chunk_block ;

	if(do_write_task(&bdata, open_flag, time_flag, mode_flag, fd, pattern) != 0)
		return -1;
	
	flashops->close(); 

	if( mode_flag ==  MODE_PATWT )
		close(fd);

	return 0;	
}
