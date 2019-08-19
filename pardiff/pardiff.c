/***************************************************************************
  pardiff.c  -  Copyright (C) 1992-2001 by Andy Wiggin

   Convert standard diff output to a parallel format

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/

#include "pardiff.h"


/* buffer to hold one line (similar to max size for vi) */
static char nextline[PARDIFF_LINE_BUF_SIZE];
static char putline[PARDIFF_LINE_BUF_SIZE];


/* other format data */
static int col_wid;
static int term_wid;
static int left_fill, center_fill, right_fill;
static int got_input = 0;
static int expand_tab_option = 1;


/* parser states */
typedef enum parser_states {
    psUnknown,
    psNeedCmd,			/* looking for next a, d or c */
    psEchoingF1,		/* copying d text */
    psEchoingF2,		/* copying a text */
    psSavingF1,			/* saving file 1 c text */
    psChewingSep,		/* get rid of dashes between f1 and f2 c text */
    psEchoingSav		/* outputting parallel text */
} parserStates;


static void
print_loop (
    int  x,
    char c
    )
{
    for ( ; x > 0; --x) {
	printf("%c", c);
    }
}

static void
put_number_pair (
    int n1,
    int n2
    )
{
    /* this routine uses the hard-coded 11-char field for file line numbers */
    if (n1 == n2) {
       printf("---%5d---", n1);
    } else {
       printf("%5d,%-5d", n1, n2);
    }
}

static void
put_number_line (
    int  x1,
    int  x2,
    int  y1,
    int  y2,
    char c
    )
{
    got_input = 1;
    print_loop(left_fill, '-');
    put_number_pair(x1, x2);
    print_loop(center_fill/2, '-');
    printf("%c", c);
    print_loop(center_fill/2, '-');
    put_number_pair(y1, y2);
    print_loop(right_fill, '-');
    printf("\n");
}

static void
put_fill( void )
{
    print_loop(col_wid, ' ');
}

static void
put_sep( void )
{
    printf(" | ");
}

static void
put_line (
    char *str
    )
{
    int i, i_off, put_i, tab_size;
    int filling = 0;

    /*
     * Read one char from the string each time through, mapping into
     * one or more characters in the output line.
     */
    for (i = i_off = 0, put_i = 0; put_i < col_wid; ++i) {
	if (filling || (str[i] == '\n')) {
	    putline[put_i++] = ' ';
	    filling = 1;
	} else if ((str[i] == '\t') && expand_tab_option) {
	    /* Assume standard tab-stops and calculate how many spaces this
	     * one looked like in the input file.
	     */
	    tab_size = 8 - ((i + i_off) % 8);
	    i_off += tab_size - 1;
	    for ( ; tab_size > 0; tab_size--) {
		putline[put_i++] = ' ';
	    }
	} else {
	    putline[put_i++] = str[i];
	}
    }

    putline[col_wid] = '\0';
    printf("%s", putline);
}

/* array to save lines in */
static char **sav_array;
static int sav_arsz = 0;
static int putting = 0;
static int sav_ind, put_ind;
#define SAV_GROW_INCR 10

static void
init_new_sav_lines( void )
{
    int i;

    for (i = sav_arsz - SAV_GROW_INCR; i < sav_arsz; ++i) {
	sav_array[i] = (char *)malloc(sizeof(char)*(col_wid+1));
    }
}

static void
sav_line (
    char *str
    )
{
    int i;
    int filling = 0;
    char *sav_str;

    /* if first call, init things */
    if (sav_arsz == 0) {
	sav_arsz = SAV_GROW_INCR;
	sav_array = (char **)malloc(sizeof(char *)*sav_arsz);
	init_new_sav_lines();
	sav_ind = 0;
    }

    if (putting) {
	/* restarting... */
	sav_ind = 0;

	putting = 0;

    } else if (sav_ind == sav_arsz) {
	sav_arsz += SAV_GROW_INCR;
	sav_array =
		(char **)realloc((char *)sav_array, sizeof(char *)*sav_arsz);
	init_new_sav_lines();
    }

    sav_str = sav_array[sav_ind];
    for (i = 0; i < col_wid; ++i) {
	if (filling || (str[i] == '\n')) {
	    sav_str[i] = ' ';
	    filling = 1;
	} else {
	    sav_str[i] = str[i];
	}
    }
    sav_str[i] = '\0';

    ++sav_ind;
}

static void
put_sav_line( void )
{
    if (!putting) {
	put_ind = 0;
	putting = 1;
    }

    if (put_ind < sav_ind) {
    	put_line(sav_array[put_ind++]);
    } else {
	put_fill();
    }
}

static void
put_other_sav( void )
{
    while (put_ind < sav_ind) {
	put_line(sav_array[put_ind++]);
	put_sep();
	put_fill();
	printf("\n");
    }
}

/*
 * Main routine
 */
static int
pardiff_main (
    int argc,
    char *argv[]
    )
{
    int eff_term_wid;		/* either real term wid or one less */
    int x1, x2, y1, y2;		/* parsed numbers from cmd lines */
    char *strhead, *strnext;
    char cmdChar;		/* a, d or c */
#ifndef PARDIFF_IS_DOS
    struct winsize wnsz;	/* OS struct storing terminal size */
    int status;			/* status of ioctl call */
#endif
    parserStates curState;	/* state of parser machine */
    int convertCrlf;            /* T => convert lines to UNIX EOL format */
    int line_len;               /* store the input line length */

#ifndef PARDIFF_IS_DOS
    convertCrlf = 1;
#else
    convertCrlf = 0;
#endif

#ifndef PARDIFF_IS_DOS
    /* get the term width */
    status = ioctl(open("/dev/tty", O_RDONLY), TIOCGWINSZ, &wnsz);

    /* Calculate format numbers */
    if (status == 0) {
	term_wid = wnsz.ws_col;
    } else {
	term_wid = PARDIFF_DFLT_TERM_WID;
    }
#else
    term_wid = PARDIFF_DFLT_TERM_WID;
#endif

#if DEBUG
    printf("status is  %d, using %d termwid, ws_col=%d\n", status,
    	term_wid, wnsz.ws_col);
#endif

    col_wid = (term_wid - 3) / 2;
    eff_term_wid = (col_wid * 2) + 3;

    /* here, leaving 11 characters for line numbers. Matches formatting in
     * the routine that outputs the line number lines.
     */
    left_fill = (col_wid / 2) - 5;
    right_fill = left_fill;
    center_fill = eff_term_wid - left_fill - right_fill - 22;

    curState = psNeedCmd;
    while (1) {
	/*
	 * Keep getting the next line till NULL is returned.
	 */
	if (fgets(nextline, PARDIFF_LINE_BUF_SIZE, stdin) == (char *)0) break;

        /*
         * Preprocess lines to get a consistent EOL
         */
        if (convertCrlf) {
#if defined __NetBSD__ || defined __FreeBSD__ || defined __OpenBSD__
            line_len = strlen(nextline);
#else
            line_len = strnlen(nextline, PARDIFF_LINE_BUF_SIZE); 
#endif
            if (line_len > 2 && nextline[line_len - 2] == 0xd) {
                nextline[line_len - 2] = '\n';
                nextline[line_len - 1] = '\0';
            }
        }

	/*
	 * Interpret this line based on the current state of things.
	 */
	switch (curState) {
	    case psNeedCmd:
		/*
		 * Doing exact parsing so diff output lines can be extracted
		 * out of streams that contain more than just diff output.
		 */
		strhead = nextline;
		x1 = (int)strtol(strhead, &strnext, 10);
		if (strhead == strnext) break;
		if (strnext[0] == ',') {
		    strhead = strnext + 1;
		    x2 = (int)strtol(strhead, &strnext, 10);
		    if (strhead == strnext) break;
		} else {
		    x2 = x1;
		}

		cmdChar = strnext[0];
		strhead = strnext + 1;
		y1 = (int)strtol(strhead, &strnext, 10);
		if (strhead == strnext) break;
		if (strnext[0] == ',') {
		    strhead = strnext + 1;
		    y2 = (int)strtol(strhead, &strnext, 10);
		    if (strhead == strnext) break;
		} else {
		    y2 = y1;
		}

		/* decide the next state based on the diff command type */
		switch (cmdChar) {
		    case 'a':
			curState = psEchoingF2;
			break;
		    case 'd':
			curState = psEchoingF1;
			break;
		    case 'c':
			curState = psSavingF1;
			break;
		    default:
			break;
		}

		/*
		 * Output the line number header line if we just got a valid
		 * command.
		 */
		put_number_line(x1, x2, y1, y2, cmdChar);

		/* convert x2,y2 into line counts */
		x2 = x2 - x1 + 1;
		y2 = y2 - y1 + 1;
		break;

	    case psEchoingF1:
		put_line(nextline + 2);
		put_sep();
		put_fill();
		printf("\n");
		--x2;
		if (x2 == 0) {
		    curState = psNeedCmd;
		}
		break;

	    case psEchoingF2:
		put_fill();
		put_sep();
		put_line(nextline + 2);
		printf("\n");
		--y2;
		if (y2 == 0) {
		    curState = psNeedCmd;
		}
		break;

  	    case psSavingF1:
		sav_line(nextline + 2);
		--x2;
		if (x2 == 0) {
		    curState = psChewingSep;
		}
		break;

	    case psChewingSep:
		curState = psEchoingSav;
		break;

	    case psEchoingSav:
		put_sav_line();
		put_sep();
		put_line(nextline + 2);
		printf("\n");
		--y2;
		if (y2 == 0) {
		    put_other_sav();
		    curState = psNeedCmd;
		}
		break;

	    case psUnknown:
	    default:
		fprintf(stderr, "%s: internal error, in state %d\n",
			argv[0], curState);
		exit(1);
		break;
	}
    }

    /* done */
    if (got_input) {
	print_loop(eff_term_wid, '-'); printf("\n");
    }

    return 0;
}

int
pardiff_usage(void)
{
    fprintf(stderr, "usage: pardiff [-C]\n");
    fprintf(stderr, "       -C:  parse context diff format\n");
    
    return 1;
}

/*
 * Main routine
 */
int
main (
    int argc,
    char *argv[]
    )
{
    int context_mode;
    int argi;

    /* cheezy arg parsing */
    context_mode = 0;
    if (argc > 2) {
        return pardiff_usage();
    }
    for (argi = 1; argi < argc; argi++) {
        switch (argv[argi][1]) {
        case 'C':
            context_mode = 1;
            break;
        default:
            return pardiff_usage();
        }
    }

    if (context_mode) {
        return pardiff_context_main(argc, argv);
    } else {
        return pardiff_main(argc, argv);
    }
}
