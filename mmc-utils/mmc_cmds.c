#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <ctype.h>
#include <sys/time.h>

#include "mmc.h"
#include "mmc_cmds.h"

#define BLOCK_SIZE 256	//In sector, mmc block layer limit the block size to 512L*256 bytes

extern char *optarg;
static const char g_cbx[5][10]={"Card\0","BGA/MCP\0","POP\0","RESERVED\0","INVALID\0"};

void p_err(char* err_buf, char* file, int line)
{
	printf("%s --file: %s, line: %d --\n",err_buf, file, line);
}

uint num_convert(char ch)
{
	uint t = 0; 
	switch (ch){
		case '0':t = 0;break;
		case '1':t = 1;break;
		case '2':t = 2;break;  
		case '3':t = 3;break;
		case '4':t = 4;break;
		case '5':t = 5;break;
		case '6':t = 6;break;
		case '7':t = 7;break;
		case '8':t = 8;break;            
		case '9':t = 9;break;
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
		p_err("ERROR: in num_convert(), not Hex input",__FILE__,__LINE__);
		printf("%c\n",ch);
		return -1;
	}
 	return t;
}

__u32 lib_get_offset(__u32 data, int offset, int bit_len){
	int bits,i;
	bits=0;
	for(i=0;i<32;i++){
		if(i>=offset && i<offset+bit_len){
			bits|=(1<<i);
		}
	}
	return ((data&bits)>>offset);
}

uint hex2int(const char * ch){
	int i, j =0;
	uint sum = 0, f = 0;
	int hex_length = strlen(ch);

	if(!(ch[0] == '0' && ch[1] == 'x')){
		p_err("ERROR: in hex2int(), hex number not start 0x",__FILE__,__LINE__);
		exit(1);
	}
	 
	for (i = 2; i < hex_length; i++){
		f = 1;
		for (j = 1; j < hex_length-i; j++){
			f *= 16;
		}
		sum += num_convert(ch[i]) * f;
	}

	return sum;
}

uint valid_digit(const char * ch){
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
		return hex2int(ch);
	}else{
		p_err("ERROR: Not valid input: should be dec or hex with 0x input",__FILE__,__LINE__);
		exit(1);
	}
}

static int read_extcsd(int fd, __u8 *ext_csd)
{
	int ret = 0;
	struct mmc_ioc_cmd idata;
	memset(&idata, 0, sizeof(idata));
	memset(ext_csd, 0, sizeof(__u8) * 512);
	idata.write_flag = 0;
	idata.opcode = MMC_SEND_EXT_CSD;
	idata.arg = 0;
	idata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
	idata.blksz = 512;
	idata.blocks = 1;
	mmc_ioc_cmd_set_data(idata, ext_csd);

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");

	return ret;
}

static int write_extcsd_value(int fd, __u8 index, __u8 value)
{
	int ret = 0;
	struct mmc_ioc_cmd idata;

	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 1;
	idata.opcode = MMC_SWITCH;
	idata.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
			(index << 16) |
			(value << 8) |
			EXT_CSD_CMD_SET_NORMAL;
	idata.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");

	return ret;
}

static int send_stop(int fd, __u8 hpi, __u32 *resp)
{
	int ret = 0;
	struct mmc_ioc_cmd idata;

	memset(&idata, 0, sizeof(idata));
	idata.opcode = MMC_STOP_TRANSMISSION;
	idata.arg = ((hpi ? 1 : 0) << 16) | hpi;
	idata.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if(ret)
		perror("ioctl");

	if(resp)	
		*resp = idata.response[0];

	return ret;
}

static int send_status(int fd, __u8 hpi, __u32 *resp)
{
	int ret = 0;
	struct mmc_ioc_cmd idata;

	memset(&idata, 0, sizeof(idata));
	idata.opcode = MMC_SEND_STATUS;
	idata.arg = (1 << 16) | hpi;
	idata.flags = MMC_RSP_R1 | MMC_CMD_AC;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");

	*resp = idata.response[0];

	return ret;
}

static void print_writeprotect_status(__u8 *ext_csd)
{
	__u8 reg;
	__u8 ext_csd_rev = ext_csd[192];

	/* A43: reserved [174:0] */
	if (ext_csd_rev >= 5) {
		printf("Boot write protection status registers"
			" [BOOT_WP_STATUS]: 0x%02x\n", ext_csd[174]);

		reg = ext_csd[EXT_CSD_BOOT_WP];
		printf("Boot Area Write protection [BOOT_WP]: 0x%02x\n", reg);
		printf(" Power ro locking: ");
		if (reg & EXT_CSD_BOOT_WP_B_PWR_WP_DIS)
			printf("not possible\n");
		else
			printf("possible\n");

		printf(" Permanent ro locking: ");
		if (reg & EXT_CSD_BOOT_WP_B_PERM_WP_DIS)
			printf("not possible\n");
		else
			printf("possible\n");

		printf(" ro lock status: ");
		if (reg & EXT_CSD_BOOT_WP_B_PWR_WP_EN)
			printf("locked until next power on\n");
		else if (reg & EXT_CSD_BOOT_WP_B_PERM_WP_EN)
			printf("locked permanently\n");
		else
			printf("not locked\n");
	}
}

static void print_userarea_writeprotect_status(__u8 *ext_csd)
{
	__u8 ext_csd_rev = ext_csd[192];

	/* A43: reserved [171:0] */
	if (ext_csd_rev >= 5) {
		printf("User write protection status registers"
			" [USER_WP]: 0x%08x\n", ext_csd[171]);
	}
}

static int mmc_read_health_status(int fd, __u32 arg, __u8 *buf)
{
	struct mmc_ioc_cmd idata;
	int ret=0;

	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 0;
	idata.opcode = MMC_GEN_CMD;
	idata.arg = arg;
	idata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
	idata.blksz = 512;
	idata.blocks = 1;
	mmc_ioc_cmd_set_data(idata, buf);

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");
	return ret;
}

static int mmc_write_health_status(int fd, __u32 arg, __u8 *buf)
{
	struct mmc_ioc_cmd idata;
	int ret=0;
	
	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 1;
	idata.opcode = MMC_GEN_CMD;
	idata.arg = arg;
	idata.data_ptr = (__u32)buf;
	idata.cmd_timeout_ms = 400;
	idata.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_ADTC;
	idata.blksz = 512;
	idata.blocks = 1;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret){
		perror("ioctl");
	}
	return ret;
}

int do_writeprotect_get(int nargs, char **argv)
{
	__u8 ext_csd[512];
	int fd, ret;
	char *device;

	CHECK(nargs != 2, "Usage: mmc writeprotect get </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if (ret) {
		fprintf(stderr, "Could not read EXT_CSD from %s\n", device);
		exit(1);
	}

	print_writeprotect_status(ext_csd);

	return ret;
}

int do_writeprotect_set(int nargs, char **argv)
{
	__u8 ext_csd[512], value;
	int fd, ret;
	char *device;

	CHECK(nargs != 2, "Usage: mmc writeprotect set </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if (ret) {
		fprintf(stderr, "Could not read EXT_CSD from %s\n", device);
		exit(1);
	}

	value = ext_csd[EXT_CSD_BOOT_WP] |
		EXT_CSD_BOOT_WP_B_PWR_WP_EN;
	ret = write_extcsd_value(fd, EXT_CSD_BOOT_WP, value);
	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to "
			"EXT_CSD[%d] in %s\n",
			value, EXT_CSD_BOOT_WP, device);
		exit(1);
	}

	return ret;
}

int do_userarea_writeprotect_get(int nargs, char **argv)
{
	__u8 ext_csd[512];
	int fd, ret;
	char *device;

	CHECK(nargs != 2, "Usage: mmc writeprotect get </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if (ret) {
		fprintf(stderr, "Could not read EXT_CSD from %s\n", device);
		exit(1);
	}

	print_userarea_writeprotect_status(ext_csd);

	return ret;
}

int do_disable_512B_emulation(int nargs, char **argv)
{
	__u8 ext_csd[512], native_sector_size, data_sector_size, wr_rel_param;
	int fd, ret;
	char *device;

	CHECK(nargs != 2, "Usage: mmc disable 512B emulation </path/to/mmcblkX>\n", exit(1));
	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if (ret) {
		fprintf(stderr, "Could not read EXT_CSD from %s\n", device);
		exit(1);
	}

	wr_rel_param = ext_csd[EXT_CSD_WR_REL_PARAM];
	native_sector_size = ext_csd[EXT_CSD_NATIVE_SECTOR_SIZE];
	data_sector_size = ext_csd[EXT_CSD_DATA_SECTOR_SIZE];

	if (native_sector_size && !data_sector_size &&
	   (wr_rel_param & EN_REL_WR)) {
		ret = write_extcsd_value(fd, EXT_CSD_USE_NATIVE_SECTOR, 1);

		if (ret) {
			fprintf(stderr, "Could not write 0x%02x to EXT_CSD[%d] in %s\n",
					1, EXT_CSD_BOOT_WP, device);
			exit(1);
		}
		printf("MMC disable 512B emulation successful.  Now reset the device to switch to 4KB native sector mode.\n");
	} else if (native_sector_size && data_sector_size) {
		printf("MMC 512B emulation mode is already disabled; doing nothing.\n");
	} else {
		printf("MMC does not support disabling 512B emulation mode.\n");
	}

	return ret;
}

int do_write_boot_en(int nargs, char **argv)
{
	__u8 ext_csd[512];
	__u8 value = 0;
	int fd, ret;
	char *device;
	int boot_area, send_ack;

	CHECK(nargs != 4, "Usage: mmc bootpart enable <partition_number> "
			  "<send_ack> </path/to/mmcblkX>\n", exit(1));

	/*
	 * If <send_ack> is 1, the device will send acknowledgment
	 * pattern "010" to the host when boot operation begins.
	 * If <send_ack> is 0, it won't.
	 */
	boot_area = strtol(argv[1], NULL, 10);
	send_ack = strtol(argv[2], NULL, 10);
	device = argv[3];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if (ret) {
		fprintf(stderr, "Could not read EXT_CSD from %s\n", device);
		exit(1);
	}

	value = ext_csd[EXT_CSD_PART_CONFIG];

	switch (boot_area) {
	case EXT_CSD_PART_CONFIG_ACC_BOOT0:
		value |= (1 << 3);
		value &= ~(3 << 4);
		break;
	case EXT_CSD_PART_CONFIG_ACC_BOOT1:
		value |= (1 << 4);
		value &= ~(1 << 3);
		value &= ~(1 << 5);
		break;
	case EXT_CSD_PART_CONFIG_ACC_USER_AREA:
		value |= (boot_area << 3);
		break;
	default:
		fprintf(stderr, "Cannot enable the boot area\n");
		exit(1);
	}
	if (send_ack)
		value |= EXT_CSD_PART_CONFIG_ACC_ACK;
	else
		value &= ~EXT_CSD_PART_CONFIG_ACC_ACK;

	ret = write_extcsd_value(fd, EXT_CSD_PART_CONFIG, value);
	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to "
			"EXT_CSD[%d] in %s\n",
			value, EXT_CSD_PART_CONFIG, device);
		exit(1);
	}
	return ret;
}

static int do_hwreset(int value, int nargs, char **argv)
{
	__u8 ext_csd[512];
	int fd, ret;
	char *device;

	CHECK(nargs != 2, "Usage: mmc hwreset enable </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if (ret) {
		fprintf(stderr, "Could not read EXT_CSD from %s\n", device);
		exit(1);
	}

	if ((ext_csd[EXT_CSD_RST_N_FUNCTION] & EXT_CSD_RST_N_EN_MASK) ==
	    EXT_CSD_HW_RESET_EN) {
		fprintf(stderr,
			"H/W Reset is already permanently enabled on %s\n",
			device);
		exit(1);
	}
	if ((ext_csd[EXT_CSD_RST_N_FUNCTION] & EXT_CSD_RST_N_EN_MASK) ==
	    EXT_CSD_HW_RESET_DIS) {
		fprintf(stderr,
			"H/W Reset is already permanently disabled on %s\n",
			device);
		exit(1);
	}

	ret = write_extcsd_value(fd, EXT_CSD_RST_N_FUNCTION, value);
	if (ret) {
		fprintf(stderr,
			"Could not write 0x%02x to EXT_CSD[%d] in %s\n",
			value, EXT_CSD_RST_N_FUNCTION, device);
		exit(1);
	}

	return ret;
}

int do_hwreset_en(int nargs, char **argv)
{
	return do_hwreset(EXT_CSD_HW_RESET_EN, nargs, argv);
}

int do_hwreset_dis(int nargs, char **argv)
{
	return do_hwreset(EXT_CSD_HW_RESET_DIS, nargs, argv);
}

int do_write_bkops_en(int nargs, char **argv)
{
	__u8 ext_csd[512], value = 0;
	int fd, ret;
	char *device;

	CHECK(nargs != 2, "Usage: mmc bkops enable </path/to/mmcblkX>\n",
			exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if (ret) {
		fprintf(stderr, "Could not read EXT_CSD from %s\n", device);
		exit(1);
	}

	if (!(ext_csd[EXT_CSD_BKOPS_SUPPORT] & 0x1)) {
		fprintf(stderr, "%s doesn't support BKOPS\n", device);
		exit(1);
	}

	ret = write_extcsd_value(fd, EXT_CSD_BKOPS_EN, BKOPS_ENABLE);
	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to EXT_CSD[%d] in %s\n",
			value, EXT_CSD_BKOPS_EN, device);
		exit(1);
	}

	return ret;
}

int do_send_stop(int nargs, char **argv)
{
	char *device;
	int fd, ret=0, hpi=0;
	int c=0;
	__u32 resp;
	char *help="Usage: mmc cmd12 -h </path/to/mmcblkX>\n\n\t -h : Use HPI.\n";
	if(nargs < 2){
		printf("%s\n",help);
		exit(1);
	}	

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"h"))!=-1){
		switch(c){
			case 'h':
				hpi=1;
				break;
			case '?':
				printf("%s\n",help);
				exit(1);
				break;
			default:
				break;
		}
	}
	
	ret = send_stop(fd, hpi, &resp);
	if (ret) {
		fprintf(stderr, "Could not send CMD12 to %s\n", device);
		exit(1);
	}

	printf("Device Status: 0x%08x\n", resp);

	return ret;
}

int do_status_get(int nargs, char **argv)
{
	__u32 response;
	int fd, ret;
	char *device;

	CHECK(nargs != 2, "Usage: mmc cmd13 </path/to/mmcblkX>\n",
		exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = send_status(fd, 0, &response);
	if (ret) {
		fprintf(stderr, "Could not read response to SEND_STATUS from %s\n", device);
		exit(1);
	}

	printf("Device Status: 0x%08x\n", response);

	return ret;
}

static unsigned int get_sector_count(__u8 *ext_csd)
{
	return (ext_csd[EXT_CSD_SEC_COUNT_3] << 24) |
	(ext_csd[EXT_CSD_SEC_COUNT_2] << 16) |
	(ext_csd[EXT_CSD_SEC_COUNT_1] << 8)  |
	ext_csd[EXT_CSD_SEC_COUNT_0];
}

static int is_blockaddresed(__u8 *ext_csd)
{
	unsigned int sectors = get_sector_count(ext_csd);

	return (sectors > (2u * 1024 * 1024 * 1024) / 512);
}

static unsigned int get_hc_wp_grp_size(__u8 *ext_csd)
{
	return ext_csd[221];
}

static unsigned int get_hc_erase_grp_size(__u8 *ext_csd)
{
	return ext_csd[224];
}

int do_enh_area_set(int nargs, char **argv)
{
	__u8 value;
	__u8 ext_csd[512];
	int fd, ret;
	char *device;
	int dry_run = 1;
	unsigned int start_kib, length_kib, enh_start_addr, enh_size_mult;
	unsigned long align;

	CHECK(nargs != 5, "Usage: mmc enh_area set <-y|-n> <start KiB> <length KiB> "
			  "</path/to/mmcblkX>\n", exit(1));

	if (!strcmp("-y", argv[1]))
		dry_run = 0;

	start_kib = strtol(argv[2], NULL, 10);
	length_kib = strtol(argv[3], NULL, 10);
	device = argv[4];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if (ret) {
		fprintf(stderr, "Could not read EXT_CSD from %s\n", device);
		exit(1);
	}

	/* assert ENH_ATTRIBUTE_EN */
	if (!(ext_csd[EXT_CSD_PARTITIONING_SUPPORT] & EXT_CSD_ENH_ATTRIBUTE_EN))
	{
		printf(" Device cannot have enhanced tech.\n");
		exit(1);
	}

	/* assert not PARTITION_SETTING_COMPLETED */
	if (ext_csd[EXT_CSD_PARTITION_SETTING_COMPLETED])
	{
		printf(" Device is already partitioned\n");
		exit(1);
	}

	align = 512l * get_hc_wp_grp_size(ext_csd) * get_hc_erase_grp_size(ext_csd);

	enh_size_mult = (length_kib + align/2l) / align;

	enh_start_addr = start_kib * 1024 / (is_blockaddresed(ext_csd) ? 512 : 1);
	enh_start_addr /= align;
	enh_start_addr *= align;

	/* set EXT_CSD_ERASE_GROUP_DEF bit 0 */
	ret = write_extcsd_value(fd, EXT_CSD_ERASE_GROUP_DEF, 0x1);
	if (ret) {
		fprintf(stderr, "Could not write 0x1 to "
			"EXT_CSD[%d] in %s\n",
			EXT_CSD_ERASE_GROUP_DEF, device);
		exit(1);
	}

	/* write to ENH_START_ADDR and ENH_SIZE_MULT and PARTITIONS_ATTRIBUTE's ENH_USR bit */
	value = (enh_start_addr >> 24) & 0xff;
	ret = write_extcsd_value(fd, EXT_CSD_ENH_START_ADDR_3, value);
	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to "
			"EXT_CSD[%d] in %s\n", value,
			EXT_CSD_ENH_START_ADDR_3, device);
		exit(1);
	}
	value = (enh_start_addr >> 16) & 0xff;
	ret = write_extcsd_value(fd, EXT_CSD_ENH_START_ADDR_2, value);
	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to "
			"EXT_CSD[%d] in %s\n", value,
			EXT_CSD_ENH_START_ADDR_2, device);
		exit(1);
	}
	value = (enh_start_addr >> 8) & 0xff;
	ret = write_extcsd_value(fd, EXT_CSD_ENH_START_ADDR_1, value);
	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to "
			"EXT_CSD[%d] in %s\n", value,
			EXT_CSD_ENH_START_ADDR_1, device);
		exit(1);
	}
	value = enh_start_addr & 0xff;
	ret = write_extcsd_value(fd, EXT_CSD_ENH_START_ADDR_0, value);
	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to "
			"EXT_CSD[%d] in %s\n", value,
			EXT_CSD_ENH_START_ADDR_0, device);
		exit(1);
	}

	value = (enh_size_mult >> 16) & 0xff;
	ret = write_extcsd_value(fd, EXT_CSD_ENH_SIZE_MULT_2, value);
	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to "
			"EXT_CSD[%d] in %s\n", value,
			EXT_CSD_ENH_SIZE_MULT_2, device);
		exit(1);
	}
	value = (enh_size_mult >> 8) & 0xff;
	ret = write_extcsd_value(fd, EXT_CSD_ENH_SIZE_MULT_1, value);
	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to "
			"EXT_CSD[%d] in %s\n", value,
			EXT_CSD_ENH_SIZE_MULT_1, device);
		exit(1);
	}
	value = enh_size_mult & 0xff;
	ret = write_extcsd_value(fd, EXT_CSD_ENH_SIZE_MULT_0, value);
	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to "
			"EXT_CSD[%d] in %s\n", value,
			EXT_CSD_ENH_SIZE_MULT_0, device);
		exit(1);
	}

	ret = write_extcsd_value(fd, EXT_CSD_PARTITIONS_ATTRIBUTE, EXT_CSD_ENH_USR);
	if (ret) {
		fprintf(stderr, "Could not write EXT_CSD_ENH_USR to "
			"EXT_CSD[%d] in %s\n",
			EXT_CSD_PARTITIONS_ATTRIBUTE, device);
		exit(1);
	}

	if (dry_run)
	{
		fprintf(stderr, "NOT setting PARTITION_SETTING_COMPLETED\n");
		exit(1);
	}

	fprintf(stderr, "setting OTP PARTITION_SETTING_COMPLETED!\n");
	ret = write_extcsd_value(fd, EXT_CSD_PARTITION_SETTING_COMPLETED, 0x1);
	if (ret) {
		fprintf(stderr, "Could not write 0x1 to "
			"EXT_CSD[%d] in %s\n",
			EXT_CSD_PARTITION_SETTING_COMPLETED, device);
		exit(1);
	}

	__u32 response;
	ret = send_status(fd, 0, &response);
	if (ret) {
		fprintf(stderr, "Could not get response to SEND_STATUS from %s\n", device);
		exit(1);
	}

	if (response & R1_SWITCH_ERROR)
	{
		fprintf(stderr, "Setting ENH_USR area failed on %s\n", device);
		exit(1);
	}

	fprintf(stderr, "Setting ENH_USR area on %s SUCCESS\n", device);
	fprintf(stderr, "Device power cycle needed for settings to take effect.\n"
		"Confirm that PARTITION_SETTING_COMPLETED bit is set using 'extcsd read'"
		"after power cycle\n");

	return 0;
}

int decode_ecsd(__u8* data, __u8 rev){
	__u32 c_prg_s_num=data[242]|data[243]<<8|data[244]<<16|data[245]<<24;
	__u32 s_count=data[212]|data[213]<<8|data[214]<<16|data[215]<<24;
	__u32 max_e_mult=data[157]|data[158]<<8|data[159]<<16;	
	__u32 e_mult=data[140]|data[141]<<8|data[142]<<16;
	__u32 e_s_addr=data[136]|data[137]<<8|data[138]<<16|data[139]<<24;
	__u32 fw_version1=data[254]|data[255]<<8|data[256]<<16|data[257]<<24;
	__u32 fw_version2=data[258]|data[259]<<8|data[260]<<16|data[261]<<24;
	
	printf("\n===================================================================\n");
	printf("|                       Extended CSD Register                     |\n");
	printf("===================================================================\n\n");
	printf("\tS_CMD_SET:\t\t\t0x%x\t%d\n",data[504],504);
	printf("\tHPI_FEATURES:\t\t\t0x%x\t%d\n",data[503],503);
	printf("\tBKOPS_SUPPORT:\t\t\t0x%x\t%d\n",data[502],502);
	printf("\tSUPPORTED_MODES:\t\t0x%x\t%d\n",data[493],493);
	if(rev > 6){
		printf("\tFIRMWARE_VERSION:\t\t0x%x%x\t%d-%d\n",fw_version2,fw_version1,261,254);
	}
	printf("\tBKOPS_STATUS:\t\t\t0x%x\t%d\n",data[246],246);
	printf("\tCORRECTLY_PRG_SECTORS_NUM:\t0x%x\t%d-%d\n",c_prg_s_num,245,242);
	printf("\tINI_TIMEOUT_AP:\t\t\t0x%x\t%d\n",data[241],241);
	printf("\tPWR_CL_DDR_52_360:\t\t0x%x\t%d\n",data[239],239);
	printf("\tPWR_CL_DDR_52_195:\t\t0x%x\t%d\n",data[238],238);
	printf("\tMIN_PERF_DDR_W_8_52:\t\t0x%x\t%d\n",data[235],235);
	printf("\tMIN_PERF_DDR_R_8_52:\t\t0x%x\t%d\n",data[234],234);
	printf("\tTRIM_MULT:\t\t\t0x%x\t%d\n",data[232],232);
	printf("\tSEC_FEATURE_SUPPORT:\t\t0x%x\t%d\n",data[231],231);
	printf("\tSEC_ERASE_MULT:\t\t\t0x%x\t%d\n",data[230],230);
	printf("\tSEC_TRIM_MULT:\t\t\t0x%x\t%d\n",data[229],229);
	printf("\tBOOT_INFO:\t\t\t0x%x\t%d\n",data[228],228);
	printf("\tBOOT_SIZE_MULT:\t\t\t0x%x\t%d\n",data[226],226);
	printf("\tACC_SIZE:\t\t\t0x%x\t%d\n",data[225],225);
	printf("\tHC_ERASE_GP_SIZE:\t\t0x%x\t%d\n",data[224],224);
	printf("\tERASE_TIMEOUT_MULT:\t\t0x%x\t%d\n",data[223],223);
	printf("\tREL_WR_SEC_C:\t\t\t0x%x\t%d\n",data[222],222);
	printf("\tHC_WP_GRP_SIZE:\t\t\t0x%x\t%d\n",data[221],221);
	printf("\tS_C_VCC:\t\t\t0x%x\t%d\n",data[220],220);
	printf("\tS_C_VCCQ:\t\t\t0x%x\t%d\n",data[219],219);
	printf("\tS_A_TIMEOUT:\t\t\t0x%x\t%d\n",data[217],217);
	printf("\tSEC_COUNT:\t\t\t0x%x\t%d-%d\n",s_count,215,212);
	printf("\tMIN_PERF_W_8_52:\t\t0x%x\t%d\n",data[210],210);
	printf("\tMIN_PERF_R_8_52:\t\t0x%x\t%d\n",data[209],209);
	printf("\tMIN_PERF_W_8_26_4_52:\t\t0x%x\t%d\n",data[208],208);
	printf("\tMIN_PERF_R_8_26_4_52:\t\t0x%x\t%d\n",data[207],207);
	printf("\tMIN_PERF_W_4_26:\t\t0x%x\t%d\n",data[206],206);
	printf("\tMIN_PERF_R_4_26:\t\t0x%x\t%d\n",data[205],205);
	printf("\tPWR_CL_26_360:\t\t\t0x%x\t%d\n",data[203],203);
	printf("\tPWR_CL_52_360:\t\t\t0x%x\t%d\n",data[202],202);
	printf("\tPWR_CL_26_195:\t\t\t0x%x\t%d\n",data[201],201);
	printf("\tPWR_CL_52_195:\t\t\t0x%x\t%d\n",data[200],200);
	printf("\tPARTITION_SWITCH_TIME:\t\t0x%x\t%d\n",data[199],199);
	printf("\tOUT_OF_INTERRUPT_TIME:\t\t0x%x\t%d\n",data[198],198);
	printf("\tCARD_TYPE:\t\t\t0x%x\t%d\n",data[196],196);
	printf("\tCSD_STRUCTURE:\t\t\t0x%x\t%d\n",data[194],194);
	printf("\tEXT_CSD_REV:\t\t\t0x%x\t%d\n",data[192],192);
	printf("\tCMD_SET:\t\t\t0x%x\t%d\n",data[191],191);
	printf("\tCMD_SET_REV:\t\t\t0x%x\t%d\n",data[189],189);
	printf("\tPOWER_CLASS:\t\t\t0x%x\t%d\n",data[187],187);
	printf("\tHS_TIMING:\t\t\t0x%x\t%d\n",data[185],185);
	printf("\tBUS_WIDTH:\t\t\t0x%x\t%d\n",data[183],183);
	printf("\tERASED_MEM_CONT:\t\t0x%x\t%d\n",data[181],181);
	printf("\tPARTITION_CONFIG:\t\t0x%x\t%d\n",data[179],179);
	printf("\tBOOT_CONFIG_PROT:\t\t0x%x\t%d\n",data[178],178);
	printf("\tBOOT_BUS_WIDTH:\t\t\t0x%x\t%d\n",data[177],177);
	printf("\tERASE_GROUP_DEF:\t\t0x%x\t%d\n",data[175],175);
	printf("\tBOOT_WP:\t\t\t0x%x\t%d\n",data[173],173);
	printf("\tUSER_WP:\t\t\t0x%x\t%d\n",data[171],171);
	printf("\tFW_CONFIG:\t\t\t0x%x\t%d\n",data[169],169);
	printf("\tRPMB_SIZE_MULT:\t\t\t0x%x\t%d\n",data[168],168);
	printf("\tWR_REL_SET:\t\t\t0x%x\t%d\n",data[167],167);
	printf("\tWR_REL_PARAM:\t\t\t0x%x\t%d\n",data[166],166);
	printf("\tBKOPS_START:\t\t\t0x%x\t%d\n",data[164],164);
	printf("\tBKOPS_EN:\t\t\t0x%x\t%d\n",data[163],163);
	printf("\tRST_n_FUNCTION:\t\t\t0x%x\t%d\n",data[162],162);
	printf("\tHPI_MGMT:\t\t\t0x%x\t%d\n",data[161],161);
	printf("\tPARTITIONING_SUPPORT:\t\t0x%x\t%d\n",data[160],160);
	printf("\tMAX_ENH_SIZE_MULT:\t\t0x%x\t%d-%d\n",max_e_mult,159,157);
	printf("\tPARTITIONS_ATTRIBUTE:\t\t0x%x\t%d\n",data[156],156);
	printf("\tPARTITION_SETTING_COMPLETED:\t0x%x\t%d\n",data[155],155);
	printf("\tENH_SIZE_MULT:\t\t\t0x%x\t%d-%d\n",e_mult,142,140);
	printf("\tGPP 143:\t\t\t0x%x\t%d\n",data[143],143);
	printf("\tGPP 146:\t\t\t0x%x\t%d\n",data[146],146);
	printf("\tENH_START_ADDR:\t\t\t0x%x\t%d-%d\n",e_s_addr,139,136);
	printf("\tSEC_BAD_BLK_MGMNT:\t\t0x%x\t%d\n",data[134],134);
	if(rev > 6){
		printf("\tMODE_CONFIG:\t\t\t0x%x\t%d\n",data[30],30);
		printf("\tFFU_STATUS:\t\t\t0x%x\t%d\n",data[26],26);
	}

	return 0;
}

int do_decode_extcsd(int nargs, char **argv)
{
	__u8 ext_csd[512], ext_csd_rev;
	int fd, ret;
	char *device;
	const char *str;

	CHECK(nargs != 2, "Usage: mmc cmd08 </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if (ret) {
		fprintf(stderr, "Could not read EXT_CSD from %s\n", device);
		exit(1);
	}

	ext_csd_rev = ext_csd[192];

	switch (ext_csd_rev) {
	case 7:
		str = "5.0";
		break;
	case 6:
		str = "4.5";
		break;
	case 5:
		str = "4.41";
		break;
	case 3:
		str = "4.3";
		break;
	case 2:
		str = "4.2";
		break;
	case 1:
		str = "4.1";
		break;
	case 0:
		str = "4.0";
		break;
	default:
		goto out_free;
	}

	ret = decode_ecsd(ext_csd, ext_csd_rev);
	
	printf("\n===================================================================\n");
	printf("|               Extended CSD Revision 1.%d (eMMC %s)              |\n", ext_csd_rev, str);
	printf("===================================================================\n\n");

out_free:
	return ret;
}

int do_read_extcsd(int nargs, char **argv)
{
	__u8 ext_csd[512], ext_csd_rev, reg;
	unsigned int max_size;
	int fd, ret;
	char *device;
	const char *str;

	CHECK(nargs != 2, "Usage: mmc cmd08 </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if (ret) {
		fprintf(stderr, "Could not read EXT_CSD from %s\n", device);
		exit(1);
	}

	ext_csd_rev = ext_csd[192];

	switch (ext_csd_rev) {
	case 7:
		str = "5.0";
		break;
	case 6:
		str = "4.5";
		break;
	case 5:
		str = "4.41";
		break;
	case 3:
		str = "4.3";
		break;
	case 2:
		str = "4.2";
		break;
	case 1:
		str = "4.1";
		break;
	case 0:
		str = "4.0";
		break;
	default:
		goto out_free;
	}
	printf("=============================================\n");
	printf("  Extended CSD rev 1.%d (eMMC %s)\n", ext_csd_rev, str);
	printf("=============================================\n\n");

	if (ext_csd_rev < 3)
		goto out_free; /* No ext_csd */

	/* Parse the Extended CSD registers.
	 * Reserved bit should be read as "0" in case of spec older
	 * than A441.
	 */
	reg = ext_csd[EXT_CSD_S_CMD_SET];
	printf("Card Supported Command sets [S_CMD_SET: 0x%02x]\n", reg);
	if (!reg)
		printf(" - Standard MMC command sets\n");

	reg = ext_csd[EXT_CSD_HPI_FEATURE];
	printf("HPI Features [HPI_FEATURE: 0x%02x]: ", reg);
	if (reg & EXT_CSD_HPI_SUPP) {
		if (reg & EXT_CSD_HPI_IMPL)
			printf("implementation based on CMD12\n");
		else
			printf("implementation based on CMD13\n");
	}

	printf("Background operations support [BKOPS_SUPPORT: 0x%02x]\n",
		ext_csd[502]);

	if (ext_csd_rev >= 7) {
		printf("Reserved blocks status [PRE_EOL_INFO: 0x%02x]\n",
			ext_csd[267]);
		printf("MLC erase count status [DEVICE_LIFE_TIME_EST_TYPE_A: 0x%02x]\n",
			ext_csd[268]);
		printf("SLC erase count status [DEVICE_LIFE_TIME_EST_TYPE_B: 0x%02x]\n",
			ext_csd[269]);
	}

	if (ext_csd_rev >= 6) {
		printf("Max Packet Read Cmd [MAX_PACKED_READS: 0x%02x]\n",
			ext_csd[501]);
		printf("Max Packet Write Cmd [MAX_PACKED_WRITES: 0x%02x]\n",
			ext_csd[500]);
		printf("Data TAG support [DATA_TAG_SUPPORT: 0x%02x]\n",
			ext_csd[499]);

		printf("Data TAG Unit Size [TAG_UNIT_SIZE: 0x%02x]\n",
			ext_csd[498]);
		printf("Tag Resources Size [TAG_RES_SIZE: 0x%02x]\n",
			ext_csd[497]);
		printf("Context Management Capabilities"
			" [CONTEXT_CAPABILITIES: 0x%02x]\n", ext_csd[496]);
		printf("Large Unit Size [LARGE_UNIT_SIZE_M1: 0x%02x]\n",
			ext_csd[495]);
		printf("Extended partition attribute support"
			" [EXT_SUPPORT: 0x%02x]\n", ext_csd[494]);
		printf("Generic CMD6 Timer [GENERIC_CMD6_TIME: 0x%02x]\n",
			ext_csd[248]);
		printf("Power off notification [POWER_OFF_LONG_TIME: 0x%02x]\n",
			ext_csd[247]);
		printf("Cache Size [CACHE_SIZE] is %d KiB\n",
			ext_csd[249] << 0 | (ext_csd[250] << 8) |
			(ext_csd[251] << 16) | (ext_csd[252] << 24));
	}

	/* A441: Reserved [501:247]
	    A43: reserved [246:229] */
	if (ext_csd_rev >= 5) {
		printf("Background operations status"
			" [BKOPS_STATUS: 0x%02x]\n", ext_csd[246]);

		/* CORRECTLY_PRG_SECTORS_NUM [245:242] TODO */

		printf("1st Initialisation Time after programmed sector"
			" [INI_TIMEOUT_AP: 0x%02x]\n", ext_csd[241]);

		/* A441: reserved [240] */
		printf("Power class for 52MHz, DDR at 3.6V"
			" [PWR_CL_DDR_52_360: 0x%02x]\n", ext_csd[239]);
		printf("Power class for 52MHz, DDR at 1.95V"
			" [PWR_CL_DDR_52_195: 0x%02x]\n", ext_csd[238]);

		/* A441: reserved [237-236] */

		if (ext_csd_rev >= 6) {
			printf("Power class for 200MHz at 3.6V"
				" [PWR_CL_200_360: 0x%02x]\n", ext_csd[237]);
			printf("Power class for 200MHz, at 1.95V"
				" [PWR_CL_200_195: 0x%02x]\n", ext_csd[236]);
		}
		printf("Minimum Performance for 8bit at 52MHz in DDR mode:\n");
		printf(" [MIN_PERF_DDR_W_8_52: 0x%02x]\n", ext_csd[235]);
		printf(" [MIN_PERF_DDR_R_8_52: 0x%02x]\n", ext_csd[234]);
		/* A441: reserved [233] */
		printf("TRIM Multiplier [TRIM_MULT: 0x%02x]\n", ext_csd[232]);
		printf("Secure Feature support [SEC_FEATURE_SUPPORT: 0x%02x]\n",
			ext_csd[231]);
	}
	if (ext_csd_rev == 5) { /* Obsolete in 4.5 */
		printf("Secure Erase Multiplier [SEC_ERASE_MULT: 0x%02x]\n",
			ext_csd[230]);
		printf("Secure TRIM Multiplier [SEC_TRIM_MULT: 0x%02x]\n",
			ext_csd[229]);
	}
	reg = ext_csd[EXT_CSD_BOOT_INFO];
	printf("Boot Information [BOOT_INFO: 0x%02x]\n", reg);
	if (reg & EXT_CSD_BOOT_INFO_ALT)
		printf(" Device supports alternative boot method\n");
	if (reg & EXT_CSD_BOOT_INFO_DDR_DDR)
		printf(" Device supports dual data rate during boot\n");
	if (reg & EXT_CSD_BOOT_INFO_HS_MODE)
		printf(" Device supports high speed timing during boot\n");

	/* A441/A43: reserved [227] */
	printf("Boot partition size [BOOT_SIZE_MULTI: 0x%02x]\n", ext_csd[226]);
	printf("Access size [ACC_SIZE: 0x%02x]\n", ext_csd[225]);

	reg = get_hc_erase_grp_size(ext_csd);
	printf("High-capacity erase unit size [HC_ERASE_GRP_SIZE: 0x%02x]\n",
		reg);
	printf(" i.e. %u KiB\n", 512 * reg);

	printf("High-capacity erase timeout [ERASE_TIMEOUT_MULT: 0x%02x]\n",
		ext_csd[223]);
	printf("Reliable write sector count [REL_WR_SEC_C: 0x%02x]\n",
		ext_csd[222]);

	reg = get_hc_wp_grp_size(ext_csd);
	printf("High-capacity W protect group size [HC_WP_GRP_SIZE: 0x%02x]\n",
		reg);
	printf(" i.e. %lu KiB\n", 512l * get_hc_erase_grp_size(ext_csd) * reg);

	printf("Sleep current (VCC) [S_C_VCC: 0x%02x]\n", ext_csd[220]);
	printf("Sleep current (VCCQ) [S_C_VCCQ: 0x%02x]\n", ext_csd[219]);
	/* A441/A43: reserved [218] */
	printf("Sleep/awake timeout [S_A_TIMEOUT: 0x%02x]\n", ext_csd[217]);
	/* A441/A43: reserved [216] */

	unsigned int sectors =	get_sector_count(ext_csd);
	printf("Sector Count [SEC_COUNT: 0x%08x]\n", sectors);
	if (is_blockaddresed(ext_csd))
		printf(" Device is block-addressed\n");
	else
		printf(" Device is NOT block-addressed\n");

	/* A441/A43: reserved [211] */
	printf("Minimum Write Performance for 8bit:\n");
	printf(" [MIN_PERF_W_8_52: 0x%02x]\n", ext_csd[210]);
	printf(" [MIN_PERF_R_8_52: 0x%02x]\n", ext_csd[209]);
	printf(" [MIN_PERF_W_8_26_4_52: 0x%02x]\n", ext_csd[208]);
	printf(" [MIN_PERF_R_8_26_4_52: 0x%02x]\n", ext_csd[207]);
	printf("Minimum Write Performance for 4bit:\n");
	printf(" [MIN_PERF_W_4_26: 0x%02x]\n", ext_csd[206]);
	printf(" [MIN_PERF_R_4_26: 0x%02x]\n", ext_csd[205]);
	/* A441/A43: reserved [204] */
	printf("Power classes registers:\n");
	printf(" [PWR_CL_26_360: 0x%02x]\n", ext_csd[203]);
	printf(" [PWR_CL_52_360: 0x%02x]\n", ext_csd[202]);
	printf(" [PWR_CL_26_195: 0x%02x]\n", ext_csd[201]);
	printf(" [PWR_CL_52_195: 0x%02x]\n", ext_csd[200]);

	/* A43: reserved [199:198] */
	if (ext_csd_rev >= 5) {
		printf("Partition switching timing "
			"[PARTITION_SWITCH_TIME: 0x%02x]\n", ext_csd[199]);
		printf("Out-of-interrupt busy timing"
			" [OUT_OF_INTERRUPT_TIME: 0x%02x]\n", ext_csd[198]);
	}

	/* A441/A43: reserved	[197] [195] [193] [190] [188]
	 * [186] [184] [182] [180] [176] */

	if (ext_csd_rev >= 6)
		printf("I/O Driver Strength [DRIVER_STRENGTH: 0x%02x]\n",
			ext_csd[197]);

	/* DEVICE_TYPE in A45, CARD_TYPE in A441 */
	reg = ext_csd[196];
	printf("Card Type [CARD_TYPE: 0x%02x]\n", reg);
	if (reg & 0x20) printf(" HS200 Single Data Rate eMMC @200MHz 1.2VI/O\n");
	if (reg & 0x10) printf(" HS200 Single Data Rate eMMC @200MHz 1.8VI/O\n");
	if (reg & 0x08) printf(" HS Dual Data Rate eMMC @52MHz 1.2VI/O\n");
	if (reg & 0x04)	printf(" HS Dual Data Rate eMMC @52MHz 1.8V or 3VI/O\n");
	if (reg & 0x02)	printf(" HS eMMC @52MHz - at rated device voltage(s)\n");
	if (reg & 0x01) printf(" HS eMMC @26MHz - at rated device voltage(s)\n");

	printf("CSD structure version [CSD_STRUCTURE: 0x%02x]\n", ext_csd[194]);
	/* ext_csd_rev = ext_csd[192] (already done!!!) */
	printf("Command set [CMD_SET: 0x%02x]\n", ext_csd[191]);
	printf("Command set revision [CMD_SET_REV: 0x%02x]\n", ext_csd[189]);
	printf("Power class [POWER_CLASS: 0x%02x]\n", ext_csd[187]);
	printf("High-speed interface timing [HS_TIMING: 0x%02x]\n",
		ext_csd[185]);
	/* bus_width: ext_csd[183] not readable */
	printf("Erased memory content [ERASED_MEM_CONT: 0x%02x]\n",
		ext_csd[181]);
	reg = ext_csd[EXT_CSD_BOOT_CFG];
	printf("Boot configuration bytes [PARTITION_CONFIG: 0x%02x]\n", reg);
	switch ((reg & EXT_CSD_BOOT_CFG_EN)>>3) {
	case 0x0:
		printf(" Not boot enable\n");
		break;
	case 0x1:
		printf(" Boot Partition 1 enabled\n");
		break;
	case 0x2:
		printf(" Boot Partition 2 enabled\n");
		break;
	case 0x7:
		printf(" User Area Enabled for boot\n");
		break;
	}
	switch (reg & EXT_CSD_BOOT_CFG_ACC) {
	case 0x0:
		printf(" No access to boot partition\n");
		break;
	case 0x1:
		printf(" R/W Boot Partition 1\n");
		break;
	case 0x2:
		printf(" R/W Boot Partition 2\n");
		break;
	case 0x3:
		printf(" R/W Replay Protected Memory Block (RPMB)\n");
		break;
	default:
		printf(" Access to General Purpose partition %d\n",
			(reg & EXT_CSD_BOOT_CFG_ACC) - 3);
		break;
	}

	printf("Boot config protection [BOOT_CONFIG_PROT: 0x%02x]\n",
		ext_csd[178]);
	printf("Boot bus Conditions [BOOT_BUS_CONDITIONS: 0x%02x]\n",
		ext_csd[177]);
	printf("High-density erase group definition"
		" [ERASE_GROUP_DEF: 0x%02x]\n", ext_csd[EXT_CSD_ERASE_GROUP_DEF]);

	print_writeprotect_status(ext_csd);

	if (ext_csd_rev >= 5) {
		/* A441]: reserved [172] */
		printf("User area write protection register"
			" [USER_WP]: 0x%02x\n", ext_csd[171]);
		/* A441]: reserved [170] */
		printf("FW configuration [FW_CONFIG]: 0x%02x\n", ext_csd[169]);
		printf("RPMB Size [RPMB_SIZE_MULT]: 0x%02x\n", ext_csd[168]);
		printf("Write reliability setting register"
			" [WR_REL_SET]: 0x%02x\n", ext_csd[167]);
		printf("Write reliability parameter register"
			" [WR_REL_PARAM]: 0x%02x\n", ext_csd[166]);
		/* sanitize_start ext_csd[165]]: not readable
		 * bkops_start ext_csd[164]]: only writable */
		printf("Enable background operations handshake"
			" [BKOPS_EN]: 0x%02x\n", ext_csd[163]);
		printf("H/W reset function"
			" [RST_N_FUNCTION]: 0x%02x\n", ext_csd[162]);
		printf("HPI management [HPI_MGMT]: 0x%02x\n", ext_csd[161]);
		reg = ext_csd[EXT_CSD_PARTITIONING_SUPPORT];
		printf("Partitioning Support [PARTITIONING_SUPPORT]: 0x%02x\n",
			reg);
		if (reg & EXT_CSD_PARTITIONING_EN)
			printf(" Device support partitioning feature\n");
		else
			printf(" Device NOT support partitioning feature\n");
		if (reg & EXT_CSD_ENH_ATTRIBUTE_EN)
			printf(" Device can have enhanced tech.\n");
		else
			printf(" Device cannot have enhanced tech.\n");

		max_size = (ext_csd[159] << 16) | (ext_csd[158] << 8) |
			ext_csd[157];
		printf("Max Enhanced Area Size [MAX_ENH_SIZE_MULT]: 0x%06x\n",
			   max_size);
		unsigned int wp_sz = get_hc_wp_grp_size(ext_csd);
		unsigned int erase_sz = get_hc_erase_grp_size(ext_csd);
		printf(" i.e. %lu KiB\n", 512l * max_size * wp_sz * erase_sz);

		printf("Partitions attribute [PARTITIONS_ATTRIBUTE]: 0x%02x\n",
			ext_csd[EXT_CSD_PARTITIONS_ATTRIBUTE]);
		reg = ext_csd[EXT_CSD_PARTITION_SETTING_COMPLETED];
		printf("Partitioning Setting"
			" [PARTITION_SETTING_COMPLETED]: 0x%02x\n",
			reg);
		if (reg)
			printf(" Device partition setting complete\n");
		else
			printf(" Device partition setting NOT complete\n");

		printf("General Purpose Partition Size\n"
			" [GP_SIZE_MULT_4]: 0x%06x\n", (ext_csd[154] << 16) |
			(ext_csd[153] << 8) | ext_csd[152]);
		printf(" [GP_SIZE_MULT_3]: 0x%06x\n", (ext_csd[151] << 16) |
			   (ext_csd[150] << 8) | ext_csd[149]);
		printf(" [GP_SIZE_MULT_2]: 0x%06x\n", (ext_csd[148] << 16) |
			   (ext_csd[147] << 8) | ext_csd[146]);
		printf(" [GP_SIZE_MULT_1]: 0x%06x\n", (ext_csd[145] << 16) |
			   (ext_csd[144] << 8) | ext_csd[143]);

		reg =	(ext_csd[EXT_CSD_ENH_SIZE_MULT_2] << 16) |
			(ext_csd[EXT_CSD_ENH_SIZE_MULT_1] << 8) |
			ext_csd[EXT_CSD_ENH_SIZE_MULT_0];
		printf("Enhanced User Data Area Size"
			" [ENH_SIZE_MULT]: 0x%06x\n", reg);
		printf(" i.e. %lu KiB\n", 512l * reg *
		       get_hc_erase_grp_size(ext_csd) *
		       get_hc_wp_grp_size(ext_csd));

		reg =	(ext_csd[EXT_CSD_ENH_START_ADDR_3] << 24) |
			(ext_csd[EXT_CSD_ENH_START_ADDR_2] << 16) |
			(ext_csd[EXT_CSD_ENH_START_ADDR_1] << 8) |
			ext_csd[EXT_CSD_ENH_START_ADDR_0];
		printf("Enhanced User Data Start Address"
			" [ENH_START_ADDR]: 0x%06x\n", reg);
		printf(" i.e. %lu bytes offset\n", (is_blockaddresed(ext_csd) ?
				1l : 512l) * reg);

		/* A441]: reserved [135] */
		printf("Bad Block Management mode"
			" [SEC_BAD_BLK_MGMNT]: 0x%02x\n", ext_csd[134]);
		/* A441: reserved [133:0] */
	}
	/* B45 */
	if (ext_csd_rev >= 6) {
		int j;
		/* tcase_support ext_csd[132] not readable */
		printf("Periodic Wake-up [PERIODIC_WAKEUP]: 0x%02x\n",
			ext_csd[131]);
		printf("Program CID/CSD in DDR mode support"
			" [PROGRAM_CID_CSD_DDR_SUPPORT]: 0x%02x\n",
			   ext_csd[130]);

		for (j = 127; j >= 64; j--)
			printf("Vendor Specific Fields"
				" [VENDOR_SPECIFIC_FIELD[%d]]: 0x%02x\n",
				j, ext_csd[j]);

		printf("Native sector size [NATIVE_SECTOR_SIZE]: 0x%02x\n",
			ext_csd[63]);
		printf("Sector size emulation [USE_NATIVE_SECTOR]: 0x%02x\n",
			ext_csd[62]);
		printf("Sector size [DATA_SECTOR_SIZE]: 0x%02x\n", ext_csd[61]);
		printf("1st initialization after disabling sector"
			" size emulation [INI_TIMEOUT_EMU]: 0x%02x\n",
			ext_csd[60]);
		printf("Class 6 commands control [CLASS_6_CTRL]: 0x%02x\n",
			ext_csd[59]);
		printf("Number of addressed group to be Released"
			"[DYNCAP_NEEDED]: 0x%02x\n", ext_csd[58]);
		printf("Exception events control"
			" [EXCEPTION_EVENTS_CTRL]: 0x%04x\n",
			(ext_csd[57] << 8) | ext_csd[56]);
		printf("Exception events status"
			"[EXCEPTION_EVENTS_STATUS]: 0x%04x\n",
			(ext_csd[55] << 8) | ext_csd[54]);
		printf("Extended Partitions Attribute"
			" [EXT_PARTITIONS_ATTRIBUTE]: 0x%04x\n",
			(ext_csd[53] << 8) | ext_csd[52]);

		for (j = 51; j >= 37; j--)
			printf("Context configuration"
				" [CONTEXT_CONF[%d]]: 0x%02x\n", j, ext_csd[j]);

		printf("Packed command status"
			" [PACKED_COMMAND_STATUS]: 0x%02x\n", ext_csd[36]);
		printf("Packed command failure index"
			" [PACKED_FAILURE_INDEX]: 0x%02x\n", ext_csd[35]);
		printf("Power Off Notification"
			" [POWER_OFF_NOTIFICATION]: 0x%02x\n", ext_csd[34]);
		printf("Control to turn the Cache ON/OFF"
			" [CACHE_CTRL]: 0x%02x\n", ext_csd[33]);
		/* flush_cache ext_csd[32] not readable */
		/*Reserved [31:0] */
	}

out_free:
	return ret;
}

int do_sanitize(int nargs, char **argv)
{
	int fd, ret;
	char *device;
	struct timeval start, end;
	__u32 response;	
	__u32 time;

	CHECK(nargs != 2, "Usage: mmc sanitize </path/to/mmcblkX>\n",
			exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	gettimeofday(&start, NULL);

	ret = write_extcsd_value(fd, EXT_CSD_SANITIZE_START, 1);
	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to EXT_CSD[%d] in %s\n",
			1, EXT_CSD_SANITIZE_START, device);
		exit(1);
	}

	printf("Sanitize in process...\n");

	for(;;){
		ret = send_status(fd, 0, &response);
		if (ret) {
			fprintf(stderr, "Could not read response to SEND_STATUS from %s\n", device);
			exit(1);
		}
		if(0x9 == ((response >> 8) & 0x1f)) //back to tran state
			break;
	}

	gettimeofday(&end, NULL);

	time = (end.tv_sec-start.tv_sec)*1000000 + (end.tv_usec-start.tv_usec);
		printf("[sanitize-cost] %u us\n", time);

	return ret;

}

int power_off_notification(int nargs, char **argv)
{
	__u8 ext_csd[512], value;
	int fd, ret;
	char *device, *option;

	CHECK(nargs != 3, "Usage: power off  </path/to/mmcblkX> short|long\n",
		exit(1));

	device = argv[1];
	option = argv[2];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if (ret) {
		fprintf(stderr, "Could not read EXT_CSD from %s\n", device);
		exit(1);
	}

	if (!(ext_csd[EXT_CSD_POWER_OFF_NOTIFICATION] & 0x11)) {
		printf("Power off notification not set, should set to POWERED_ON first\n");
		exit(1);
	}

	if(!strcmp(option,"short")){
		ret = write_extcsd_value(fd, EXT_CSD_POWER_OFF_NOTIFICATION, EXT_CSD_POWER_OFF_SHORT);
		value = EXT_CSD_POWER_OFF_SHORT;
	}else if(!strcmp(option,"long")){
		ret = write_extcsd_value(fd, EXT_CSD_POWER_OFF_NOTIFICATION, EXT_CSD_POWER_OFF_LONG);
		value = EXT_CSD_POWER_OFF_LONG;
	}else{
		printf("Usage: ./power-off </path/to/mmcblkX> short|long\n");
		exit(1);
	}

	if (ret) {
		fprintf(stderr, "Could not write 0x%02x to EXT_CSD[%d] in %s\n",
			value, EXT_CSD_BKOPS_EN, device);
		exit(1);
	}

	return ret;
}

int do_go_idle(int nargs, char **argv)
{
	char *device;
	struct mmc_ioc_cmd idata;
	int fd;
	__u32 arg=0;
	int c=0, ret=0;

	CHECK(nargs != 3, "Usage: mmc cmd00 -a[arg: 0x00000000|0xF0F0F0F0|0xFFFFFFFA] </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"a:"))!=-1){
		switch(c){
			case 'a':
				arg=valid_digit(optarg);
				break;
			case '?':
				printf("Usage: mmc cmd00 -a[arg: 0x00000000|0xF0F0F0F0|0xFFFFFFFA] </path/to/mmcblkX>\n");
				exit(1);
				break;
			default:
				break;
		}
	}
	memset(&idata, 0, sizeof(idata));
	idata.opcode = MMC_GO_IDLE_STATE;
	idata.arg = arg;
	idata.flags = MMC_RSP_SPI_S1 |  MMC_CMD_BC;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");

	return ret;
}

int do_send_op_cond(int nargs, char **argv)
{
	char *device;
	struct mmc_ioc_cmd idata;
	int fd;
	__u32 arg=0;
	int c=0, ret=0;

	CHECK(nargs != 3, "Usage: mmc cmd1 -a[arg: 0x40FF8080] </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"a:"))!=-1){
		switch(c){
			case 'a':
				arg=valid_digit(optarg);
				break;
			case '?':
				printf("Usage: mmc cmd1 -a[arg: 0x40FF8080] </path/to/mmcblkX>\n");
				exit(1);
				break;
			default:
				break;
		}
	}
	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 0;
	idata.opcode = MMC_SEND_OP_COND;
	idata.arg = arg;
	idata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R3 | MMC_CMD_BCR;


again:	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");
	printf("Response: 0x%08x\n", idata.response[0]);
	
	if(!(idata.response[0] >> 31))
		goto again;

	return ret;
}

int do_all_send_cid(int nargs, char **argv)
{
	char *device;
	struct mmc_ioc_cmd idata;
	int fd;
	int ret=0;

	CHECK(nargs != 2, "Usage: mmc cmd02 </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 0;
	idata.opcode = MMC_ALL_SEND_CID;
	idata.flags = MMC_RSP_R2 | MMC_CMD_BCR;


	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");
	printf("Response: 0x%8x\n", idata.response[0]);
	printf("Response: 0x%8x\n", idata.response[1]);
	printf("Response: 0x%8x\n", idata.response[2]);
	printf("Response: 0x%8x\n", idata.response[3]);

	return ret;
}

void dump_csd(__u32 *data){
	__u32 csize= (lib_get_offset(data[1],0,10)<<2) | lib_get_offset(data[2],30,2);

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
}

int do_send_csd(int nargs, char **argv)
{
	char *device;
	struct mmc_ioc_cmd idata;
	int fd;
	__u8 rca=1;
	int c=0, ret=0;

	CHECK(nargs != 3, "Usage: mmc cmd09 -r[RCA] </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"r:"))!=-1){
		switch(c){
			case 'r':
				rca=valid_digit(optarg);
				break;
			case '?':
				printf("Usage: mmc cmd09 -r[RCA] </path/to/mmcblkX>\n");
				exit(1);
				break;
			default:
				break;
		}
	}
	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 0;
	idata.opcode = MMC_SEND_CSD;
	idata.arg = rca << 16;
	idata.flags = MMC_RSP_R2 | MMC_CMD_AC;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");
	dump_csd(idata.response);
	return ret;
}

const char *decode_cbx(int number){
	if(number<0 || number >4){
		return g_cbx[4];
	}
	return g_cbx[number];
}

uint dump_cid(__u32 *data){

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
	printf("\tCard/BGA:\t%s\n",decode_cbx(lib_get_offset(data[0],16,2)));
	printf("\tMID:\t\t0x%x\n",lib_get_offset(data[0],24,8));
	
	return 0;
}

int do_send_cid(int nargs, char **argv)
{
	char *device;
	struct mmc_ioc_cmd idata;
	int fd;
	__u8 rca=1;
	int c=0, ret=0;

	CHECK(nargs != 3, "Usage: mmc cmd10 -r[RCA] </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"r:"))!=-1){
		switch(c){
			case 'r':
				rca=valid_digit(optarg);
				break;
			case '?':
				printf("Usage: mmc cmd10 -r[RCA] </path/to/mmcblkX>\n");
				exit(1);
				break;
			default:
				break;
		}
	}
	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 0;
	idata.opcode = MMC_SEND_CID;
	idata.arg = rca << 16;
	idata.flags = MMC_RSP_R2 | MMC_CMD_AC;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");
	dump_cid(idata.response);
	return ret;
}

int do_set_dsr(int nargs, char **argv)
{
	char *device;
	struct mmc_ioc_cmd idata;
	int fd;
	__u32 arg=0;
	int c=0, ret=0;

	CHECK(nargs != 3, "Usage: mmc cmd04 -a[DSR] </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"a:"))!=-1){
		switch(c){
			case 'a':
				arg=valid_digit(optarg);
				break;
			case '?':
				printf("Usage: mmc cmd04 -a[DSR] </path/to/mmcblkX>\n");
				exit(1);
				break;
			default:
				break;
		}
	}
	memset(&idata, 0, sizeof(idata));
	idata.opcode = MMC_SET_DSR;
	idata.arg = arg;
	idata.flags = MMC_RSP_SPI_S1 | MMC_CMD_BC;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");

	return ret;
}

int do_sleep_awake(int nargs, char **argv)
{
	char *device;
	struct mmc_ioc_cmd idata;
	int fd;
	__u8 rca=0,mode=0;
	int c=0, ret=0;

	CHECK(nargs != 4, "Usage: mmc cmd05 -r[RCA] -m[mode 1:sleep 0:awake] </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"r:m:"))!=-1){
		switch(c){
			case 'r':
				rca=valid_digit(optarg);
				break;
			case 'm':
				mode=valid_digit(optarg);
				break;
			case '?':
				printf("Usage: mmc cmd05 -r[RCA] -m[mode 1:sleep 0:awake] </path/to/mmcblkX>\n");
				exit(1);
				break;
			default:
				break;
		}
	}
	memset(&idata, 0, sizeof(idata));
	idata.opcode = MMC_SLEEP_AWAKE;
	idata.arg = (rca << 16) | (mode << 15);
	idata.flags = MMC_RSP_R1B | MMC_CMD_AC;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");

	return ret;
}

int do_config_extcsd(int nargs, char **argv)
{
	char *device;
	struct mmc_ioc_cmd idata;
	int fd;
	__u8 mode=0, index=0, value=0;
	int c=0, ret=0;

	CHECK(nargs != 5, "Usage: mmc cmd06 -m[mode] -a[index] -d[value] </path/to/mmcblkX>\n\n\t -m : Mode select. 1 for set bits, 2 for clear bits, 3 for write byte (recommanded).\n\t -a : Extended CSD index.\n\t -d : Value to be set.\n",
			  exit(1));

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"m:a:d:"))!=-1){
		switch(c){
			case 'm':
				mode=valid_digit(optarg);
				break;
			case 'a':
				index=valid_digit(optarg);
				break;
			case 'd':
				value=valid_digit(optarg);
				break;
			case '?':
				printf("Usage: mmc cmd06 -m[mode] -a[index] -d[value] </path/to/mmcblkX>\n");
				exit(1);
				break;
			default:
				break;
		}
	}
	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 1;
	idata.opcode = MMC_SWITCH;
	idata.arg = (mode << 24) | (index << 16) | (value << 8) | EXT_CSD_CMD_SET_NORMAL;
	idata.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");

	return ret;
}

int do_select_deselect_card(int nargs, char **argv)
{
	char *device;
	struct mmc_ioc_cmd idata;
	int fd;
	__u8 rca=0;
	int c=0, ret=0;

	CHECK(nargs != 3, "Usage: mmc cmd07 -r[RCA] </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"r:"))!=-1){
		switch(c){
			case 'r':
				rca=valid_digit(optarg);
				break;
			case '?':
				printf("Usage: mmc cmd07 -r[RCA] </path/to/mmcblkX>\n");
				exit(1);
				break;
			default:
				break;
		}
	}
	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 0;
	idata.opcode = MMC_SELECT_DESELECT_CARD;
	idata.arg = rca << 16;
	idata.flags = MMC_RSP_R1B | MMC_CMD_AC;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret)
		perror("ioctl");

	return ret;
}

//int do_partition_switch(int nargs, char **argv)
int do_single_read(int nargs, char **argv)
{
	char *device;
	struct mmc_ioc_cmd idata;
	struct timeval start, end;
	int fd, ret=0,i, prtime=0, quiet=0;
	int c=0;
	__u8 data_out[512];
	__u32 arg=0;
	__u32 time=0;
	char *help = "Usage: mmc cmd17 -s[start] [-t] [-q] </path/to/mmcblkX>\n";

	if(nargs < 3){
		printf("%s\n", help);
		exit(1);
	}

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"s:tq"))!=-1){
		switch(c){
			case 's':
				arg=valid_digit(optarg);
				break;
			case 't':
				prtime=1;
				break;
			case 'q':
				quiet=1;
				break;
			case '?':
				printf("%s\n", help);
				exit(1);
				break;
			default:
				break;
		}
	}

	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 0;
	idata.opcode = MMC_READ_SINGLE_BLOCK;
	idata.arg = arg;
	idata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
	idata.blksz = 512;
	idata.blocks = 1;
	mmc_ioc_cmd_set_data(idata, data_out);

	gettimeofday(&start, NULL);
	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret){
		perror("ioctl");
		return ret;
	}
	gettimeofday(&end, NULL);	


	if(!quiet){
		for(i=0; i<512; i++){
			printf("0x%02x\t", data_out[i]);
			if(0 == (i+1)%16)
				printf("\n");
		}
	}

	if(!quiet && prtime){
		time = (end.tv_sec-start.tv_sec)*1000000 + (end.tv_usec-start.tv_usec);
		printf("CMD17, 1 sector, spends %u us\n", time);
	}
	
	return ret;
}

int do_multiple_read(int nargs, char **argv)
{
	char *device,*target=NULL;
	struct mmc_ioc_cmd idata;
	struct timeval start, end;
	int fd, ret=0, i=0, stop=0, prtime=0, quiet=0;
	int c=0, use_pat=0, loop=0;
	FILE *fp;
	__u32 args=0, arge=0, blocks=0;
	__u32 time=0;
	char *help = "Usage: mmc cmd18 -s[start] -e[end] -p[output to target] [-n] [-t] [-q] </path/to/mmcblkX>\n\n\t -n : No auto cmd12. If controller can handle atuo stop, this option is useless.\n\t -t : Print time spent.\n\t -q : Quiet, no print.\n";

	if(nargs < 4){
		printf("%s\n", help);
		exit(1);
	}

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"s:e:p:ntq"))!=-1){
		switch(c){
			case 's':
				args=valid_digit(optarg);
				break;
			case 'e':
				arge=valid_digit(optarg);
				break;
			case 'p':
				use_pat=1;
				target=optarg;
				break;
			case 'n':
				stop=1;
				break;
			case 't':
				prtime=1;
				break;
			case 'q':
				quiet=1;
				break;
			case '?':
				printf("%s\n", help);
				exit(1);
				break;
			default:
				break;
		}
	}
	if((blocks=arge-args+1) < 1){
		printf("End address should NOT be less than start address\n");
		exit(1);
	}
	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 0;
	idata.opcode = MMC_READ_MULTIPLE_BLOCK;
	idata.arg = args;
	idata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
	idata.blksz = 512;
	idata.blocks = blocks;
	__u8 data_out[512*blocks];
	mmc_ioc_cmd_set_data(idata, data_out);

	gettimeofday(&start, NULL);
	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret){
		perror("ioctl");
		return ret;
	}
	gettimeofday(&end, NULL);
	
	if(!stop){
		ret = send_stop(fd, 0, NULL);
		if(ret)
			return ret;
	}

	if(!use_pat && !quiet){
		loop = 512*blocks;
		printf("\nSector %d:\n",args);
		for(i=0; i<loop; i++){
			printf("0x%02x\t", data_out[i]);
			if(0 == (i+1)%16)
				printf("\n");
			if(0 == (i+1)%512 && i != (loop-1)){
				printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
				printf("\nSector %d:\n",++args);
			}
		}
	}else if(1 == use_pat){
		fp=fopen(target,"w+");	
		if(!fp){
			printf("Fail to create target out file\n");
			exit(1);
		}
		ret=fwrite(data_out,512,blocks,fp);
		fclose(fp);
	}

	if(prtime){
		time = (end.tv_sec-start.tv_sec)*1000000 + (end.tv_usec-start.tv_usec);
		printf("CMD18, %d sector(s), spends %u us\n", blocks, time);
	}

	close(fd);
	return ret;
}

static int do_write(int fd, int opcode, __u32 arg, void * data_in, __u32 len)
{
	struct mmc_ioc_cmd idata;
	int ret=0;

	if(!len || !data_in)
		return 0;

	memset(&idata, 0, sizeof(idata));
	idata.write_flag = 1;
	idata.opcode = opcode;
	idata.arg = arg;
	idata.data_ptr = (__u32)data_in;
	idata.cmd_timeout_ms = 400;
	idata.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_ADTC;
	idata.blksz = 512;
	idata.blocks = len;

	ret = ioctl(fd, MMC_IOC_CMD, &idata);
	if (ret){
		perror("ioctl");
	}
	return ret;
}

int do_single_write(int nargs, char **argv)
{
	char *device;
	struct timeval start, end;
	int fd, ret=0;
	int c=0, prtime=0;
	__u32 arg=0,ch=0;
	__u32 *data_in;
	__u32 time=0;
	char *help = "Usage: mmc cmd24 -s[start] -d[data] [-t] </path/to/mmcblkX>\n\n\t -t : Print time spent.\n";

	if(nargs < 4){
		printf("%s\n", help);
		exit(1);
	}

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"s:d:t"))!=-1){
		switch(c){
			case 's':
				arg=valid_digit(optarg);
				break;
			case 'd':
				ch=valid_digit(optarg);
				break;
			case 't':
				prtime = 1;
				break;
			case '?':
				printf("%s\n", help);
				exit(1);
				break;
			default:
				break;
		}
	}

	data_in = (__u32 *)malloc(sizeof(__u8)*512);
	if(!data_in){
		printf("Error: failed at malloc\n");
		exit(1);
	}

	memset(data_in,ch,sizeof(__u8)*512);

	gettimeofday(&start, NULL);

	do_write(fd, MMC_WRITE_SINGLE_BLOCK, arg, data_in, 1);	

	gettimeofday(&end, NULL);

	if(prtime){
		time = (end.tv_sec-start.tv_sec)*1000000 + (end.tv_usec-start.tv_usec);
		printf("CMD24, 1 sector, spends %u us\n", time);
	}

	free(data_in);
	close(fd);
	return ret;
}

int do_multiple_write(int nargs, char **argv)
{
	char *device,*pattern=NULL;
	struct timeval start, end;
	int fd, ret=0, blocks=0, stop=0, prtime=0;
	int c=0,use_pat=0,size=0;
	FILE *fp=NULL;
	__u32 arg=0, arge=0, ch=0;
	__u8 *data_in;
	__u32 time=0;
	__u32 response;

	char *help="Usage: mmc cmd25 -s[start] -e[end] -d[data] -p[pattern data] [-n] [-t] </path/to/mmcblkX>\n\n\t -d : Data to write.\n\t -p : Path to pattern data file.\n\t -n : No auto cmd12. Use this option if controller can handle auto stop.\n\t -t : Print time spent.\n\nExample : mmc cmd25 -s0 -e2 -d0x55 -t /dev/mmcblk0";

	if(nargs < 5){
		printf("%s\n",help);
		exit(1);
	}	

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"s:e:d:p:nt"))!=-1){
		switch(c){
			case 's':
				arg=valid_digit(optarg);
				break;
			case 'e':
				arge=valid_digit(optarg);
				break;
			case 'd':
				ch=valid_digit(optarg);
				break;
			case 'p':
				use_pat=1;
				pattern=optarg;
				break;
			case 'n':
				stop=1;
				break;
			case 't':
				prtime=1;
				break;
			case '?':
				printf("%s\n",help);
				exit(1);
				break;
			default:
				break;
		}
	}

	if((blocks=arge-arg+1) < 1){
		printf("End address should NOT be less than start address\n");
		exit(1);
	}
	data_in = (__u8 *)malloc(512*blocks);
	if(!data_in){
		printf("Error: failed at malloc\n");
		exit(1);
	}

	memset(data_in, 0, 512*blocks);
	if(use_pat){
		fp=fopen(pattern, "rb");
		if(!fp){
			printf("Fail to open pattern data file!\n");
			exit(1);
		}
		fseek(fp,0,SEEK_END);
		size=ftell(fp);
		rewind(fp);
		if(size > blocks*512)
			ret=fread(data_in, blocks*512, 1, fp);
		else
			ret=fread(data_in, size, 1, fp);
		fclose(fp);
	}else{
		memset(data_in, ch, 512*blocks);
	}
	
	ret = do_write(fd, MMC_WRITE_MULTIPLE_BLOCK, arg, data_in, blocks);
	if(ret)
		goto out;

	gettimeofday(&start, NULL);

	if(!stop){
		ret = send_stop(fd, 0, NULL);
		if(ret)
			goto out;
	}
	
	for(;;){
		ret = send_status(fd, 0, &response);
		if (ret) {
			fprintf(stderr, "Could not read response to SEND_STATUS from %s\n", device);
			exit(1);
		}
		if(0x9 == ((response >> 8) & 0x1f)) //back to tran state
			break;	
	}

	gettimeofday(&end, NULL);

	if(prtime){
		time = (end.tv_sec-start.tv_sec)*1000000 + (end.tv_usec-start.tv_usec);
		printf("[prog-cost] %u us\n", time);
	}

out:
	free(data_in);
	close(fd);
	return ret;
}

int do_get_bad_block_general_info(int nargs, char **argv)
{
	char *device;
	int fd, ret=0;
	__u32 arg;
	__u8 data_out[512]={0};
	CHECK(nargs != 2, "Usage: mmc bad block </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

        arg = 1         | /* read/write : read mode */
              0x08 << 1;  /* cmd index : bad block count */
	ret = mmc_read_health_status(fd, arg, data_out);
	if(ret)
		goto out;

	printf("Retrieving...\n");
	sleep(3);
	printf("Bad block info:\n");
	printf("=========================================================================================\n");
	printf("| Total initial bad block count | Total later bad block count | Total spare block count |\n");
	printf("|");
	printf("            0x%02x%02x             |", data_out[0], data_out[1]);
	printf("           0x%02x%02x            |", data_out[2], data_out[3]);
	printf("         0x%02x%02x          |", data_out[4], data_out[5]);
	printf("\n=========================================================================================\n");
out:
	close(fd);
	return ret;
}

int do_get_general_ec_info(int nargs, char **argv)
{
	char *device;
	int fd, ret=0;
	__u8 data_out[512]={0};
	__u32 arg=0;
	
	CHECK(nargs != 2, "Usage: mmc bad block </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

        arg = 1         | /* read/write : read mode */
              0x10 << 1;  /* cmd index : total ec */
	ret = mmc_read_health_status(fd, arg, data_out);
	if(ret)
		goto out;

	printf("Retrieving...\n");
	sleep(3);
	printf("Total Erase Count info:\n");
	printf("=========================================================================================\n");
	printf("| total Min Erase Count | total Max Erase Count | total Average Erase Count |\n");
	printf("|       0x%02x%02x         |        0x%02x%02x        |           0x%02x%02x         |", 
			data_out[0], data_out[1], data_out[2], data_out[3], data_out[4], data_out[5]);
	printf("\n=========================================================================================\n");

        arg = 1         | /* read/write : read mode */
              0x11 << 1;  /* cmd index :  MLC ec */
	memset(data_out, 0, 512);
	ret = mmc_read_health_status(fd, arg, data_out);
	if(ret)
		goto out;

	printf("Retrieving...\n");
	sleep(3);
	printf("MLC Erase Count info:\n");
	printf("=========================================================================================\n");
	printf("|  MLC Min Erase Count  |  MLC Max Erase Count  |  MLC Average Erase Count  |\n");
	printf("|       0x%02x%02x         |        0x%02x%02x        |           0x%02x%02x         |", 
			data_out[0], data_out[1], data_out[2], data_out[3], data_out[4], data_out[5]);
	printf("\n=========================================================================================\n");

        arg = 1         | /* read/write : read mode */
              0x12 << 1;  /* cmd index :  SLC ec */
	memset(data_out, 0, 512);
	ret = mmc_read_health_status(fd, arg, data_out);
	if(ret)
		goto out;

	printf("Retrieving...\n");
	sleep(3);
	printf("SLC Erase Count info:\n");
	printf("=========================================================================================\n");
	printf("|  SLC Min Erase Count  |  SLC Max Erase Count  |  SLC Average Erase Count  |\n");
	printf("|       0x%02x%02x         |        0x%02x%02x        |           0x%02x%02x         |", 
			data_out[0], data_out[1], data_out[2], data_out[3], data_out[4], data_out[5]);
	printf("\n=========================================================================================\n");

out:
	close(fd);
	return ret;
}


int do_get_detail_ec(int nargs, char **argv)
{
	char *device;
	int fd, ret=0;
	__u8 *data_buf=NULL;
	__u8 *type_buf=NULL;
	__u32 arg=0;
	__u32 table_num=0;
	__u32 index=0, i=0;
	__u16 blk=0,ec=0,type=0;
	char *tp=NULL;

	CHECK(nargs != 2, "Usage: mmc pe cycle </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}
	
	data_buf = (__u8*)malloc(512);
	type_buf = (__u8*)malloc(512);

        /* read total number of block erase count tables */
        arg = 1         | /* read/write : read mode */
              0x41 << 1;  /* cmd index : table number */
	ret = mmc_read_health_status(fd, arg, data_buf);
	if(ret)
		goto out;

	table_num = *data_buf;
	printf("Total Table: %d\n", table_num);
	
	while (table_num--) {
		/* Retrieve block erase count from each tables */
		arg = 1         | /* read/write : read mode */
		      0x42 << 1 | /* cmd index : block erase count */
		      table_num << 8; /* table index : table index to read */
		memset(data_buf, 0, 512);
		ret = mmc_read_health_status(fd, arg, data_buf);
		if(ret)
			goto out;

		/* Retrieve block address type from each tables */
		arg = 1         | /* read/write : read mode */
		      0x43 << 1 | /* cmd index : block address type */
		      table_num << 8; /* table index : table index to read */
		memset(type_buf, 0, 512);
		ret = mmc_read_health_status(fd, arg, type_buf);
		if(ret)
			goto out;

		/* The top half of data block stores the block number while the bottom half stores the EC respectively. */
		index = 512 / 4;
		i=0;
		while(index--) {
			if(((__u16)data_buf[i]<<8 | (__u16)data_buf[i+1]) != 0) {
				blk = (__u16)data_buf[i]<<8 | (__u16)data_buf[i+1];
				ec = (__u16)data_buf[i+256]<<8 | (__u16)data_buf[i+257];
				type =  (__u16)type_buf[i+256]<<8 | (__u16)type_buf[i+257];
				tp = (type == 0) ? "MLC" : "SLC";
				printf("BLK %d EC %d Type %s\n", blk, ec, tp);
				i += 2;
			}
		}	
	}
out:	
	close(fd);
	free(data_buf);
	return ret;
}

int do_get_SSR(int nargs, char **argv)
{
	char *device;
	int fd, ret=0;
	__u8 data_out[512]={0};
	__u32 arg=0;
	CHECK(nargs != 2, "Usage: mmc ssr  </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

        arg = 1         | /* read/write : read mode */
              0x1C << 1;  /* cmd index : SSR */
	ret = mmc_read_health_status(fd, arg, data_out);
	if(ret)
		goto out;

	printf("Retrieving...\n");
	sleep(3);
	printf("Initial Bad Block Count:     0x%02x%02x\n", data_out[0],data_out[1]);
	printf("Run time Bad Block Count:    0x%02x%02x\n", data_out[2],data_out[3]);
	printf("Remaining Spare Block Count: 0x%02x%02x\n", data_out[4],data_out[5]);
	printf("\n");
	printf("MLC Min Erase Count:         0x%02x%02x%02x%02x\n", 
		data_out[16],data_out[17],data_out[18],data_out[19]);
	printf("MLC Max Erase Count:         0x%02x%02x%02x%02x\n", 
		data_out[20],data_out[21],data_out[22],data_out[23]);
	printf("MLC Average Erase Count:     0x%02x%02x%02x%02x\n", 
		data_out[24],data_out[25],data_out[26],data_out[27]);
	printf("MLC Total Erase Count:       0x%02x%02x%02x%02x\n", 
		data_out[28],data_out[29],data_out[30],data_out[31]);
	printf("\n");
	printf("SLC Min Erase Count:         0x%02x%02x%02x%02x\n", 
		data_out[32],data_out[33],data_out[34],data_out[35]);
	printf("SLC Max Erase Count:         0x%02x%02x%02x%02x\n", 
		data_out[36],data_out[37],data_out[38],data_out[39]);
	printf("SLC Average Erase Count:     0x%02x%02x%02x%02x\n", 
		data_out[40],data_out[41],data_out[42],data_out[43]);
	printf("SLC Total Erase Count:       0x%02x%02x%02x%02x\n", 
		data_out[44],data_out[45],data_out[46],data_out[47]);
	printf("\n");
	printf("Cumulative Initialization Count: 0x%02x%02x%02x%02x\n", 
		data_out[51],data_out[50],data_out[49],data_out[48]);
	printf("Reclaim Read Count:              0x%02x%02x%02x%02x\n", 
		data_out[55],data_out[54],data_out[53],data_out[52]);
	printf("Written data size in 100MB unit: 0x%02x%02x%02x%02x\n", 
		data_out[71],data_out[70],data_out[69],data_out[68]);
	printf("\n");
	printf("Power Loss Count:         0x%02x%02x%02x%02x\n", 
		data_out[195],data_out[194],data_out[193],data_out[192]);
	printf("\n");
	printf("Auto Initialed Refresh read operation number:     0x%02x%02x%02x%02x\n",
		data_out[211],data_out[210],data_out[209],data_out[208]);
	printf("Auto Initialed Refresh auto read scan process:    0x%02x%02x%02x%02x\n",
		data_out[215],data_out[214],data_out[213],data_out[212]);
	
out:
	close(fd);
	return ret;
}

int do_cache_enable(int nargs, char **argv)
{
	char *device;
	int fd = 0, ret = 0;
	__u8 ext_csd[512] = {0};
	__u32 cache_size;
	
	CHECK(nargs != 2, "Usage: mmc cache enable  </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if(ret)
		goto out;
	cache_size = (ext_csd[252] << 24)|(ext_csd[251] << 16) | (ext_csd[250] << 8)
		| ext_csd[249];
	
	if(cache_size ==  0x0){
		printf("Cache not supported. see ECSD[252:249]\n");
		exit(0);
	}

	ret = write_extcsd_value(fd, EXT_CSD_CACHE_CTRL, EXT_CSD_CACHE_ON);
	if(ret)
		return ret;
out:
	close(fd);
	return ret;
}

int do_cache_flush(int nargs, char **argv)
{
	char *device;
	int fd = 0, ret = 0;
	__u8 ext_csd[512] = {0};
	__u32 cache_size;
	
	CHECK(nargs != 2, "Usage: mmc cache flush  </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if(ret)
		goto out;

	cache_size = (ext_csd[252] << 24)|(ext_csd[251] << 16) | (ext_csd[250] << 8)
		| ext_csd[249];
	
	if(cache_size ==  0x0){
		printf("Cache not supported. see ECSD[252:249]\n");
		exit(0);
	}

	ret = write_extcsd_value(fd, EXT_CSD_CACHE_FLUSH, EXT_CSD_CACHE_FLUSH_TRIG);
	if(ret)
		return ret;
out:	
	close(fd);
	return ret;
}

int do_cache_disable(int nargs, char **argv)
{
	char *device;
	int fd = 0, ret = 0;
	__u8 ext_csd[512] = {0};
	__u32 cache_size;
	
	CHECK(nargs != 2, "Usage: mmc cache disable  </path/to/mmcblkX>\n",
			  exit(1));

	device = argv[1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	ret = read_extcsd(fd, ext_csd);
	if(ret)
		goto out;

	cache_size = (ext_csd[252] << 24)|(ext_csd[251] << 16) | (ext_csd[250] << 8)
		| ext_csd[249];
	
	if(cache_size ==  0x0){
		printf("Cache not supported. see ECSD[252:249]\n");
		exit(0);
	}

	ret = write_extcsd_value(fd, EXT_CSD_CACHE_CTRL, EXT_CSD_CACHE_OFF);
	if(ret)
		return ret;
out:	
	close(fd);
	return ret;
}

int do_ffu(int nargs, char **argv)
{
	char *device,*pattern=NULL;
	int fd, ret=0, blocks=0, stop=0;
	int c=0;
	FILE *fp=NULL;
	__u8 *data_in;
	__u32 size=0;
	__u32 saddr=0xFFFF;	
	__u8 ext_csd[512];
	char *help="Usage: mmc ffu -f[file] [-n] </path/to/mmcblkX>\n\n\t -f : Path to firmare binary file.\n\t -n : No auto cmd12. Use this option if controller can handle auto stop.\n";
	int write_offset= 0;
	int write_blk_cnt= 0;

	if(nargs < 3){
		printf("%s\n",help);
		exit(1);
	}	

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"f:nt"))!=-1){
		switch(c){
			case 'f':
				pattern=optarg;
				break;
			case 'n':
				stop=1;
				break;
			case '?':
				printf("%s\n",help);
				exit(1);
				break;
			default:
				break;
		}
	}

	ret = read_extcsd(fd, ext_csd);
	if(!(ext_csd[493] & 0x1)){
		printf("FFU not supported. see ECSD[493].\n");
		exit(0);
	}else if(ext_csd[169] & 0x1){
		printf("FFU supported, but FW update is disabled permanently. See ECSD[169]\n");
		exit(0);
	}

	ret = write_extcsd_value(fd, EXT_CSD_MODE_CONFIG, EXT_CSD_MODE_CONFIG_FFU);
	if(ret)
		return ret;

	fp=fopen(pattern, "rb");
	if(!fp){
		printf("Fail to open FW bin file!\n");
		exit(1);
	}

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	rewind(fp);

	blocks = (size%512) ? size/512+1 : size/512;

	data_in = (__u8 *)malloc(blocks*512);
	if(!data_in){
		printf("Error: failed at malloc\n");
		goto out;
	}
	memset(data_in, 0, blocks*512);
	ret = fread(data_in, size, 1, fp);

	write_offset = 0;
	while(blocks > 0) {
		if(blocks > 128) 
			write_blk_cnt = 128;
		else
			write_blk_cnt = blocks;
		
		ret = do_write(fd, MMC_WRITE_MULTIPLE_BLOCK, saddr, data_in + write_offset, write_blk_cnt);
		if(ret)
			goto out;
		
		if(!stop){
			ret = send_stop(fd, 0, NULL);
			if(ret)
				goto out;
		}

		write_offset += write_blk_cnt * 512;
		blocks -= write_blk_cnt;
	}

	ret = read_extcsd(fd, ext_csd);
	if(ret)
		goto out;

	switch(ext_csd[26]){
		case EXT_CSD_FFU_STATUS_GENERAL_ERROR :
			printf("FFU general error, status: 0x%2x\n", ext_csd[26]);
			break;
		case EXT_CSD_FFU_STATUS_INSTALL_ERROR :	
			printf("FFU firmware install error, status: 0x%2x\n", ext_csd[26]);
			break;
		case EXT_CSD_FFU_STATUS_DOWNLOAD_ERROR :	
			printf("FFU firmware download error, status: 0x%2x\n", ext_csd[26]);
			break;
		default:
			printf("FFU success, firmware shall be installed automatically during next power cycle.\n");
			break;	
	}

	ret = write_extcsd_value(fd, EXT_CSD_MODE_CONFIG, 0);
	if(ret)
		goto out;

out:
	if(data_in)
		free(data_in);
	fclose(fp);
	close(fd);
	return ret;
}

int do_set_air(int nargs, char **argv)
{
	char *device;
	int fd = 0, ret = 0, c = 0;
	__u8  data_in[512] = {0};
	__u32 arg = 0, readop_num = 0, chunk = 0;
	__u32 response = 0;

	char *help="Usage: mmc air -c[readop_num] -n[size] </path/to/mmcblkX>\n\n\t -c : number of read CMD after which FW should start check for refresh.\n\t -n : quantity of LBAs to be checked each time.\n";

	if(nargs < 4){
		printf("%s\n",help);
		exit(1);
	}	

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"c:n:h"))!=-1){
		switch(c){
			case 'c':
				readop_num = valid_digit(optarg);
				break;
			case 'n':
				chunk = valid_digit(optarg);
				break;
			case 'h':
			case '?':
				printf("%s\n",help);
				exit(1);
				break;
			default:
				break;
		}
	}

	data_in[0] = 0x1; /* read scan enable */
	data_in[1] = (readop_num >> 24) & 0xFF;
	data_in[2] = (readop_num >> 16) & 0xFF;
	data_in[3] = (readop_num >> 8) & 0xFF;
	data_in[4] = readop_num & 0xFF;
	data_in[5] = (chunk >> 24) & 0xFF;
	data_in[6] = (chunk >> 16) & 0xFF;
	data_in[7] = (chunk >> 8) & 0xFF;
	data_in[8] = chunk & 0xFF;

        arg = 0         | /* read/write : write mode */
              0x54 << 1;  /* cmd index : Auto Initialized Refresh */
	ret = mmc_write_health_status(fd, arg, data_in);
	if(ret)
		goto out;
	
	while(1){
		ret = send_status(fd, 0, &response);
		if (ret) {
			fprintf(stderr, "Could not read response to SEND_STATUS from %s\n", device);
			goto out;
		}
		if(0x9 == ((response >> 8) & 0x1f)) //back to tran state
			break;	
	}
	
out:
	close(fd);
	return ret;

}

int do_set_uir(int nargs, char **argv)
{
	char *device;
	int fd = 0, ret = 0, c = 0;
	__u8  data_in[512] = {0};
	__u32 arg = 0, startAddr = 0, endAddr = 0, eccThres = 0;
	__u32 response = 0;
	int blind = 0;

	char *help="Usage: mmc uir [-b] -s[start address] -e[end address] -t[ecc threshold] </path/to/mmcblkX>\n\n\t -s : start LBA address \n\t -e : end LBA address \n\t -t : refresh ecc threshold \n\t -b : using blind user initialed refresh (default in standard mode)\n";

	if(nargs < 3){
		printf("%s\n",help);
		exit(1);
	}	

	device = argv[nargs-1];
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while((c=getopt(nargs,argv,"s:e:t:bh"))!=-1){
		switch(c){
			case 's':
				startAddr = valid_digit(optarg);
				break;
			case 'n':
				endAddr = valid_digit(optarg);
				break;
			case 't':
				eccThres = valid_digit(optarg);
				break;
			case 'b':
				blind = 1;
				break;				
			case 'h':
			case '?':
				printf("%s\n",help);
				exit(1);
				break;
			default:
				break;
		}
	}

        arg = 0         | /* read/write : write mode */
              0x52 << 1;  /* cmd index : User Initialized Refresh */
	
	if(0 == blind) {
		data_in[0] = (startAddr >> 24) & 0xFF;
		data_in[1] = (startAddr >> 16) & 0xFF;
		data_in[2] = (startAddr >> 8) & 0xFF;
		data_in[3] = startAddr & 0xFF;
		data_in[4] = (endAddr >> 24) & 0xFF;
		data_in[5] = (endAddr >> 16) & 0xFF;
		data_in[6] = (endAddr >> 8) & 0xFF;
		data_in[7] = endAddr & 0xFF;
		data_in[8] = eccThres & 0xFF;
	}

	ret = mmc_write_health_status(fd, arg, data_in);
	if(ret)
		goto out;
	
	while(1){
		ret = send_status(fd, 0, &response);
		if (ret) {
			fprintf(stderr, "Could not read response to SEND_STATUS from %s\n", device);
			goto out;
		}
		if(0x9 == ((response >> 8) & 0x1f)) //back to tran state
			break;	
	}
	
out:
	close(fd);
	return ret;

}

int do_print_state_diagram(int nargs, char **argv)
{
	printf("\n--------------------------------------------------------------------eMMC state diagram-------------------------------------------------------------\n\n");
	printf("		                    v                      ^                           ^\n");
	printf("			            v                      ^                           ^\n");
	printf("	                            v                      ^                           ^\n");
	printf("			            v                      ^              |==========================|\n");
	printf("			      |===========|          |===========|        | CMD0 with arg=0x00000000 |\n");
	printf("		              |    CMD3   |          |   CMD15   |        |    or arg=0xF0F0F0       |                     ^\n");
	printf("			      |===========|          |===========|        |==========================|                     ^\n");
	printf("				    v                      ^                           ^                                   ^\n");
	printf("				    v                      ^                           ^                                   ^\n");
	printf("	  	                    v                      ^                           ^                              RESET signal\n");
	printf("			            v                      ^                           ^                                   ^\n");
	printf("Card identification mode            v                      ^                           ^                                   ^\n");
	printf("====================================v======================^===========================^===================================^===============\n");
	printf("		 |                  v                      ^                           ^                                   ^\n");
	printf("Interrupt mode   |                  v             from all states except       from all states in              from all states except ina\n");
	printf("		 |                  v             Sleep state(slp) in          data-transfer mode\n");
	printf("		 |                  v             data-transfer mode\n");
	printf("		 |                  v\n");
	printf("		 |                  v                  |===============|            [********STATE********]\n");
	printf("		 |                  v                  | CMD13 & CMD55 |            [     Sending-data    ]\n");
	printf("		 |                  v  [**STATE**]     |===============|            [        (data)       ]\n");
	printf("[*****STATE****] |   |==========|   v  [  Sleep  ]     no state transition          [*********************]\n");
	printf("[   Wait-IRQ   ]< < <|  CMD40   |   v  [  (slp)  ]     in data-stransfer mode        v         v         ^\n");
	printf("[     (irq)    ] |   |==========|   v  [*********]                                   v         v         ^\n");
	printf("[**************] |        ^         v    ^     v                                     v         v         ^\n");
	printf("        v        |        ^         v    ^     v                                     v   |==========| |==============|\n");
	printf("	v        |        ^         v  |=========|                                   v   |   CMD12  | |CMD11,17,18,21|\n");
	printf("	v        |        ^         v  |   CMD5  |                                   v   |  or done | |,30,53,56(r)  |\n");
	printf("	v        |        ^         v  |=========|        |=========|< < < < < < < < <   |==========| |==============|\n");
	printf("        v        |        ^         v    v     v     v < <|   CMD7  |< < <                     v         ^\n");
	printf("	v        |        ^         v    v     v     v    |=========|    ^                     v         ^            |========|\n");
	printf("|==============| |        ^< < < <[*****STATE*****]  v                   ^                     v         ^            |CMD16,23|\n");
	printf("|Any start bit | > > > > > > > > >[    Stand-by   ]< <    |=========|    ^ < < < < <[********STATE********]> > > > > >|.35.36  |\n");
	printf("| detected on  | |       > > > > >[    (stby)     ]> > > >|   CMD7  |> > > > > > > >[       Transfer      ]< < < < < <|========|\n");
	printf("|   the bus    | |       ^     > >[***************]> >    |=========|    v < < < < <[        (tran)       ]> > > > > > > > >\n");
	printf("|==============| |       ^    ^                      v                   v     > > >[*********************]< <             v\n");
	printf("	         |       ^    ^                      v   < < < < < < < < <     ^               v               ^           v\n");
	printf("	         |       ^    ^    |============|    v  v                      ^               v               ^           v\n");
	printf("	         |       ^    ^ < <|CMD4,9,10,39|< < <  v       |===========|  ^      |===================|    ^      |=========|\n");
	printf("	         |       ^         |============|       v       | Operation |> >      |  CMD20,24,25,26   |    ^      |  CMD19  |\n");
	printf("	         |       ^                          |=======|   | complete  |         |  27,42,54,56(w)   |    ^      |=========|\n");
	printf("	          |============|                    |CMD6,28|   |===========|         |===================|    ^           v\n");
	printf("	          |  Operation |                    |,29,38 |> >      ^                         v              ^           v\n");
	printf("	          |  Complete  |                    |=======|  v      ^                         v              ^    [****STATE****]\n");
	printf("	          |============|                               v      ^ |========|    [*******STATE*******]    ^    [   Bus test  ]\n");
	printf("                         ^                   |=========|       v      ^ |CMD24,25|> > [    Receive-data   ]    ^    [    (btst)   ]\n");
	printf("                         ^              v < <|   CMD7  |< < <  v      ^ |========|    [       (rcv)       ]    ^    [*************]\n");
	printf("                         ^              v    |=========|    ^  v      ^      ^        [*******************]    ^           v\n");
	printf("               [*******STATE*******]    v                   ^  > >[*******STATE*******]           v            ^           v\n");
	printf("               [     Disconnect    ]< < <                    < < <[     Programming   ]     |============|     ^      |=========|\n");
	printf("               [       (dis)       ]> > >                    > > >[       (prg)       ]< < <|  CMD12 or  |     ^      |  CMD14  |\n");
	printf("               [*******************]    v                   ^     [*******************]     |transfer end|     ^      |=========|\n");
	printf("                                        v    |=========|    ^          ^         v          |============|     ^           v\n");
	printf("                                        > > >|   CMD7  |> > ^          ^         v                             ^           v\n");
	printf("                                             |=========|        |=======================|                      ^ < < < < < <\n");
	printf("                                                                |High Priority Interrupt|\n");
	printf("						                |     (CMD12/CMD13)     |\n");
	printf("                                                                |=======================|\n");
	printf("\n\n--------------------------------------------------------------------eMMC state diagram-------------------------------------------------------------\n");
	return 0;
}
