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
extern int optind, opterr, optopt;


struct user_arg arg={0,0,0,0,0,0,0};



void help_menu(){
	printf("Usage: cmd19 [-adt] [address]\n\n");
  printf("emmc cmd19 for bus test write operation.\n\n");
  printf("Options:\n");
  printf("\t-a\tNUM\tspecify address\n");
  printf("\t-d\tNUM\tspecify const data\n");
  printf("\t-t\t\tdisplay the time of cmd\n");
}

int validate_all_inputs(){
	if(arg.special<0 || arg.special >0xff){
		printf("Error: data(-d) is not in allowed range (0x0-0xff).\n");
		return 1;
	}
	return 0;
}



int main(int argc, char * const argv[])
{
  int fd;
  int c;
  struct emmc_sndcmd_req mmc_cmd;
  struct config_req req;
  struct r1 r1_res;
  struct block_data bdata;
//  char file_path[50]={0};
  int time_flag=0;
  
  opterr=1;
  g_failFlag=0;
  
  
	
	while(optind<argc){
    c = getopt(argc, argv,"a:d:t");
   // printf("%c\n",c);
    switch(c){
    	case 'a':                               //arg
   		  arg.sdata=lib_valid_digit(optarg);
    	  break;
    	case 'd':                               //const data
   		  arg.special=lib_valid_digit(optarg);
   		  arg.mode= MODE_CONSTWT;
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
  
  req.flag=REQ_ADDRESS_MODE;
  if(flashops->req(&req)<0){
  	printf("FLASHAPP ERR: cmd18 req failure.\n");
  	exit(1);
  }
  
  
  bdata.saddr=arg.sdata;
  bdata.len=SEC_SIZE;
  bdata.pdata=(uint8_t*)malloc(sizeof(int8_t)*bdata.len);
  if(req.data==REQ_ADDRESS_MODE_BYTE){
 		bdata.saddr*=SEC_SIZE;
  }
  
  if(arg.mode == MODE_CONSTWT){
  	memset(bdata.pdata, arg.special, sizeof(uint8_t)*bdata.len);
  }
  else{
  	printf("FLASHAPP ERR:\tinvalid mode selected 0x%x\n",arg.mode);
  	return -1;
	}
	
  lib_build_cmd(&mmc_cmd,19,bdata.saddr,NULL,bdata.pdata,bdata.len,0,0);
	if(req.data==REQ_ADDRESS_MODE_SEC){
 		bdata.saddr*=SEC_SIZE;
  }
  
  if(flashops->cmd(&mmc_cmd)<0){
  	printf("FLASHAPP ERR: cmd19 failure.\n");
  	exit(1);
  }
  r1_res=lib_r1_check(mmc_cmd.restoken);
  
  
  if(r1_res.error){
  	printf("Cmd19 failed. Error code: 0x%x\n",r1_res.error);
  	g_failFlag=1;
  }
  printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));
  
  flashops->close();

  //lib_validate_data_region(&bdata,NULL,0); 
  free(mmc_cmd.data);
  
  if(time_flag){
  	printf("cmd19 time: %d us\n",mmc_cmd.time);
  }
  
  
  if(g_failFlag){
  	exit(1);
  }
  
  return 0;	
}
