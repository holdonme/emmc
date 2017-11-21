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

#define MODE_PRINT    0
#define MODE_CONSTCMP 1
#define MODE_PATCMP   2
#define MODE_PATWRITE 3


extern char *optarg;
extern int optind, opterr, optopt;


struct user_arg arg={0,0,0,0,0,0,0};



void help_menu(){
	printf("Usage: cmd14 [-aduit] [arg]\n\n");
  printf("emmc cmd14 for bus test read operation.\n\n");
  printf("Options:\n");
  printf("\t-a\tNUM\tspecify address\n");
  printf("\t-d\tNUM\tspecify const data to compare\n");
  printf("\t-u\t\tuse pat data to compare\n");
  printf("\t-i\t\twrite data to pat file\n");
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
  int time_flag=0;
  
  opterr=1;
  g_failFlag=0;
  uint8_t *cmp_pattern=NULL;
  char file_path[50]={0};
  
	
	while(optind<argc){
    c = getopt(argc, argv,"a:d:uit");
   // printf("%c\n",c);
    switch(c){
    	case 'a':                               //address
   		  arg.sdata=lib_valid_digit(optarg);
    	  break;
    	case 'd':                               //const data
   		  arg.special=lib_valid_digit(optarg);
   		  arg.mode=MODE_CONSTCMP;
    	  break;
    	case 'u':                               //pat data cmp
   		  arg.mode=MODE_PATCMP;
    	  break;
    	case 'i':                               //pat data write
   		  arg.mode=MODE_PATWRITE;
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
  	printf("FLASHAPP ERR: cmd14 req failure.\n");
  	exit(1);
  }
  
  bdata.saddr=arg.sdata;
  bdata.len=SEC_SIZE;
  bdata.pdata=(uint8_t*)malloc(sizeof(uint8_t)*bdata.len);
  bdata.cdata=arg.special;
  if(req.data==REQ_ADDRESS_MODE_BYTE){
  //	printf("it is BYTE address\n");
 		bdata.saddr*=SEC_SIZE;
  }
  
  
  
  memset(bdata.pdata, 0, sizeof(uint8_t)*bdata.len);
  lib_build_cmd(&mmc_cmd,14,bdata.saddr,NULL,bdata.pdata,bdata.len,0,0);
	if(req.data==REQ_ADDRESS_MODE_SEC){
 		bdata.saddr*=SEC_SIZE;
  }
  
  if(flashops->cmd(&mmc_cmd)<0){
  	if(bdata.pdata) free(bdata.pdata);
  	printf("FLASHAPP ERR: cmd14 failure.\n");
  	exit(1);
  }
  r1_res=lib_r1_check(mmc_cmd.restoken);
  
  
  if(r1_res.error){
  	printf("Cmd14 failed. Error code: 0x%x\n",r1_res.error);
  	g_failFlag=1;
  }
  printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));
  
  flashops->close();
  if(arg.mode == MODE_PRINT){
  	lib_validate_data_region(&bdata,NULL,0);
  }
  else if(arg.mode == MODE_CONSTCMP){
  	if(lib_validate_data_region(&bdata,NULL,1)){
  		printf("Cmd14 compare failed.\n");
  	}
  	else{
  		printf("Cmd14 compare pass.\n");
  	}
  }
  else if(arg.mode == MODE_PATCMP){
  	strcpy(file_path,DPATPATH);
  	cmp_pattern=(uint8_t *)malloc(bdata.len*sizeof(uint8_t));
  	if(lib_readPattern2Mem(cmp_pattern, file_path, bdata.len)){
  	  printf("FLASHAPP ERR:\tread pattern failure, %s\n",file_path);
  	  if(cmp_pattern) free(cmp_pattern);
  	  if(bdata.pdata) free(bdata.pdata);
  		return -1;
  	}
    if(lib_validate_data_region(&bdata,cmp_pattern,1)){
  		printf("Cmd14 compare failed.\n");
  	}
  	else{
  		printf("Cmd14 compare pass.\n");
  	}
    free(cmp_pattern);
  }
  else if(arg.mode == MODE_PATWRITE){
  	strcpy(file_path,DOUTPATPATH);
  	cmp_pattern=(uint8_t *)malloc(bdata.len*sizeof(uint8_t ));
  	lib_writePattern2File(bdata.pdata, file_path, bdata.len);
  	free(cmp_pattern);
  }
  else{
  	printf("FLASHAPP ERR:\tinvalid mode selected 0x%x\n",arg.mode);
  	if(bdata.pdata) free(bdata.pdata);
  	return -1;
	}
  free(bdata.pdata);
  
  if(time_flag){
  	printf("cmd14 time: %d us\n",mmc_cmd.time);
  }
  
  if(g_failFlag){
  	exit(1);
  }
  
  return 0;	
}
