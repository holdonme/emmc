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
	printf("Usage: cmd35 [-a] [arg]\n\n");
  printf("emmc cmd35: erase group start\n\n");
  printf("Options:\n");
  printf("\t-a\tNUM\tspecify address for erase group start\n");
}

int validate_all_inputs(){

	return 0;
}



int main(int argc, char * const argv[])
{
  int fd;
  int c;
  struct emmc_sndcmd_req mmc_cmd;
  struct config_req req;
  struct r1 r1_res;
  
  opterr=1;
  g_failFlag=0;
  
  
	
	while(optind<argc){
    c = getopt(argc, argv,"a:");
   // printf("%c\n",c);
    switch(c){
    	case 'a':                               //arg
   		  arg.special=lib_valid_digit(optarg);
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
  	printf("FLASHAPP_ERROR: cmd35 req failure.\n");
  	exit(1);
  }
  //printf("check again\n");
  if(req.data==REQ_ADDRESS_MODE_BYTE){
 		arg.special*=SEC_SIZE;
  }
  
  lib_build_cmd(&mmc_cmd,35,arg.special,NULL,NULL,0,0,0);
	if(req.data==REQ_ADDRESS_MODE_SEC){
 		arg.special*=SEC_SIZE;
  }  
  
  if(flashops->cmd(&mmc_cmd)<0){
  	printf("FLASHAPP ERR: cmd35 failure.\n");
  	exit(1);
  }
  r1_res=lib_r1_check(mmc_cmd.restoken);
  
  if(r1_res.error){
  	printf("Cmd35 failed. Error code: 0x%x\n",r1_res.error);
  	g_failFlag=1;
  }
  printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));
  
  flashops->close();
  
  if(g_failFlag){
  	exit(1);
  }
  
 // exit(1);
  return 0;	
}


