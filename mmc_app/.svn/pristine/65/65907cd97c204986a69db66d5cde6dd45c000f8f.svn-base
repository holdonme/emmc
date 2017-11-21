/*******************************************************************************/
/*   File        : flash_lib.h                                                 */
/*   Created by  : Youxin He                                                   */
/*   Modified by : 2010-11-09                                                  */
/*   Description : This is the head file for the user application code         */
/*                 to include.                                                 */
/*******************************************************************************/

#ifndef _FLASH_LIB_H_
#define _FLASH_LIB_H_
#include <stdint.h>
#include <sys/time.h>

extern struct timeval g_begin;
extern struct timeval g_end;
extern int g_fd;
extern int g_failFlag;
extern int g_expectfail;

extern void lib_error(char* err_buf, char* file, int line);
extern uint lib_valid_digit(const char * ch);
extern uint lib_convert(char ch);
extern uint lib_hex2int(const char * ch);
extern uint lib_validate_data_region(struct block_data* bdata, uint8_t* cdata, uint validate);
extern uint lib_readPattern2Mem(uint8_t* pData, char* filePath, int size);
extern uint lib_writePattern2File(uint8_t* pData, char* filePath, int size);
extern uint lib_build_cmd(struct emmc_sndcmd_req* cmd,
							uint32_t opcode,
							uint32_t argument,
							uint32_t *restoken,
							uint8_t  *data,
							uint32_t len,
							uint32_t time, 
							uint32_t flag);

extern struct r1 lib_r1_check(uint32_t* restoken);
extern const char* lib_decode_status(int number);
extern const char* lib_decode_cbx(int number);
extern uint32_t lib_get_offset(uint32_t data, int offset, int bit_len);
extern uint lib_decode_cid(uint32_t* data);
extern uint lib_decode_csd(uint32_t* data);
extern uint lib_decode_xcsd(uint8_t* data);
extern uint lib_getFileSize(char* filePath);

void run_power_down(int mode, float value);
void run_power_up(void);
void System(char *buf);
pid_t Fork(void);
void dump_buf(uint8_t *buf , int len);
void dump_block(uint8_t *buf, int len, int block_addr);
int select_timer(int usec);

int flash_write(uint32_t sec_addr, uint8_t* buf, uint32_t len, int chunk_size);
int flash_read(uint32_t sec_addr, uint8_t* buf, uint32_t len, int chunk_size);

int validate_buf(uint8_t *buf, int len, uint8_t *pat, int offset);
int do_read_task(struct block_data *bdata, int open_flag, int time_flag, int mode_flag, int fd, uint8_t pattern);
int do_write_task(struct block_data *bdata, int open_flag, int time_flag, int mode_flag, int fd, uint8_t pattern);

#endif//end _FLASH_LIB_H_

