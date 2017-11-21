#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmc_cmds.h"

#define MMC_VERSION	"M.8.26"

#define BASIC_HELP 0
#define ADVANCED_HELP 1
#define HELP_ALL 2

typedef int (*CommandFunction)(int argc, char **argv);

struct Command {
	CommandFunction	func;	/* function which implements the command */
	int	nargs;		/* if == 999, any number of arguments
				   if >= 0, number of arguments,
				   if < 0, _minimum_ number of arguments */
	char	*verb;		/* verb */
	char	*help;		/* help lines; from the 2nd line onward they 
                                   are automatically indented */
        char    *adv_help;      /* advanced help message; from the 2nd line 
                                   onward they are automatically indented */

	/* the following fields are run-time filled by the program */
	char	**cmds;		/* array of subcommands */
	int	ncmds;		/* number of subcommand */
};

static struct Command commands[] = {
	/*
	 *	avoid short commands different for the case only
	 */
	{ do_writeprotect_get, -1,
	  "writeprotect get", DO_WRITEPROTECT_GET_USAGE, NULL
	},
	{ do_writeprotect_set, -1,
	  "writeprotect set", "<device>\n"
		"Set the eMMC write protect status of <device>.\nThis sets the eMMC to be write-protected until next boot.",
	  NULL
	},
	{ do_userarea_writeprotect_get, -1,
	  "ua wp get", "<device>\n"
		"Determine the eMMC user area write protect status of <device>.",
	  NULL
	},
	{ do_disable_512B_emulation, -1,
	  "disable 512B emulation", "<device>\n"
		"Set the eMMC data sector size to 4KB by disabling emulation on\n<device>.",
	  NULL
	},
	{ do_write_boot_en, -3,
	  "bootpart enable", "<boot_partition> " "<send_ack> " "<device>\n"
		"Enable the boot partition for the <device>.\nTo receive acknowledgment of boot from the card set <send_ack>\nto 1, else set it to 0.",
	  NULL
	},
	{ do_hwreset_en, -1,
	  "hwreset enable", "<device>\n"
		"Permanently enable the eMMC H/W Reset feature on <device>.\nNOTE!  This is a one-time programmable (unreversible) change.",
	  NULL
	},
	{ do_hwreset_dis, -1,
	  "hwreset disable", "<device>\n"
		"Permanently disable the eMMC H/W Reset feature on <device>.\nNOTE!  This is a one-time programmable (unreversible) change.",
	  NULL
	},
	{ do_write_bkops_en, -1,
	  "bkops enable", "<device>\n"
		"Enable the eMMC BKOPS feature on <device>.\nNOTE!  This is a one-time programmable (unreversible) change.",
	  NULL
	},
	{ do_enh_area_set, -4,
	  "enh area set", "<-y|-n> " "<start KiB> " "<length KiB> " "<device>\n"
		"Enable the enhanced user area for the <device>.\nDry-run only unless -y is passed.\nNOTE!  This is a one-time programmable (unreversible) change.",
	  NULL
	},
	{ do_read_extcsd, -1,
	  "read_ecsd", "<device>\n"
		"Read extended CSD registers by CMD8.\n",
	  NULL
	},
	{ do_sanitize, -1,
	  "sanitize", "<device>\n"
		"Send Sanitize command to the <device>.\nThis will delete the unmapped memory region of the device.",
	  NULL
	},
	{ power_off_notification, -1,
	  "power off", "<device>\n"
		"Send power off notification signal to the <device>.\nThis will send the device a power off notification to allow the device to better prepare itself for being powered off.\n",
		"Usage: power off  </path/to/mmcblkX> short|long\n",
	  NULL
	},
	{ do_cache_enable, -1,
	  "cache enable", "<device>\n"
		"enable cache on feature to the <device>\n",
		"Usage: cache enable </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_cache_flush, -1,
	  "cache flush", "<device>\n"
		"Flush data in cache to NAND on <device>\n",
		"Usage: cache flush </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_cache_disable, -1,
	  "cache disable", "<device>\n"
		"disable cache feature to the <device>\n",
		"Usage: cache disable <path/to/mmcblkX>\n",
	  NULL
	},
	{ do_go_idle, -1,
	  "cmd0", "<device>\n"
		"Software reset, put eMMC to idle, pre-idle and alternative boot mode.\n",
		"Usage: mmc cmd00 -a[arg: 0x00000000|0xF0F0F0F0|0xFFFFFFFA] </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_send_op_cond, -1,
	  "cmd1", "<device>\n"
		"Asks the Device, in idle state, to send its Operating Conditions Register contents in the response on the CMD line.\n",
		"Usage: mmc cmd1 -a[arg: 0xC0FF8080] </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_all_send_cid, -1,
	  "cmd2", "<device>\n"
		"Asks the Device to send its CID number on the CMD line\n",
		"Usage: mmc cmd02 </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_set_dsr, -1,
	  "cmd4", "<device>\n"
		"Programs the DSR of the Device.\n",
		"Usage: mmc cmd04 -a[DSR] </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_sleep_awake, -1,
	  "cmd5", "<device>\n"
		"Toggles the Device between Sleep state and Standby state.\n",
		"Usage: mmc cmd05 -r[RCA] -m[mode] </path/to/mmcblkX>\n\n -m: 1 for sleep, 0 for awake.\n",
	  NULL
	},
	{ do_config_extcsd, -1,
	  "cmd6", "<device>\n"
		"Config Extended CSD(ECSD) register of eMMC.\n",
		"Usage: mmc cmd06 -m[mode] -a[index] -d[value] </path/to/mmcblkX>\n\n\t -m : Mode select. 1 for set bits, 2 for clear bits, 3 for write byte (recommanded).\n\t -a : Extended CSD index.\n\t -d : Value to be set.\n",
	  NULL
	},
	{ do_select_deselect_card, -1,
	  "cmd7", "<device>\n"
		"This toggles a device between the standby and transfer states or between the programming and disconnect states. \nIn both cases the eMMC is selected by its own relative address and gets deselected by any other address, address 0 deselects the eMMC.\n",
		"Usage: mmc cmd07 -r[RCA] </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_decode_extcsd, -1,
	  "cmd8", "<device>\n"
		"Read ECSD register from eMMC.\n",
	  NULL
	},
	{ do_send_csd, -1,
	  "cmd9", "<device>\n"
		"Addressed Device sends its Device-specific data(CSD) on the CMD line.\n",
		"Usage: mmc cmd09 -r[RCA] </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_send_cid, -1,
	  "cmd10", "<device>\n"
		"Addressed Device sends its Device identification(CID) on CMD the line.\n",
		"Usage: mmc cmd10 -r[RCA] </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_send_stop, -1,
	  "cmd12", "<device>\n"
		"Stop command.\n",
		"Usage: mmc cmd12 -h </path/to/mmcblkX>\n\n\t -h : Use HPI.\n",
	  NULL
	},
	{ do_status_get, -1,
	  "cmd13", "<device>\n"
		"Get status from eMMC.\n",
		"Usage: mmc cmd13 </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_single_read, -1,
	  "cmd17", "<device>\n"
		"Read out only one sector of data from eMMC.\n",
		"Usage: mmc cmd17 -s[start] [-t] [-q] </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_multiple_read, -1,
	  "cmd18", "<device>\n"
		"Read out multiple sectors of data from eMMC.\n",
		"Usage: mmc cmd18 -s[start] -e[end] -p[output to target] [-n] [-t] [-q] </path/to/mmcblkX>\n\n\t -n : No auto cmd12. If controller can handle atuo stop, this option is useless.\n\t -t : Print time spent.\n\t -q : Quiet, no print.\n",
	  NULL
	},
	{ do_single_write, -1,
	  "cmd24", "<device>\n"
		"Write one sector of data to eMMC.\n",
		"Usage: mmc cmd24 -s[start] -d[data] [-t] </path/to/mmcblkX>\n\n\t -t : Print time spent.\n",
	  NULL
	},
	{ do_multiple_write, -1,
	  "cmd25", "<device>\n"
		"Write multiple sectors of data to eMMC.\n",
		"Usage: mmc cmd25 -s[start] -e[end] -d[data] -p[pattern data] [-n] [-t] </path/to/mmcblkX>\n\n\t -d : Data to write.\n\t -p : Path to pattern data file.\n\t -n : No auto cmd12. Use this option if controller can handle auto stop.\n\t -t : Print time spent.\n\nExample : mmc cmd25 -s0 -e2 -d0x55 -t /dev/mmcblk0",
	  NULL
	},
	{ do_get_bad_block_general_info, -1,
	  "bad block", "<device>\n"
		"Read the general bad block information. One of the health command functions (cmd56).\n",
		"Usage: mmc bad block </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_get_general_ec_info, -1,
	  "general pe cycle", "<device>\n"
		"Read general erase count. One of the health command functions (cmd56).\n",
		"Usage: mmc general pe cycle </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_get_detail_ec, -1,
	  "detail pe cycle", "<device>\n"
		"Read the detail erase count of each block. One of the health command functions (cmd56).\n",
		"Usage: mmc detail pe cycle </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_get_SSR, -1,
	  "ssr", "<device>\n"
		"Read Secure Smart Report. One of the health command functions (cmd56).\n",
		"Usage: mmc ssr </path/to/mmcblkX>\n",
	  NULL
	},
	{ do_ffu, -1,
	  "ffu", "<device>\n"
		"Field firmware update, download the FW code into eMMC.\n", 
		"Usage: mmc ffu -f[file] [-n] </path/to/mmcblkX>\n\n\t -f : Path to firmare binary file.\n\t -n : No auto cmd12. Use this option if controller can handle auto stop.\n",
	  NULL
	},
	{ do_set_air, -1,
	  "air", "<device>\n Set auto initialed refresh.\n",
	  	 "Usage: mmc air -c[readop_num] -n[size] </path/to/mmcblkX>\n\n\t -c : number of read CMD after which FW should start check for refresh.\n\t -n : quantity of LBAs to b     e checked each time.\n", NULL
	},
	{ do_set_uir, -1,
	  "uir", "<device>\n Set auto initialed refresh.\n",
		 "Usage: mmc uir [-b] -s[start address] -e[end address] -t[ecc threshold] </path/to/mmcblkX>\n\n\t -s : start LBA address \n\t -e : end LBA address \n\t -t : refresh e     cc threshold \n\t -b : using blind user initialed refresh (default in standard mode)\n",
	  NULL
	},
	{ do_print_state_diagram, -1,
	  "print","state diagram\n"
		"Show the internal state machine diagram of eMMC.\n",
	  NULL
	},
	{ 0, 0, 0, 0 }
};

static char *get_prgname(char *programname)
{
	char	*np;
	np = strrchr(programname,'/');
	if(!np)
		np = programname;
	else
		np++;

	return np;
}

static void print_help(char *programname, struct Command *cmd, int helptype)
{
	char	*pc;

	if (helptype == ADVANCED_HELP && cmd->adv_help){
		printf("\t%s %s ", programname, cmd->verb );
		for(pc = cmd->adv_help; *pc; pc++){
			putchar(*pc);
			if(*pc == '\n')
				printf("\t\t");
		}
	}
	else if(helptype == HELP_ALL){
		printf("\n%s %s ", programname, cmd->verb );
		for(pc = cmd->help; *pc; pc++){
			putchar(*pc);
			if(*pc == '\n')
				printf("\t");
		}
		if(cmd->adv_help){
			printf("\n\n");
			for(pc = cmd->adv_help; *pc; pc++){
				putchar(*pc);
			}
		}
	}else{
		printf("\t%s %s ", programname, cmd->verb );
		for(pc = cmd->help; *pc; pc++){
			putchar(*pc);
			if(*pc == '\n')
				printf("\t\t");
		}
	}
	putchar('\n');
}

static void help(char *np)
{
	struct Command *cp;

	printf("Usage:\n");
	for( cp = commands; cp->verb; cp++ )
		print_help(np, cp, BASIC_HELP);

	printf("\n\t%s help|--help|-h\n\t\tShow the help.\n",np);
	printf("\n\t%s <cmd> --help\n\t\tShow detailed help for a command or subset of commands.\n",np);
	printf("\n%s\n", MMC_VERSION);
}

static int split_command(char *cmd, char ***commands)
{
	int	c, l;
	char	*p, *s;

	for( *commands = 0, l = c = 0, p = s = cmd ; ; p++, l++ ){
		if ( *p && *p != ' ' )
			continue;

		/* c + 2 so that we have room for the null */
		(*commands) = realloc( (*commands), sizeof(char *)*(c + 2));
		(*commands)[c] = strndup(s, l);
		c++;
		l = 0;
		s = p+1;
		if( !*p ) break;
	}

	(*commands)[c] = 0;
	return c;
}

/*
	This function checks if the passed command is ambiguous
*/
static int check_ambiguity(struct Command *cmd, char **argv){
	int		i;
	struct Command	*cp;
	/* check for ambiguity */
	for( i = 0 ; i < cmd->ncmds ; i++ ){
		int match;
		for( match = 0, cp = commands; cp->verb; cp++ ){
			int	j, skip;
			char	*s1, *s2;

			if( cp->ncmds < i )
				continue;

			for( skip = 0, j = 0 ; j < i ; j++ )
				if( strcmp(cmd->cmds[j], cp->cmds[j])){
					skip=1;
					break;
				}
			if(skip)
				continue;

			if( !strcmp(cmd->cmds[i], cp->cmds[i]))
				continue;
			for(s2 = cp->cmds[i], s1 = argv[i+1];
				*s1 == *s2 && *s1; s1++, s2++ ) ;
			if( !*s1 )
				match++;
		}
		if(match){
			int j;
			fprintf(stderr, "ERROR: in command '");
			for( j = 0 ; j <= i ; j++ )
				fprintf(stderr, "%s%s",j?" ":"", argv[j+1]);
			fprintf(stderr, "', '%s' is ambiguous\n",argv[j]);
			return -2;
		}
	}
	return 0;
}

/*
 * This function, compacts the program name and the command in the first
 * element of the '*av' array
 */
static int prepare_args(int *ac, char ***av, char *prgname, struct Command *cmd ){

	char	**ret;
	int	i;
	char	*newname;

	ret = (char **)malloc(sizeof(char*)*(*ac+1));
	newname = (char*)malloc(strlen(prgname)+strlen(cmd->verb)+2);
	if( !ret || !newname ){
		free(ret);
		free(newname);
		return -1;
	}

	ret[0] = newname;
	for(i=0; i < *ac ; i++ )
		ret[i+1] = (*av)[i];

	strcpy(newname, prgname);
	strcat(newname, " ");
	strcat(newname, cmd->verb);

	(*ac)++;
	*av = ret;

	return 0;

}

/*
	This function performs the following jobs:
	- show the help if '--help' or 'help' or '-h' are passed
	- verify that a command is not ambiguous, otherwise show which
	  part of the command is ambiguous
	- if after a (even partial) command there is '--help' show detailed help
	  for all the matching commands
	- if the command doesn't match show an error
	- finally, if a command matches, they return which command matched and
	  the arguments

	The function return 0 in case of help is requested; <0 in case
	of uncorrect command; >0 in case of matching commands
	argc, argv are the arg-counter and arg-vector (input)
	*nargs_ is the number of the arguments after the command (output)
	**cmd_  is the invoked command (output)
	***args_ are the arguments after the command

*/
static int parse_args(int argc, char **argv,
		      CommandFunction *func_,
		      int *nargs_, char **cmd_, char ***args_ )
{
	struct Command	*cp;
	struct Command	*matchcmd=0;
	char		*prgname = get_prgname(argv[0]);
	int		i=0, helprequested=0;

	if( argc < 2 || !strcmp(argv[1], "help") ||
		!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")){
		help(prgname);
		return 0;
	}

	for( cp = commands; cp->verb; cp++ )
		if( !cp->ncmds)
			cp->ncmds = split_command(cp->verb, &(cp->cmds));

	for( cp = commands; cp->verb; cp++ ){
		int     match;

		if( argc-1 < cp->ncmds )
			continue;
		for( match = 1, i = 0 ; i < cp->ncmds ; i++ ){
			char	*s1, *s2;
			s1 = cp->cmds[i];
			s2 = argv[i+1];

			for(s2 = cp->cmds[i], s1 = argv[i+1];
				*s1 == *s2 && *s1;
				s1++, s2++ ) ;
			if( *s1 ){
				match=0;
				break;
			}
		}

		if(argc>i+1 && !strcmp(argv[i+1],"--help")){
			if(!helprequested)
			print_help(prgname, cp, HELP_ALL);
			helprequested=1;
			continue;
		}

		if(!match)
			continue;

		matchcmd = cp;
		*nargs_  = argc-matchcmd->ncmds-1;
		*cmd_ = matchcmd->verb;
		*args_ = argv+matchcmd->ncmds+1;
		*func_ = cp->func;

		break;
	}

	if(helprequested){
		printf("\n%s\n", MMC_VERSION);
		return 0;
	}

	if(!matchcmd){
		fprintf( stderr, "ERROR: unknown command '%s'\n",argv[1]);
		help(prgname);
		return -1;
	}

//	if(check_ambiguity(matchcmd, argv))
		//return -2;

	/* check the number of argument */
	if (matchcmd->nargs < 0 && matchcmd->nargs < -*nargs_ ){
		fprintf(stderr, "ERROR: '%s' requires minimum %d arg(s)\n",
			matchcmd->verb, -matchcmd->nargs);
			return -2;
	}
	if(matchcmd->nargs >= 0 && matchcmd->nargs != *nargs_ && matchcmd->nargs != 999){
		fprintf(stderr, "ERROR: '%s' requires %d arg(s)\n",
			matchcmd->verb, matchcmd->nargs);
			return -2;
	}
	
        if (prepare_args( nargs_, args_, prgname, matchcmd )){
                fprintf(stderr, "ERROR: not enough memory\\n");
		return -20;
        }


	return 1;
}
int main(int ac, char **av )
{
	char		*cmd=0, **args=0;
	int		nargs=0, r;
	CommandFunction func=0;

	r = parse_args(ac, av, &func, &nargs, &cmd, &args);
	if( r <= 0 ){
		/* error or no command to parse*/
		exit(-r);
	}

	exit(func(nargs, args));
}

