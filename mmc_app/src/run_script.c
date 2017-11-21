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

#include "flash_define.h"
#include "flash_lib.h"
#include "flash_ops.h"
#include "mmc-user.h"

char script_path_buf[100] = "/bin/script.sh" ;

pid_t Wait(int *status)
{
	int ret;
	do{
		ret = wait(status);
	}while( (ret == -1) && (errno == EINTR) );

	return ret;
}


int script_power_task(int loop_index, int mode, int value)
{
	int ret = 0;
	char cmd_buf[100];
	int delay = 0;
	pid_t pid;

	if( (pid = fork() ) < 0){ 
		printf("fork error!\n");
		return -1;
	}

	if(pid == 0){	
		// In child thread

		setsid();	// new process group

		printf("In child thread\n");
		sprintf(cmd_buf, "while true; do  %s ; done", script_path_buf);
		ret = system(cmd_buf);

		if(ret)
			printf("In child thread, script run error!\n");
		else
			printf("In child thread, script run over\n");

		_exit(0);
	}

	// In parent thread		
	srandom(time(NULL) + getpid());
	delay = random() % 60;			// delay unit is second
	printf("delay %d seconds to kill child group\n", delay);
	select_timer(delay*1000 * 1000 );

	run_power_down(mode, value);
	ret = kill(-getpgid(pid), SIGKILL);
	printf("send SIGKILL to child group %d,ret %d,parent group is %d\n", getpgid(pid), ret, getpgid(getpid()) );


	printf("In parent thread, cut off power when write [vcc/vccq  %d.%dv]\n", value/10, value%10);


	while( Wait(NULL) >0 ) ;	//wait for all child process

	run_power_up();

	sleep(1);

	ret = system("mmcinit");

	return ret;
}

void help_menu(void)
{
	printf("Usage: run_script -a [mode] -d [time or volt] -f [path] -n [count]\n");
	printf("Options:\n"); 
	printf("-a : set power loss mode\n");
	printf("     2-- power off in ARG us, with option '-d'\n");
	printf("     3-- set voltage to ARG v, with option '-d'\n");
	printf("-d : NUM time (unit us) or\n");
	printf("     NUM volt (unit v)\n");
  	printf("-f : script path, default is /bin/script.sh\n");
	printf("-n : the times of the loop\n");
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int c;
	int fd = 0;

	int mode  = 0;
	int value  = 0;
	int loop = 100;

	int i = 0;

	while(optind<argc){
	   	c = getopt(argc, argv,"f:t:v:q:n:h");
	    switch(c){
		case 'a': 
			mode = lib_valid_digit(optarg);
			break;

		case 'd':
			value = lib_valid_digit(optarg); 
			break;


		case 'f':
			fd = open(optarg, O_RDWR);
			if(fd < 0 ) {
				printf("no such file %s\n", optarg);
				exit(-1);
			}

			close(fd);
			strcpy(script_path_buf, optarg);

			break;

		case 'n':                              
			loop = lib_valid_digit(optarg);
			break;
		case 'h':
			default:
				help_menu();
				return -1;
		}
  	}

	System("mmcinit");
	System("mmcconfig -a");

	for(i = 0; i < loop; i++) {
		ret = script_power_task(i, mode, value);	
		if(ret){
			printf("Powerloss test fail in loop[%d]\n", i);
			return -1;
		}
	}
	return 0;
}
