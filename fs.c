/*
	Quake file system

	Copyright (C) 2003 Mathieu Olivier
	Copyright (C) 1999,2000  contributors of the QuakeForge project

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
*/

#include "quakedef.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>

#ifdef WIN32
# include <direct.h>
# include <io.h>
#else
# include <pwd.h>
# include <sys/stat.h>
# include <unistd.h>
#endif

#ifndef PATH_MAX
# define PATH_MAX 512
#endif

#include "fs.h"

/*

All of Quake's data access is through a hierchal file system, but the contents
of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and
all game directories.  The sys_* files pass this to host_init in
quakeparms_t->basedir.  This can be overridden with the "-basedir" command
line parm to allow code debugging in a different directory.  The base
directory is only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that
all generated files (savegames, screenshots, demos, config files) will be
saved to.  This can be overridden with the "-game" command line parameter.
The game directory can never be changed while quake is executing.  This is a
precacution against having a malicious server instruct clients to write files
over areas they shouldn't.

*/


/*
=============================================================================

TYPES

=============================================================================
*/

// Our own file structure on top of FILE
typedef enum
{
	FS_FLAG_NONE		= 0,
	FS_FLAG_PACKED		= (1 << 0)	// inside a package (PAK or PK3)
//	FS_FLAG_COMPRESSED	= (1 << 1)  // compressed (inside a PK3 file)
} fs_flags_t;

struct qfile_s
{
	fs_flags_t	flags;
	FILE*		stream;
	size_t		length;		// file size (PACKED only)
	size_t		offset;		// offset into a package (PACKED only)
	size_t		position;	// current position in the file (PACKED only)
};


// PAK files on disk
typedef struct
{
	char name[56];
	int filepos, filelen;
} dpackfile_t;

typedef struct
{
	char id[4];
	int dirofs;
	int dirlen;
} dpackheader_t;


// Packages in memory
typedef struct
{
	char name[MAX_QPATH];
	int filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char filename[MAX_OSPATH];
	FILE *handle;
	int numfiles;
	packfile_t *files;
	mempool_t *mempool;
	struct pack_s *next;
} pack_t;


// Search paths for files (including packages)
typedef struct searchpath_s
{
	// only one of filename / pack will be used
	char filename[MAX_OSPATH];
	pack_t *pack;
	struct searchpath_s *next;
} searchpath_t;


/*
=============================================================================

VARIABLES

=============================================================================
*/

mempool_t *fs_mempool;
mempool_t *pak_mempool;

int fs_filesize;

pack_t *packlist = NULL;

searchpath_t *fs_searchpaths;

// LordHavoc: was 2048, increased to 65536 and changed info[MAX_PACK_FILES] to a temporary alloc
#define MAX_FILES_IN_PACK       65536

char fs_gamedir[MAX_OSPATH];
char fs_basedir[MAX_OSPATH];

qboolean fs_modified;   // set true if using non-id files


/*
=============================================================================

PRIVATE FUNCTIONS

=============================================================================
*/


/*
============
FS_CreatePath

LordHavoc: Previously only used for CopyFile, now also used for FS_WriteFile.
============
*/
void FS_CreatePath (char *path)
{
	char *ofs, save;

	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/' || *ofs == '\\')
		{
			// create the directory
			save = *ofs;
			*ofs = 0;
			FS_mkdir (path);
			*ofs = save;
		}
	}
}


/*
============
FS_Path_f

============
*/
void FS_Path_f (void)
{
	searchpath_t *s;

	Con_Printf ("Current search path:\n");
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s->pack)
		{
			Con_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		}
		else
			Con_Printf ("%s\n", s->filename);
	}
}


/*
=================
FS_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *FS_LoadPackFile (const char *packfile)
{
	dpackheader_t header;
	int i, numpackfiles;
	FILE *packhandle;
	pack_t *pack;
	// LordHavoc: changed from stack array to temporary alloc, allowing huge pack directories
	dpackfile_t *info;

	packhandle = fopen (packfile, "rb");
	if (!packhandle)
		return NULL;

	fread ((void *)&header, 1, sizeof(header), packhandle);
	if (memcmp(header.id, "PACK", 4))
		Sys_Error ("%s is not a packfile", packfile);
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	if (header.dirlen % sizeof(dpackfile_t))
		Sys_Error ("%s has an invalid directory size", packfile);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error ("%s has %i files", packfile, numpackfiles);

	pack = Mem_Alloc(pak_mempool, sizeof (pack_t));
	strcpy (pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->mempool = Mem_AllocPool(packfile);
	pack->files = Mem_Alloc(pack->mempool, numpackfiles * sizeof(packfile_t));
	pack->next = packlist;
	packlist = pack;

	info = Mem_Alloc(tempmempool, sizeof(*info) * numpackfiles);
	fseek (packhandle, header.dirofs, SEEK_SET);
	fread ((void *)info, 1, header.dirlen, packhandle);

// parse the directory
	for (i = 0;i < numpackfiles;i++)
	{
		strcpy (pack->files[i].name, info[i].name);
		pack->files[i].filepos = LittleLong(info[i].filepos);
		pack->files[i].filelen = LittleLong(info[i].filelen);
	}

	Mem_Free(info);

	Con_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}


/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void FS_AddGameDirectory (char *dir)
{
	stringlist_t *list, *current;
	searchpath_t *search;
	pack_t *pak;
	char pakfile[MAX_OSPATH];

	strcpy (fs_gamedir, dir);

	// add the directory to the search path
	search = Mem_Alloc(pak_mempool, sizeof(searchpath_t));
	strcpy (search->filename, dir);
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	// add any paks in the directory
	list = listdirectory(dir);
	for (current = list;current;current = current->next)
	{
		if (matchpattern(current->text, "*.pak", true))
		{
			sprintf (pakfile, "%s/%s", dir, current->text);
			pak = FS_LoadPackFile (pakfile);
			if (pak)
			{
				search = Mem_Alloc(pak_mempool, sizeof(searchpath_t));
				search->pack = pak;
				search->next = fs_searchpaths;
				fs_searchpaths = search;
			}
			else
				Con_Printf("unable to load pak \"%s\"\n", pakfile);
		}
	}
	freedirectory(list);
}


/*
============
FS_FileExtension
============
*/
char *FS_FileExtension (const char *in)
{
	static char exten[8];
	int i;

	while (*in && *in != '.')
		in++;
	if (!*in)
		return "";
	in++;
	for (i=0 ; i<7 && *in ; i++,in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}


/*
================
FS_Init
================
*/
void FS_Init (void)
{
	int i;
	searchpath_t *search;

	fs_mempool = Mem_AllocPool("file management");
	pak_mempool = Mem_AllocPool("paks");

	Cmd_AddCommand ("path", FS_Path_f);

	strcpy(fs_basedir, ".");

	// -basedir <path>
	// Overrides the system supplied base directory (under GAMENAME)
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
		strcpy (fs_basedir, com_argv[i+1]);

	i = strlen (fs_basedir);
	if (i > 0 && (fs_basedir[i-1] == '\\' || fs_basedir[i-1] == '/'))
		fs_basedir[i-1] = 0;

	// start up with GAMENAME by default (id1)
	strcpy(com_modname, GAMENAME);
	FS_AddGameDirectory (va("%s/"GAMENAME, fs_basedir));
	if (gamedirname[0])
	{
		fs_modified = true;
		strcpy(com_modname, gamedirname);
		FS_AddGameDirectory (va("%s/%s", fs_basedir, gamedirname));
	}

	// -game <gamedir>
	// Adds basedir/gamedir as an override game
	i = COM_CheckParm ("-game");
	if (i && i < com_argc-1)
	{
		fs_modified = true;
		strcpy(com_modname, com_argv[i+1]);
		FS_AddGameDirectory (va("%s/%s", fs_basedir, com_argv[i+1]));
	}

	// -path <dir or packfile> [<dir or packfile>] ...
	// Fully specifies the exact search path, overriding the generated one
	i = COM_CheckParm ("-path");
	if (i)
	{
		fs_modified = true;
		fs_searchpaths = NULL;
		while (++i < com_argc)
		{
			if (!com_argv[i] || com_argv[i][0] == '+' || com_argv[i][0] == '-')
				break;

			search = Mem_Alloc(pak_mempool, sizeof(searchpath_t));
			if ( !strcmp(FS_FileExtension(com_argv[i]), "pak") )
			{
				search->pack = FS_LoadPackFile (com_argv[i]);
				if (!search->pack)
					Sys_Error ("Couldn't load packfile: %s", com_argv[i]);
			}
			else
				strcpy (search->filename, com_argv[i]);
			search->next = fs_searchpaths;
			fs_searchpaths = search;
		}
	}
}


/*
=============================================================================

MAIN FUNCTIONS

=============================================================================
*/

/*
====================
FS_Open

Internal function used to create a qfile_t and open the relevant file on disk
====================
*/
static qfile_t* FS_SysOpen (const char* filepath, const char* mode)
{
	qfile_t* file;

	file = Mem_Alloc (fs_mempool, sizeof (*file));
	memset (file, 0, sizeof (*file));

	file->stream = fopen (filepath, mode);
	if (!file->stream)
	{
		Mem_Free (file);
		return NULL;
	}

	return file;
}


/*
===========
FS_OpenRead
===========
*/
qfile_t *FS_OpenRead (const char *path, int offs, int len)
{
	qfile_t* file;

	file = FS_SysOpen (path, "rb");
	if (!file)
	{
		Sys_Error ("Couldn't open %s", path);
		return NULL;
	}

	// Normal file
	if (offs < 0 || len < 0)
	{
		FS_Seek (file, 0, SEEK_END);
		len = FS_Tell (file);
		FS_Seek (file, 0, SEEK_SET);
	}
	// Packed file
	else
	{
		FS_Seek (file, offs, SEEK_SET);

		file->flags |= FS_FLAG_PACKED;
		file->length = len;
		file->offset = offs;
		file->position = 0;
	}

	fs_filesize = len;

	return file;
}

/*
===========
FS_FOpenFile

If the requested file is inside a packfile, a new qfile_t* will be opened
into the file.

Sets fs_filesize
===========
*/
qfile_t *FS_FOpenFile (const char *filename, qboolean quiet)
{
	searchpath_t *search;
	char netpath[MAX_OSPATH];
	pack_t *pak;
	int i, filenamelen;

	filenamelen = strlen (filename);

	// search through the path, one element at a time
	search = fs_searchpaths;

	for ( ; search ; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			pak = search->pack;
			for (i=0 ; i<pak->numfiles ; i++)
				if (!strcmp (pak->files[i].name, filename))
				{       // found it!
					if (!quiet)
						Sys_Printf ("PackFile: %s : %s\n",pak->filename, pak->files[i].name);
					// open a new file in the pakfile
					return FS_OpenRead (pak->filename, pak->files[i].filepos, pak->files[i].filelen);
				}
		}
		else
		{
			sprintf (netpath, "%s/%s",search->filename, filename);

			if (!FS_SysFileExists (netpath))
				continue;

			if (!quiet)
				Sys_Printf ("FindFile: %s\n",netpath);
			return FS_OpenRead (netpath, -1, -1);
		}
	}

	if (!quiet)
		Sys_Printf ("FindFile: can't find %s\n", filename);

	fs_filesize = -1;
	return NULL;
}


/*
====================
FS_Open

Open a file. The syntax is the same as fopen
====================
*/
qfile_t* FS_Open (const char* filepath, const char* mode, qboolean quiet)
{
	// If the file is opened in "write" or "append" mode
	if (strchr (mode, 'w') || strchr (mode, 'a'))
	{
		char real_path [MAX_OSPATH];

		// Open the file on disk directly
		snprintf (real_path, sizeof (real_path), "%s/%s", fs_gamedir, filepath);

		// Create directories up to the file
		FS_CreatePath (real_path);

		return FS_SysOpen (real_path, mode);
	}

	// Else, we look at the various search paths
	return FS_FOpenFile (filepath, quiet);
}


/*
====================
FS_Close

Close a file
====================
*/
int FS_Close (qfile_t* file)
{
	if (fclose (file->stream))
		return EOF;

	Mem_Free (file);
	return 0;
}


/*
====================
FS_Write

Write "datasize" bytes into a file
====================
*/
size_t FS_Write (qfile_t* file, const void* data, size_t datasize)
{
	return fwrite (data, 1, datasize, file->stream);
}


/*
====================
FS_Read

Read up to "buffersize" bytes from a file
====================
*/
size_t FS_Read (qfile_t* file, void* buffer, size_t buffersize)
{
	size_t nb;

	// If the file belongs to a package, we must take care
	// to not read after the end of the file
	if (file->flags & FS_FLAG_PACKED)
	{
		size_t remain = file->length - file->position;
		if (buffersize > remain)
			buffersize = remain;
	}

	nb = fread (buffer, 1, buffersize, file->stream);

	// Update the position index if the file is packed
	if ((file->flags & FS_FLAG_PACKED) && nb > 0)
		file->position += nb;

	return nb;
}


/*
====================
FS_Flush

Flush the file output stream
====================
*/
int FS_Flush (qfile_t* file)
{
	return fflush (file->stream);
}


/*
====================
FS_Printf

Print a string into a file
====================
*/
int FS_Printf (qfile_t* file, const char* format, ...)
{
	int result;
	va_list args;

	va_start (args, format);
	result = vfprintf (file->stream, format, args);
	va_end (args);

	return result;
}


/*
====================
FS_Getc

Get the next character of a file
====================
*/
int FS_Getc (qfile_t* file)
{
	int c;

	// If the file belongs to a package, we must take care
	// to not read after the end of the file
	if (file->flags & FS_FLAG_PACKED)
	{
		if (file->position >= file->length)
			return EOF;
	}

	c = fgetc (file->stream);

	// Update the position index if the file is packed
	if ((file->flags & FS_FLAG_PACKED) && c != EOF)
		file->position++;

	return c;
}


/*
====================
FS_Seek

Move the position index in a file
====================
*/
int FS_Seek (qfile_t* file, long offset, int whence)
{
	// Packed files receive a special treatment
	if (file->flags & FS_FLAG_PACKED)
	{
		switch (whence)
		{
			case SEEK_CUR:
				offset += file->position;
				// It continues on the next case (no break)

			case SEEK_SET:
				if (offset < 0 || offset > file->length)
					return -1;
				if (fseek (file->stream, file->offset + offset, SEEK_SET) == -1)
					return -1;
				file->position = offset;
				return 0;

			case SEEK_END:
				if (offset > 0 || -offset > file->length)
					return -1;
				if (fseek (file->stream, file->offset + file->length + offset, SEEK_SET) == -1)
					return -1;
				file->position = file->length + offset;
				return 0;

			default:
				return -1;
		}
	}

	return fseek (file->stream, offset, whence);
}


/*
====================
FS_Tell

Give the current position in a file
====================
*/
long FS_Tell (qfile_t* file)
{
	if (file->flags & FS_FLAG_PACKED)
		return file->position;

	return ftell (file->stream);
}


/*
====================
FS_Gets

Extract a line from a file
====================
*/
char* FS_Gets (qfile_t* file, char* buffer, int buffersize)
{
	if (!fgets (buffer, buffersize, file->stream))
		return NULL;

	// Check that we didn't read after the end of a packed file, and update the position
	if (file->flags & FS_FLAG_PACKED)
	{
		size_t len = strlen (buffer);
		size_t max = file->length - file->position;

		if (len > max)
		{
			buffer[max] = '\0';
			file->position = file->length;
		}
		else
			file->position += len;
	}

	return buffer;
}


/*
==========
FS_Getline

Dynamic length version of fgets. DO NOT free the buffer.
==========
*/
char *FS_Getline (qfile_t *file)
{
	static int  size = 256;
	static char *buf = 0;
	char        *t;
	int         len;

	if (!buf)
		buf = Mem_Alloc (fs_mempool, size);

	if (!FS_Gets (file, buf, size))
		return 0;

	len = strlen (buf);
	while (buf[len - 1] != '\n' && buf[len - 1] != '\r')
	{
		t = Mem_Alloc (fs_mempool, size + 256);
		memcpy(t, buf, size);
		Mem_Free(buf);
		size += 256;
		buf = t;
		if (!FS_Gets (file, buf + len, size - len))
			break;
		len = strlen (buf);
	}
	while ((len = strlen(buf)) && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
		buf[len - 1] = 0;
	return buf;
}


/*
====================
FS_Eof

Extract a line from a file
====================
*/
int FS_Eof (qfile_t* file)
{
	if (file->flags & FS_FLAG_PACKED)
		return (file->position == file->length);
	
	return feof (file->stream);
}


/*
============
FS_LoadFile

Filename are relative to the quake directory.
Always appends a 0 byte.
============
*/
qbyte *FS_LoadFile (const char *path, qboolean quiet)
{
	qfile_t *h;
	qbyte *buf;

	// look for it in the filesystem or pack files
	h = FS_Open (path, "rb", quiet);
	if (!h)
		return NULL;

	buf = Mem_Alloc(tempmempool, fs_filesize+1);
	if (!buf)
		Sys_Error ("FS_LoadFile: not enough available memory for %s (size %i)", path, fs_filesize);

	((qbyte *)buf)[fs_filesize] = 0;

	FS_Read (h, buf, fs_filesize);
	FS_Close (h);

	return buf;
}


/*
============
FS_WriteFile

The filename will be prefixed by the current game directory
============
*/
qboolean FS_WriteFile (const char *filename, void *data, int len)
{
	FILE *handle;
	char name[MAX_OSPATH];

	sprintf (name, "%s/%s", fs_gamedir, filename);

	// Create directories up to the file
	FS_CreatePath (name);

	handle = fopen (name, "wb");
	if (!handle)
	{
		Con_Printf ("FS_WriteFile: failed on %s\n", name);
		return false;
	}

	Con_DPrintf ("FS_WriteFile: %s\n", name);
	fwrite (data, 1, len, handle);
	fclose (handle);
	return true;
}


/*
=============================================================================

OTHERS FUNCTIONS

=============================================================================
*/

/*
============
FS_StripExtension
============
*/
void FS_StripExtension (const char *in, char *out)
{
	char *last = NULL;
	while (*in)
	{
		if (*in == '.')
			last = out;
		else if (*in == '/' || *in == '\\' || *in == ':')
			last = NULL;
		*out++ = *in++;
	}
	if (last)
		*last = 0;
	else
		*out = 0;
}


/*
==================
FS_DefaultExtension
==================
*/
void FS_DefaultExtension (char *path, const char *extension)
{
	const char *src;

	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strcat (path, extension);
}


qboolean FS_FileExists (const char *filename)
{
	searchpath_t *search;
	char netpath[MAX_OSPATH];
	pack_t *pak;
	int i;

	for (search = fs_searchpaths;search;search = search->next)
	{
		if (search->pack)
		{
			pak = search->pack;
			for (i = 0;i < pak->numfiles;i++)
				if (!strcmp (pak->files[i].name, filename))
					return true;
		}
		else
		{
			sprintf (netpath, "%s/%s",search->filename, filename);
			if (FS_SysFileExists (netpath))
				return true;
		}
	}

	return false;
}


qboolean FS_SysFileExists (const char *path)
{
#if WIN32
	FILE *f;

	f = fopen (path, "rb");
	if (f)
	{
		fclose (f);
		return true;
	}

	return false;
#else
	struct stat buf;

	if (stat (path,&buf) == -1)
		return false;

	return true;
#endif
}

void FS_mkdir (const char *path)
{
#if WIN32
	_mkdir (path);
#else
	mkdir (path, 0777);
#endif
}
