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

#define ARG_BB_INFO    0x11
#define ARG_BB_BLK_INFO    0x81
#define ARG_ERASE_INFO 0x21
#define ARG_ERASE_BLK_INFO 0x83
#define ARG_BLK_TYPE_INFO 0x83
#define ARG_ERASE_BLK_INFO_STEP2 0x85
#define ARG_BLK_TYPE_INFO_STEP2 0x87


#define MODE_BB 0
#define MODE_EC_CHIP 1
#define MODE_BLK_AGE 2
#define MODE_BLK_TYPE 3
#define MODE_BB_BLK 4



extern char *optarg;
extern int optind, opterr, optopt;


struct user_arg arg={0x11,0,0,0,0,0,0};



void help_menu(){
	printf("Usage: cmd56 [-aebtgykd] [arg]\n\n");
  printf("emmc cmd56 app cmd: used for phison controller only. defualt value goes to bad block count\n\n");
  printf("Options:\n");
  printf("\t-a\tNUM\tspecify arg, 0x11 for bad block count info and 0x21 for erase count info, other arg may fail\n");
  printf("\t-e\t\tspecify mode to erase count info (chip level)\n");
  printf("\t-g\t\tspecify mode to erase count info (detail blk level)\n");
  printf("\t-b\t\tspecify mode to bad blk info (chip level))\n");
  printf("\t-k\t\tspecify mode to bad blk info (detail blk level))\n");
  printf("\t-y\t\tspecify mode to blk type info (detail blk level)\n");
  printf("\t-d\tNUM\tspecify die number, only for bad blk info detail blk level.\n");
  printf("\t-t\t\tdisplay the time of cmd\n");
}

int validate_all_inputs(){
	if(arg.special<0 || arg.special >0xff){
		printf("Error: data(-d) is not in allowed range (0x0-0xff).\n");
		return 1;
	}
	if(arg.sdata!=ARG_BB_INFO && arg.sdata!=ARG_ERASE_INFO
	 && arg.sdata!=ARG_ERASE_BLK_INFO && arg.sdata!=ARG_BLK_TYPE_INFO && arg.sdata!=ARG_ERASE_BLK_INFO_STEP2 && arg.sdata!=ARG_BLK_TYPE_INFO_STEP2 && arg.sdata!=ARG_BB_BLK_INFO){
		printf("Error: arg is incorrect.\n");
		return 1;
	}
	return 0;
}



int main(int argc, char * const argv[])
{
  int fd;
  int c;
  struct emmc_sndcmd_req mmc_cmd;
  struct r1 r1_res;
  struct block_data bdata;
  int time_flag=0;
  int blk_age_table_cnt=0;
  int i,j;
  int die =0;
  uint blk,page_addr,info,bus;//used for bb detail blk
  int skip_flag=0;//used for bb detail blk

  opterr=1;
  g_failFlag=0;
	
	while(optind<argc){
    c = getopt(argc, argv,"a:ebtgykd:");
    switch(c){
    	case 'a':                               //address
   		  arg.sdata=lib_valid_digit(optarg);
    	  break;
    	case 'b':                               //bad blk
   		  arg.sdata=ARG_BB_INFO;
   		  arg.mode=MODE_BB;
    	  break;
    	case 'e':                               //erase count
   		  arg.sdata=ARG_ERASE_INFO;
   		  arg.mode=MODE_EC_CHIP;
    	  break;
    	case 'g':                               //blk age
   		  arg.sdata=ARG_ERASE_BLK_INFO;
   		  arg.mode=MODE_BLK_AGE;
    	  break;
    	case 'y':                               //blk type
   		  arg.sdata=ARG_BLK_TYPE_INFO;
   		  arg.mode=MODE_BLK_TYPE;
    	  break;
    	case 'k':                               //bad blk detail info
   		  arg.sdata=ARG_BB_BLK_INFO;
   		  arg.mode=MODE_BB_BLK;
    	  break;
    	case 'd':                               //bad blk detail info
   		  die=lib_valid_digit(optarg);
    	  break;
    	case 't':                               //time flag
   		  time_flag=1;
    	  break;
    	case '?':
    	default:
    	  (void)help_menu();
    	  return -1;
    	  break;
    }
  }

  if(validate_all_inputs()){
  	(void)help_menu();
  	return -1;
  }

  fd=flashops->open();
  if(fd<0){
    printf("FLASHAPP_ERROR:open failure.\n");
    exit(1);
  }
  
  
  bdata.saddr=arg.sdata;
  bdata.len=SEC_SIZE;
  bdata.pdata=(uint8_t*)malloc(sizeof(uint8_t)*bdata.len);
  bdata.cdata=arg.special;
  
  memset(bdata.pdata, 0, sizeof(uint8_t)*bdata.len);
  
  if(arg.mode!=MODE_BB_BLK){
	  lib_build_cmd(&mmc_cmd,56,bdata.saddr,NULL,bdata.pdata,bdata.len,0,0);
	  
	  if(flashops->cmd(&mmc_cmd)<0){
	  	if(bdata.pdata) free(bdata.pdata);
	  	printf("FLASHAPP ERR: cmd56 failure.\n");
	  	exit(1);
	  }
	  r1_res=lib_r1_check(mmc_cmd.restoken);
	  
	  
	  if(r1_res.error){
	  	printf("Cmd56 failed. Error code: 0x%x\n",r1_res.error);
	  	g_failFlag=1;
	  }
	  printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));

		dump_buf(bdata.pdata, 512);
	}
  
  
  
  if(arg.mode == MODE_BB){
  	printf("Total_Initial_Bad_Block_Count:\t0x%x\n",(bdata.pdata[0]<<8)|bdata.pdata[1]);
  	printf("Total_Later_Bad_Block_Count:\t0x%x\n",(bdata.pdata[2]<<8)|bdata.pdata[3]);
  	printf("Total_Spare_Block_Count:\t0x%x\n",(bdata.pdata[4]<<8)|bdata.pdata[5]);
  }
  else if(arg.mode == MODE_EC_CHIP){
  	printf("Minimum_Erase_Count:\t0x%x\n",(bdata.pdata[0]<<8)|bdata.pdata[1]);
  	printf("Maximum_Erase_Count:\t0x%x\n",(bdata.pdata[2]<<8)|bdata.pdata[3]);
  	printf("Average_Erase_Count:\t0x%x\n",(bdata.pdata[4]<<8)|bdata.pdata[5]);
  	
  }
  else if(arg.mode == MODE_BLK_AGE){
  	blk_age_table_cnt=bdata.pdata[0];
  	printf("Number of blk table %d\n",blk_age_table_cnt);
  	for(i=0;i<blk_age_table_cnt;i++){
  		bdata.saddr=ARG_ERASE_BLK_INFO_STEP2;//set arg
  		bdata.saddr|=(i<<8);//set the blk table index;
  		lib_build_cmd(&mmc_cmd,56,bdata.saddr,NULL,bdata.pdata,bdata.len,0,0);
  		  if(flashops->cmd(&mmc_cmd)<0){
			  	if(bdata.pdata) free(bdata.pdata);
			  	printf("FLASHAPP ERR: cmd56 step2 failure.\n");
			  	exit(1);
			  }
			  r1_res=lib_r1_check(mmc_cmd.restoken);
			  			  
			  if(r1_res.error){
			  	printf("Cmd56 step2 failed. Error code: 0x%x\n",r1_res.error);
			  	g_failFlag=1;
			  }
			  for(j=0;j<0x100;j+=2){
			  	if(((bdata.pdata[j]<<8)|bdata.pdata[j+1])!=0)
				{
			  		printf("Blk %d EC: %d\n",(bdata.pdata[j]<<8)|bdata.pdata[j+1],(bdata.pdata[j+0x100]<<8)|bdata.pdata[j+0x101]);
				}
			  }
  	}//loop for table end
  }//MODE_BLK_AGE
  else if(arg.mode == MODE_BLK_TYPE){
  	blk_age_table_cnt=bdata.pdata[0];
  	printf("Number of blk table %d\n",blk_age_table_cnt);
  	for(i=0;i<blk_age_table_cnt;i++){
  		bdata.saddr=ARG_BLK_TYPE_INFO_STEP2;//set arg
  		bdata.saddr|=(i<<8);//set the blk table index;
  		lib_build_cmd(&mmc_cmd,56,bdata.saddr,NULL,bdata.pdata,bdata.len,0,0);
  		  if(flashops->cmd(&mmc_cmd)<0){
			  	if(bdata.pdata) free(bdata.pdata);
			  	printf("FLASHAPP ERR: cmd56 step2 failure.\n");
			  	exit(1);
			  }
			  r1_res=lib_r1_check(mmc_cmd.restoken);
			  			  
			  if(r1_res.error){
			  	printf("Cmd56 step2 failed. Error code: 0x%x\n",r1_res.error);
			  	g_failFlag=1;
			  }
			 // printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));
			  for(j=0;j<0x100;j+=2){
			  	if(((bdata.pdata[j]<<8)|bdata.pdata[j+1])!=0)
			  		printf("Blk %d type: %d %s\n",(bdata.pdata[j]<<8)|bdata.pdata[j+1],bdata.pdata[j+0x101],(bdata.pdata[j+0x101]==1)?"SLC":"MLC");
			  }
  	}//loop for table end
  }//MODE_BLK_TYPE
  else if(arg.mode==MODE_BB_BLK){
  	for(i=0;i<4;i++){

  		bdata.saddr=ARG_BB_BLK_INFO;//set arg
  		bdata.saddr|=(die<<8);//set die index;
  		bdata.saddr|=(i<<16);//set table index;
  		lib_build_cmd(&mmc_cmd,56,bdata.saddr,NULL,bdata.pdata,bdata.len,0,0);
  		  if(flashops->cmd(&mmc_cmd)<0){
			  	if(bdata.pdata) free(bdata.pdata);
			  	printf("FLASHAPP ERR: cmd56 failure.\n");
			  	exit(1);
			  }
			  r1_res=lib_r1_check(mmc_cmd.restoken);
			  			  
			  if(r1_res.error){
			  	printf("Cmd56 failed. Error code: 0x%x\n",r1_res.error);
			  	g_failFlag=1;
			  }
			 // printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));
			  for(j=0;j<0x200;j+=8){
					if(bdata.pdata[j] || bdata.pdata[j+1] || bdata.pdata[j+2] || bdata.pdata[j+3] || bdata.pdata[j+4] || bdata.pdata[j+5] || bdata.pdata[j+6] || bdata.pdata[j+7] || bdata.pdata[j+8]){
						blk=(bdata.pdata[j]<<8)|bdata.pdata[j+1];
						page_addr=(bdata.pdata[j+2]<<8)|bdata.pdata[j+3];
						info=bdata.pdata[j+4];
						bus=bdata.pdata[j+5];
						printf("BB die %d blk %d info %d (%s) page_addr 0x%x bus %d\n",die,blk,info,(info==0x1||info==0x10)?"Ers_fail":((info==0x20||info==0x11)?"Pgm_fail":"unknown"),page_addr,bus);
					}
					else{
						skip_flag=1;
						break;
					}
			  }
			  if(skip_flag) break;
  	}//loop for table end  	
  }//MODE_BB_BLK
  	
  free(bdata.pdata);
  flashops->close();
  
  if(time_flag){
  	printf("cmd56 time: %d us\n",mmc_cmd.time);
  }
  
  if(g_failFlag){
  	exit(1);
  }
  
  return 0;	
}
