#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>

#include "flash_define.h"
#include "flash_lib.h"
#include "flash_ops.h"
#include "mmc-user.h"

#define VDRCMD_CK_SETTING 0x85
#define VDRCMD_CK_STATS   0x83
#define VDRCMD_RD_HWPAGE  0x84
#define VDRCMD_RD_L2P	  0x88
#define VDRCMD_WT_HWPAGE  0x08
#define VDRCMD_RD_WPPAGE  0x82
#define VDRCMD_WT_WPPAGE  0x01
#define VDRCMD_CD_LOADER  0x09
#define VDRCMD_EX_PRAM    0x2E
#define VDRCMD_WT_CODE    0x11
#define VDRCMD_VF_CODE    0x12
#define VDRCMD_RD_DIPAGE  0xA2
#define VDRCMD_ER_DIPAGE  0x20
#define VDRCMD_WT_DIPAGE  0x21
#define VDRCMD_PR_FORMAT  0x07
#define VDRCMD_CD_SETTING 0xE0

extern char *optarg;
extern int optind;

void help_menu()
{
    printf("Usage: cmd60 -c CODE [-s NUM | -f NAME] [-l NUM]\n\n");
    printf("vendor write command\n\n");
    printf("Options:\n");
    printf("\t-c\tNUM\tspecify write code\n");
    printf("\t\t\tVDRCMD_CK_SETTING\t0x85\n");
    printf("\t\t\tVDRCMD_CK_STATS\t\t0x83\n");
    printf("\t\t\tVDRCMD_RD_HWPAGE\t0x84\n");
    printf("\t\t\tVDRCMD_WT_HWPAGE\t0x08  with -f option\n");
    printf("\t\t\tVDRCMD_RD_WPPAGE\t0x82\n");
    printf("\t\t\tVDRCMD_WT_WPPAGE\t0x01  with -f option\n");
    printf("\t\t\tVDRCMD_CD_LOADER\t0x09  with -f option\n");
    printf("\t\t\tVDRCMD_EX_PRAM\t\t0x2E\n");
    printf("\t\t\tVDRCMD_WT_CODE\t\t0x11  with -f option\n");
    printf("\t\t\tVDRCMD_VF_CODE\t\t0x12  with -f option\n");
    printf("\t\t\tVDRCMD_RD_DIPAGE\t0xA2\n");
    printf("\t\t\tVDRCMD_ER_DIPAGE\t0x20\n");
    printf("\t\t\tVDRCMD_WT_DIPAGE\t0x21\n");
    printf("\t\t\tVDRCMD_PR_FORMAT\t0x07  with -f option\n");
    printf("\t\t\tVDRCMD_CD_SETTING\t0xE0  with -p option\n");
    printf("\t\t\tVDRCMD_RD_L2P\t\t0x88  with -l option\n");
	printf("\t\t\t\n");
    printf("\t-s\tNUM\twrite block count (default 1)\n");
    printf("\t-f\tNAME\tfile name\n");
    printf("\t-p\tNUM\tchip type, 0-PS8222AA  1-PS8222BB\n");
    printf("\t-l\tNUM\tlogic address\n");
}

//return value
// 1. ture, it is header file
// 0. false,it is not header file

int checkHeader(char* filePath)
{
	uint8_t tmp;
	int mark_cnt = 0;
	int i=0;
	FILE* fp;
	
	if((fp=fopen(filePath, "rb")) == NULL)
		return 0; 

	for(i=0;i<SEC_SIZE;i++){
		fread(&tmp, 1, 1, fp); 
		if(i==0 && tmp==0x4B)   mark_cnt++;//check 'K'
		if(i==1 && tmp==0x53)   mark_cnt++;//check 'S'
		if(i==510 && tmp==0x55) mark_cnt++;//check 0x55
		if(i==511 && tmp==0xaa) mark_cnt++;//check 0xaa
	}

	fclose(fp);

	return (mark_cnt == 4);
}


int main(int argc, char * const argv[])
{
	int c;
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
	struct block_data bdata;

	uint8_t chip_type = 0 ;
  	uint32_t logic_addr = 0;
	int ret;


	int blk_cnt = 1;
	int command_code = 0;

	int file_flag = 0;
	char file_path[50]={0};
	struct stat st;
	int fd;
  
	while(optind<argc){
		c = getopt(argc, argv,"c:f:s:p:l:h");
		switch(c){ 
		case 's':                               //number of sectors to be write 
			blk_cnt = lib_valid_digit(optarg); 
			break;

		case 'c':
			command_code = lib_valid_digit(optarg); 
			break;
		case 'p':                               //const data 
			chip_type = lib_valid_digit(optarg); 
			if( chip_type != 0 && chip_type != 1) { 
				printf("chip_type only support 0 or 1\n"); 
				return -1;
			}

			break;

		case 'f':                               //file name 
			strcpy(file_path,optarg); 
			if(access(file_path, O_RDONLY) < 0){
				printf("access [%s] error\n", file_path);
				return -1;
			}

			file_flag = 1;
			break;

		case 'l':
			logic_addr = lib_valid_digit(optarg);
			break;

		case 'h':
		default: 
			help_menu(); 
			return -1; 
		} 
	}
  
	switch(command_code){
		case VDRCMD_CK_SETTING:
		case VDRCMD_CK_STATS:
		case VDRCMD_RD_HWPAGE:
		case VDRCMD_WT_HWPAGE:
		case VDRCMD_RD_WPPAGE:
		case VDRCMD_WT_WPPAGE:
		case VDRCMD_CD_LOADER:
		case VDRCMD_EX_PRAM:
		case VDRCMD_WT_CODE:
		case VDRCMD_VF_CODE:
		case VDRCMD_RD_DIPAGE:
		case VDRCMD_ER_DIPAGE:
		case VDRCMD_WT_DIPAGE:
		case VDRCMD_PR_FORMAT:
		case VDRCMD_CD_SETTING:
		case VDRCMD_RD_L2P:
			break;
		default: 
			printf("Error: write command is invalid.\n");
			return -1;
	}


	if( flashops->open() < 0)
		return -1;

	bdata.saddr=0;
	bdata.len = blk_cnt * SEC_SIZE;

	if(file_flag){
		fd = open(file_path, O_RDWR);
		if(fd < 0){
			printf("open file [%s] error\n", file_path);
			return -1;
		}

		ret = fstat(fd, &st);
		if(ret < 0){
			printf("fstat() error\n");
			return -1;
		}

		// the first block store header info
		if( st.st_size % 512 == 0)
			bdata.len = st.st_size + 512; 
		else
			bdata.len = (st.st_size/512 + 2)*512; 


		bdata.pdata=(uint8_t*)malloc(bdata.len);
		memset(bdata.pdata, 0, bdata.len);

		if( command_code == VDRCMD_PR_FORMAT){
			read(fd, &bdata.pdata[0], bdata.len);
		}
		else{
			//if there is header in the file -> acutually we don't need header, load then overwrite it to 0.
			if(checkHeader(file_path)){ 
				printf("Header file from %s\n",file_path);
				read(fd, &bdata.pdata[0], bdata.len);
				memset(bdata.pdata, 0, SEC_SIZE);
			}
			else{
				read(fd, &bdata.pdata[SEC_SIZE], bdata.len);
			} 
		}
	}
	else{
		bdata.pdata=(uint8_t*)malloc(bdata.len); 
		memset(bdata.pdata, 0, bdata.len);
	}
	
	bdata.pdata[0] = command_code;

	switch(command_code){
		case VDRCMD_CD_LOADER:
			if(!file_flag){
				printf("command %d must use file input\n",VDRCMD_CD_LOADER);
				return -1;
			}

			bdata.pdata[1] = bdata.len/SEC_SIZE - 1;
			break;

		case VDRCMD_WT_CODE:
		case VDRCMD_VF_CODE:
			if(!file_flag){
				printf("command %d must use file input\n",VDRCMD_WT_CODE);
				return -1;
			}

			bdata.pdata[1] = ((bdata.len/SEC_SIZE - 1)>>8);
			bdata.pdata[2] = (bdata.len/SEC_SIZE - 1)&0xff;
			break;

		case VDRCMD_CD_SETTING:
			bdata.pdata[2] = 0x00; // abCid[0]
			bdata.pdata[3] = 0x00; // abCid[1] /* Manufacturing_Date*/
			bdata.pdata[4] = 0x00; // abCid[2] /* Product serial number*/
			bdata.pdata[5] = 0x00; // abCid[3] /* Product serial number*/
			bdata.pdata[6] = 0x00; // abCid[4] /* Product serial number*/
			bdata.pdata[7] = 0x00; // abCid[5] /* Product serial number*/
			bdata.pdata[8] = 0x00; // abCid[6] /* Product revision*/
			bdata.pdata[9] = 0x48; // abCid[7] /* Product Name */
			bdata.pdata[10] = 0x49; // abCid[8] /* Product Name */
			bdata.pdata[11] = 0x50; // abCid[9] /* Product Name */ 
			bdata.pdata[12] = 0x51; // abCid[10] /* Product Name */
			bdata.pdata[13] = 0x52; // abCid[11] /* Product Name */
			bdata.pdata[14] = 0x53; // abCid[12] /* Product Name */
			bdata.pdata[15] = 0x01; // abCid[13] /* OEM/AppID*/  
			bdata.pdata[16] = 0x01; // abCid[14] /*Card/BGA*/
			bdata.pdata[17] = 0xFE; // abCid[15] /*ManufacturerID*/ 
			
			//used to inform MC about uC
			bdata.pdata[100] = 0x01;//Enable SLC OTF;
			bdata.pdata[101] = chip_type;// 00 -> PS8222AA ; 01 --> PS8222BB	
			break;

		case VDRCMD_RD_L2P:
			bdata.pdata[1] = (logic_addr>>24)&0xff; // logic addr
			bdata.pdata[2] = (logic_addr>>16)&0xff; // logic addr
			bdata.pdata[3] = (logic_addr>>8)&0xff; //  logic addr
			bdata.pdata[4] = (logic_addr>>0)&0xff; //  logic addr
			break;

		default:
			break;
	}
	
	
	lib_build_cmd(&mmc_cmd,60,0,NULL,bdata.pdata,bdata.len,0,FLAG_AUTOCMD12);

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;

	r1_res = lib_r1_check(mmc_cmd.restoken);

	printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));

	if(r1_res.error)
		printf("Cmd60 failed. Error code: 0x%x\n",r1_res.error);

	flashops->close();

	free(bdata.pdata);

	return 0;	
}
