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
	printf("Usage: cmd17 -a ADDR  [-d PATTERN | -u FILE_NAME | -i FILE_NAME]  [-t|q]\n\n");
	printf("Options:\n");
	printf("\t-a\tNUM \taddress\n");
	printf("\t-d\tNUM \tuse pattern to compare\n");
	printf("\t-u\tFILE\tuse file to compare\n");
	printf("\t-i\tFILE\twrite data to file\n"); 
	printf("\t-t\t\tdisplay the time of cmd\n");
	printf("\t-q\t\tdon't print the data\n");
}


int main(int argc, char * const argv[])
{
	int fd = -1;
	int c;
	struct block_data bdata;
	uint32_t pattern = 0;

	char file_path[50];

	int saddr_block = 0;

	int mode_flag = MODE_PRINT;
	int time_flag = 0;

	while(optind<argc){
		c = getopt(argc, argv,"a:d:u:i:tqh");
		switch(c){

		case 'a':                               //address 
			saddr_block = lib_valid_digit(optarg); 
			break;

		case 'd':                               //const data 
			pattern = lib_valid_digit(optarg); 
			mode_flag = MODE_CONSTCMP; 
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

		case 'q':                               //don't print data 
			mode_flag = MODE_QUIET; 
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
	bdata.len   = 1;
	bdata.chunk = 1 ;

	if( do_read_task(&bdata, 0, time_flag, mode_flag, fd, pattern) != 0 )
		return -1;
	
	flashops->close();

	if( (mode_flag == MODE_PATCMP) || (mode_flag == MODE_PATWRITE) )
		close(fd);

	return 0;	
}
