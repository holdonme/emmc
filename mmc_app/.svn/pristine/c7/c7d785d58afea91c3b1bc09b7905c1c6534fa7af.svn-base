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
	printf("Usage: cmd61 [-s NUM] [-u FILE | -i FILE] [-p]\n\n"); 
	printf("vendor read command\n\n");
	printf("Options:\n");
    printf("\t-s\tNUM\tread block count (default 1)\n");
	printf("\t-u\tFILE\tuse file to compare\n"); 
	printf("\t-i\tFILE\twrite data to file\n"); 
	printf("\t-p\t\tL2P show physical address\n");
}

int main(int argc, char * const argv[])
{
	int c;
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
	struct block_data bdata;

	uint8_t *buf = NULL;
	int mode_flag = MODE_PRINT;

	char file_path[50];
	int fd = 0;

	int show_L2P = 0;
	int blk_cnt = 1;
	
	while(optind<argc){
		c = getopt(argc, argv,"s:u:i:ph");

		switch(c){
		case 's':                               //sector number 
			blk_cnt = lib_valid_digit(optarg); 
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

		case 'p':                               
			show_L2P = 1;
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

	if( flashops->open() < 0)
		return -1;

	bdata.len= blk_cnt * SEC_SIZE;

	bdata.pdata = malloc(bdata.len);
	memset(bdata.pdata, 0, bdata.len);

	lib_build_cmd(&mmc_cmd,61,0,NULL,bdata.pdata,bdata.len,0,FLAG_AUTOCMD12);

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;

	r1_res = lib_r1_check(mmc_cmd.restoken);

	printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));
	if(r1_res.error)
		printf("Cmd61 failed. Error code: 0x%x\n",r1_res.error);

	flashops->close();

	if(mode_flag == MODE_PATCMP){
		buf = malloc(bdata.len);
		read(fd, buf, bdata.len);

		if(validate_buf(buf, bdata.len, bdata.pdata, 0))
			printf("Cmd61 compare failed.\n");
		else
			printf("Cmd61 compare pass.\n");

		free(buf);
	}
	else if(mode_flag == MODE_PATWRITE){
		write(fd, bdata.pdata, bdata.len);
	}
	else{
		dump_block(bdata.pdata, bdata.len, 0);
	}

	if(show_L2P) {
		printf("Phisical address is:\n");
		printf("FDevice:%d\n",bdata.pdata[0]);
		printf("FPlane :%d\n",bdata.pdata[1]);
		printf("Fblock: %x %x\n",bdata.pdata[2],bdata.pdata[3]);
		printf("Fpage:  %x %x\n",bdata.pdata[4],bdata.pdata[5]);
	} 

	free(bdata.pdata);

	if( (mode_flag == MODE_PATCMP) || (mode_flag == MODE_PATWRITE) )
		close(fd);
	
	return 0;	
}
