/***************************************************************************
 * parcdiff.c  -  Copyright (C) 2001 Adam Bernstein                        *
 *                                                                         *
 * Convert context diff output to a parallel format.  This program is      *
 * based upon the idea of pardiff.c, written by Andy Wiggin.               *
 *                                                                         *
 * Usage: diff -C4 f1 f2 | parcdiff                                        *
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

#ifndef _MSC_VER
#define _strdup strdup
#endif

#define TAB_STOP 4

typedef struct _pardiff_t {
    char *file1;
    char *file2;
    int  window_width;
    char linenum1[32];
    char linenum2[32];
    int  change1;
    int  change2;
    int  width1;
    int  width2;
} pardiff_t;

typedef struct _list_element_t {
    char *line;
    struct _list_element_t *next;
} list_element_t;

typedef struct _list_t {
    int    len;
    struct _list_element_t *head;
    struct _list_element_t *cur;
    struct _list_element_t *tail;
} list_t;

/*
 * Linked list API
 */
static int  add_list(list_t *list, char *in_line);
static int  get_list_len(list_t *list);
static char *get_list_first(list_t *list);
static char *get_list_next(list_t *list);
static void free_list(list_t *list);

/*
 * Context diff processing functions
 */
static int  get_diff_file_names(FILE *fp, pardiff_t *ctx);
static void process_file(FILE *fp, pardiff_t *ctx);
static void print_lists(list_t *l1, list_t *l2, pardiff_t *ctx);
static void detab_line(char *in_line);

int
pardiff_context_main(int argc, char *argv[])
{
    FILE *fp;
    pardiff_t ctx;

    memset(&ctx, 0, sizeof(ctx));
    if (argc == 2) {
        fp = stdin;
    }
    else {
        fp = fopen(argv[2], "r");
        if (!fp) {
            perror("fopen");
            return 1;
        }
    }

    ctx.window_width = get_term_width();

    if (get_diff_file_names(fp, &ctx) == 1) {
        return 1;
    }
    process_file(fp, &ctx);
    return 0;
}

static int add_list(list_t *list, char *in_line)
{
    list_element_t *element;

    element = (list_element_t *)calloc(1, sizeof(list_element_t));
    if (!element) abort();

    element->line = _strdup(in_line);
    if (!element->line) abort();

    if (list->head == NULL) {
        list->head = element;
        list->tail = list->head;
    }
    else {
        list->tail->next = element;
        list->tail = element;
    }
    list->len++;
    return 0;
}

static int get_list_len(list_t *list)
{
    int len = -1;
    if (list) {
        len = list->len;
    }
    return len;
}

static char *get_list_first(list_t *list)
{
    char *cp = NULL;
    if (list) {
        if (list->head) {
            list->cur = list->head;
            if (list->cur) {
                cp = list->cur->line;
            }
        }
    }
    return cp;
}

static char *get_list_next(list_t *list)
{
    char *cp = NULL;
    if (list) {
        if (list->cur) {
            cp = list->cur->line;
            list->cur = list->cur->next;
        }
    }
    return cp;
}

static void free_list(list_t *list)
{
    list_element_t *cur = list->head;
    while (cur) {
        list_element_t *next = cur->next;
        free(cur->line);
        free(cur);
        cur = next;
    }
    memset(list, 0, sizeof(*list));
}

static void print_lists(list_t *l1, list_t *l2, pardiff_t *ctx)
{
    char *cp1 = NULL;
    char *cp2 = NULL;
    int len;
    int maxlen;
    int width1;
    int width2;
    int length_diff = 0;

    if (get_list_len(l1) <= 0 && get_list_len(l2) <= 0) {
        return;
    }

    /* get line max length of each list */
    get_list_first(l1);
    get_list_first(l2);
    maxlen = -1;
    do {
        cp1 = get_list_next(l1);
        if (cp1) {
            len = (int)strlen(cp1);
            if (maxlen < len) {
                maxlen = len;
            }
        }
    } while (cp1);
    ctx->width1 = maxlen;

    maxlen = -1;
    do {
        cp2 = get_list_next(l2);
        if (cp2) {
            len = (int)strlen(cp2);
            if (maxlen < len) {
                maxlen = len;
            }
        }
    } while (cp2);
    ctx->width2 = maxlen;

    /*
     * Print output adds 3 characters, so must subtract from window width
     */
    if (((ctx->width1 + 3) <= ctx->window_width/2 &&
         (ctx->width2 + 3) <= ctx->window_width/2) ||
         ((ctx->width1 + ctx->width2 + 4) > ctx->window_width))
    {
        width1 = (ctx->window_width - 3)/2;
        width2 = (ctx->window_width - 3)-width1;
    }
    else if (ctx->width1 > ctx->width2) {
        width1 = ctx->width1;
        width2 = ctx->window_width - width1 - 4;
    }
    else {
        width1 = ctx->window_width - ctx->width2 - 4;
        width2 = ctx->width2;
    }

    printf("+%s", ctx->linenum1);
    for (size_t i = strlen(ctx->linenum1); i < (size_t)width1; i++) {
        putchar('-');
    }
    printf("+%s", ctx->linenum2);
    for (size_t i = strlen(ctx->linenum2); i < (size_t)width2; i++) {
        putchar('-');
    }
    printf("+\n");

    /*
     * When the number of lines before the first change line (line
     * starting with !) differ, must emit blank additional blank
     * lines to make these lines align.
     */
    if (ctx->change1 != -1 && ctx->change2 != -1) {
        length_diff = ctx->change1 - ctx->change2;
    }

    get_list_first(l1);
    get_list_first(l2);
    do {
        if (length_diff) {
            if (length_diff < 0) {
                cp1 = NULL;
                cp2 = get_list_next(l2);
                length_diff++;
            }
            else {
                cp1 = get_list_next(l1);
                cp2 = NULL;
                length_diff--;
            }
        }
        else {
            cp1 = get_list_next(l1);
            cp2 = get_list_next(l2);
        }

        len = 0;
        putchar('|');
        if (cp1) {
            printf("%.*s", width1, cp1);
            len = (int)strlen(cp1);
        }
        if (len < width1) {
            printf("%*c", width1-len, ' ');
        }
        putchar('|');

        len = 0;
        if (cp2) {
            printf("%.*s", width2, cp2);
            len = (int)strlen(cp2);
        }
        if (len < width2) {
            printf("%*c", width2-len, ' ');
        }
        printf("|\n");
    } while (cp1 || cp2);

    putchar('+');
    for (int i = 0; i < width1; i++) {
        putchar('-');
    }
    putchar('+');
    for (int i = 0; i < width2; i++) {
        putchar('-');
    }
    putchar('+');
    printf("\n\n");
}

static void process_file(FILE *fp, pardiff_t *ctx)
{
    char line[PARDIFF_LINE_BUF_SIZE];
    char *cp;
    char *ep;
    int state = 0;
    list_t list1;
    list_t list2;
    int linenum;

    ctx->linenum1[0] = ctx->linenum2[0] = '\0';
    memset(&list1, 0, sizeof(list1));
    memset(&list2, 0, sizeof(list2));
    ctx->change1 = ctx->change2 = -1;
    linenum = 0;
    do {
        if (!fgets(line, sizeof(line)-1, fp)) {
            continue;
        }
        linenum++;
        line[strlen(line)-1] = '\0';
        detab_line(line);
        cp = strstr(line, "**********");
        if (cp == line) {
            state = 1;
        }

        if (state == 1) {
            cp = strstr(line, "*** ");
            if (cp == line) {
                state = 2;
                linenum = 0;

                print_lists(&list1, &list2, ctx);
                ctx->linenum1[0] = ctx->linenum2[0] = '\0';
                ctx->change1 = ctx->change2 = -1;

                cp += 4;
                for (ep = cp; *ep && *ep != ' '; ep++)
                    ;
                strncat(ctx->linenum1, cp, ep-cp);

                free_list(&list1);
                free_list(&list2);
            }
        }
        else if (state == 2) {
            cp = strstr(line, "--- ");
            if (cp == line) {
                state = 3;
                linenum = 0;

                cp += 4;
                for (ep = cp; *ep && *ep != ' '; ep++)
                    ;
                strncat(ctx->linenum2, cp, ep-cp);
            }
            else {
                if (line[0] == '!') {
                    if (ctx->change1 == -1) {
                        ctx->change1 = linenum;
                    }

                }
                add_list(&list1, line);
            }
        }
        else if (state == 3) {
                if (line[0] == '!') {
                    if (ctx->change2 == -1) {
                        ctx->change2 = linenum;
                    }
                }
                add_list(&list2, line);
        }
    } while (!feof(fp));
    print_lists(&list1, &list2, ctx);
}

int get_diff_file_names(FILE *fp, pardiff_t *ctx)
{
    char line[PARDIFF_LINE_BUF_SIZE];
    char *cp;
    char *ep;

    if (fgets(line, sizeof(line)-1, fp)) {
        cp = strstr(line, "*** ");
        if (cp != line) {
            fprintf(stderr, "pardiff: line 1 bad format\n");
            return 1;
        }
        cp += 4;

        /* 26 = 1 for tab; 24 for date field; 1 for \n from fgets() */
        ep = line + strlen(line) - 26;
    } else {
        /* empty input stream */
        return 1;
    }
    ctx->file1 = (char *)malloc(ep - cp + 1);
    if (!ctx->file1) abort();
    strncpy(ctx->file1, cp, ep-cp);
    ctx->file1[ep-cp] = '\0';

    if (fgets(line, sizeof(line)-1, fp)) {
        cp = strstr(line, "--- ");
        if (cp != line) {
            fprintf(stderr, "pardiff: line 2 bad format\n");
            return 1;
        }
        cp += 4;

        /* 26 = 1 for tab; 24 for date field; 1 for \n from fgets() */

        ep = line + strlen(line) - 26;
    }
    ctx->file2 = (char *)malloc(ep - cp + 1);
    if (!ctx->file2) abort();
    strncpy(ctx->file2, cp, ep-cp);
    ctx->file1[ep-cp] = '\0';
    return 0;
}

static void detab_line(char *in_line)
{
    char line_buf[PARDIFF_LINE_BUF_SIZE];

    int  j = 0;
    for (int i = 0, c_count = 0; in_line[i]; i++) {
        /*
         * Count characters on input line
         */
        c_count++;
        if (in_line[i] == '\t') {
            /*
             * Loop puts spaces up to tabstop position before nth tabstop.
             * Trailing printf puts space in nth tabstop position.  c_count
             * is already right for last space put.
             */
            for(; c_count % TAB_STOP != 0;  j++, c_count++) {
                line_buf[j] = ' ';
            }
            line_buf[j++] = ' ';
        }
        else {
            line_buf[j++] = in_line[i];
        }
    }
    line_buf[j++] = '\0';
    strcpy(in_line, line_buf);
}
