/*
	DarkPlaces file system

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

CONSTANTS

=============================================================================
*/

// Magic numbers of a ZIP file (big-endian format)
#define ZIP_DATA_HEADER	0x504B0304  // "PK\3\4"
#define ZIP_CDIR_HEADER	0x504B0102  // "PK\1\2"
#define ZIP_END_HEADER	0x504B0506  // "PK\5\6"

// Other constants for ZIP files
#define ZIP_MAX_COMMENTS_SIZE		((unsigned short)0xFFFF)
#define ZIP_END_CDIR_SIZE			22
#define ZIP_CDIR_CHUNK_BASE_SIZE	46
#define ZIP_LOCAL_CHUNK_BASE_SIZE	30

// Zlib constants (from zlib.h)
#define Z_SYNC_FLUSH	2
#define MAX_WBITS		15
#define Z_OK			0
#define Z_STREAM_END	1
#define ZLIB_VERSION	"1.1.4"


/*
=============================================================================

TYPES

=============================================================================
*/

// Zlib stream (from zlib.h)
// Warning: some pointers we don't use directly have
// been cast to "void*" for a matter of simplicity
typedef struct
{
	qbyte			*next_in;	// next input byte
	unsigned int	avail_in;	// number of bytes available at next_in
	unsigned long	total_in;	// total nb of input bytes read so far

	qbyte			*next_out;	// next output byte should be put there
	unsigned int	avail_out;	// remaining free space at next_out
	unsigned long	total_out;	// total nb of bytes output so far

	char			*msg;		// last error message, NULL if no error
	void			*state;		// not visible by applications

	void			*zalloc;	// used to allocate the internal state
	void			*zfree;		// used to free the internal state
	void			*opaque;	// private data object passed to zalloc and zfree

	int				data_type;	// best guess about the data type: ascii or binary
	unsigned long	adler;		// adler32 value of the uncompressed data
	unsigned long	reserved;	// reserved for future use
} z_stream;


// Our own file structure on top of FILE
typedef enum
{
	FS_FLAG_NONE		= 0,
	FS_FLAG_PACKED		= (1 << 0),	// inside a package (PAK or PK3)
	FS_FLAG_DEFLATED	= (1 << 1)	// file is compressed using the deflate algorithm (PK3 only)
} fs_flags_t;

#define ZBUFF_SIZE 1024
typedef struct
{
	z_stream	zstream;
	size_t		real_length;			// length of the uncompressed file
	size_t		in_ind, in_max;			// input buffer index and counter
	size_t		in_position;			// position in the compressed file
	size_t		out_ind, out_max;		// output buffer index and counter
	size_t		out_position;			// how many bytes did we uncompress until now?
	qbyte		input [ZBUFF_SIZE];
	qbyte		output [ZBUFF_SIZE];
} ztoolkit_t;

struct qfile_s
{
	fs_flags_t	flags;
	FILE*		stream;
	size_t		length;		// file size on disk (PACKED only)
	size_t		offset;		// offset into a package (PACKED only)
	size_t		position;	// current position in the file (PACKED only)
	ztoolkit_t*	z;			// used for inflating (DEFLATED only)
};


// ------ PK3 files on disk ------ //

// You can get the complete ZIP format description from PKWARE website

typedef struct
{
	unsigned int signature;
	unsigned short disknum;
	unsigned short cdir_disknum;	// number of the disk with the start of the central directory
	unsigned short localentries;	// number of entries in the central directory on this disk
	unsigned short nbentries;		// total number of entries in the central directory on this disk
	unsigned int cdir_size;			// size of the central directory
	unsigned int cdir_offset;		// with respect to the starting disk number
	unsigned short comment_size;
} pk3_endOfCentralDir_t;


// ------ PAK files on disk ------ //
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
typedef enum
{
	FILE_FLAG_NONE		= 0,
	FILE_FLAG_TRUEOFFS	= (1 << 0),	// the offset in packfile_t is the true contents offset
	FILE_FLAG_DEFLATED	= (1 << 1)	// file compressed using the deflate algorithm
} file_flags_t;

typedef struct
{
	char name [MAX_QPATH];
	file_flags_t flags;
	size_t offset;
	size_t packsize;	// size in the package
	size_t realsize;	// real file size (uncompressed)
} packfile_t;

typedef struct pack_s
{
	char filename [MAX_OSPATH];
	FILE *handle;
	int ignorecase; // PK3 ignores case
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

FUNCTION PROTOTYPES

=============================================================================
*/

void FS_Dir_f(void);
void FS_Ls_f(void);

static packfile_t* FS_AddFileToPack (const char* name, pack_t* pack,
									 size_t offset, size_t packsize,
									 size_t realsize, file_flags_t flags);


/*
=============================================================================

VARIABLES

=============================================================================
*/

mempool_t *fs_mempool;
mempool_t *pak_mempool;

int fs_filesize;

pack_t *packlist = NULL;

searchpath_t *fs_searchpaths = NULL;

#define MAX_FILES_IN_PACK	65536

char fs_gamedir[MAX_OSPATH];
char fs_basedir[MAX_OSPATH];

qboolean fs_modified;   // set true if using non-id files


/*
=============================================================================

PRIVATE FUNCTIONS - PK3 HANDLING

=============================================================================
*/

// Functions exported from zlib
#ifdef WIN32
# define ZEXPORT WINAPI
#else
# define ZEXPORT
#endif

static int (ZEXPORT *qz_inflate) (z_stream* strm, int flush);
static int (ZEXPORT *qz_inflateEnd) (z_stream* strm);
static int (ZEXPORT *qz_inflateInit2_) (z_stream* strm, int windowBits, const char *version, int stream_size);
static int (ZEXPORT *qz_inflateReset) (z_stream* strm);

#define qz_inflateInit2(strm, windowBits) \
        qz_inflateInit2_((strm), (windowBits), ZLIB_VERSION, sizeof(z_stream))

static dllfunction_t zlibfuncs[] =
{
	{"inflate",			(void **) &qz_inflate},
	{"inflateEnd",		(void **) &qz_inflateEnd},
	{"inflateInit2_",	(void **) &qz_inflateInit2_},
	{"inflateReset",	(void **) &qz_inflateReset},
	{NULL, NULL}
};

// Handle for Zlib DLL
static dllhandle_t zlib_dll = NULL;


/*
====================
PK3_CloseLibrary

Unload the Zlib DLL
====================
*/
void PK3_CloseLibrary (void)
{
	Sys_UnloadLibrary (&zlib_dll);
}


/*
====================
PK3_OpenLibrary

Try to load the Zlib DLL
====================
*/
qboolean PK3_OpenLibrary (void)
{
	const char* dllname;

	// Already loaded?
	if (zlib_dll)
		return true;

#ifdef WIN32
	dllname = "zlib.dll";
#else
	dllname = "libz.so";
#endif

	// Load the DLL
	if (! Sys_LoadLibrary (dllname, &zlib_dll, zlibfuncs))
	{
		Con_Printf ("Compressed files support disabled\n");
		return false;
	}

	Con_Printf ("Compressed files support enabled\n");
	return true;
}


/*
====================
PK3_GetEndOfCentralDir

Extract the end of the central directory from a PK3 package
====================
*/
qboolean PK3_GetEndOfCentralDir (const char *packfile, FILE *packhandle, pk3_endOfCentralDir_t *eocd)
{
	long filesize, maxsize;
	qbyte *buffer, *ptr;
	int ind;

	// Get the package size
	fseek (packhandle, 0, SEEK_END);
	filesize = ftell (packhandle);
	if (filesize < ZIP_END_CDIR_SIZE)
		return false;

	// Load the end of the file in memory
	if (filesize < ZIP_MAX_COMMENTS_SIZE + ZIP_END_CDIR_SIZE)
		maxsize = filesize;
	else
		maxsize = ZIP_MAX_COMMENTS_SIZE + ZIP_END_CDIR_SIZE;
	buffer = Mem_Alloc (tempmempool, maxsize);
	fseek (packhandle, filesize - maxsize, SEEK_SET);
	if (fread (buffer, 1, maxsize, packhandle) != (unsigned long) maxsize)
	{
		Mem_Free (buffer);
		return false;
	}

	// Look for the end of central dir signature around the end of the file
	maxsize -= ZIP_END_CDIR_SIZE;
	ptr = &buffer[maxsize];
	ind = 0;
	while (BuffBigLong (ptr) != ZIP_END_HEADER)
	{
		if (ind == maxsize)
		{
			Mem_Free (buffer);
			return false;
		}

		ind++;
		ptr--;
	}

	memcpy (eocd, ptr, ZIP_END_CDIR_SIZE);
	eocd->signature = LittleLong (eocd->signature);
	eocd->disknum = LittleShort (eocd->disknum);
	eocd->cdir_disknum = LittleShort (eocd->cdir_disknum);
	eocd->localentries = LittleShort (eocd->localentries);
	eocd->nbentries = LittleShort (eocd->nbentries);
	eocd->cdir_size = LittleLong (eocd->cdir_size);
	eocd->cdir_offset = LittleLong (eocd->cdir_offset);
	eocd->comment_size = LittleShort (eocd->comment_size);

	Mem_Free (buffer);

	return true;
}


/*
====================
PK3_BuildFileList

Extract the file list from a PK3 file
====================
*/
int PK3_BuildFileList (pack_t *pack, const pk3_endOfCentralDir_t *eocd)
{
	qbyte *central_dir, *ptr;
	unsigned int ind;
	int remaining;

	// Load the central directory in memory
	central_dir = Mem_Alloc (tempmempool, eocd->cdir_size);
	fseek (pack->handle, eocd->cdir_offset, SEEK_SET);
	fread (central_dir, 1, eocd->cdir_size, pack->handle);

	// Extract the files properties
	// The parsing is done "by hand" because some fields have variable sizes and
	// the constant part isn't 4-bytes aligned, which makes the use of structs difficult
	remaining = eocd->cdir_size;
	pack->numfiles = 0;
	ptr = central_dir;
	for (ind = 0; ind < eocd->nbentries; ind++)
	{
		size_t namesize, count;

		// Checking the remaining size
		if (remaining < ZIP_CDIR_CHUNK_BASE_SIZE)
		{
			Mem_Free (central_dir);
			return -1;
		}
		remaining -= ZIP_CDIR_CHUNK_BASE_SIZE;

		// Check header
		if (BuffBigLong (ptr) != ZIP_CDIR_HEADER)
		{
			Mem_Free (central_dir);
			return -1;
		}

		namesize = BuffLittleShort (&ptr[28]);	// filename length

		// Check encryption, compression, and attributes
		// 1st uint8  : general purpose bit flag
		//    Check bits 0 (encryption), 3 (data descriptor after the file), and 5 (compressed patched data (?))
		// 2nd uint8 : external file attributes
		//    Check bits 3 (file is a directory) and 5 (file is a volume (?))
		if ((ptr[8] & 0x29) == 0 && (ptr[38] & 0x18) == 0)
		{
			// Still enough bytes for the name?
			if ((size_t) remaining < namesize || namesize >= sizeof (*pack->files))
			{
				Mem_Free (central_dir);
				return -1;
			}

			// WinZip doesn't use the "directory" attribute, so we need to check the name directly
			if (ptr[ZIP_CDIR_CHUNK_BASE_SIZE + namesize - 1] != '/')
			{
				char filename [sizeof (pack->files[0].name)];
				size_t offset, packsize, realsize;
				file_flags_t flags;

				// Extract the name (strip it if necessary)
				if (namesize >= sizeof (filename))
					namesize = sizeof (filename) - 1;
				memcpy (filename, &ptr[ZIP_CDIR_CHUNK_BASE_SIZE], namesize);
				filename[namesize] = '\0';

				if (BuffLittleShort (&ptr[10]))
					flags = FILE_FLAG_DEFLATED;
				else
					flags = 0;
				offset = BuffLittleLong (&ptr[42]);
				packsize = BuffLittleLong (&ptr[20]);
				realsize = BuffLittleLong (&ptr[24]);
				FS_AddFileToPack (filename, pack, offset, packsize, realsize, flags);
			}
		}

		// Skip the name, additionnal field, and comment
		// 1er uint16 : extra field length
		// 2eme uint16 : file comment length
		count = namesize + BuffLittleShort (&ptr[30]) + BuffLittleShort (&ptr[32]);
		ptr += ZIP_CDIR_CHUNK_BASE_SIZE + count;
		remaining -= count;
	}

	Mem_Free (central_dir);
	return pack->numfiles;
}


/*
====================
FS_LoadPackPK3

Create a package entry associated with a PK3 file
====================
*/
pack_t *FS_LoadPackPK3 (const char *packfile)
{
	FILE *packhandle;
	pk3_endOfCentralDir_t eocd;
	pack_t *pack;
	int real_nb_files;

	packhandle = fopen (packfile, "rb");
	if (!packhandle)
		return NULL;

	if (! PK3_GetEndOfCentralDir (packfile, packhandle, &eocd))
		Sys_Error ("%s is not a PK3 file", packfile);

	// Multi-volume ZIP archives are NOT allowed
	if (eocd.disknum != 0 || eocd.cdir_disknum != 0)
		Sys_Error ("%s is a multi-volume ZIP archive", packfile);

	// We only need to do this test if MAX_FILES_IN_PACK is lesser than 65535
	// since eocd.nbentries is an unsigned 16 bits integer
	#if MAX_FILES_IN_PACK < 65535
	if (eocd.nbentries > MAX_FILES_IN_PACK)
		Sys_Error ("%s contains too many files (%hu)", packfile, eocd.nbentries);
	#endif

	// Create a package structure in memory
	pack = Mem_Alloc (pak_mempool, sizeof (pack_t));
	pack->ignorecase = true; // PK3 ignores case
	strlcpy (pack->filename, packfile, sizeof (pack->filename));
	pack->handle = packhandle;
	pack->numfiles = eocd.nbentries;
	pack->mempool = Mem_AllocPool (packfile);
	pack->files = Mem_Alloc (pack->mempool, eocd.nbentries * sizeof(packfile_t));
	pack->next = packlist;
	packlist = pack;

	real_nb_files = PK3_BuildFileList (pack, &eocd);
	if (real_nb_files <= 0)
		Sys_Error ("%s is not a valid PK3 file", packfile);

	Con_Printf("Added packfile %s (%i files)\n", packfile, real_nb_files);
	return pack;
}


/*
====================
PK3_GetTrueFileOffset

Find where the true file data offset is
====================
*/
void PK3_GetTrueFileOffset (packfile_t *file, pack_t *pack)
{
	qbyte buffer [ZIP_LOCAL_CHUNK_BASE_SIZE];
	size_t count;

	// Already found?
	if (file->flags & FILE_FLAG_TRUEOFFS)
		return;

	// Load the local file description
	fseek (pack->handle, file->offset, SEEK_SET);
	count = fread (buffer, 1, ZIP_LOCAL_CHUNK_BASE_SIZE, pack->handle);
	if (count != ZIP_LOCAL_CHUNK_BASE_SIZE || BuffBigLong (buffer) != ZIP_DATA_HEADER)
		Sys_Error ("Can't retrieve file %s in package %s", file->name, pack->filename);

	// Skip name and extra field
	file->offset += BuffLittleShort (&buffer[26]) + BuffLittleShort (&buffer[28]) + ZIP_LOCAL_CHUNK_BASE_SIZE;

	file->flags |= FILE_FLAG_TRUEOFFS;
}


/*
=============================================================================

OTHER PRIVATE FUNCTIONS

=============================================================================
*/


/*
====================
FS_AddFileToPack

Add a file to the list of files contained into a package
====================
*/
static packfile_t* FS_AddFileToPack (const char* name, pack_t* pack,
									 size_t offset, size_t packsize,
									 size_t realsize, file_flags_t flags)
{
	int (*strcmp_funct) (const char* str1, const char* str2);
	size_t left, right, middle;
	int diff;
	packfile_t *file;

	strcmp_funct = pack->ignorecase ? strcasecmp : strcmp;

	// Look for the slot we should put that file into (binary search)
	left = 0;
	right = pack->numfiles;
	while (left != right)
	{
		middle = (left + right - 1) / 2;
		diff = strcmp_funct (pack->files[middle].name, name);

		// If we found the file, there's a problem
		if (!diff)
			Sys_Error ("Package %s contains several time the file %s\n",
					   pack->filename, name);

		// If we're too far in the list
		if (diff > 0)
			right = middle;
		else
			left = middle + 1;
	}

	// We have to move the right of the list by one slot to free the one we need
	file = &pack->files[left];
	memmove (file + 1, file, (pack->numfiles - left) * sizeof (*file));
	pack->numfiles++;

	strlcpy (file->name, name, sizeof (file->name));
	file->offset = offset;
	file->packsize = packsize;
	file->realsize = realsize;
	file->flags = flags;

	return file;
}


/*
============
FS_CreatePath

Only used for FS_Open.
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

	Con_Print("Current search path:\n");
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s->pack)
		{
			Con_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		}
		else
			Con_Printf("%s\n", s->filename);
	}
}


/*
=================
FS_LoadPackPAK

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *FS_LoadPackPAK (const char *packfile)
{
	dpackheader_t header;
	int i, numpackfiles;
	FILE *packhandle;
	pack_t *pack;
	dpackfile_t *info;	// temporary alloc, allowing huge pack directories

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
	pack->ignorecase = false; // PAK is case sensitive
	strlcpy (pack->filename, packfile, sizeof (pack->filename));
	pack->handle = packhandle;
	pack->numfiles = 0;
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
		size_t offset = LittleLong (info[i].filepos);
		size_t size = LittleLong (info[i].filelen);

		FS_AddFileToPack (info[i].name, pack, offset, size, size, FILE_FLAG_TRUEOFFS);
	}

	Mem_Free(info);

	Con_Printf("Added packfile %s (%i files)\n", packfile, numpackfiles);
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

	strlcpy (fs_gamedir, dir, sizeof (fs_gamedir));

#ifndef AKVERSION
	// add the directory to the search path
	search = Mem_Alloc(pak_mempool, sizeof(searchpath_t));
	strlcpy (search->filename, dir, sizeof (search->filename));
	search->next = fs_searchpaths;
	fs_searchpaths = search;
#endif

	list = listdirectory(dir);

	// add any PAK package in the directory
	for (current = list;current;current = current->next)
	{
		if (matchpattern(current->text, "*.pak", true))
		{
			snprintf (pakfile, sizeof (pakfile), "%s/%s", dir, current->text);
			pak = FS_LoadPackPAK (pakfile);
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

	// add any PK3 package in the director
	for (current = list;current;current = current->next)
	{
		if (matchpattern(current->text, "*.pk3", true))
		{
			snprintf (pakfile, sizeof (pakfile), "%s/%s", dir, current->text);
			pak = FS_LoadPackPK3 (pakfile);
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

// Unpacked files have the priority over packed files if AKVERSION is defined
#ifdef AKVERSION
	// add the directory to the search path
	search = Mem_Alloc(pak_mempool, sizeof(searchpath_t));
	strlcpy (search->filename, dir, sizeof (search->filename));
	search->next = fs_searchpaths;
	fs_searchpaths = search;
#endif
}


/*
============
FS_FileExtension
============
*/
char *FS_FileExtension (const char *in)
{
	static char exten[8];
	const char *slash, *backslash, *colon, *dot, *separator;
	int i;

	slash = strrchr(in, '/');
	backslash = strrchr(in, '\\');
	colon = strrchr(in, ':');
	dot = strrchr(in, '.');
	separator = slash;
	if (separator < backslash)
		separator = backslash;
	if (separator < colon)
		separator = colon;
	if (dot == NULL || dot < separator)
		return "";
	dot++;
	for (i = 0;i < 7 && dot[i];i++)
		exten[i] = dot[i];
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
	Cmd_AddCommand ("dir", FS_Dir_f);
	Cmd_AddCommand ("ls", FS_Ls_f);

	strcpy(fs_basedir, ".");
	strcpy(fs_gamedir, ".");

	PK3_OpenLibrary ();

	// -basedir <path>
	// Overrides the system supplied base directory (under GAMENAME)
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
	{
		strlcpy (fs_basedir, com_argv[i+1], sizeof (fs_basedir));
		i = strlen (fs_basedir);
		if (i > 0 && (fs_basedir[i-1] == '\\' || fs_basedir[i-1] == '/'))
			fs_basedir[i-1] = 0;
	}

	// -path <dir or packfile> [<dir or packfile>] ...
	// Fully specifies the exact search path, overriding the generated one
	i = COM_CheckParm ("-path");
	if (i)
	{
		fs_modified = true;
		while (++i < com_argc)
		{
			if (!com_argv[i] || com_argv[i][0] == '+' || com_argv[i][0] == '-')
				break;

			search = Mem_Alloc(pak_mempool, sizeof(searchpath_t));
			if (!strcasecmp (FS_FileExtension(com_argv[i]), "pak"))
			{
				search->pack = FS_LoadPackPAK (com_argv[i]);
				if (!search->pack)
					Sys_Error ("Couldn't load packfile: %s", com_argv[i]);
			}
			else if (!strcasecmp (FS_FileExtension (com_argv[i]), "pk3"))
			{
				search->pack = FS_LoadPackPK3 (com_argv[i]);
				if (!search->pack)
					Sys_Error ("Couldn't load packfile: %s", com_argv[i]);
			}
			else
				strlcpy (search->filename, com_argv[i], sizeof (search->filename));
			search->next = fs_searchpaths;
			fs_searchpaths = search;
		}
		return;
	}

	// start up with GAMENAME by default (id1)
	strlcpy (com_modname, GAMENAME, sizeof (com_modname));
	FS_AddGameDirectory (va("%s/"GAMENAME, fs_basedir));

	// add the game-specific path, if any
	if (gamedirname[0])
	{
		fs_modified = true;
		strlcpy (com_modname, gamedirname, sizeof (com_modname));
		FS_AddGameDirectory (va("%s/%s", fs_basedir, gamedirname));
	}

	// -game <gamedir>
	// Adds basedir/gamedir as an override game
	i = COM_CheckParm ("-game");
	if (i && i < com_argc-1)
	{
		fs_modified = true;
		strlcpy (com_modname, com_argv[i+1], sizeof (com_modname));
		FS_AddGameDirectory (va("%s/%s", fs_basedir, com_argv[i+1]));
	}
}


/*
====================
FS_SysOpen

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
		// We set fs_filesize here for normal files
		fseek (file->stream, 0, SEEK_END);
		fs_filesize = ftell (file->stream);
		fseek (file->stream, 0, SEEK_SET);
	}
	// Packed file
	else
	{
		fseek (file->stream, offs, SEEK_SET);

		file->flags |= FS_FLAG_PACKED;
		file->length = len;
		file->offset = offs;
		file->position = 0;
	}

	return file;
}


/*
====================
FS_FindFile

Look for a file in the packages and in the filesystem

Return the searchpath where the file was found (or NULL)
and the file index in the package if relevant
====================
*/
static searchpath_t *FS_FindFile (const char *name, int* index, qboolean quiet)
{
	searchpath_t *search;
	pack_t *pak;
	int (*strcmp_funct) (const char* str1, const char* str2);

	// search through the path, one element at a time
	for (search = fs_searchpaths;search;search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			size_t left, right, middle;

			pak = search->pack;
			strcmp_funct = pak->ignorecase ? strcasecmp : strcmp;

			// Look for the file (binary search)
			left = 0;
			right = pak->numfiles;
			while (left != right)
			{
				int diff;

				middle = (left + right - 1) / 2;
				diff = strcmp_funct (pak->files[middle].name, name);

				// Found it
				if (!diff)
				{
					if (!quiet)
						Sys_Printf("FS_FindFile: %s in %s\n",
									pak->files[middle].name, pak->filename);

					if (index != NULL)
						*index = middle;
					return search;
				}

				// If we're too far in the list
				if (diff > 0)
					right = middle;
				else
					left = middle + 1;
			}
		}
		else
		{
			char netpath[MAX_OSPATH];
			snprintf(netpath, sizeof(netpath), "%s/%s", search->filename, name);
			if (FS_SysFileExists (netpath))
			{
				if (!quiet)
					Sys_Printf("FS_FindFile: %s\n", netpath);

				if (index != NULL)
					*index = -1;
				return search;
			}
		}
	}

	if (!quiet)
		Sys_Printf("FS_FindFile: can't find %s\n", name);

	if (index != NULL)
		*index = -1;
	return NULL;
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
	packfile_t *packfile;
	int i;
	qfile_t *file;

	search = FS_FindFile (filename, &i, quiet);

	// Not found?
	if (search == NULL)
	{
		fs_filesize = -1;
		return NULL;
	}

	// Found in the filesystem?
	if (i < 0)
	{
		char netpath[MAX_OSPATH];
		snprintf(netpath, sizeof(netpath), "%s/%s", search->filename, filename);
		return FS_OpenRead(netpath, -1, -1);
	}

	// So, we found it in a package...
	packfile = &search->pack->files[i];

	// If we don't have the true offset, get it now
	if (! (packfile->flags & FILE_FLAG_TRUEOFFS))
		PK3_GetTrueFileOffset (packfile, search->pack);

	// No Zlib DLL = no compressed files
	if (!zlib_dll && (packfile->flags & FILE_FLAG_DEFLATED))
	{
		Con_Printf("WARNING: can't open the compressed file %s\n"
					"You need the Zlib DLL to use compressed files\n",
					filename);
		fs_filesize = -1;
		return NULL;
	}

	// open a new file in the pakfile
	file = FS_OpenRead (search->pack->filename, packfile->offset, packfile->packsize);
	fs_filesize = packfile->realsize;

	if (packfile->flags & FILE_FLAG_DEFLATED)
	{
		ztoolkit_t *ztk;

		file->flags |= FS_FLAG_DEFLATED;

		// We need some more variables
		ztk = Mem_Alloc (fs_mempool, sizeof (*file->z));

		ztk->real_length = packfile->realsize;

		// Initialize zlib stream
		ztk->zstream.next_in = ztk->input;
		ztk->zstream.avail_in = 0;

		/* From Zlib's "unzip.c":
		 *
		 * windowBits is passed < 0 to tell that there is no zlib header.
		 * Note that in this case inflate *requires* an extra "dummy" byte
		 * after the compressed stream in order to complete decompression and
		 * return Z_STREAM_END.
		 * In unzip, i don't wait absolutely Z_STREAM_END because I known the
		 * size of both compressed and uncompressed data
		 */
		if (qz_inflateInit2 (&ztk->zstream, -MAX_WBITS) != Z_OK)
			Sys_Error ("inflate init error (file: %s)", filename);

		ztk->zstream.next_out = ztk->output;
		ztk->zstream.avail_out = sizeof (ztk->output);

		file->z = ztk;
	}

	return file;
}


/*
=============================================================================

MAIN PUBLIC FUNCTIONS

=============================================================================
*/

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

	if (file->z)
	{
		qz_inflateEnd (&file->z->zstream);
		Mem_Free (file->z);
	}

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
	size_t count, nb;
	ztoolkit_t *ztk;

	// Quick path for unpacked files
	if (! (file->flags & FS_FLAG_PACKED))
		return fread (buffer, 1, buffersize, file->stream);

	// If the file isn't compressed
	if (! (file->flags & FS_FLAG_DEFLATED))
	{
		// We must take care to not read after the end of the file
		count = file->length - file->position;
		if (buffersize > count)
			buffersize = count;

		nb = fread (buffer, 1, buffersize, file->stream);

		file->position += nb;
		return nb;
	}

	// If the file is compressed, it's more complicated...
	ztk = file->z;

	// First, we copy as many bytes as we can from "output"
	if (ztk->out_ind < ztk->out_max)
	{
		count = ztk->out_max - ztk->out_ind;

		nb = (buffersize > count) ? count : buffersize;
		memcpy (buffer, &ztk->output[ztk->out_ind], nb);
		ztk->out_ind += nb;
		file->position += nb;
	}
	else
		nb = 0;

	// We cycle through a few operations until we have inflated enough data
	while (nb < buffersize)
	{
		// NOTE: at this point, "output" should always be empty

		// If "input" is also empty, we need to fill it
		if (ztk->in_ind == ztk->in_max)
		{
			size_t remain;

			// If we are at the end of the file
			if (ztk->out_position == ztk->real_length)
				return nb;

			remain = file->length - ztk->in_position;
			count = (remain > sizeof (ztk->input)) ? sizeof (ztk->input) : remain;
			fread (ztk->input, 1, count, file->stream);

			// Update indexes and counters
			ztk->in_ind = 0;
			ztk->in_max = count;
			ztk->in_position += count;
		}

		// Now that we are sure we have compressed data available, we need to determine
		// if it's better to inflate it in "output" or directly in "buffer" (we are in this
		// case if we still need more bytes than "output" can contain)

		ztk->zstream.next_in = &ztk->input[ztk->in_ind];
		ztk->zstream.avail_in = ztk->in_max - ztk->in_ind;

		// If output will be able to contain at least 1 more byte than the data we need
		if (buffersize - nb < sizeof (ztk->output))
		{
			int error;

			// Inflate the data in "output"
			ztk->zstream.next_out = ztk->output;
			ztk->zstream.avail_out = sizeof (ztk->output);
			error = qz_inflate (&ztk->zstream, Z_SYNC_FLUSH);
			if (error != Z_OK && error != Z_STREAM_END)
				Sys_Error ("Can't inflate file");
			ztk->in_ind = ztk->in_max - ztk->zstream.avail_in;
			ztk->out_max = sizeof (ztk->output) - ztk->zstream.avail_out;
			ztk->out_position += ztk->out_max;

			// Copy the requested data in "buffer" (as much as we can)
			count = (buffersize - nb > ztk->out_max) ? ztk->out_max : buffersize - nb;
			memcpy (&((qbyte*)buffer)[nb], ztk->output, count);
			ztk->out_ind = count;
		}

		// Else, we inflate directly in "buffer"
		else
		{
			int error;

			// Inflate the data in "buffer"
			ztk->zstream.next_out = &((qbyte*)buffer)[nb];
			ztk->zstream.avail_out = buffersize - nb;
			error = qz_inflate (&ztk->zstream, Z_SYNC_FLUSH);
			if (error != Z_OK && error != Z_STREAM_END)
				Sys_Error ("Can't inflate file");
			ztk->in_ind = ztk->in_max - ztk->zstream.avail_in;

			// Invalidate the output data (for FS_Seek)
			ztk->out_max = 0;
			ztk->out_ind = 0;

			// How much data did it inflate?
			count = buffersize - nb - ztk->zstream.avail_out;
			ztk->out_position += count;
		}

		nb += count;
		file->position += count;
	}

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
FS_Print

Print a string into a file
====================
*/
int FS_Print(qfile_t* file, const char *msg)
{
	return FS_Write(file, msg, strlen(msg));
}

/*
====================
FS_Printf

Print a string into a file
====================
*/
int FS_Printf(qfile_t* file, const char* format, ...)
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
FS_VPrintf

Print a string into a file
====================
*/
int FS_VPrintf(qfile_t* file, const char* format, va_list ap)
{
	return vfprintf (file->stream, format, ap);
}


/*
====================
FS_Getc

Get the next character of a file
====================
*/
int FS_Getc (qfile_t* file)
{
	char c;

	if (FS_Read (file, &c, 1) != 1)
		return EOF;

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
	// Quick path for unpacked files
	if (! (file->flags & FS_FLAG_PACKED))
		return fseek (file->stream, offset, whence);

	// Seeking in compressed files is more a hack than anything else,
	// but we need to support it, so here it is.
	if (file->flags & FS_FLAG_DEFLATED)
	{
		ztoolkit_t *ztk = file->z;
		qbyte buffer [sizeof (ztk->output)];  // it's big to force inflating into buffer directly

		switch (whence)
		{
			case SEEK_CUR:
				offset += file->position;
				break;

			case SEEK_SET:
				break;

			case SEEK_END:
				offset += ztk->real_length;
				break;

			default:
				return -1;
		}
		if (offset < 0 || offset > (long) ztk->real_length)
			return -1;

		// If we need to go back in the file
		if (offset <= (long) file->position)
		{
			// If we still have the data we need in the output buffer
			if (file->position - offset <= ztk->out_ind)
			{
				ztk->out_ind -= file->position - offset;
				file->position = offset;
				return 0;
			}

			// Else, we restart from the beginning of the file
			ztk->in_ind = 0;
			ztk->in_max = 0;
			ztk->in_position = 0;
			ztk->out_ind = 0;
			ztk->out_max = 0;
			ztk->out_position = 0;
			file->position = 0;
			fseek (file->stream, file->offset, SEEK_SET);

			// Reset the Zlib stream
			ztk->zstream.next_in = ztk->input;
			ztk->zstream.avail_in = 0;
			qz_inflateReset (&ztk->zstream);
		}

		// Skip all data until we reach the requested offset
		while ((long) file->position < offset)
		{
			size_t diff = offset - file->position;
			size_t count, len;

			count = (diff > sizeof (buffer)) ? sizeof (buffer) : diff;
			len = FS_Read (file, buffer, count);
			if (len != count)
				return -1;
		}

		return 0;
	}

	// Packed files receive a special treatment too, because
	// we need to make sure it doesn't go outside of the file
	switch (whence)
	{
		case SEEK_CUR:
			offset += file->position;
			break;

		case SEEK_SET:
			break;

		case SEEK_END:
			offset += file->length;
			break;

		default:
			return -1;
	}
	if (offset < 0 || offset > (long) file->length)
		return -1;

	if (fseek (file->stream, file->offset + offset, SEEK_SET) == -1)
		return -1;
	file->position = offset;
	return 0;
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
	size_t ind;

	// Quick path for unpacked files
	if (! (file->flags & FS_FLAG_PACKED))
		return fgets (buffer, buffersize, file->stream);

	for (ind = 0; ind < (size_t) buffersize - 1; ind++)
	{
		int c = FS_Getc (file);
		switch (c)
		{
			// End of file
			case -1:
				if (!ind)
					return NULL;

				buffer[ind] = '\0';
				return buffer;

			// End of line
			case '\r':
			case '\n':
				buffer[ind] = '\n';
				buffer[ind + 1] = '\0';
				return buffer;

			default:
				buffer[ind] = c;
		}

	}

	buffer[buffersize - 1] = '\0';
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
	{
		if (file->flags & FS_FLAG_DEFLATED)
			return (file->position == file->z->real_length);

		return (file->position == file->length);
	}

	return feof (file->stream);
}


/*
============
FS_LoadFile

Filename are relative to the quake directory.
Always appends a 0 byte.
============
*/
qbyte *FS_LoadFile (const char *path, mempool_t *pool, qboolean quiet)
{
	qfile_t *h;
	qbyte *buf;

	// look for it in the filesystem or pack files
	h = FS_Open (path, "rb", quiet);
	if (!h)
		return NULL;

	buf = Mem_Alloc(pool, fs_filesize+1);
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
	qfile_t *handle;

	handle = FS_Open (filename, "wb", false);
	if (!handle)
	{
		Con_Printf("FS_WriteFile: failed on %s\n", filename);
		return false;
	}

	Con_DPrintf("FS_WriteFile: %s\n", filename);
	FS_Write (handle, data, len);
	FS_Close (handle);
	return true;
}


/*
=============================================================================

OTHERS PUBLIC FUNCTIONS

=============================================================================
*/

/*
============
FS_StripExtension
============
*/
void FS_StripExtension (const char *in, char *out, size_t size_out)
{
	char *last = NULL;

	if (size_out == 0)
		return;

	while (*in && size_out > 1)
	{
		if (*in == '.')
			last = out;
		else if (*in == '/' || *in == '\\' || *in == ':')
			last = NULL;
		*out++ = *in++;
		size_out--;
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
void FS_DefaultExtension (char *path, const char *extension, size_t size_path)
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

	strlcat (path, extension, size_path);
}


/*
==================
FS_FileExists

Look for a file in the packages and in the filesystem
==================
*/
qboolean FS_FileExists (const char *filename)
{
	return (FS_FindFile (filename, NULL, true) != NULL);
}


/*
==================
FS_SysFileExists

Look for a file in the filesystem only
==================
*/
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

/*
===========
FS_Search

Allocate and fill a search structure with information on matching filenames.
===========
*/
fssearch_t *FS_Search(const char *pattern, int caseinsensitive, int quiet)
{
	fssearch_t *search;
	searchpath_t *searchpath;
	pack_t *pak;
	int i, basepathlength, numfiles, numchars;
	stringlist_t *dir, *dirfile, *liststart, *listcurrent, *listtemp;
	const char *slash, *backslash, *colon, *separator;
	char *basepath;
	char netpath[MAX_OSPATH];
	char temp[MAX_OSPATH];

	while(!strncmp(pattern, "./", 2))
		pattern += 2;
	while(!strncmp(pattern, ".\\", 2))
		pattern += 2;

	search = NULL;
	liststart = NULL;
	listcurrent = NULL;
	listtemp = NULL;
	slash = strrchr(pattern, '/');
	backslash = strrchr(pattern, '\\');
	colon = strrchr(pattern, ':');
	separator = pattern;
	if (separator < slash)
		separator = slash;
	if (separator < backslash)
		separator = backslash;
	if (separator < colon)
		separator = colon;
	basepathlength = separator - pattern;
	basepath = Mem_Alloc (tempmempool, basepathlength + 1);
	if (basepathlength)
		memcpy(basepath, pattern, basepathlength);
	basepath[basepathlength] = 0;

	// search through the path, one element at a time
	for (searchpath = fs_searchpaths;searchpath;searchpath = searchpath->next)
	{
		// is the element a pak file?
		if (searchpath->pack)
		{
			// look through all the pak file elements
			pak = searchpath->pack;
			for (i = 0;i < pak->numfiles;i++)
			{
				strcpy(temp, pak->files[i].name);
				while (temp[0])
				{
					if (matchpattern(temp, (char *)pattern, true))
					{
						for (listtemp = liststart;listtemp;listtemp = listtemp->next)
							if (!strcmp(listtemp->text, temp))
								break;
						if (listtemp == NULL)
						{
							listcurrent = stringlistappend(listcurrent, temp);
							if (liststart == NULL)
								liststart = listcurrent;
							if (!quiet)
								Sys_Printf("SearchPackFile: %s : %s\n", pak->filename, temp);
						}
					}
					// strip off one path element at a time until empty
					// this way directories are added to the listing if they match the pattern
					slash = strrchr(temp, '/');
					backslash = strrchr(temp, '\\');
					colon = strrchr(temp, ':');
					separator = temp;
					if (separator < slash)
						separator = slash;
					if (separator < backslash)
						separator = backslash;
					if (separator < colon)
						separator = colon;
					*((char *)separator) = 0;
				}
			}
		}
		else
		{
			// get a directory listing and look at each name
			snprintf(netpath, sizeof (netpath), "%s/%s", searchpath->filename, basepath);
			if ((dir = listdirectory(netpath)))
			{
				for (dirfile = dir;dirfile;dirfile = dirfile->next)
				{
					snprintf(temp, sizeof(temp), "%s/%s", basepath, dirfile->text);
					if (matchpattern(temp, (char *)pattern, true))
					{
						for (listtemp = liststart;listtemp;listtemp = listtemp->next)
							if (!strcmp(listtemp->text, temp))
								break;
						if (listtemp == NULL)
						{
							listcurrent = stringlistappend(listcurrent, temp);
							if (liststart == NULL)
								liststart = listcurrent;
							if (!quiet)
								Sys_Printf("SearchDirFile: %s\n", temp);
						}
					}
				}
				freedirectory(dir);
			}
		}
	}

	if (liststart)
	{
		liststart = stringlistsort(liststart);
		numfiles = 0;
		numchars = 0;
		for (listtemp = liststart;listtemp;listtemp = listtemp->next)
		{
			numfiles++;
			numchars += strlen(listtemp->text) + 1;
		}
		search = Z_Malloc(sizeof(fssearch_t) + numchars + numfiles * sizeof(char *));
		search->filenames = (char **)((char *)search + sizeof(fssearch_t));
		search->filenamesbuffer = (char *)((char *)search + sizeof(fssearch_t) + numfiles * sizeof(char *));
		search->numfilenames = numfiles;
		numfiles = 0;
		numchars = 0;
		for (listtemp = liststart;listtemp;listtemp = listtemp->next)
		{
			search->filenames[numfiles] = search->filenamesbuffer + numchars;
			strcpy(search->filenames[numfiles], listtemp->text);
			numfiles++;
			numchars += strlen(listtemp->text) + 1;
		}
		if (liststart)
			stringlistfree(liststart);
	}

	Mem_Free(basepath);
	return search;
}

void FS_FreeSearch(fssearch_t *search)
{
	Z_Free(search);
}

extern int con_linewidth;
int FS_ListDirectory(const char *pattern, int oneperline)
{
	int numfiles;
	int numcolumns;
	int numlines;
	int columnwidth;
	int linebufpos;
	int i, j, k, l;
	const char *name;
	char linebuf[4096];
	fssearch_t *search;
	search = FS_Search(pattern, true, true);
	if (!search)
		return 0;
	numfiles = search->numfilenames;
	if (!oneperline)
	{
		// FIXME: the names could be added to one column list and then
		// gradually shifted into the next column if they fit, and then the
		// next to make a compact variable width listing but it's a lot more
		// complicated...
		// find width for columns
		columnwidth = 0;
		for (i = 0;i < numfiles;i++)
		{
			l = strlen(search->filenames[i]);
			if (columnwidth < l)
				columnwidth = l;
		}
		// count the spacing character
		columnwidth++;
		// calculate number of columns
		numcolumns = con_linewidth / columnwidth;
		// don't bother with the column printing if it's only one column
		if (numcolumns >= 2)
		{
			numlines = (numfiles + numcolumns - 1) / numcolumns;
			for (i = 0;i < numlines;i++)
			{
				linebufpos = 0;
				for (k = 0;k < numcolumns;k++)
				{
					l = i * numcolumns + k;
					if (l < numfiles)
					{
						name = search->filenames[l];
						for (j = 0;name[j] && j < (int)sizeof(linebuf) - 1;j++)
							linebuf[linebufpos++] = name[j];
						// space out name unless it's the last on the line
						if (k < (numcolumns - 1) && l < (numfiles - 1))
							for (;j < columnwidth && j < (int)sizeof(linebuf) - 1;j++)
								linebuf[linebufpos++] = ' ';
					}
				}
				linebuf[linebufpos] = 0;
				Con_Printf("%s\n", linebuf);
			}
		}
		else
			oneperline = true;
	}
	if (oneperline)
		for (i = 0;i < numfiles;i++)
			Con_Printf("%s\n", search->filenames[i]);
	FS_FreeSearch(search);
	return numfiles;
}

static void FS_ListDirectoryCmd (const char* cmdname, int oneperline)
{
	const char *pattern;
	if (Cmd_Argc() > 3)
	{
		Con_Printf("usage:\n%s [path/pattern]\n", cmdname);
		return;
	}
	if (Cmd_Argc() == 2)
		pattern = Cmd_Argv(1);
	else
		pattern = "*";
	if (!FS_ListDirectory(pattern, oneperline))
		Con_Print("No files found.\n");
}

void FS_Dir_f(void)
{
	FS_ListDirectoryCmd("dir", true);
}

void FS_Ls_f(void)
{
	FS_ListDirectoryCmd("ls", false);
}

