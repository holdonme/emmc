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

#define BUF_SIZE    (512 * 1024)
#define CHUNK_SIZE  (256 * 1024)

extern char *optarg;
extern int optind;

int TC1_CurLoop = 0;
int TC2_CurLoop = 0;
int TC3_CurLoop = 0;
int TC4_CurLoop = 0;
int TC5_CurLoop = 0;
int TC6_CurLoop = 0;
int TC7_CurLoop = 0;
int TC8_CurLoop = 0;
int TC9_CurLoop = 0;

static uint32_t getRandNum(){
	srand((unsigned)time(NULL));
	return(rand());
}

static int data_dump(uint8_t * buf, uint32_t size){
	int i = 1;
	for (i = 1; i < size; i++){
		printf("%3x", *(buf+i-1));
		if(i%32 == 0)
			printf("\n");
	}
	printf("\n");
	return 1;
}

static int compare_4kdata(uint32_t addr, uint8_t pattern){
	uint32_t size=4096;
	uint8_t *pbuf = malloc(size);
	int i;
	int ret = 0;
	ret = flash_read(addr, pbuf, 4096, 4096);
	if(ret){
		printf("flash read error!\n");
		free(pbuf);
		return -1;
	}
	for (i = 0; i < size; i++){
		if(*(pbuf + i) != pattern) {
			printf("sector addr 0x%x compare error!\n",addr);
			data_dump(pbuf, size);
			return -2;
		}
	}
	free(pbuf);
	return 0;
} 

int eCSDLoad(uint8_t *buf)
{
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
	if(flashops->open()<0)
		return -1;

	lib_build_cmd(&mmc_cmd, 8, 0,NULL, buf, SEC_SIZE,0,0);

	if(flashops->cmd(&mmc_cmd)<0)
		return -1;

	r1_res=lib_r1_check(mmc_cmd.restoken);

	if(r1_res.error){
		printf("Cmd8 failed. Error code: 0x%x\n",r1_res.error);
		return -1;
	}

	printf("Response status 0x%x %s\n",r1_res.sts,lib_decode_status(r1_res.sts));

	flashops->close();
	return 0;
}


void help_menu(char *name)
{
	printf("Usage: %s -t NUM -n Loop_Count [-p|h]\n", name);
	printf("Options:\n"); 
	printf("-t : Test Case ID \n");
	printf("-n : Test Loop count\n");
	printf("-p : precondition don't need to be configured on DUT\n");
	printf("-h : help list\n\n");
	printf("TC ID       Description\n");
	printf("    1       powerloss during write when BKOPS>=2\n");
	printf("    2       powerloss during manual BKOPS\n");
	printf("    3       powerloss during write HPI\n");
	printf("    4       powerloss during manual BKOPS HPI\n");
	printf("    5       powerloss during PON\n");
	printf("    6       powerloss during cache flush\n");
	printf("    7       powerloss during idle/auto-standby\n");
	printf("    8       stress powerloss during power on\n");
	printf("    9       powerloss during sleep/wake up\n");
}

int TC1_task(uint32_t givenLoopCnt)
{
/***TC1: power loss during write when BKOPS >= 2***/
	int loop_cnt = 5000;
	uint32_t sec_addr = 0;
	uint32_t delay_us = 0;
	char cmd_buf[128];
	uint8_t *buf, *pbuf;
	uint8_t pattern;
	int bufsize = 4096; //64K
	int i;
	uint8_t eCSDTable[SEC_SIZE];
	int ret = 0;
	pid_t pid;

	buf=malloc(bufsize);
	if(givenLoopCnt != 0)
		loop_cnt = givenLoopCnt;

	
	while(TC1_CurLoop < loop_cnt)
	{
		TC1_CurLoop++;

		printf("\n***[index %d] TC1_S1. PL during rand write with BKOPS > 2 ***\n", TC1_CurLoop);
		/*1GB test span, 4KB aligned*/
		sec_addr = getRandNum() % 0x200000;
		sec_addr = sec_addr/8;
		sec_addr = sec_addr*8;
		/*delay with in 3ms*/
		//delay_us = getRandNum() % 5000;
		delay_us = 3000 * TC1_CurLoop / loop_cnt;
		//sprintf(cmd_buf, "power -a 2 -w 2 -d %d", delay_us);
		sprintf(cmd_buf, "power_loss_config -d %d", delay_us);
		
		/*the pattern is same for 32MB span by using corresponding address bits*/
		pattern = (sec_addr >> 16) & 0xFF;
		for(pbuf = buf, i = 0; i < bufsize; i++){
			*(pbuf++) = pattern;
		}
		
		system(cmd_buf);
		ret = flash_write(sec_addr, buf, bufsize, 4096);
		if(ret)
			printf("write failure due to PL!\n");


		printf("\n*** [index %d] TC1_S2. power recovery, PL again during CMD1 ***\n", TC1_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000;
		/*reset FPGA host*/
		System("dump_reg -a 12");
		System("power -a 1");
		System("cmd0");

		fflush(stdout);	
		if( (pid = fork() ) < 0){ 
			printf("fork error! errno %d\n", errno);
			goto err;
		}
		if( pid == 0){
		//In child thread
			ret = system("cmd1");
			if(ret)
				printf("In child thread, cmd1 error!\n");
			else
				printf("In child thread, cmd1 over\n");
			exit(0);
		}
		// In parent thread
		select_timer(delay_us);
		System("power -a 0");
		wait(NULL);


		printf("\n*** [index %d] TC1_S3. power recovery, PL again immediately after power on ***\n", TC1_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		system("dump_reg -a 12");
		system("power -a 1");
		select_timer(delay_us);
		system("power -a 0");


		printf("\n*** [index %d] TC1_S4. power-up and check flash data ***\n", TC1_CurLoop);
		System("power -a 1");
		System("dump_reg -a 12");
		System("mmcinit");
		if(compare_4kdata(sec_addr, pattern)) {
			printf("data corrupted!\n");
			goto err;
		}
		if (eCSDLoad(eCSDTable)!=0) {
			printf("read eCSD table failed!\n");
			goto err;
		}
		if(eCSDTable[246] < 2) {
			free(buf);
			return(1);
		}
	}
	free(buf);
	return(0);
err:
	free(buf);
	return(-1);
}

int TC2_task(uint32_t givenLoopCnt)
{
/***TC2: power during manual BKOPS***/
	int loop_cnt = 1000;
	uint32_t delay_us = 0;
	char cmd_buf[128];
	uint8_t eCSDTable[SEC_SIZE];
	pid_t pid;
	int ret = 0;
	
	if(givenLoopCnt != 0)
		loop_cnt = givenLoopCnt;
	
	while(TC2_CurLoop < loop_cnt)
	{
		TC2_CurLoop++;

		printf("\n***[index %d] TC2_S1. PL during manual BKOPS ***\n", TC2_CurLoop);
		fflush(stdout);	
		/*delay within 3ms*/
		delay_us = getRandNum() % 3000;
		sprintf(cmd_buf, "power -a 2 -w 2 -d %d", delay_us);

		/*in parent process*/
		fflush(stdout);	
		if( (pid = fork() ) < 0){
			printf("fork error! errno %d\n", errno);
			goto err;
		}
		if (pid == 0){
			/*in child process*/
			/*start BKOPS*/
			System("cmd6 -m 3 -a 164 -d 1");
			exit(0);
		}
		/*in parent process*/
		System(cmd_buf);
		wait(NULL);


		printf("\n*** [index %d] TC2_S2. power recovery, PL again during CMD1 ***\n", TC2_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		System("dump_reg -a 12");
		System("power -a 1");
		System("cmd0");

		fflush(stdout);	
		if( (pid = fork() ) < 0){ 
			printf("fork error! errno %d\n", errno);
			goto err;
		}
		if( pid == 0){
		//In child thread
			ret = system("cmd1");
			if(ret)
				printf("In child thread, cmd1 error!\n");
			else
				printf("In child thread, cmd1 over\n");
			fflush(stdout);	
			exit(0);
		}
		// In parent thread
		select_timer(delay_us);
		System("power -a 0");
		wait(NULL);


		printf("\n*** [index %d] TC2_S3. power recovery, PL again immediately after power on ***\n", TC2_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		system("dump_reg -a 12");
		system("power -a 1");
		select_timer(delay_us);
		system("power -a 0");


		printf("\n*** [index %d] TC2_S4. power-up and check eCSD ***\n", TC2_CurLoop);
		sleep(2);
		System("power -a 1");
		System("dump_reg -a 12");
		System("mmcinit");
		if (eCSDLoad(eCSDTable)!=0) {
			printf("read eCSD table failed!\n");
			goto err;
		}
		if(eCSDTable[246] < 2) {
			return(1);
		}
	}
	return(0);
err:
	return(-1);
}

int TC3_task(uint32_t givenLoopCnt)
{
/***TC3: power loss during HPI during write when card dirty***/
	int loop_cnt = 1000;
	uint32_t sec_addr = 0;
	uint32_t delay_us = 0;
	char cmd_buf[128];
	uint8_t *buf, *pbuf;
	uint8_t pattern;
	int bufsize = 4096; //64K
	int i;
	uint8_t eCSDTable[SEC_SIZE];
	pid_t pid;
	int ret = 0;

	buf=malloc(bufsize);
	
	if(givenLoopCnt != 0)
		loop_cnt = givenLoopCnt;
	
	while(TC3_CurLoop < loop_cnt)
	{
		TC3_CurLoop++;

		printf("\n***[index %d] TC3_S1. PL to HPI during write busy ***\n", TC3_CurLoop);
		fflush(stdout);	
		
		/*PL with in 3ms after HPI cmd12*/
		delay_us = getRandNum() % 3000;
		sprintf(cmd_buf, "power -a 2 -w 2 -d %d", delay_us);

		/*1GB test span, 4KB aligned*/
		sec_addr = getRandNum() % 0x200000;
		sec_addr = sec_addr/8;
		sec_addr = sec_addr*8;
		
		/*the pattern is same for 32MB span by using corresponding address bits*/
		pattern = (sec_addr >> 16) & 0xFF;
		for(pbuf = buf, i = 0; i < bufsize; i++){
			*(pbuf++) = pattern;
		}

		/* HPI active */
		system("cmd6 -m 3 -a 161 -d 0x1");
		sleep(2);
	
		ret = flash_write_CMD12_R1(sec_addr, buf, bufsize, 4096);
		/*reset FPGA controller logic*/
		system("dump_reg -a 15");
		system("cmd13 -a 0x10001");
		System(cmd_buf);

		
		printf("\n*** [index %d] TC3_S2. power recovery, PL again during CMD1 ***\n", TC3_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		System("dump_reg -a 12");
		System("power -a 1");
		System("cmd0");

		fflush(stdout);	
		if( (pid = fork() ) < 0){ 
			printf("fork error! errno %d\n", errno);
			goto err;
		}
		if( pid == 0){
		//In child thread
			ret = system("cmd1");
			if(ret)
				printf("In child thread, cmd1 error!\n");
			else
				printf("In child thread, cmd1 over\n");
			fflush(stdout);	
			exit(0);
		}
		// In parent thread
		select_timer(delay_us);
		System("power -a 0");
		wait(NULL);

		printf("\n*** [index %d] TC3_S3. power recovery, PL again immediately after power on ***\n", TC3_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		system("dump_reg -a 12");
		system("power -a 1");
		select_timer(delay_us);
		system("power -a 0");


		printf("\n*** [index %d] TC3_S4. power-up and check flash data ***\n", TC3_CurLoop);
		System("power -a 1");
		System("dump_reg -a 12");
		System("mmcinit");
		if(compare_4kdata(sec_addr, pattern)) {
			printf("data corrupted!\n");
			goto err;
		}
		if (eCSDLoad(eCSDTable)!=0) {
			printf("read eCSD table failed!\n");
			goto err;
		}
		if(eCSDTable[246] < 2) {
			free(buf);
			return(1);
		}
	}
	free(buf);
	return(0);
err:
	free(buf);
	return(-1);
}

int TC4_task(uint32_t givenLoopCnt)
{
/***TC4: power loss during HPI during BKOPS when card dirty***/
	int loop_cnt = 1000;
	uint32_t delay_us = 0;
	char cmd_buf[128];
	uint8_t eCSDTable[SEC_SIZE];
	pid_t pid;
	int ret = 0;
	
	if(givenLoopCnt != 0)
		loop_cnt = givenLoopCnt;
	
	while(TC4_CurLoop < loop_cnt)
	{
		TC4_CurLoop++;

		printf("\n***[index %d] TC4_S1. PL to HPI during BKOPS ***\n", TC4_CurLoop);
		/*delay within 3ms*/
		delay_us = getRandNum() % 3000;
		sprintf(cmd_buf, "power -a 2 -w 2 -d %d", delay_us);
		
		/* HPI active */
		system("cmd6 -m 3 -a 161 -d 0x1");
		sleep(2);
		fflush(stdout);	
		if( (pid = fork() ) < 0 ){
			printf("fork error! errno %d\n", errno);
			return(-1);
		}
		if (pid == 0){
			/*in child process, start BKOPS*/
			System("cmd6 -m 3 -a 164 -d 1");
			exit(0);
		}
		/*in parent process*/
		select_timer(1000);
		system("cmd13 -a 0x10001");
		System(cmd_buf);
		wait(NULL);

		
		printf("\n*** [index %d] TC4_S2. power recovery, PL again during CMD1 ***\n", TC4_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		System("dump_reg -a 12");
		System("power -a 1");
		System("cmd0");

		fflush(stdout);	
		if( (pid = fork() ) < 0){ 
			printf("fork error! errno %d\n", errno);
			return(-1);;
		}
		if( pid == 0){
		//In child thread
			ret = system("cmd1");
			if(ret)
				printf("In child thread, cmd1 error!\n");
			else
				printf("In child thread, cmd1 over\n");
			fflush(stdout);	
			exit(0);
		}
		// In parent thread
		select_timer(delay_us);
		System("power -a 0");
		wait(NULL);


		printf("\n*** [index %d] TC4_S3. power recovery, PL again immediately after power on ***\n", TC4_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		system("dump_reg -a 12");
		system("power -a 1");
		select_timer(delay_us);
		system("power -a 0");


		printf("\n*** [index %d] TC4_S4. power-up and check flash data ***\n", TC4_CurLoop);
		System("power -a 1");
		System("dump_reg -a 12");
		System("mmcinit");
		if (eCSDLoad(eCSDTable)!=0) {
			printf("read eCSD table failed!\n");
			return(-1);
		}
		if(eCSDTable[246] < 2) {
			return(1);
		}
	}
	return(0);
}

int TC5_task(uint32_t givenLoopCnt)
{
/***TC5: power loss during PON busy***/
	int loop_cnt = 5000;
	uint32_t sec_addr = 0;
	uint32_t delay_us = 0;
	char cmd_buf[128];
	uint8_t *buf, *pbuf;
	uint8_t pattern;
	int bufsize = 4096; //64K
	int i;
	uint8_t eCSDTable[SEC_SIZE];
	pid_t pid;
	int ret = 0;

	buf=malloc(bufsize);
	
	if(givenLoopCnt != 0)
		loop_cnt = givenLoopCnt;
	
	while(TC5_CurLoop < loop_cnt)
	{
		TC5_CurLoop++;

		printf("\n***[index %d] TC5_S1. PL during PON ***\n", TC5_CurLoop);
		/*1GB test span, 4KB aligned*/
		sec_addr = getRandNum() % 0x200000;
		sec_addr = sec_addr/8;
		sec_addr = sec_addr*8;
		/*the pattern is same for 32MB span by using corresponding address bits*/
		pattern = (sec_addr >> 16) & 0xFF;
		for(pbuf = buf, i = 0; i < bufsize; i++){
			*(pbuf++) = pattern;
		}
		
		/*turned on PON and then write data*/
		System("cmd6 -m 3 -a 34 -d 1");
		sleep(2);
		ret = flash_write(sec_addr, buf, bufsize, 4096);
		if(ret) {
			printf("write failure!\n");
			goto err;
		}
		
		/*delay less than 3ms*/
		delay_us = getRandNum() % 3000;
		sprintf(cmd_buf, "power -a 2 -w 2 -d %d", delay_us);
	
		fflush(stdout);	
		if( (pid = fork() ) < 0 ){
			printf("fork error! errno %d\n", errno);
			goto err;
		}
		if (pid == 0){
			/*in child process, issue PON*/
			System("cmd6 -m 3 -a 34 -d 3");
			exit(0);
		}
		/*in parent process*/
		System(cmd_buf);
		wait(NULL);

		
		printf("\n*** [index %d] TC5_S2. power recovery, PL again during CMD1 ***\n", TC5_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		System("dump_reg -a 12");
		System("power -a 1");
		System("cmd0");

		fflush(stdout);	
		if( (pid = fork() ) < 0){ 
			printf("fork error! errno %d\n", errno);
			goto err;
		}
		if( pid == 0){
		//In child thread
			ret = system("cmd1");
			if(ret)
				printf("In child thread, cmd1 error!\n");
			else
				printf("In child thread, cmd1 over\n");
			fflush(stdout);
			exit(0);
		}
		// In parent thread
		select_timer(delay_us);
		System("power -a 0");
		wait(NULL);


		printf("\n*** [index %d] TC5_S3. power recovery, PL again immediately after power on ***\n", TC5_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		system("dump_reg -a 12");
		system("power -a 1");
		select_timer(delay_us);
		system("power -a 0");


		printf("\n*** [index %d] TC5_S4. power-up and check flash data ***\n", TC5_CurLoop);
		System("power -a 1");
		System("dump_reg -a 12");
		System("mmcinit");
		if(compare_4kdata(sec_addr, pattern)) {
			printf("data corrupted!\n");
			goto err;
		}
		if (eCSDLoad(eCSDTable)!=0) {
			printf("read eCSD table failed!\n");
			goto err;
		}
	}
	free(buf);
	return(0);
err:
	free(buf);
	return(-1);
}

int TC6_task(uint32_t givenLoopCnt)
{
/***TC6: power loss during cache flush***/
	int loop_cnt = 1000;
	int write_loop = 5;
	uint32_t sec_addr[5], pattern[5];
	uint32_t delay_us = 0;
	char cmd_buf[128];
	uint8_t *buf, *pbuf;
	int bufsize = 4096; //64K
	int i, j;
	uint8_t eCSDTable[SEC_SIZE];
	pid_t pid;
	int ret = 0;

	buf=malloc(bufsize);
	
	if(givenLoopCnt != 0)
		loop_cnt = givenLoopCnt;
	
	while(TC6_CurLoop < loop_cnt)
	{
		TC6_CurLoop++;

		printf("\n***[index %d] T6_S1. 5 times rand write ***\n", TC6_CurLoop);
		/*enable cache*/
		System("cmd6 -m 3 -a 33 -d 1");
		sleep(2);
		
		for (i = 0; i != write_loop; i++) {
			/*1GB test span, 4KB aligned*/
			sec_addr[i] = getRandNum() % 0x200000;
			sec_addr[i] = sec_addr[i]/8;
			sec_addr[i] = sec_addr[i]*8;
			/*the pattern is same for 32MB span by using corresponding address bits*/
			pattern[i] = (sec_addr[i] >> 16) & 0xFF;
			for(pbuf = buf, j = 0; j < bufsize; j++){
				*(pbuf++) = pattern[i];
			}
			ret = flash_write(sec_addr[i], buf, bufsize, 4096);
			if(ret) {
				printf("write failure!\n");
				goto err;
			}
		}


		printf("\n*** [index %d] TC6_S2. PL during flush cache ***\n", TC6_CurLoop);
		/*delay less than 3 ms*/
		delay_us = getRandNum() % 3000;
		sprintf(cmd_buf, "power -a 2 -w 2 -d %d", delay_us);
	
		fflush(stdout);	
		if( (pid = fork() ) < 0 ){
			printf("fork error! errno %d\n", errno);
			goto err;
		}
		if (pid == 0){
			/*in child process, flush cache*/
			System("cmd6 -m 3 -a 32 -d 1");
			exit(0);
		}
		/*in parent process*/
		System(cmd_buf);
		wait(NULL);

		
		printf("\n*** [index %d] TC6_S3. power recovery, PL again during CMD1 ***\n", TC6_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		System("dump_reg -a 12");
		System("power -a 1");
		System("cmd0");

		fflush(stdout);	
		if( (pid = fork() ) < 0){ 
			printf("fork error! errno %d\n", errno);
			goto err;
		}
		if( pid == 0){
		//In child thread
			ret = system("cmd1");
			if(ret)
				printf("In child thread, cmd1 error!\n");
			else
				printf("In child thread, cmd1 over\n");
			fflush(stdout);	
			exit(0);
		}
		// In parent thread
		select_timer(delay_us);
		System("power -a 0");
		wait(NULL);


		printf("\n*** [index %d] TC6_S4. power recovery, PL again immediately after power on ***\n", TC6_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		system("dump_reg -a 12");
		system("power -a 1");
		select_timer(delay_us);
		system("power -a 0");


		printf("\n*** [index %d] TC6_S5. power-up and check flash data ***\n", TC6_CurLoop);
		System("power -a 1");
		System("dump_reg -a 12");
		System("mmcinit");
		if (eCSDLoad(eCSDTable)!=0) {
			printf("read eCSD table failed!\n");
			goto err;
		}
		
		for (i = 0; i != write_loop; i++){
			if(compare_4kdata(sec_addr[i], pattern[i])) {
				printf("data corrupted!\n");
				goto err;
			}
		}

	}
	free(buf);
	return(0);
err:
	free(buf);
	return(-1);
}

int TC7_task(uint32_t givenLoopCnt)
{
/***TC7: power loss during idle to test auto-standby***/
	int loop_cnt = 1000;
	uint32_t sec_addr = 0;
	uint32_t delay_us = 0;
	uint8_t *buf, *pbuf;
	uint8_t pattern;
	int bufsize = 4096; //64K
	int i;
	uint8_t eCSDTable[SEC_SIZE];
	pid_t pid;
	int ret = 0;

	buf=malloc(bufsize);
	
	if(givenLoopCnt != 0)
		loop_cnt = givenLoopCnt;
	
	while(TC7_CurLoop < loop_cnt)
	{
		TC7_CurLoop++;

		printf("\n***[index %d] TC7_S1. PL during idle ***\n", TC7_CurLoop);
		/*1GB test span, 4KB aligned*/
		sec_addr = getRandNum() % 0x200000;
		sec_addr = sec_addr/8;
		sec_addr = sec_addr*8;
		/*the pattern is same for 32MB span by using corresponding address bits*/
		pattern = (sec_addr >> 16) & 0xFF;
		for(pbuf = buf, i = 0; i < bufsize; i++){
			*(pbuf++) = pattern;
		}
		
		/*delay between 35 ms ~ 60ms*/
		delay_us = getRandNum() % 2500;
		delay_us = (delay_us + 3500)*10;
		printf("power loss after idle for %d us!\n", delay_us);
		
		ret = flash_write(sec_addr, buf, bufsize, 4096);
		if(ret) {
			printf("write failure!\n");
			goto err;
		}
		
		select_timer(delay_us);
		system("power -a 0");

		
		printf("\n*** [index %d] TC7_S2. power recovery, PL again during CMD1 ***\n", TC7_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		System("dump_reg -a 12");
		System("power -a 1");
		System("cmd0");

		fflush(stdout);	
		if( (pid = fork() ) < 0){ 
			printf("fork error! errno %d\n", errno);
			goto err;
		}
		if( pid == 0){
		//In child thread
			ret = system("cmd1");
			if(ret)
				printf("In child thread, cmd1 error!\n");
			else
				printf("In child thread, cmd1 over\n");
			fflush(stdout);	
			exit(0);
		}
		// In parent thread
		select_timer(delay_us);
		System("power -a 0");
		wait(NULL);


		printf("\n*** [index %d] TC7_S3. power recovery, PL again immediately after power on ***\n", TC7_CurLoop);
		sleep(2);
		/*PL is booked randomly in 0~3 ms*/
		delay_us = getRandNum() % 3000; 
		/*reset FPGA host*/
		system("dump_reg -a 12");
		system("power -a 1");
		select_timer(delay_us);
		system("power -a 0");


		printf("\n*** [index %d] TC7_S4. power-up and check flash data ***\n", TC7_CurLoop);
		System("power -a 1");
		System("dump_reg -a 12");
		System("mmcinit");
		if (eCSDLoad(eCSDTable)!=0) {
			printf("read eCSD table failed!\n");
			goto err;
		}
		if(compare_4kdata(sec_addr, pattern)) {
			printf("data corrupted!\n");
			goto err;
		}
	}
	free(buf);
	return(0);
err:
	free(buf);
	return(-1);
}

int TC8_task(uint32_t givenLoopCnt)
{
/***TC8: 10k power loss during power on***/
	int loop_cnt = 10000;
	uint32_t delay_us = 0;
	uint8_t eCSDTable[SEC_SIZE];
	pid_t pid;

	printf("\n*** TC8 PL during initialization ***\n");
	
	if(givenLoopCnt != 0)
		loop_cnt = givenLoopCnt;
	
	while(TC8_CurLoop < loop_cnt)
	{
		TC8_CurLoop++;
		printf(">>> TC8 Loop %d\n", TC8_CurLoop);
		/*delay with in 1~5ms*/
		delay_us = getRandNum() % 4000 + 1000;
		System("dump_reg -a 12");
		System("power -a 1");

		fflush(stdout);	
		if( (pid = fork() ) < 0){ 
			printf("fork error! errno %d\n", errno);
			goto err;
		}
		if( pid == 0){
		//In child thread
			system("cmd0");
			system("cmd1");
			system("cmd2");
			system("cmd3");
			system("cmd7");
			exit(0);
		}
		// In parent thread
		select_timer(delay_us);
		system("power -a 0");

		wait(NULL);
		sleep(2);
	}


	system("dump_reg -a 12");
	system("power -a 1");
	system("mmcinit");
	if (eCSDLoad(eCSDTable)!=0) {
		printf("read eCSD table failed!\n");
		goto err;
	}
	
	return(0);
err:
	return(-1);
}

int TC9_task(uint32_t givenLoopCnt)
{
/***TC9: power loss during sleep/wakeup***/
	int loop_cnt = 2000;
	uint32_t delay_us = 0;
	char cmd_buf[128];
	uint8_t eCSDTable[SEC_SIZE];
	pid_t pid;

	printf("\n*** TC9 10 us step PL during sleep wakeup ***\n");
	
	if(givenLoopCnt != 0)
		loop_cnt = givenLoopCnt;
	
	while(TC9_CurLoop < loop_cnt)
	{
		TC9_CurLoop++;
		printf(">>> TC9 Loop %d\n", TC9_CurLoop);

		/*delay with in 5~35ms*/
		delay_us = getRandNum() % 30000 + 5000;
		sprintf(cmd_buf, "power -a 2 -w 2 -d %d", delay_us);
		system("dump_reg -a 12");
		System("power -a 1");
		system("mmcinit");

		fflush(stdout);	
		if( (pid = fork() ) < 0){ 
			printf("fork error! errno %d\n", errno);
			goto err;
		}
		if( pid == 0){
		//In child thread
			system("cmd7 -a 0");
			system("cmd5 -a 0x18000");
			select_timer(10000);
			system("cmd5");
			system("cmd7");
			exit(0);
		}
		// In parent thread
		System(cmd_buf);
		wait(NULL);
	}


	system("dump_reg -a 12");
	system("power -a 1");
	if (eCSDLoad(eCSDTable)!=0) {
		printf("read eCSD table failed!\n");
		goto err;
	}
	
	return(0);
err:
	return(-1);
}

int main(int argc, char *argv[])
{
/*
	1. Component low level PL test
		a. Prepare dirty card, PL test during random write to target eMMC
		b. random write to target eMMC until BKOPS_STATUS = 3, PL test during continuous write operation when auto-BKOPS is trigged
		c. random write to target eMMC until BKOPS_STATUS >= 2, PL test during manual BKOPS
		d. Use HPI to interrupt program / erase operation, PL test during HPI busy
		e. random write to target eMMC until BKOPS_STATUS = 3, Use HPI to interrupt continuous write operation when auto-BKOPS is trigged, PL test during HPI busy
		f. random write to target eMMC until BKOPS_STATUS >= 2, start manual BKOPS,  Use HPI to interrupt BKOPS, PL test during HPI busy
		g. PL test during PON busy
		h. Enable cache on, random write, then flush cache, PL test during cache flush
		i. PL test (interval scan) after device idle for 90ms to 110 ms
		j. After PL test a~i, perform PL test during next time power on initialization
		k. After PL test a~i, perform PL test during next time CMD1 initialization
		l. 10K PL test during power on initialization (power on/off immediately to interrupt init sequence continuously)
		
	2. Others (for specified functions)
		a. PL test during FFU
		b. PL test during sleep/wake up
*/
	uint8_t eCSDTable[SEC_SIZE];
	char cmdbuf[128];
	uint8_t flag_BKOPS = 0;


	uint8_t testCurStep = 0;
	uint32_t totalLoopCnt = 0;	
	uint8_t preCondFlag = 0;
	int ret = 0;
	char c;

	while(optind < argc) {
		c = getopt(argc, argv, "t:n:ph");
		switch(c) {
		case 't':
			testCurStep = lib_valid_digit(optarg);
			break;
		case 'n':
			totalLoopCnt = lib_valid_digit(optarg);
			break;
		case 'p':	
			preCondFlag = 1;
			break;
		case 'h':
		default:
			help_menu(argv[0]);
			return -1;
		}
	}

	System("mmcinit");
	eCSDLoad(eCSDTable);
	if (preCondFlag == 0) {
		sprintf(cmdbuf, "./CreatePreCond_eMMCPLTest.sh");
		System(cmdbuf);
	}

	while(1)
	{
		if(testCurStep == 0){
			break;
		}
		if(testCurStep == 1){
			//TC1: power loss during write when BKOPS >= 2
			flag_BKOPS = 2;
			sprintf(cmdbuf, "./trigBKOPS.sh %d", flag_BKOPS);
			System(cmdbuf);
			ret = TC1_task(totalLoopCnt);
			if(ret < 0)
				goto TC_FAIL;
			else if(ret > 0)
				continue;
			else
				testCurStep=0;
		}
		if(testCurStep == 2){
			//TC2: power loss during manual BKOPS
			flag_BKOPS = 2;
			sprintf(cmdbuf, "./trigBKOPS.sh %d", flag_BKOPS);
			System(cmdbuf);
			ret = TC2_task(totalLoopCnt);
			if(ret < 0)
				goto TC_FAIL;
			else if(ret > 0)
				continue;
			else
				testCurStep=0;
		}
		if(testCurStep == 3){
			//TC3: power loss during HPI during write when card dirty
			flag_BKOPS = 2;
			sprintf(cmdbuf, "./trigBKOPS.sh %d", flag_BKOPS);
			System(cmdbuf);
			ret = TC3_task(totalLoopCnt);
			if(ret < 0)
				goto TC_FAIL;
			else if(ret > 0)
				continue;
			else
				testCurStep=0;
		}
		if(testCurStep == 4){
			//TC4: power loss during HPI during BKOPS when card dirty
			flag_BKOPS = 2;
			sprintf(cmdbuf, "./trigBKOPS.sh %d", flag_BKOPS);
			System(cmdbuf);
			ret = TC4_task(totalLoopCnt);
			if(ret < 0)
				goto TC_FAIL;
			else if(ret > 0)
				continue;
			else
				testCurStep=0;
		}
		if(testCurStep == 5){
			//TC5: power loss during PON busy
			ret = TC5_task(totalLoopCnt);
			if(ret < 0)
				goto TC_FAIL;
			else
				testCurStep=0;
		}
		if(testCurStep == 6){
			//TC6: power loss during cache flush
			ret = TC6_task(totalLoopCnt);
			if(ret < 0)
				goto TC_FAIL;
			else
				testCurStep=0;
		}
		if(testCurStep == 7){
			//TC7: power loss during idle to check ASTDBY
			ret = TC7_task(totalLoopCnt);
			if(ret < 0)
				goto TC_FAIL;
			else
				testCurStep=0;
		}
		if(testCurStep == 8){
			//TC8: 10k power loss during power on
			ret = TC8_task(totalLoopCnt);
			if(ret < 0)
				goto TC_FAIL;
			else
				testCurStep=0;
		}
		if(testCurStep == 9){
			//TC9: power loss during sleep/wakeup
			ret = TC9_task(totalLoopCnt);
			if(ret < 0)
				goto TC_FAIL;
			else
				testCurStep=0;
		}
	}
		
	sprintf(cmdbuf, "./DataValidate_eMMCPLTest.sh");
	System(cmdbuf);
	fflush(stdout);	
	return ret;

TC_FAIL:
	printf("TC failed at step %d\n!", testCurStep);
	fflush(stdout);	
	return ret;
}
