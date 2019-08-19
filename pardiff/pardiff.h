
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef __MSDOS__

/* include files to determine the width of the output terminal */
#include <fcntl.h>
#if defined __NetBSD__ || defined __FreeBSD__ || defined __OpenBSD__
#include <sys/ttycom.h>
#else
#include <termio.h>
#endif

#else
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
int pardiff_context_main(int, char *[]);

/*
 * Standard usage routine
 */
int pardiff_usage(void);
 

#endif

