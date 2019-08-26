/***************************************************************************
 * pardiff.c  -  Copyright (C) 1992-2001 by Andy Wiggin                    *
 *                                                                         *
 * Convert standard diff output to a parallel format.                      *
 *                                                                         *
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "pardiff.h"

static int width_opt = -1;

/* buffer to hold one line (similar to max size for vi) */
static char nextline[PARDIFF_LINE_BUF_SIZE] = "";
static char putline[PARDIFF_LINE_BUF_SIZE] = "";

/* other format data */
static int col_wid = 0;
static int term_wid = 0;
static int left_fill = 0;
static int center_fill = 0;
static int right_fill = 0;
static int got_input = 0;
static int expand_tab_option = 1;

/* parser states */
typedef enum parser_states {
    psUnknown,
    psNeedCmd,      /* looking for next a, d or c */
    psEchoingF1,    /* copying d text */
    psEchoingF2,    /* copying a text */
    psSavingF1,     /* saving file 1 c text */
    psChewingSep,   /* get rid of dashes between f1 and f2 c text */
    psEchoingSav    /* outputting parallel text */
} parserStates;

static void
print_loop(int x, char c)
{
    for ( ; x > 0; --x) {
        putchar(c);
    }
}

static void
put_number_pair(int n1, int n2)
{
    /* this routine uses the hard-coded 11-char field for file line numbers */
    if (n1 == n2) {
       printf("---%5d---", n1);
    } else {
       printf("%5d,%-5d", n1, n2);
    }
}

static void
put_number_line(int x1, int x2, int y1, int y2, char c)
{
    got_input = 1;
    print_loop(left_fill, '-');
    put_number_pair(x1, x2);
    print_loop(center_fill/2, '-');
    putchar(c);
    print_loop(center_fill/2, '-');
    put_number_pair(y1, y2);
    print_loop(right_fill, '-');
    printf("\n");
}

static void
put_fill(void)
{
    print_loop(col_wid, ' ');
}

static void
put_sep(void)
{
    printf(" | ");
}

static void
put_line(const char *str)
{
    /* Read one char from the string each time through, mapping into
     * one or more characters in the output line.
     */
    for (int i = 0, i_off = 0, put_i = 0, filling = 0; put_i < col_wid; ++i) {
        if (filling || (str[i] == '\n')) {
            putline[put_i++] = ' ';
            filling = 1;
        } else if ((str[i] == '\t') && expand_tab_option) {
            /* Assume standard tab-stops and calculate how many spaces this
             * one looked like in the input file.
             */
            int tab_size = 8 - ((i + i_off) % 8);
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
static char **sav_array = NULL;
static int sav_arsz = 0;
static int putting = 0;
static int sav_ind = 0;
static int put_ind = 0;
static const int SAV_GROW_INCR = 10;

static void
init_new_sav_lines(void)
{
    for (int i = sav_arsz - SAV_GROW_INCR; i < sav_arsz; ++i) {
        sav_array[i] = (char *)malloc(sizeof(char) * ((size_t)col_wid + 1));
        if (!sav_array[i]) abort();
    }
}

static void
sav_line(const char *str)
{
    /* if first call, init things */
    if (sav_arsz == 0) {
        sav_arsz = SAV_GROW_INCR;
        sav_array = (char **)malloc(sizeof(char *) * sav_arsz);
        if (!sav_array) abort();
        init_new_sav_lines();
        sav_ind = 0;
    }

    if (putting) {
        /* restarting... */
        sav_ind = 0;

        putting = 0;

    } else if (sav_ind == sav_arsz) {
        char** sav_array_new;
        sav_arsz += SAV_GROW_INCR;
        sav_array_new = (char **)realloc(sav_array, sizeof(char *) * sav_arsz);
        if (!sav_array_new) abort();
        sav_array = sav_array_new;
        init_new_sav_lines();
    }

    char *sav_str = sav_array[sav_ind];
    int i = 0;
    for (int filling = 0; i < col_wid; ++i) {
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
put_sav_line(void)
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
put_other_sav(void)
{
    while (put_ind < sav_ind) {
        put_line(sav_array[put_ind++]);
        put_sep();
        put_fill();
        printf("\n");
    }
}

int
get_term_width(void)
{
    if (width_opt > 0) return width_opt;

#ifdef PARDIFF_IS_DOS
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE conout = CreateFile("CONOUT$", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    const int coninfo_ok = (conout && GetConsoleScreenBufferInfo(conout, &csbi))
                            || GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &csbi)
                            || GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    if (conout) CloseHandle(conout);
    if (coninfo_ok)
    {
        const int columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        //const int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        if (columns > 0) return columns;
    }
#endif
#else
    struct winsize wnsz; /* OS struct storing terminal size */
    if (ioctl(open("/dev/tty", O_RDONLY), TIOCGWINSZ, &wnsz) == 0) {
        return wnsz.ws_col;
    }
#endif

    return PARDIFF_DFLT_TERM_WID;
}

/*
 * Main routine
 */
static int
pardiff_main(const char *prog, FILE *fp)
{
    int eff_term_wid = 0;       /* either real term wid or one less */
    int x1 = 0;
    int x2 = 0;
    int y1 = 0;
    int y2 = 0;                 /* parsed numbers from cmd lines */
    char *strhead = NULL;
    char *strnext = NULL;
    char cmdChar = 0;           /* a, d or c */
    parserStates curState = psUnknown;  /* state of parser machine */
#ifdef PARDIFF_IS_DOS
    const int convertCrlf = 0;  /* T => convert lines to UNIX EOL format */
#else
    const int convertCrlf = 1;
#endif

    /* Calculate format numbers */
    term_wid = get_term_width();

    col_wid = (term_wid - 3) / 2;
    eff_term_wid = (col_wid * 2) + 3;

    /* here, leaving 11 characters for line numbers. Matches formatting in
     * the routine that outputs the line number lines.
     */
    left_fill = (col_wid / 2) - 5;
    right_fill = left_fill;
    center_fill = eff_term_wid - left_fill - right_fill - 22;

    curState = psNeedCmd;
    for (;;)
    {
        /*
         * Keep getting the next line till NULL is returned.
         */
        if (fgets(nextline, PARDIFF_LINE_BUF_SIZE, fp) == NULL) break;

        /*
         * Preprocess lines to get a consistent EOL
         */
        if (convertCrlf) {
#if defined __NetBSD__ || defined __FreeBSD__ || defined __OpenBSD__
            size_t line_len = strlen(nextline);
#else
            size_t line_len = strnlen(nextline, PARDIFF_LINE_BUF_SIZE);
#endif
            if (line_len > 2 && nextline[line_len - 2] == '\r') {
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
                        prog, curState);
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

static int
do_pardiff(const char *prog, const char *fn, int context_mode)
{
    FILE *fp = NULL;
    if (!fn || (fn[0] == '-' && fn[1] == '\0')) {
        fp = stdin;
    }
    else {
#ifdef _MSC_VER
        if (fopen_s(&fp, fn, "r") != 0) fp = NULL;
#else
        fp = fopen(fn, "r");
#endif
        if (!fp) {
            perror("fopen");
            return -1;
        }
    }

    const int rc = context_mode
        ? pardiff_context_main(prog, fp)
        : pardiff_main(prog, fp);

    if (fp != stdin) fclose(fp);

    return rc;
}

static int
pardiff_usage(const char *prog)
{
    fprintf(stderr,
            "pardiff " VERSION "\n"
            "usage: %s [-h] [-v] [-C] [-w{width}] [file|-] ...\n"
            "  -h            display this help and exit\n"
            "  -v            output version information and exit\n"
            "  -C            parse context diff format\n"
            "  -w{width}     use specific console width\n",
            prog);
    return 1;
}

/*
 * Main routine
 */
int
main(int argc, char *argv[])
{
    const char *const prog = argv[0];

    int context_mode = 0;

    int argi = 1;
    for (; argi < argc; ++argi)
    {
        const char *const arg = argv[argi];
        if (arg[0] != '-' || arg[1] == '\0') break;
        if (arg[1] == 'w') {
            const char *width_arg = NULL;
            if (arg[2] == '\0') {
                if (++argi >= argc) {
                    return pardiff_usage(prog); /* missing width argument */
                }
                width_arg = argv[argi];
            }
            else {
                width_arg = arg + 2;
            }
            const char *cp = width_arg;
            while (isdigit(*cp)) ++cp;
            int width = (cp > width_arg && *cp == '\0') ? atoi(width_arg) : -1;
            if (width <= 0) {
                return pardiff_usage(prog); /* invalid width argument */
            }
            width_opt = width;
        }
        else if (arg[1] == '-' && arg[2] == '\0') {
            ++argi;
            break;
        }
        else {
            const char* ap = arg;
            while (*++ap) {
                switch (*ap) {
                case 'h':
                default:
                    return pardiff_usage(prog);
                case 'v':
                    printf("pardiff " VERSION "\n");
                    return 0;
                case 'C':
                    context_mode = 1;
                    break;
                }
            }
        }
    }

    if (argi >= argc) {
        return do_pardiff(prog, NULL, context_mode);
    }

    const int m = (argi + 1) < argc;
    int rc = 0;
    int stdin_done = 0;
    for (; argi < argc; ++argi) {
        const char *const fn = argv[argi];
        if (*fn == '\0') continue;
        const int do_stdin = (fn[0] == '-' && fn[1] == '\0');
        if (do_stdin && stdin_done) continue;
        if (m) {
            if (do_stdin) printf("stdin:\n");
            else printf("file: %s\n", fn);
        }
        const int do_rc = do_pardiff(prog, fn, context_mode);
        if (do_stdin) stdin_done = 1;
        if (rc == 0) rc = do_rc;
        if (m && (argi + 1) < argc) putchar('\n');
    }

    return rc;
}
