/***************************************************************************
  pardiff.h  -  Copyright (C) 2001 by Andy Wiggin

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

#ifndef _PARDIFF_H_
#define _PARDIFF_H_

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__MSDOS__) && !defined(_WIN32)
/* include files to determine the width of the output terminal */
#include <sys/ioctl.h>
#include <fcntl.h>
#if defined __NetBSD__ || defined __FreeBSD__ || defined __OpenBSD__
#include <sys/ttycom.h>
#else
#include <termio.h>
#endif
#else
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
/* this macro is only defined if compiling under MS-DOS */
#define  PARDIFF_IS_DOS
#endif

/* buffer to hold one line (similar to max size for vi) */
#define PARDIFF_LINE_BUF_SIZE 2048

/* output width size if ioctl fails */
#define PARDIFF_DFLT_TERM_WID 80

/*
 * Context diff filter
 */
extern int pardiff_context_main(const char *prog, FILE *fp);

extern int get_term_width(void);

#endif // !def _PARDIFF_H_
