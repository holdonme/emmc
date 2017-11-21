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

#define MODE_RELIABLE 1

extern char *optarg;
extern int optind, opterr, optopt;


struct user_arg arg={0,0,0,0,0,0,0};



void help_menu(){
	printf("Usage: cmd23 [-br] [arg]\n\n");
  printf("emmc cmd23 set block count\n\n");
  printf("Options:\n");
  printf("\t-b\tNUM\tspecify block number\n");
  printf("\t-r\tNUM\tspecify to use reliable write request\n");
}

int validate_all_inputs(){

	return 0;
}



int main(int argc, char * const argv[])
{
  int fd;
  int c;
  struct emmc_sndcmd_req mmc_cmd;
  struct r1 r1_res;
  
  opterr=1;
  g_failFlag=0;
  
  
	
	while(optind<argc){
    c = getopt(argc, argv,"b:r");
   // printf("%c\n",c);
    switch(c){
    	case 'b':                               //arg
   		  arg.special=lib_valid_digit(optarg);
    	  break;
      case 'r':                               //arg
   		  arg.mode=MODE_RELIABLE;
    	  break;
    	case '?':
    	default:
    	  (void)help_menu();
    	  return -1;
    	  break;
    }
  }
  
  if(validate_all_inputs()){
  	return -1;
  }

  fd=flashops->open();
  if(fd<0){
    printf("FLASHAPP_ERROR:open failure.\n");
    exit(1);
  }
  
  if(arg.mode == MODE_RELIABLE) arg.special |= (1<<31);
  
  
  lib_build_cmd(&mmc_cmd,23,arg.special,NULL,NULL,0,0,0);
  
  
  if(flashops->cmd(&mmc_cmd)<0){
  	printf("FLASHAPP ERR: cmd23 failure.\n");
  	exit(1);
  }
  r1_res=lib_r1_check(mmc_cmd.restoken);
  
  if(r1_res.error){
  	printf("Cmd23 failed. Error code: 0x%x\n",r1_res.error);
  	g_failFlag=1;
  }
  printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));
  
  flashops->close();
  
  if(g_failFlag){
  	exit(1);
  }
  
  return 0;	
}
