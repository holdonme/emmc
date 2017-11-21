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


struct user_arg arg={0,0x0,0,0,0,1,FLAG_AUTOCMD12};



void help_menu(){
	printf("Usage: mmcrandread [-secn] [arg]\n\n");
  printf("Perform random read, all the address is in sector mode.This function is used for random read performance test.\n\n");
  printf("Options:\n");
  printf("\t-s\tNUM\tspecify start address\n");
  printf("\t-e\tNUM\tspecify end address\n");
  printf("\t-c\tNUM\tspecify chunk size in number of sector(512B)\n");
  printf("\t-n\tNUM\tspecify number of chunks to be random read\n");
/*  printf("\t-d\tNUM\tspecify const data to compare\n");
  printf("\t-u\t\tuse pat data to compare\n");
  printf("\t-i\t\twrite data to pat file\n");
  printf("\t-t\t\tdisplay the time of cmd\n");*/
}

int validate_all_inputs(){
	if(arg.sdata > arg.edata){
		printf("Error: start addr should be larger than end addr.\n");
		return 1;
	}
	return 0;
}



int main(int argc, char * const argv[])
{
  int fd;
  int c,i;
//  struct emmc_sndcmd_req mmc_cmd;
  struct config_req req;
//  struct r1 r1_res;
//  struct block_data bdata;
  
  uint8_t *data=NULL;
  uint *addr=NULL;
//  char file_path[50]={0};
  
  opterr=1;
  g_failFlag=0;
//  int time_flag=0;
  uint chunk=8;
  
  
	
	while(optind<argc){
    c = getopt(argc, argv,"s:e:c:n:h");
   // printf("%c\n",c);
    switch(c){
    	case 's':                               //start address
   		  arg.sdata=lib_valid_digit(optarg);
    	  break;
    	case 'e':                               //end address
   		  arg.edata=lib_valid_digit(optarg);
    	  break;
    	case 'n':                               //number of chunks to be read
   		  arg.special=lib_valid_digit(optarg);
    	  break;
    	case 'c':                               //chunk size in number of 512B
   		  chunk=lib_valid_digit(optarg);
    	  break;
    	case 'h':
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
  	printf("FLASHAPP ERR: mmcread req failure.\n");
  	exit(1);
  }

 	//arg.sdata*=SEC_SIZE;
 	chunk*=SEC_SIZE;
 	
  data=(uint8_t*)malloc(sizeof(uint8_t)*chunk);
  if(!data){
  	printf("FLASHAPP ERR: malloc fail for data array\n");
  }
  memset(data, 0, sizeof(uint8_t)*chunk);
  
  addr=(uint*)malloc(sizeof(uint)*arg.special);
  if(!addr){
  	printf("FLASHAPP ERR: malloc fail for addr array\n");
  }
  memset(addr, 0, sizeof(uint)*arg.special);
  
  //gen a random addr array for random read access
  srand((unsigned)time(NULL));
  for(i=0;i<arg.special;i++){
  	addr[i]=arg.sdata+(rand()%(arg.edata-arg.sdata+1));
  //	printf("mmc debug: 0x%x\n",addr[i]);
  	addr[i]*=SEC_SIZE;//change to byte address
  }
  
  if(flashops->randread(data,addr,arg.special,chunk,req.data)<0){
  	printf("FLASHAPP ERR: mmcrandread failure.\n");
  	exit(1);
  }
 /* if(req.data==REQ_ADDRESS_MODE_SEC){//the print and compare use byte address
 		bdata.saddr*=SEC_SIZE;
  }*/
    
/*  lib_build_cmd(&mmc_cmd,18,bdata.saddr,NULL,bdata.pdata,bdata.len,0,arg.flag);
  if(req.data==REQ_ADDRESS_MODE_SEC){
 		bdata.saddr*=SEC_SIZE;
  }

  
  if(flashops->cmd(&mmc_cmd)<0){
  	printf("FLASHAPP ERR: cmd18 failure.\n");
  	if(bdata.pdata) free(bdata.pdata);
  	exit(1);
  }
  r1_res=lib_r1_check(mmc_cmd.restoken);
  
  
  if(r1_res.error){
  	printf("Cmd18 failed. Error code: 0x%x\n",r1_res.error);
  	g_failFlag=1;
  }
  printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));*/
  
  flashops->close();

/*  if(arg.mode == MODE_PRINT){
  	lib_validate_data_region(&bdata,NULL,0);
  }
  else if(arg.mode == MODE_CONSTCMP){
  	if(lib_validate_data_region(&bdata,NULL,1)){
  		printf("mmcread compare failed.\n");
  	}
  	else{
  		printf("mmcread compare pass.\n");
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
  		printf("mmcread compare failed.\n");
  	}
  	else{
  		printf("mmcread compare pass.\n");
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
	}*/
  free(data);
  free(addr);
  
/*  if(g_failFlag){
  	exit(1);
  }*/
  
  return 0;	
}
