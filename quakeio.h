/*
	quakeio.h

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/
#ifndef __quakeio_h
#define __quakeio_h

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include <zlib.h>

//#include "QF/gcc_attr.h"

typedef struct {
	FILE *file;
#ifdef HAVE_ZLIB
	gzFile *gzfile;
#endif
} QFile;

void Qexpand_squiggle(const char *path, char *dest);
int Qrename(const char *old, const char *new);
QFile *Qopen(const char *path, const char *mode);
QFile *Qdopen(int fd, const char *mode);
void Qclose(QFile *file);
int Qread(QFile *file, void *buf, int count);
int Qwrite(QFile *file, void *buf, int count);
//int Qprintf(QFile *file, const char *fmt, ...) __attribute__((format(printf,2,3)));
int Qprintf(QFile *file, const char *fmt, ...);
char *Qgets(QFile *file, char *buf, int count);
int Qgetc(QFile *file);
int Qputc(QFile *file, int c);
int Qseek(QFile *file, long offset, int whence);
long Qtell(QFile *file);
int Qflush(QFile *file);
int Qeof(QFile *file);
char *Qgetline(QFile *file);

#endif /*__quakeio_h*/
