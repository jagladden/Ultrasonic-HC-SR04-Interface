#include <avr/io.h>
#include <avr/interrupt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>


/* Size of buffer for commands from server */
#define	CMDBUFSIZE	20
#define VERSION "1.0"

typedef struct command {
	char arg1[CMDBUFSIZE];
	char arg2[CMDBUFSIZE];
	int (*cmdptr)(char *, char *);
} CMD_DESRCIPTION;

typedef struct cmdlist {
	const char *cmdname;
	int (*cmdptr)(char *, char *);
} CMD_LIST_ITEM;

static char echo_enabled;

//Prototypes
static int parse_setup_cmd(char *buf, CMD_DESRCIPTION *cmd_descr);
static void serputchar(char c);
static unsigned char sergetchar(void);
static void serputstr(const char *line);
static int sergetline(char *buf, int bsize);
static int parse_setup_cmd(char *buf, CMD_DESRCIPTION *cmd_descr);
static int echo_command(char *param1, char *param2);
static int dist_command(char *param1, char *param2);


volatile static long int dummy;
extern volatile unsigned short avgPulseLength;

void process_commands() {
	int status;
	long int i;
	char cmdbuf[CMDBUFSIZE];
	CMD_DESRCIPTION cmd_descr;

    /* Delay to allow clock to stabilize before attempting to use serial port */
	for (i = 0; i < 10000; ++i) {
            dummy = i * i;
	}
	serputstr("\rUltrasonic Distance Controller - " VERSION "\r\n");

    /* Loop and wait for commands. All the actual hardware control takes place in the
    interrupt routine. */
	while(1) {
		status = sergetline(cmdbuf, sizeof(cmdbuf));
		if (status != 0) {
			if (!echo_enabled) {
				serputstr("ERR6\r");		//Buffer overflow
			} else {
				serputstr("ERR6 command line buffer overflow\r\n");
			}
            continue;
        }

        if (strlen(cmdbuf) == 0) {			//Empty command
            serputstr("OK\r");
			if (echo_enabled) serputstr("\n");
			continue;
        }

        /* Parse the command */
        status = parse_setup_cmd(cmdbuf, &cmd_descr);
        if (status != 0) {
            if (!echo_enabled) {
                serputstr("ERR1\r");		//Bad command verb
			} else {
                serputstr("ERR1 bad command verb\r\n");
			}
			continue;
		}

		/* Dispatch the command */
		status = (*cmd_descr.cmdptr)(cmd_descr.arg1, cmd_descr.arg2);
		if (status == -1) {
            if (!echo_enabled) {
                serputstr("ERR2\r");		//Parameter missing
            } else {
                serputstr("ERR2 parameter missing\r\n");
            }
            continue;

        } else if (status == -2) {
            if (!echo_enabled) {
                serputstr("ERR3\r");		//Bad parameter value
            } else {
                serputstr("ERR3 bad parameter value\r\n");
            }
			continue;
				
		} else if (status == -3) {
			if (!echo_enabled) {
				serputstr("ERR4\r");		//Illegal state
			} else {
				serputstr("ERR4 command not legal in current state\r\n");
			}
			continue;
				
		} else if (status == -4) {
         	if (!echo_enabled) {
	         	serputstr("ERR5\r");		//State memory full
	         	} else {
	         	serputstr("ERR5 State memory full\r\n");
         	}
         	continue;
		
		} else if (status == -5) {
			continue;						//No additional response
			
		} else {
            serputstr("OK\r");			//Success
            if (echo_enabled) serputstr("\n");
            continue;
		}	
	}
}

/* Command dispatch list.  These are the recognized server commands. */
const CMD_LIST_ITEM commands[] = {
	
	{"echo", echo_command},
	{"dist", dist_command}
	
};

/* Function to parse command and parameter. If successful the function
   copies a pointer to the function that implements the parsed command,
   and copies the parameter (if included), into the structure passed by
   reference as argument "cmd_descr".  The function returns "0" to indicate
   success or "-1" to indicate failure. */
 int parse_setup_cmd(char *buf, CMD_DESRCIPTION *cmd_descr)
{
	char cbuf[CMDBUFSIZE];
	char tempstr[CMDBUFSIZE];
	char *tokptr;
	int num_cmds, i;

    strcpy(tempstr, buf);
	/* Clear the argument strings. */
	cmd_descr->arg1[0] = 0;
	cmd_descr->arg2[0] = 0;

    /* The first token in the string is the command */
	tokptr = strtok(buf, " ");			//Space is the only delimiter
	if (tokptr == NULL) {
            return -1;					//Empty command line
	} else {
            strcpy(cbuf, tokptr);
	}

    /* Check for the first argument */
	tokptr = strtok(NULL, " ");
	if (tokptr != NULL) {
            strcpy(cmd_descr->arg1, tokptr);
	} else {
            cmd_descr->arg1[0] = 0;
    }

	/* Check for the second argument */
	tokptr = strtok(NULL, " ");
	if (tokptr != NULL) {
		strcpy(cmd_descr->arg2, tokptr);
	} else {
		cmd_descr->arg2[0] = 0;
	}

    /* Attempt to look up the command */
	num_cmds = sizeof(commands) / sizeof(CMD_LIST_ITEM);
	for (i = 0; i < num_cmds; ++i) {
            if (strcmp(cbuf, commands[i].cmdname) == 0) {
                cmd_descr->cmdptr = commands[i].cmdptr;
				return 0;				//Success
            }
	}

	return -2;					//Command not found
}

static int echo_command(char *param1, char *param2)
{
	int val;

	if (strlen(param1) == 0) return -1;     //Missing parameter
	val = atoi(param1);
	if (val < 0 || val > 1) return -2;		//Bad parameter value
	echo_enabled = val;
	return 0;
}

static int dist_command(char *param1, char *param2)
{
	const double tick = 1.0 / (16e6 / 64.0);
	const double sos = 1125.0 * 12.0;			//Speed of sound in inches per second
	double apl_secs;
	double inches;
	char buf[50];	
	unsigned long apl_ticks;
	
	 cli();
	 apl_ticks = avgPulseLength;
	 sei();
	 
	 apl_secs = apl_ticks * tick;
	 inches = apl_secs * sos / 2.0;		//Path length (to target and back) is double the distance
	 sprintf(buf, "%4.2f", inches);
	 serputstr(buf);
	 serputstr("\r");
	 if (echo_enabled) serputstr("\n");

	 return -5;	//Not an error, but suppresses "OK" from command dispatcher
}


static void serputchar(char c)
{
    while(!( UCSR0A & (1<<UDRE0)) );
    UDR0 = c;
}

static unsigned char sergetchar(void)
{
    while(!(UCSR0A & (1<<RXC0)));
    return UDR0;
}

static void serputstr(const char *line)
{
    int len, i;

    len = strlen(line);
    for (i = 0; i < len; ++i) {
        serputchar(line[i]);
    }
}

static int sergetline(char *buf, int bsize)
{
    int i;
    unsigned char c;

    i = 0;
    while(1) {
        c = sergetchar();
        c = tolower(c);                             //Convert to lower case

        /* Handle backspace and delete the same */
        if (c == 0x08 || c == 0x7f) {
            if (i > 0) {
                if (echo_enabled) serputstr("\b \b");
                --i;
            }
            continue;
        }
        if (c == 0x0A) continue;                    //Ignore line feeds
        if (c == 0x0D) {                            //Carriage return terminates
            if (echo_enabled) serputstr("\r\n");
            break;
        }
        buf[i++] = c;
        if (echo_enabled) serputchar(c);
        if (i >= (bsize - 1)) return -1;            //Buffer overflow
    }
    buf[i] = 0;                                     //Terminate string
    return 0;                                       //Good status
}


