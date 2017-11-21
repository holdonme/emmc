#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

extern void System(char *buf);

enum chunk_type {
	CHUNK_04K_t = 0,
	CHUNK_08K_t,
	CHUNK_16K_t,
	CHUNK_32K_t,
	CHUNK_64K_t,
	CHUNK_max_t,
};

int chunk_array[CHUNK_max_t] = { 
	 4096/512, 
	 8192/512, 
	16384/512,
	32768/512,
	65536/512,
};

uint32_t chunk_count[CHUNK_max_t+1] = {0};

int do_one_write(uint32_t start_addr, uint32_t end_addr)
{
	char cmd_buf[100];
	sprintf(cmd_buf, "cmd25 -s 0x%x -e 0x%x -c 0x%x -d 0x55 -t", start_addr, end_addr, end_addr-start_addr+1);
	System(cmd_buf);
	return 0;
}

int do_4G_write(void)
{
	int i = 0;
	int count = 0;
	int chunk_size = 0;
	enum chunk_type t;

	uint32_t start_addr = 0;
	uint32_t end_addr = 0;

	char *chunk_name[] = {
		"04K",
		"08K",
		"16K",
		"32K",
		"64K",
		"04M",
	};

	srandom(time(NULL) + getpid());
		
	while(start_addr < 0x800000) {	// FIXME only test first 4GB ?

		if( count % 5 == 0) {
			t = random() % CHUNK_max_t;
			chunk_size = chunk_array[t];	
			chunk_count[t]++;
		}
		else{
			chunk_size =  0x2000;
			chunk_count[CHUNK_max_t]++;
		}

		//printf("CHUNK_max_t %d, t is %d, chunk_size is %d\n", CHUNK_max_t, t, chunk_size);

		end_addr = start_addr + chunk_size - 1; 

		do_one_write(start_addr, end_addr);
	
		start_addr = end_addr + 1;	

		count++;
		//if(count >= 5)
		//	break;
	}

	for( i=0 ; i<(CHUNK_max_t+1); i++){
		printf("%s count: %d\n", chunk_name[i], chunk_count[i]);
	}

	return 0;
}


int main(int argc, char *argv[])
{
	int n = 0;

/*
	int c;
	int percent = 0;

	while(optind<argc){
	   	c = getopt(argc, argv,"p:h");
	    switch(c){
		case 'p': 
			percent = atoi(optarg);
			break;

		case 'h':
		default:
			return -1;
    	}
  	}

	printf("percent %%%d\n",percent );
*/

	for( n=0; n<100; n++){

		if( n % 10 == 0)
			System("cmd56 -g");

		do_4G_write();
	}

	return 0;
}
