/*******************************************************************************/
/*   File        : flash_lib.c                                                 */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This file provide the library function for the user         */
/*                 application code.                                           */
/*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <malloc.h>
#include <sys/stat.h> 
#include <errno.h>

#include "flash_define.h"
#include "flash_ops.h"
#include "mmc-user.h"

/*globals*/
int g_fd;
int g_failFlag=0;
int g_expectfail=0;
struct timeval g_begin;
struct timeval g_end;

static const char g_stat[11][10]={"IDLE\0", "READY\0", "IDENT\0", "STBY\0",
								  "TRAN\0", "DATA\0",  "RCV\0",   "PRG\0",
								  "DIS\0",  "BTST\0",  "INVALID\0"};

static const char g_cbx[5][10]={"Card\0","BGA/MCP\0","POP\0","RESERVED\0","INVALID\0"};

/*subs*/
void lib_error(char*, char*, int);
uint lib_valid_digit(const char * ch);
uint lib_convert(char ch);
uint lib_hex2int(const char * ch);
uint lib_validate_data_region(struct block_data* bdata, uint8_t* cdata, uint validate);
uint lib_readPattern2Mem(uint8_t* pData, char* filePath, int size);
uint lib_writePattern2File(uint8_t* pData, char* filePath, int size);
uint lib_build_cmd(struct emmc_sndcmd_req* cmd,
							uint32_t opcode,
							uint32_t argument,
							uint32_t *restoken,
							uint8_t  *data,
							uint32_t len,
							uint32_t time, 
							uint32_t flag);

const char* lib_decode_status(int number);
const char* lib_decode_cbx(int number);
struct r1 lib_r1_check(uint32_t* restoken);
uint32_t lib_get_offset(uint32_t data, int offset, int bit_len);
uint lib_decode_cid(uint32_t* data);
uint lib_decode_csd(uint32_t* data);
uint lib_decode_xcsd(uint8_t* data);
uint lib_getFileSize(char* filePath);

void run_power_down(int mode, float value);
void run_power_up(void);
void System(char *buf);
int select_timer(int usec);

/*******************************************************************************/
/*   Function    : lib_error                                                   */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used for Error infor print                 */
/*                 out, which will trace the file and line for                 */
/*                 debug                                                       */
/*******************************************************************************/
void lib_error(char* err_buf, char* file, int line)
{
	printf("%s --file: %s, line: %d --\n",err_buf, file, line);
}



/*******************************************************************************/
/*   Function    : lib_valid_digit                                             */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used to validate the input ch whether      */
/*                 it is a valid digit input. It can be either in dec or       */
/*                 hex start with '0x'.                                        */
/*******************************************************************************/
uint lib_valid_digit(const char * ch)
{
	int hex_length = strlen(ch);
	int digit_flag=1;
	int i;
	
	for(i=0;i<hex_length;i++){
		if(!isdigit(ch[i])){
			digit_flag=0;
		}
	}
	
	if(digit_flag){
		return atoi(ch);
	}
	else if(ch[0] == '0' && ch[1] == 'x'){
		return lib_hex2int(ch);
	}
	else{ 
		printf("FLASHLIB ERROR: in lib_valid_digit(), not a num [%s]\n", ch); 
		exit(1); 
	}
}

/*******************************************************************************/
/*   Function    : lib_convert                                                 */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used to convert the input ch to            */
/*                 dec number, input ch should be 1 char data for hex.         */
/*******************************************************************************/
uint lib_convert(char ch)
{
	uint t = 0;
 
	switch (ch) {
		case '0':t = 0; break;
		case '1':t = 1; break;
		case '2':t = 2; break;  
		case '3':t = 3; break;
		case '4':t = 4; break;
		case '5':t = 5; break;
		case '6':t = 6; break;
		case '7':t = 7; break;
		case '8':t = 8; break;            
		case '9':t = 9; break;
		case 'A':
		case 'a':t = 10;break;
		case 'B':
		case 'b':t = 11;break;
		case 'C':
		case 'c':t = 12;break;
		case 'D':
		case 'd':t = 13;break;
		case 'E':
		case 'e':t = 14;break;
		case 'F':
		case 'f':t = 15;break;              

		default:
			lib_error("FLASHLIB ERROR: in lib_convert(), not Hex input",__FILE__,__LINE__);
			printf("%c\n",ch);
			return RFAIL;
	}

	return t;
}

/*******************************************************************************/
/*   Function    : lib_hex2int                                                 */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used to convert the input ch to            */
/*                 dec number, input ch should be start with '0x' for a        */
/*                 hex number.                                                 */
/*******************************************************************************/
uint lib_hex2int(const char * ch)
{
	int i, j =0;
	uint sum = 0, f = 0;
	int hex_length = strlen(ch);

	if(!(ch[0] == '0' && ch[1] == 'x')){
		lib_error("FLASHLIB ERROR: in lib_hex2int(), hex number not start 0x",__FILE__,__LINE__);
		exit(1);
	}

	for (i = 2; i < hex_length; i++) {
		f = 1;

		for (j = 1; j < hex_length-i; j++)
			f *= 16;

		sum += lib_convert(ch[i]) * f;
	}

	return sum;
}


/*******************************************************************************/
/*   Function    : lib_validate_data_region                                    */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used for validate data and print info      */
/*******************************************************************************/
uint lib_validate_data_region(struct block_data* bdata, uint8_t* cdata, uint validate)
{
	int inc=0;
	uint failCnt=0;
	uint currentAddr=0;
	uint8_t* tmp=bdata->pdata;
	int i;
	
	while(inc < bdata->len){
		currentAddr=(bdata->saddr)+inc;
    	if(validate){//will do cmp
    		if(cdata){//cmp with cdata blk level array
      			if(*(tmp) != cdata[inc]){
      				if(failCnt<=MAXFAILCNT)
     	  	  			printf("Validate fail addr:0x%x, expect 0x%x, actual 0x%x\n",(bdata->saddr)+inc, cdata[inc], *(tmp));
    	    		failCnt++;
    	  		}
      		} 
			else{//cmp with bdata cdata solid data 
				if(*(tmp) != bdata->cdata){ 
					if(failCnt<=MAXFAILCNT) 
						printf("Validate fail addr:0x%x, expect 0x%x, actual 0x%x\n",(bdata->saddr)+inc, bdata->cdata, *(tmp)); 
					failCnt++; 
				}
			}
    	} 
		else{//validate is Null 
			if(inc==0){ 
				if(bdata->saddr>0x100000)
					printf("\t");
				printf("\t0x0\t0x1\t0x2\t0x3\t0x4\t0x5\t0x6\t0x7\t0x8\t0x9\t0xa\t0xb\t0xc\t0xd\t0xe\t0xf\n");
				printf("0x%x\t",((bdata->saddr)/0x10)*0x10);
				for(i=0;i<(bdata->saddr)%0x10;i++)
					printf("\t");
      		} 
			if(currentAddr%0x10==0 && inc!=0)
				printf("0x%x\t",currentAddr); 
			printf("0x%x\t",*(tmp)); 
			if(currentAddr%0x10==0xf)//last data print enter
      			printf("\n");
    	}
		tmp++;
		inc++; 
	}
	
	if(!validate) 
		printf("\n");
  
	if(validate && failCnt){
		if(failCnt>MAXFAILCNT)
			printf("Warning: too many fail addresses, only print 0x%x failures\n",MAXFAILCNT);

		if(!g_expectfail)
      		g_failFlag=1;

		printf("Validate fail, failnum = 0x%x\n",failCnt);
		return failCnt;
	}
	
	return 0;
}


/*******************************************************************************/
/*   Function    : lib_readPattern2Mem                                         */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used to read the pattern file into the     */
/*                 program memory point pData based on the filePath and size.  */
/*                 data point pData should be allocate outside for enough      */
/*                 memory size to prevent the memory leakage.                  */
/*******************************************************************************/
uint lib_readPattern2Mem(uint8_t* pData, char* filePath, int size)
{
	FILE* fp;
	int i=0;
	
	if(!pData){ 
		lib_error("FLASHLIB ERROR: in lib_readPattern2Mem(), memory alloc failed",__FILE__,__LINE__); 
		return RFAIL; 
	}
	
	if((fp=fopen(filePath, "rb")) == NULL) { 
		lib_error("FLASHLIB ERROR: in lib_readPattern2Mem(), cannot open file.\n",__FILE__,__LINE__); 
		return RFAIL; 
	} 

	for(i=0;i<size;i++){ 
		fread(&pData[i], 1, 1, fp); 
		pData[i]&=0xff;   
	}
	
	fclose(fp); 
	return 0;
}

/*******************************************************************************/
/*   Function    : lib_getFileSize                                             */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used to get the file size in bytes         */
/*******************************************************************************/
uint lib_getFileSize(char* filePath)
{
	struct stat st; 
	if(stat(filePath,&st)==0)
		return st.st_size; 

	return 0;
}



/*******************************************************************************/
/*   Function    : lib_writePattern2File                                       */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used to write the pattern file from the    */
/*                 program memory point pData based on the filePath and size.  */
/*                 data point pData array size should be aligned with size     */
/*                 to prevent the invalid data write to pattern file.          */
/*******************************************************************************/
uint lib_writePattern2File(uint8_t* pData, char* filePath, int size)
{
	FILE* fp;
	int i=0;

	if((fp=fopen(filePath, "wb")) == NULL){
		lib_error("FLASHLIB ERROR: in lib_writePattern2File(), cannot open file.\n",__FILE__,__LINE__);
		return RFAIL;
	}

	for(i=0;i<size;i++)
		fwrite(&pData[i], 1, 1, fp);

	fclose(fp);

	return 0;
}


/*******************************************************************************/
/*   Function    : lib_build_cmd                                               */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This function is used for build emmc_sndcmd_req struct.     */
/*******************************************************************************/
uint lib_build_cmd(struct emmc_sndcmd_req* cmd,
						uint32_t opcode,
						uint32_t argument,
						uint32_t *restoken,
						uint8_t  *data,
						uint32_t len,
						uint32_t time, 
						uint32_t flag)
{
	int byte;

	cmd->opcode=opcode;
	cmd->argument=argument;

	if(restoken!=NULL){ 
		for(byte=0;byte<EMMC_RESTOKEN_MAXLEN;byte++) 
			cmd->restoken[byte]=restoken[byte];
	}

	cmd->data=data;
	cmd->len=len;
	cmd->time=time;
	cmd->flag=flag;

	return 0;
}

/*******************************************************************************/
/*   Function    : lib_r1_check                                                */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2011-07-01                                                  */
/*   Description : This function is used for check the r1 response             */
/*                 status and error bits                                       */
/*******************************************************************************/
struct r1 lib_r1_check(uint32_t *restoken)
{
	struct r1 r1_res;
	r1_res.error = restoken[0] & R1_ERROR;
	r1_res.sts   = (restoken[0] & R1_STS)>>9;
	
	return r1_res;
}


/*******************************************************************************/
/*   Function    : lib_decode_status                                           */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2011-07-01                                                  */
/*   Description : This function is used to decode the status of R1 response   */
/*                 for status.                                                 */
/*******************************************************************************/
const char* lib_decode_status(int number)
{
	if(number<0 || number >10)
		return g_stat[10];

	return g_stat[number];
}

/*******************************************************************************/
/*   Function    : lib_decode_cbx                                              */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2011-07-01                                                  */
/*   Description : This function is used to decode the status cbx in CID       */
/*******************************************************************************/
const char* lib_decode_cbx(int number)
{
	if(number<0 || number >4) 
		return g_cbx[4];

	return g_cbx[number];
}



/*******************************************************************************/
/*   Function    : lib_get_offset                                              */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2011-07-01                                                  */
/*   Description : This function is used to get the offset of part of data.    */
/*******************************************************************************/
uint32_t lib_get_offset(uint32_t data, int offset, int bit_len)
{
	int bits,i;
	bits=0;
	for(i=0;i<32;i++){
		if(i>=offset && i<offset+bit_len){
			bits|=(1<<i);
		}
	}

	return ((data&bits)>>offset);
}


/*******************************************************************************/
/*   Function    : lib_decode_cid                                              */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2011-07-01                                                  */
/*   Description : This function is used to decode the cid.                    */
/*******************************************************************************/
uint lib_decode_cid(uint32_t* data)
{
	char pnm[7];
	
	pnm[0]=(char)lib_get_offset(data[0],0,8);
	pnm[1]=(char)lib_get_offset(data[1],24,8);
	pnm[2]=(char)lib_get_offset(data[1],16,8);
	pnm[3]=(char)lib_get_offset(data[1],8,8);
	pnm[4]=(char)lib_get_offset(data[1],0,8);
	pnm[5]=(char)lib_get_offset(data[2],24,8);
	pnm[6]='\0';
	
	printf("Card ID info:\n");
	printf("\tMDT:\t\t0x%x\n",lib_get_offset(data[3],8,8));
	printf("\tPSN:\t\t0x%x%x\n",lib_get_offset(data[2],0,16),lib_get_offset(data[3],16,16));
	printf("\tPRV:\t\t0x%x\n",lib_get_offset(data[2],16,8));
	printf("\tPNM:\t\t%s\n",pnm);
	printf("\tOEM:\t\t0x%x\n",lib_get_offset(data[0],8,8));
	printf("\tCard/BGA:\t%s\n",lib_decode_cbx(lib_get_offset(data[0],16,2)));
	printf("\tMID:\t\t0x%x\n",lib_get_offset(data[0],24,8));
	
	return 0;
}

/*******************************************************************************/
/*   Function    : lib_decode_csd                                              */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2011-07-01                                                  */
/*   Description : This function is used to decode the csd.                    */
/*******************************************************************************/
uint lib_decode_csd(uint32_t* data)
{
	uint32_t csize= (lib_get_offset(data[1],0,10)<<2) | lib_get_offset(data[2],30,2);

	printf("CSD info:\n");
	printf("\tCSD_STRUCTURE:\t\t0x%x\n",lib_get_offset(data[0],30,2));
	printf("\tSPEC_VERS:\t\t0x%x\n",lib_get_offset(data[0],26,4));
	printf("\tTAAC:\t\t\t0x%x\n",lib_get_offset(data[0],16,8));
	printf("\tNSAC:\t\t\t0x%x\n",lib_get_offset(data[0],8,8));
	printf("\tTRAN_SPEED:\t\t0x%x\n",lib_get_offset(data[0],0,8));
	printf("\tCCC:\t\t\t0x%x\n",lib_get_offset(data[1],20,12));
	printf("\tREAD_BL_LEN:\t\t0x%x\n",lib_get_offset(data[1],16,4));
	printf("\tREAD_BL_PARTIAL:\t0x%x\n",lib_get_offset(data[1],15,1));
	printf("\tWRITE_BLK_MISALIGN:\t0x%x\n",lib_get_offset(data[1],14,1));
	printf("\tREAD_BLK_MISALIGN:\t0x%x\n",lib_get_offset(data[1],13,1));
	printf("\tDSR_IMP:\t\t0x%x\n",lib_get_offset(data[1],12,1));
	printf("\tC_SIZE:\t\t\t0x%x\n",csize);
	printf("\tVDD_R_CURR_MIN:\t\t0x%x\n",lib_get_offset(data[2],27,3));
	printf("\tVDD_R_CURR_MAX:\t\t0x%x\n",lib_get_offset(data[2],24,3));
	printf("\tVDD_W_CURR_MIN:\t\t0x%x\n",lib_get_offset(data[2],21,3));
	printf("\tVDD_W_CURR_MAX:\t\t0x%x\n",lib_get_offset(data[2],18,3));
	printf("\tC_SIZE_MULT:\t\t0x%x\n",lib_get_offset(data[2],15,3));
	printf("\tERASE_GRP_SIZE:\t\t0x%x\n",lib_get_offset(data[2],10,5));
	printf("\tERASE_GRP_MULT:\t\t0x%x\n",lib_get_offset(data[2],5,5));
	printf("\tWP_GRP_SIZE:\t\t0x%x\n",lib_get_offset(data[2],0,5));
	printf("\tWP_GRP_ENABLE:\t\t0x%x\n",lib_get_offset(data[3],31,1));
	printf("\tDEFAULT_ECC:\t\t0x%x\n",lib_get_offset(data[3],29,2));
	printf("\tR2W_FACTOR:\t\t0x%x\n",lib_get_offset(data[3],26,3));
	printf("\tWRITE_BL_LEN:\t\t0x%x\n",lib_get_offset(data[3],22,4));
	printf("\tWRITE_BL_PARTIAL:\t0x%x\n",lib_get_offset(data[3],21,1));
	printf("\tCONTENT_PROT_APP:\t0x%x\n",lib_get_offset(data[3],16,1));
	printf("\tFILE_FORMAT_GRP:\t0x%x\n",lib_get_offset(data[3],15,1));
	printf("\tCOPY:\t\t\t0x%x\n",lib_get_offset(data[3],14,1));
	printf("\tPERM_WRITE_PROTECT:\t0x%x\n",lib_get_offset(data[3],13,1));
	printf("\tTMP_WRITE_PROTECT:\t0x%x\n",lib_get_offset(data[3],12,1));
	printf("\tFILE_FORMAT:\t\t0x%x\n",lib_get_offset(data[3],10,2));
	printf("\tECC:\t\t\t0x%x\n",lib_get_offset(data[3],8,2));
		
	return 0;
}


/*******************************************************************************/
/*   Function    : lib_decode_xcsd                                             */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2011-07-01                                                  */
/*   Description : This function is used to decode the csd.                    */
/*******************************************************************************/
uint lib_decode_xcsd(uint8_t* data)
{
	uint32_t c_prg_s_num=data[242]|data[243]<<8|data[244]<<16|data[245]<<24;
	uint32_t s_count=data[212]|data[213]<<8|data[214]<<16|data[215]<<24;
	uint32_t max_e_mult=data[157]|data[158]<<8|data[159]<<16;	
	uint32_t e_s_addr=data[136]|data[137]<<8|data[138]<<16|data[139]<<24;		
	uint32_t s_cache_size=data[249]|data[250]<<8|data[251]<<16|data[252]<<24;
	uint32_t ffu_arg   =data[487]|data[488]<<8|data[489]<<16|data[490]<<24;
	uint32_t device_version=data[262]|data[263]<<8;
	uint32_t pre_load_size=data[22]|data[23]<<8|data[24]<<16|data[25]<<24;
	uint32_t max_pre_load_size=data[18]|data[19]<<8|data[20]<<16|data[21]<<24;
	uint32_t num_fw_sect_prog = data[302]|data[303]<<8|data[304]<<16|data[305]<<24;
	uint64_t FW_version = (uint64_t)(data[254])\
						| (uint64_t)(data[255])<<8\
						| (uint64_t)(data[256])<<16\
						| (uint64_t)(data[257])<<24\
						| (uint64_t)(data[258])<<32\
						| (uint64_t)(data[259])<<40\
						| (uint64_t)(data[260])<<48\
						| (uint64_t)(data[261])<<56;
	uint32_t e_s_mult=data[140]|data[141]<<8|data[142]<<16;

	uint32_t exception_ctrl=data[56]|data[57]<<8;
	uint32_t exception_status=data[54]|data[55]<<8;;
	uint32_t ext_partition_attr=data[52]|data[53]<<8;;

	printf("XCSD info:\n");
	printf("\tEXT_SECURITY_ERR:\t\t\t0x%x\t\t%d\n",data[505],505);
	printf("\tS_CMD_SET:\t\t\t\t0x%x\t\t%d\n",data[504],504);
	printf("\tHPI_FEATURES:\t\t\t\t0x%x\t\t%d\n",data[503],503);
	printf("\tBKOPS_SUPPORT:\t\t\t\t0x%x\t\t%d\n",data[502],502);
	printf("\tMAX_PACKED_READS:\t\t\t0x%x\t\t%d\n",data[501],501);
	printf("\tMAX_PACKED_WRITES:\t\t\t0x%x\t\t%d\n",data[500],500);
	printf("\tDATA_TAG_SUPPORT:\t\t\t0x%x\t\t%d\n",data[499],499);
	printf("\tTAG_UNIT_SIZE:\t\t\t\t0x%x\t\t%d\n",data[498],498);
	printf("\tTAG_RES_SIZE:\t\t\t\t0x%x\t\t%d\n",data[497],497);
	printf("\tCONTEXT_CAPABILITIES:\t\t\t0x%x\t\t%d\n",data[496],496);
	printf("\tLARGE_UNIT_SIZE_M1:\t\t\t0x%x\t\t%d\n",data[495],495);
	printf("\tEXT_SUPPORT:\t\t\t\t0x%x\t\t%d\n",data[494],494);
	printf("\tSUPPORTED_MODES:\t\t\t0x%x\t\t%d\n",data[493],493);
	printf("\tFFU_FEATURES:\t\t\t\t0x%x\t\t%d\n",data[492],492);
	printf("\tOPRATION_CODE_TIMEOUT:\t\t\t0x%x\t\t%d\n",data[491],491);
	printf("\tFFU_ARG:\t\t\t\t0x%x\t%d-%d\n",ffu_arg,490,487);
	printf("\tBARRIER_SUPPORT:\t\t\t0x%x\t\t%d\n",data[486],486);
	printf("\tCMDQ_SUPPORT:\t\t\t\t0x%x\t\t%d\n",data[308],308);
	printf("\tCMDQ_DEPTH:\t\t\t\t0x%x\t\t%d\n",data[307],307);
        printf("\tNUM_OF_FW_SEC_CORRECTLY_PROG:\t\t0x%x\t\t%d-%d\n",num_fw_sect_prog,305,302);
        printf("\tDEVICE_LIFE_TIME_EST_TYP_B:\t\t0x%x\t\t%d\n",data[269],269);
        printf("\tDEVICE_LIFE_TIME_EST_TYP_A:\t\t0x%x\t\t%d\n",data[268],268);
        printf("\tPRE_EOL_INFO:\t\t\t\t0x%x\t\t%d\n",data[267],267);
	printf("\tOPTIMAL_READ_SIZE:\t\t\t0x%x\t\t%d\n",data[266],266);
	printf("\tOPTIMAL_WRITE_SIZE:\t\t\t0x%x\t\t%d\n",data[265],265);
	printf("\tOPTIMAL_TRIM_SIZE:\t\t\t0x%x\t\t%d\n",data[264],264);
	printf("\tDEVICE_VERSION:\t\t\t\t0x%x\t\t%d-%d\n",device_version,263,262);
	printf("\tFIRMWARE_VERSION:\t\t\t0x%llx\t\t%d-%d\n",FW_version,261,254);
	printf("\tPWR_CL_DDR_200_360:\t\t\t0x%x\t\t%d\n",data[253],253);
	printf("\tCACHE_SIZE:\t\t\t\t0x%x\t\t%d-%d\n",s_cache_size,252,249);
	printf("\tGENERIC_CMD6_TIME:\t\t\t0x%x\t\t%d\n",data[248],248);
	printf("\tPOWER_OFF_LONG_TIME:\t\t\t0x%x\t\t%d\n",data[247],247);
	printf("\tBKOPS_STATUS:\t\t\t\t0x%x\t\t%d\n",data[246],246);
	printf("\tCORRECTLY_PRG_SECTORS_NUM:\t\t0x%x\t\t%d-%d\n",c_prg_s_num,245,242);
	printf("\tINI_TIMEOUT_AP:\t\t\t\t0x%x\t\t%d\n",data[241],241);
	printf("\tCACHE_FLUSH_POLICY:\t\t\t0x%x\t\t%d\n",data[240],240);
	printf("\tPWR_CL_DDR_52_360:\t\t\t0x%x\t\t%d\n",data[239],239);
	printf("\tPWR_CL_DDR_52_195:\t\t\t0x%x\t\t%d\n",data[238],238);
	printf("\tPWR_CL_200_195:\t\t\t\t0x%x\t\t%d\n",data[237],237);
	printf("\tPWR_CL_200_130:\t\t\t\t0x%x\t\t%d\n",data[236],236);
	printf("\tMIN_PERF_DDR_W_8_52:\t\t\t0x%x\t\t%d\n",data[235],235);
	printf("\tMIN_PERF_DDR_R_8_52:\t\t\t0x%x\t\t%d\n",data[234],234);
	printf("\tTRIM_MULT:\t\t\t\t0x%x\t\t%d\n",data[232],232);
	printf("\tSEC_FEATURE_SUPPORT:\t\t\t0x%x\t\t%d\n",data[231],231);
	printf("\tSEC_ERASE_MULT:\t\t\t\t0x%x\t\t%d\n",data[230],230);
	printf("\tSEC_TRIM_MULT:\t\t\t\t0x%x\t\t%d\n",data[229],229);
	printf("\tBOOT_INFO:\t\t\t\t0x%x\t\t%d\n",data[228],228);
	printf("\tBOOT_SIZE_MULT:\t\t\t\t0x%x\t\t%d\n",data[226],226);
	printf("\tACC_SIZE:\t\t\t\t0x%x\t\t%d\n",data[225],225);
	printf("\tHC_ERASE_GP_SIZE:\t\t\t0x%x\t\t%d\n",data[224],224);
	printf("\tERASE_TIMEOUT_MULT:\t\t\t0x%x\t\t%d\n",data[223],223);
	printf("\tREL_WR_SEC_C:\t\t\t\t0x%x\t\t%d\n",data[222],222);
	printf("\tHC_WP_GRP_SIZE:\t\t\t\t0x%x\t\t%d\n",data[221],221);
	printf("\tS_C_VCC:\t\t\t\t0x%x\t\t%d\n",data[220],220);
	printf("\tS_C_VCCQ:\t\t\t\t0x%x\t\t%d\n",data[219],219);
	printf("\tPSA_TIMEOUT:\t\t\t\t0x%x\t\t%d\n",data[218],218);
	printf("\tS_A_TIMEOUT:\t\t\t\t0x%x\t\t%d\n",data[217],217);
	printf("\tSLEEP_NOTIFICATION_TIME:\t\t0x%x\t\t%d\n",data[216],216);
	printf("\tSEC_COUNT:\t\t\t\t0x%x\t%d-%d\n",s_count,215,212);
	printf("\tSECURE_WP_INFO:\t\t\t\t0x%x\t\t%d\n",data[211],211);
	printf("\tMIN_PERF_W_8_52:\t\t\t0x%x\t\t%d\n",data[210],210);
	printf("\tMIN_PERF_R_8_52:\t\t\t0x%x\t\t%d\n",data[209],209);
	printf("\tMIN_PERF_W_8_26_4_52:\t\t\t0x%x\t\t%d\n",data[208],208);
	printf("\tMIN_PERF_R_8_26_4_52:\t\t\t0x%x\t\t%d\n",data[207],207);
	printf("\tMIN_PERF_W_4_26:\t\t\t0x%x\t\t%d\n",data[206],206);
	printf("\tMIN_PERF_R_4_26:\t\t\t0x%x\t\t%d\n",data[205],205);
	printf("\tPWR_CL_26_360:\t\t\t\t0x%x\t\t%d\n",data[203],203);
	printf("\tPWR_CL_52_360:\t\t\t\t0x%x\t\t%d\n",data[202],202);
	printf("\tPWR_CL_26_195:\t\t\t\t0x%x\t\t%d\n",data[201],201);
	printf("\tPWR_CL_52_195:\t\t\t\t0x%x\t\t%d\n",data[200],200);
	printf("\tPARTITION_SWITCH_TIME:\t\t\t0x%x\t\t%d\n",data[199],199);
	printf("\tOUT_OF_INTERRUPT_TIME:\t\t\t0x%x\t\t%d\n",data[198],198);
	printf("\tDRIVER_STRENGTH:\t\t\t0x%x\t\t%d\n",data[197],197);
	printf("\tCARD_TYPE:\t\t\t\t0x%x\t\t%d\n",data[196],196);
	printf("\tCSD_STRUCTURE:\t\t\t\t0x%x\t\t%d\n",data[194],194);
	printf("\tEXT_CSD_REV:\t\t\t\t0x%x\t\t%d\n",data[192],192);
	printf("\tCMD_SET:\t\t\t\t0x%x\t\t%d\n",data[191],191);
	printf("\tCMD_SET_REV:\t\t\t\t0x%x\t\t%d\n",data[189],189);
	printf("\tPOWER_CLASS:\t\t\t\t0x%x\t\t%d\n",data[187],187);
	printf("\tHS_TIMING:\t\t\t\t0x%x\t\t%d\n",data[185],185);
	printf("\tSTROBE_SUPPORT:\t\t\t\t0x%x\t\t%d\n",data[184],184);
	printf("\tBUS_WIDTH:\t\t\t\t0x%x\t\t%d\n",data[183],183);
	printf("\tERASED_MEM_CONT:\t\t\t0x%x\t\t%d\n",data[181],181);
	printf("\tPARTITION_CONFIG:\t\t\t0x%x\t\t%d\n",data[179],179);
	printf("\tBOOT_CONFIG_PROT:\t\t\t0x%x\t\t%d\n",data[178],178);
	printf("\tBOOT_BUS_WIDTH:\t\t\t\t0x%x\t\t%d\n",data[177],177);
	printf("\tERASE_GROUP_DEF:\t\t\t0x%x\t\t%d\n",data[175],175);
	printf("\tBOOT_WP_STATUS:\t\t\t\t0x%x\t\t%d\n",data[174],174);
	printf("\tBOOT_WP:\t\t\t\t0x%x\t\t%d\n",data[173],173);
	printf("\tUSER_WP:\t\t\t\t0x%x\t\t%d\n",data[171],171);
	printf("\tFW_CONFIG:\t\t\t\t0x%x\t\t%d\n",data[169],169);
	printf("\tRPMB_SIZE_MULT:\t\t\t\t0x%x\t\t%d\n",data[168],168);
	printf("\tWR_REL_SET:\t\t\t\t0x%x\t\t%d\n",data[167],167);
	printf("\tWR_REL_PARAM:\t\t\t\t0x%x\t\t%d\n",data[166],166);
	printf("\tSANITIZE_START:\t\t\t\t0x%x\t\t%d\n",data[165],165);
	printf("\tBKOPS_START:\t\t\t\t0x%x\t\t%d\n",data[164],164);
	printf("\tBKOPS_EN:\t\t\t\t0x%x\t\t%d\n",data[163],163);
	printf("\tRST_n_FUNCTION:\t\t\t\t0x%x\t\t%d\n",data[162],162);
	printf("\tHPI_MGMT:\t\t\t\t0x%x\t\t%d\n",data[161],161);
	printf("\tPARTITIONING_SUPPORT:\t\t\t0x%x\t\t%d\n",data[160],160);
	printf("\tMAX_ENH_SIZE_MULT:\t\t\t0x%x\t\t%d-%d\n",max_e_mult,159,157);
	printf("\tPARTITIONS_ATTRIBUTE:\t\t\t0x%x\t\t%d\n",data[156],156);
	printf("\tPARTITION_SETTING_COMPLETED:\t\t0x%x\t\t%d\n",data[155],155);
	printf("\tGPP 151:\t\t\t\t0x%x\t\t%d\n",data[151],151);
        printf("\tGPP 147:\t\t\t\t0x%x\t\t%d\n",data[147],147);
	printf("\tGPP 143:\t\t\t\t0x%x\t\t%d\n",data[143],143);
	printf("\tENH_SIZE_MULT:\t\t\t\t0x%x\t\t%d-%d\n",e_s_mult,142,140);
	printf("\tENH_START_ADDR:\t\t\t\t0x%x\t\t%d-%d\n",e_s_addr,139,136);
	printf("\tSEC_BAD_BLK_MGMNT:\t\t\t0x%x\t\t%d\n",data[134],134);
	printf("\tPRODUCTION_STATE_AWARENESS:\t\t0x%x\t\t%d\n",data[133],133);
	printf("\tTCASE_SUPPORT:\t\t\t\t0x%x\t\t%d\n",data[132],132);
	printf("\tPERIODIC_WAKEUP:\t\t\t0x%x\t\t%d\n",data[131],131);
	printf("\tPROGRAM_CID_CSD_DDR_SUPPORT:\t\t0x%x\t\t%d\n",data[130],130);
	printf("\tNATIVE_SECTOR_SIZE:\t\t\t0x%x\t\t%d\n",data[63],63);
	printf("\tUSE_NATIVE_SECTOR:\t\t\t0x%x\t\t%d\n",data[62],62);
	printf("\tDATA_SECTOR_SIZE:\t\t\t0x%x\t\t%d\n",data[61],61);
	printf("\tINI_TIMEOUT_EMU:\t\t\t0x%x\t\t%d\n",data[60],60);
	printf("\tCLASS_6_CTRL:\t\t\t\t0x%x\t\t%d\n",data[59],59);
	printf("\tDYNCAP_NEEDED:\t\t\t\t0x%x\t\t%d\n",data[58],58);
	printf("\tEXCEPTION_EVENTS_CTRL:\t\t\t0x%x\t\t%d-%d\n",exception_ctrl,57,56);
	printf("\tEXCEPTION_EVENTS_STATUS:\t\t0x%x\t\t%d-%d\n",exception_status,55,54);
	printf("\tEXT_PARTITIONS_ATTRIBUTE:\t\t0x%x\t\t%d-%d\n",ext_partition_attr,53,52);
	printf("\tCONTEXT_CONF[51]:\t\t\t0x%x\t\t%d\n",data[51],51);
	printf("\tCONTEXT_CONF[37]:\t\t\t0x%x\t\t%d\n",data[37],37);
	printf("\tPACKED_COMMAND_STATUS:\t\t\t0x%x\t\t%d\n",data[36],36);
	printf("\tPACKED_FAILURE_INDEX:\t\t\t0x%x\t\t%d\n",data[35],35);
	printf("\tPOWER_OFF_NOTIFICATION:\t\t\t0x%x\t\t%d\n",data[34],34);
	printf("\tCACHE_CTRL:\t\t\t\t0x%x\t\t%d\n",data[33],33);
	printf("\tFLUSH_CACHE:\t\t\t\t0x%x\t\t%d\n",data[32],32);
	printf("\tBARRIER_CTRL:\t\t\t\t0x%x\t\t%d\n",data[31],31);
	printf("\tMODE_CONFIG:\t\t\t\t0x%x\t\t%d\n",data[30],30);
	printf("\tMODE_OPERATION_CODES:\t\t\t0x%x\t\t%d\n",data[29],29);
	printf("\tFFU_STATUS:\t\t\t\t0x%x\t\t%d\n",data[26],26);
	printf("\tPRE_LOADING_DATA_SIZE:\t\t\t0x%x\t\t%d-%d\n",pre_load_size,25,22);
	printf("\tMAX_PRE_LOADING_DATA_SIZE:\t\t0x%x\t\t%d-%d\n",max_pre_load_size,21,18);
	printf("\tPRODUCT_STATE_AWARENESS_ENABLEMENT:\t0x%x\t\t%d\n",data[17],17);
	printf("\tSECURE_REMOVAL_TYPE:\t\t\t0x%x\t\t%d\n",data[16],16);
	printf("\tCMDQ_MODE_EN:\t\t\t\t0x%x\t\t%d\n",data[15],15);
	return 0;
}

void System(char *buf)
{
	if(system(buf)){
		printf("call \"%s\" failed, process quit\n", buf);
		exit(1);
	}
}

pid_t Fork(void)
{
	pid_t pid;	
	pid = fork();

	if( pid < 0){ 
		printf("fork error! errno %d\n", errno);
		exit(1);
	}

	return pid;
}

void run_power_down(int mode, float value)
{ 
	char cmd_buf[100];
	sprintf(cmd_buf, "power -a %d -d %f", mode, value);
	System(cmd_buf);
}


void run_power_up()
{
	System("power -a 1");
}

int select_timer(int usec)
{ 
	struct timeval tv_start;
	struct timeval tv_end;
	struct timeval tv;

	uint32_t actual_usec = 0;

	gettimeofday (&tv_start, NULL);

	tv.tv_sec = 0;
	tv.tv_usec = usec;

	select(0, NULL, NULL, NULL, &tv);	// when timeout, should return 0

	gettimeofday(&tv_end, NULL);

	actual_usec = (tv_end.tv_sec - tv_start.tv_sec) * 1000000 + tv_end.tv_usec - tv_start.tv_usec;

	//printf("delay [%d] us\n", actual_usec);
	
	return actual_usec;
}

void dump_buf(uint8_t *buf , int len) 
{
    int count =0 ; 

    while( count < len) {
        if(count % 16 == 0)
            printf("\n%03d:    ", count);
        printf("%02x ", *buf);
        count++;
        buf++;
    }    
    printf("\n");
    return; 
}

void dump_block(uint8_t *buf, int len, int block_addr)
{
	int count = 0;
	while( count < len) {
		printf("\nBLOCK [%d]:\n", block_addr);
		dump_buf(buf+count, 512);
		count += 512;
		block_addr++;
	}
}

int flash_write(uint32_t sec_addr, uint8_t* buf, uint32_t len, uint32_t chunk_size)
{
	int fd;
	int i;
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
	struct block_data bdata;
	struct config_req req;
	int ret = 0;
	
	if( (len % chunk_size) !=0 )
		return 1;
	
	fd = flashops->open();
	if(fd<0){ 
		printf("FLASHAPP_ERROR:open failure.\n");
		ret = -1;
		goto err;
	}
	
	req.flag=REQ_ADDRESS_MODE;
	if(flashops->req(&req)<0){
		printf("FLASHAPP ERR: cmd25 req failure.\n");
		ret = -1;
		goto err;
	}

	bdata.saddr = sec_addr;
	bdata.pdata = buf;
	bdata.len   = chunk_size;
	if(req.data==REQ_ADDRESS_MODE_BYTE)
	   bdata.saddr*=SEC_SIZE;
	    
	for( i=0; i<len/chunk_size; i++) { 

		lib_build_cmd(&mmc_cmd,25,bdata.saddr,NULL,bdata.pdata,bdata.len,0,(FLAG_AUTOCMD12|FLAG_NOCMDTIME));
	 
	    if(flashops->cmd(&mmc_cmd)<0) {
	    	printf("FLASHAPP_ERROR: cmd25 failure.\n");
			ret = -1;
			goto err;
	    }
	
	    r1_res=lib_r1_check(mmc_cmd.restoken); 
	
	    if(r1_res.error) {
	    	printf("FLASHAPP_ERROR: cmd25 failed. Error code: 0x%x\n",r1_res.error);
			ret = -1;
			goto err;
	    }
	
		if(req.data==REQ_ADDRESS_MODE_BYTE) 
			bdata.saddr += chunk_size; 
		else 
			bdata.saddr += chunk_size / SEC_SIZE;

	  	bdata.pdata += chunk_size;
	}

err:
	flashops->close(); 

	return ret;
}


int flash_read(uint32_t sec_addr, uint8_t* buf, uint32_t len, int chunk_size)
{
	int fd;
	int i;
	struct emmc_sndcmd_req mmc_cmd;
	struct r1 r1_res;
	struct block_data bdata;
	struct config_req req;
	
	if( (len % chunk_size) !=0 )
		return 1;
	
	fd=flashops->open();
	if(fd<0){
		printf("FLASHAPP_ERROR:open failure.\n");
		return -1;
	}
	
	req.flag=REQ_ADDRESS_MODE;
	if(flashops->req(&req)<0){
		printf("FLASHAPP ERR: cmd18 req failure.\n");
		return 1;
	}

	bdata.saddr = sec_addr;
	bdata.pdata = buf;
	bdata.len   = chunk_size;
	if(req.data==REQ_ADDRESS_MODE_BYTE)
	   bdata.saddr*=SEC_SIZE;

	memset(buf, 0, len);
	
	for( i=0; i<len/chunk_size; i++) { 
		lib_build_cmd(&mmc_cmd,18,bdata.saddr,NULL,bdata.pdata,bdata.len,0,(FLAG_AUTOCMD12|FLAG_NOCMDTIME));
		
		if(flashops->cmd(&mmc_cmd)<0){
			printf("FLASHAPP ERR: cmd18 failure.\n");
			return -1;
		}
		r1_res=lib_r1_check(mmc_cmd.restoken);
		
		if(r1_res.error){
			printf("cmd18 failed. Error code: 0x%x\n",r1_res.error);
			return -1;
		}

		if(req.data==REQ_ADDRESS_MODE_BYTE) 
			bdata.saddr += chunk_size; 
		else 
			bdata.saddr += chunk_size / SEC_SIZE;

	  	bdata.pdata += chunk_size;
	}
	
	flashops->close();
	
	return 0;
}


int validate_buf(uint8_t *buf, int len, uint8_t *pat, int block_addr)
{
	int fail_cnt = 0;
	int inc = 0;

    while(inc < len) {
		if(buf[inc] != pat[inc]){
			fail_cnt++;
			if(fail_cnt <= MAXFAILCNT)
				printf("Validate fail block: %d, offset: %03d, expect 0x%02x, actual 0x%02x\n",
					block_addr, inc, pat[inc], buf[inc] );
		}

		inc++;
		
		if(inc % 512 == 0)
			block_addr++;
    }

	if(fail_cnt){
		if(fail_cnt > MAXFAILCNT)
			printf("Warning: too many fail, only print 0x%x failures\n",MAXFAILCNT);

		printf("Validate fail, failnum = 0x%x\n",fail_cnt);
		return -1;
	}

	return 0;
}

#if 0
int read_file_to_mem(uint8_t *buf, int len, char *file_path, int offset)
{
	int fd;
	int ret;

	if(!buf){ 
		printf("mem is NULL\n");
		return -1; 
	}

	fd = open(file_path, O_RDWR);
	if(fd < 0){
		printf("%s() open file [%s] error\n", __FUNCTION__, file_path);
		return -1;
	}

	ret = lseek(fd, offset, SEEK_SET);
	if(ret < 0){
		printf("lseek error\n");
		return -1;
	}

	
	ret = read(fd, buf, len);
	if(ret < 0){
		printf("read error\n");
		return -1;
	}
	
	close(fd); 

	return ret;
}


int write_mem_to_file(uint8_t *buf, int len, char *file_path, int offset)
{
	int fd;
	int ret;

	if(!buf){ 
		printf("mem is NULL\n");
		return -1; 
	}

	fd = open(file_path, O_RDWR, S_IRWXU);
	if(fd < 0){
		printf("%s() open file [%s] error\n", __FUNCTION__, file_path);
		return -1;
	}

	ret = lseek(fd, offset, SEEK_SET);
	if(ret < 0){
		printf("lseek error\n");
		return -1;
	}

	ret = write(fd, buf, len);
	if(ret < 0){
		printf("read error\n");
		return -1;
	}
	
	close(fd); 

	return ret;
}
#endif


int do_read_task(struct block_data *bdata, int open_flag, int time_flag, int mode_flag, int fd, uint8_t pattern)
{
	struct emmc_sndcmd_req cmd;
	int ret;
	
	int count_block = 0;
	int left_block = 0;
	int trans_block = 0;
	
	int trans_byte = 0;

	// all argument unit is block
	uint32_t saddr_block = bdata->saddr;
	uint32_t len_block   = bdata->len;
	uint32_t chunk_block = bdata->chunk ;

	int cmd_opcode = (len_block == 1) ? 17 : 18 ;
	uint32_t chunk_byte = chunk_block * SEC_SIZE;

	uint8_t *buf = malloc(chunk_byte);
	uint8_t *tmp = malloc(chunk_byte);

	memset(buf, 0, chunk_byte);
	memset(tmp, 0, chunk_byte);

	if(mode_flag == MODE_PATCMP) {
		struct stat st; 
		int file_block;

		ret = fstat(fd, &st);
		if(ret < 0){
			printf("fstat() error\n");
			return -1;
		}

		if( st.st_size % 512 == 0)
			file_block = st.st_size/512; 
		else
			file_block = st.st_size/512 + 1; 

		len_block = (file_block < len_block) ? file_block : len_block;
	}

	while(count_block < len_block){
		left_block = len_block - count_block;
		trans_block = (left_block >= chunk_block) ? chunk_block : left_block;
		trans_byte = trans_block * SEC_SIZE;

		lib_build_cmd(&cmd, cmd_opcode, saddr_block, NULL, buf, trans_byte, 0, open_flag);
		
		if(flashops->cmd(&cmd)<0)
			return -1;

		if(time_flag)
	  		printf("read time 0x%x-0x%x: %d us,  chunk size: 0x%x blocks\n",saddr_block,(saddr_block+trans_block-1), cmd.time, trans_block);

		switch( mode_flag){
			case MODE_PRINT:
				dump_block(buf, trans_byte, saddr_block);
				break;

			case MODE_CONSTCMP:
				memset(tmp, pattern, trans_byte);
				if(validate_buf(buf, trans_byte, tmp, saddr_block)){
					printf("mmcread compare failed.\n"); 
					return -1;
				}
				break;

			case MODE_PATWRITE:
				ret = write(fd, buf, trans_byte);
				if( ret < 0) {
					printf("mmcread write mem to file error\n");
					return -1;
				}
				break;

			case MODE_PATCMP:
				memset(tmp, 0, trans_byte);
				ret = read(fd, tmp, trans_byte);
				if( ret < 0){
					printf("mmcread read file to mem error\n");
					return -1;
				}

				if(validate_buf(buf, trans_byte, tmp, saddr_block)){
					printf("mmcread compare failed.\n"); 
					return -1;
				}
				break;

			case MODE_QUIET:
			default:
				break;
		}

		saddr_block += trans_block;
		count_block += trans_block;
	}

	if( (mode_flag == MODE_CONSTCMP) || (mode_flag ==  MODE_PATCMP) ){
		printf("mmcread compare pass.\n"); 
	}
	
	return 0;
}



int do_write_task(struct block_data *bdata, int open_flag, int time_flag, int mode_flag, int fd, uint8_t pattern)
{
	struct emmc_sndcmd_req cmd;
	int ret;
	
	int count_block = 0;
	int left_block = 0;
	int trans_block = 0;
	
	int trans_byte = 0;

	// all argument unit is block
	uint32_t saddr_block = bdata->saddr;
	uint32_t len_block   = bdata->len;
	uint32_t chunk_block = bdata->chunk ;

	uint32_t chunk_byte = chunk_block * SEC_SIZE;

	uint8_t *buf = malloc(chunk_byte);
	memset(buf, 0, chunk_byte);

	if(mode_flag == MODE_PATWT) {
		struct stat st; 
		int file_block;

		ret = fstat(fd, &st);
		if(ret < 0){
			printf("fstat() error\n");
			return -1;
		}

		if( st.st_size % 512 == 0)
			file_block = st.st_size/512; 
		else
			file_block = st.st_size/512 + 1; 

		len_block = (file_block < len_block) ? file_block : len_block;
	}

	int cmd_opcode = (len_block == 1) ? 24 : 25 ;

	while(count_block < len_block){
		left_block = len_block - count_block;
		trans_block = (left_block >= chunk_block) ? chunk_block : left_block;
		trans_byte = trans_block * SEC_SIZE;

		switch(mode_flag){
			case MODE_CONSTWT:
				memset(buf, pattern, trans_byte);
				break;

			case MODE_PATWT:
				memset(buf, 0, trans_byte);
				ret = read(fd, buf, trans_byte);
				if(ret < 0)
					return -1; 
				break;

			default:
				break;
		}

		lib_build_cmd(&cmd, cmd_opcode, saddr_block, NULL, buf, trans_byte, 0, open_flag);
			
		if(flashops->cmd(&cmd)<0)
			return -1;

		if(time_flag)
	  		printf("write time 0x%x-0x%x: %d us,  chunk size: 0x%x blocks\n",saddr_block,(saddr_block+trans_block-1), cmd.time, trans_block); 

		saddr_block += trans_block;
		count_block += trans_block;
	}

	return 0;
}
