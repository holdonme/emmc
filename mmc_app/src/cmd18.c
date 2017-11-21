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
	printf("              [-d PATTERN | -u FILE_NAME | -i FILE_NAME] [-n|t|q]\n\n");
	printf("Options:\n"); 
	printf("\t-s\tNUM \tstart address\n"); 
	printf("\t-e\tNUM \tend address\n"); 
	printf("\t-c\tNUM \tchunk size(unit block)\n"); 
	printf("\t-t\t\tdisplay the time of cmd\n");
  	printf("\t-n\t\tclose-end transfer (default open-end)\n");
	printf("\t                                    \n");
	printf("Dump block data to stdout(default)/file or do nothing:\n");
	printf("\t                                    \n");
	printf("\t-i\tFILE\twrite block data to file\n"); 
  	printf("\t-q\t\tdon't print block data\n");
	printf("\t                                       \n");
	printf("Compare block data with pattern or file:\n");
	printf("\t                                    \n");
	printf("\t-d\tNUM \tuse pattern to compare\n"); 
	printf("\t-u\tFILE\tuse file to compare\n"); 
}

int main(int argc, char * const argv[])
{
	int c;
	int fd;
	struct block_data bdata;
	uint8_t pattern = 0;

	char file_path[100];

	int saddr_block = 0;
	int eaddr_block = 0; 
	int chunk_block = 0;

	int mode_flag = MODE_PRINT;
	int open_flag = FLAG_AUTOCMD12;
	int time_flag = 0;
  
	while(optind<argc){
		c = getopt(argc, argv,"s:e:d:c:u:i:ntqh");
		switch(c){
			case 's':                               //start address 
				saddr_block = lib_valid_digit(optarg); 
				break;

			case 'e':                               //end address 
				eaddr_block = lib_valid_digit(optarg); 
				break;

			case 'd':                               //const data to compare 
				pattern = lib_valid_digit(optarg); 
				mode_flag = MODE_CONSTCMP;
			  	break;

			case 'c':                               //chunk size in number of 512B 
				chunk_block = lib_valid_digit(optarg); 
				break;

			case 'u':                               //file to cmppare 
				strcpy(file_path,optarg); 
				mode_flag = MODE_PATCMP; 

				//check file
				if(access(file_path, O_RDONLY) < 0){
					printf("access [%s] error\n", file_path);
					return -1;
				}
				break;

			case 'i':                               //write data to file 
				strcpy(file_path,optarg);
				mode_flag = MODE_PATWRITE; 

				//create file
				fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC,S_IRWXU);
				if(fd < 0){
					printf("create file error\n");
					return -1;
				}
				close(fd);

				break;

			case 'n':                               //not issue cmd12 
				open_flag=0; 
				break;

			case 't':                               //time flag 
				time_flag=1; 
				break;

			case 'q':                               //don't print data 
				mode_flag = MODE_QUIET; 
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

	if( (mode_flag == MODE_PATCMP) || (mode_flag == MODE_PATWRITE) ){
		fd = open(file_path, O_RDWR);
		if(fd < 0){
			printf("open file [%s] error\n", file_path);
			return -1;
		}
	}


	if(flashops->open()<0)
		return -1;

	// all argument unit is block
	bdata.saddr = saddr_block; 
	bdata.len   = eaddr_block - saddr_block + 1;
	bdata.chunk = (chunk_block == 0) ? bdata.len : chunk_block ;

	if( do_read_task(&bdata, open_flag, time_flag, mode_flag, fd, pattern) != 0 )
		return -1;
	
	flashops->close();
	
	if( (mode_flag == MODE_PATCMP) || (mode_flag == MODE_PATWRITE) )
		close(fd);

	return 0;	
}
