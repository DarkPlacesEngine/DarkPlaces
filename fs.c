/*
	DarkPlaces file system

	Copyright (C) 2003-2005 Mathieu Olivier
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

#include "fs.h"

// Win32 requires us to add O_BINARY, but the other OSes don't have it
#ifndef O_BINARY
# define O_BINARY 0
#endif


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
precaution against having a malicious server instruct clients to write files
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


typedef enum
{
	QFILE_FLAG_NONE		= 0,
	QFILE_FLAG_PACKED	= (1 << 0),	// inside a package (PAK or PK3)
	QFILE_FLAG_DEFLATED	= (1 << 1)	// file is compressed using the deflate algorithm (PK3 only)
} qfile_flags_t;

#define FILE_BUFF_SIZE 2048
typedef struct
{
	z_stream	zstream;
	size_t		comp_length;			// length of the compressed file
	size_t		in_ind, in_len;			// input buffer current index and length
	size_t		in_position;			// position in the compressed file
	qbyte		input [FILE_BUFF_SIZE];
} ztoolkit_t;

struct qfile_s
{
	qfile_flags_t	flags;
	int				handle;					// file descriptor
	size_t			real_length;			// uncompressed file size (for files opened in "read" mode)
	size_t			position;				// current position in the file
	size_t			offset;					// offset into the package (0 if external file)
	int				ungetc;					// single stored character from ungetc, cleared to EOF when read

	// Contents buffer
	size_t			buff_ind, buff_len;		// buffer current index and length
	qbyte			buff [FILE_BUFF_SIZE];

	// For zipped files
	ztoolkit_t*		ztk;
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
	PACKFILE_FLAG_NONE		= 0,
	PACKFILE_FLAG_TRUEOFFS	= (1 << 0),	// the offset in packfile_t is the true contents offset
	PACKFILE_FLAG_DEFLATED	= (1 << 1)	// file compressed using the deflate algorithm
} packfile_flags_t;

typedef struct
{
	char name [MAX_QPATH];
	packfile_flags_t flags;
	size_t offset;
	size_t packsize;	// size in the package
	size_t realsize;	// real file size (uncompressed)
} packfile_t;

typedef struct pack_s
{
	char filename [MAX_OSPATH];
	int handle;
	int ignorecase;  // PK3 ignores case
	int numfiles;
	packfile_t *files;
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
									 size_t realsize, packfile_flags_t flags);


/*
=============================================================================

VARIABLES

=============================================================================
*/

mempool_t *fs_mempool;

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
	const char* dllnames [] =
	{
#ifdef WIN32
		"zlib.dll",
#elif defined(MACOSX)
		"libz.dylib",
#else
		"libz.so.1",
		"libz.so",
#endif
		NULL
	};

	// Already loaded?
	if (zlib_dll)
		return true;

	// Load the DLL
	if (! Sys_LoadLibrary (dllnames, &zlib_dll, zlibfuncs))
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
qboolean PK3_GetEndOfCentralDir (const char *packfile, int packhandle, pk3_endOfCentralDir_t *eocd)
{
	long filesize, maxsize;
	qbyte *buffer, *ptr;
	int ind;

	// Get the package size
	filesize = lseek (packhandle, 0, SEEK_END);
	if (filesize < ZIP_END_CDIR_SIZE)
		return false;

	// Load the end of the file in memory
	if (filesize < ZIP_MAX_COMMENTS_SIZE + ZIP_END_CDIR_SIZE)
		maxsize = filesize;
	else
		maxsize = ZIP_MAX_COMMENTS_SIZE + ZIP_END_CDIR_SIZE;
	buffer = Mem_Alloc (tempmempool, maxsize);
	lseek (packhandle, filesize - maxsize, SEEK_SET);
	if (read (packhandle, buffer, maxsize) != (ssize_t) maxsize)
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
	lseek (pack->handle, eocd->cdir_offset, SEEK_SET);
	read (pack->handle, central_dir, eocd->cdir_size);

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
				packfile_flags_t flags;

				// Extract the name (strip it if necessary)
				if (namesize >= sizeof (filename))
					namesize = sizeof (filename) - 1;
				memcpy (filename, &ptr[ZIP_CDIR_CHUNK_BASE_SIZE], namesize);
				filename[namesize] = '\0';

				if (BuffLittleShort (&ptr[10]))
					flags = PACKFILE_FLAG_DEFLATED;
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

	// If the package is empty, central_dir is NULL here
	if (central_dir != NULL)
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
	int packhandle;
	pk3_endOfCentralDir_t eocd;
	pack_t *pack;
	int real_nb_files;

	packhandle = open (packfile, O_RDONLY | O_BINARY);
	if (packhandle < 0)
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
	pack = Mem_Alloc(fs_mempool, sizeof (pack_t));
	pack->ignorecase = true; // PK3 ignores case
	strlcpy (pack->filename, packfile, sizeof (pack->filename));
	pack->handle = packhandle;
	pack->numfiles = eocd.nbentries;
	pack->files = Mem_Alloc(fs_mempool, eocd.nbentries * sizeof(packfile_t));
	pack->next = packlist;
	packlist = pack;

	real_nb_files = PK3_BuildFileList (pack, &eocd);
	if (real_nb_files < 0)
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
void PK3_GetTrueFileOffset (packfile_t *pfile, pack_t *pack)
{
	qbyte buffer [ZIP_LOCAL_CHUNK_BASE_SIZE];
	size_t count;

	// Already found?
	if (pfile->flags & PACKFILE_FLAG_TRUEOFFS)
		return;

	// Load the local file description
	lseek (pack->handle, pfile->offset, SEEK_SET);
	count = read (pack->handle, buffer, ZIP_LOCAL_CHUNK_BASE_SIZE);
	if (count != ZIP_LOCAL_CHUNK_BASE_SIZE || BuffBigLong (buffer) != ZIP_DATA_HEADER)
		Sys_Error ("Can't retrieve file %s in package %s", pfile->name, pack->filename);

	// Skip name and extra field
	pfile->offset += BuffLittleShort (&buffer[26]) + BuffLittleShort (&buffer[28]) + ZIP_LOCAL_CHUNK_BASE_SIZE;

	pfile->flags |= PACKFILE_FLAG_TRUEOFFS;
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
									 size_t realsize, packfile_flags_t flags)
{
	int (*strcmp_funct) (const char* str1, const char* str2);
	int left, right, middle;
	packfile_t *pfile;

	strcmp_funct = pack->ignorecase ? strcasecmp : strcmp;

	// Look for the slot we should put that file into (binary search)
	left = 0;
	right = pack->numfiles - 1;
	while (left <= right)
	{
		int diff;

		middle = (left + right) / 2;
		diff = strcmp_funct (pack->files[middle].name, name);

		// If we found the file, there's a problem
		if (!diff)
			Sys_Error ("Package %s contains the file %s several times\n",
					   pack->filename, name);

		// If we're too far in the list
		if (diff > 0)
			right = middle - 1;
		else
			left = middle + 1;
	}

	// We have to move the right of the list by one slot to free the one we need
	pfile = &pack->files[left];
	memmove (pfile + 1, pfile, (pack->numfiles - left) * sizeof (*pfile));
	pack->numfiles++;

	strlcpy (pfile->name, name, sizeof (pfile->name));
	pfile->offset = offset;
	pfile->packsize = packsize;
	pfile->realsize = realsize;
	pfile->flags = flags;

	return pfile;
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
	int packhandle;
	pack_t *pack;
	dpackfile_t *info;

	packhandle = open (packfile, O_RDONLY | O_BINARY);
	if (packhandle < 0)
		return NULL;
	read (packhandle, (void *)&header, sizeof(header));
	if (memcmp(header.id, "PACK", 4))
		Sys_Error ("%s is not a packfile", packfile);
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	if (header.dirlen % sizeof(dpackfile_t))
		Sys_Error ("%s has an invalid directory size", packfile);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error ("%s has %i files", packfile, numpackfiles);

	pack = Mem_Alloc(fs_mempool, sizeof (pack_t));
	pack->ignorecase = false; // PAK is case sensitive
	strlcpy (pack->filename, packfile, sizeof (pack->filename));
	pack->handle = packhandle;
	pack->numfiles = 0;
	pack->files = Mem_Alloc(fs_mempool, numpackfiles * sizeof(packfile_t));
	pack->next = packlist;
	packlist = pack;

	info = Mem_Alloc(tempmempool, sizeof(*info) * numpackfiles);
	lseek (packhandle, header.dirofs, SEEK_SET);
	read (packhandle, (void *)info, header.dirlen);

	// parse the directory
	for (i = 0;i < numpackfiles;i++)
	{
		size_t offset = LittleLong (info[i].filepos);
		size_t size = LittleLong (info[i].filelen);

		FS_AddFileToPack (info[i].name, pack, offset, size, size, PACKFILE_FLAG_TRUEOFFS);
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
void FS_AddGameDirectory (const char *dir)
{
	stringlist_t *list, *current;
	searchpath_t *search;
	pack_t *pak;
	char pakfile[MAX_OSPATH];

	strlcpy (fs_gamedir, dir, sizeof (fs_gamedir));

	list = listdirectory(dir);

	// add any PAK package in the directory
	for (current = list;current;current = current->next)
	{
		if (matchpattern(current->text, "*.pak", true))
		{
			dpsnprintf (pakfile, sizeof (pakfile), "%s/%s", dir, current->text);
			pak = FS_LoadPackPAK (pakfile);
			if (pak)
			{
				search = Mem_Alloc(fs_mempool, sizeof(searchpath_t));
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
			dpsnprintf (pakfile, sizeof (pakfile), "%s/%s", dir, current->text);
			pak = FS_LoadPackPK3 (pakfile);
			if (pak)
			{
				search = Mem_Alloc(fs_mempool, sizeof(searchpath_t));
				search->pack = pak;
				search->next = fs_searchpaths;
				fs_searchpaths = search;
			}
			else
				Con_Printf("unable to load pak \"%s\"\n", pakfile);
		}
	}
	freedirectory(list);

	// Add the directory to the search path
	// (unpacked files have the priority over packed files)
	search = Mem_Alloc(fs_mempool, sizeof(searchpath_t));
	strlcpy (search->filename, dir, sizeof (search->filename));
	search->next = fs_searchpaths;
	fs_searchpaths = search;
}


/*
================
FS_AddGameHierarchy
================
*/
void FS_AddGameHierarchy (const char *dir)
{
#ifndef WIN32
	const char *homedir;
#endif

	// Add the common game directory
	FS_AddGameDirectory (va("%s/%s", fs_basedir, dir));

#ifndef WIN32
	// Add the personal game directory
	homedir = getenv ("HOME");
	if (homedir != NULL && homedir[0] != '\0')
		FS_AddGameDirectory (va("%s/.%s/%s", homedir, gameuserdirname, dir));
#endif
}


/*
============
FS_FileExtension
============
*/
static const char *FS_FileExtension (const char *in)
{
	const char *separator, *backslash, *colon, *dot;

	separator = strrchr(in, '/');
	backslash = strrchr(in, '\\');
	if (separator < backslash)
		separator = backslash;
	colon = strrchr(in, ':');
	if (separator < colon)
		separator = colon;

	dot = strrchr(in, '.');
	if (dot == NULL || dot < separator)
		return "";

	return dot + 1;
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

	fs_mempool = Mem_AllocPool("file management", 0, NULL);

	strcpy(fs_basedir, ".");
	strcpy(fs_gamedir, ".");

	PK3_OpenLibrary ();

	// -basedir <path>
	// Overrides the system supplied base directory (under GAMENAME)
// COMMANDLINEOPTION: Filesystem: -basedir <path> chooses what base directory the game data is in, inside this there should be a data directory for the game (for example id1)
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
// COMMANDLINEOPTION: Filesystem: -path <path ..> specifies the full search path manually, overriding the generated one, example: -path c:\quake\id1 c:\quake\pak0.pak c:\quake\pak1.pak (not recommended)
	i = COM_CheckParm ("-path");
	if (i)
	{
		fs_modified = true;
		while (++i < com_argc)
		{
			if (!com_argv[i] || com_argv[i][0] == '+' || com_argv[i][0] == '-')
				break;

			search = Mem_Alloc(fs_mempool, sizeof(searchpath_t));
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

	// add the game-specific paths
	// gamedirname1 (typically id1)
	FS_AddGameHierarchy (gamedirname1);

	// add the game-specific path, if any
	if (gamedirname2)
	{
		fs_modified = true;
		FS_AddGameHierarchy (gamedirname2);
	}

	// set the com_modname (reported in server info)
	strlcpy(com_modname, gamedirname1, sizeof(com_modname));

	// -game <gamedir>
	// Adds basedir/gamedir as an override game
	// LordHavoc: now supports multiple -game directories
	for (i = 1;i < com_argc;i++)
	{
		if (!com_argv[i])
			continue;
		if (!strcmp (com_argv[i], "-game") && i < com_argc-1)
		{
			i++;
			fs_modified = true;
			FS_AddGameHierarchy (com_argv[i]);
			// update the com_modname
			strlcpy (com_modname, com_argv[i], sizeof (com_modname));
		}
	}

	// If "-condebug" is in the command line, remove the previous log file
	if (COM_CheckParm ("-condebug") != 0)
		unlink (va("%s/qconsole.log", fs_gamedir));
}

void FS_Init_Commands(void)
{
	Cvar_RegisterVariable (&scr_screenshot_name);

	Cmd_AddCommand ("path", FS_Path_f);
	Cmd_AddCommand ("dir", FS_Dir_f);
	Cmd_AddCommand ("ls", FS_Ls_f);

	// set the default screenshot name to either the mod name or the
	// gamemode screenshot name
	if (fs_modified)
		Cvar_SetQuick (&scr_screenshot_name, com_modname);
	else
		Cvar_SetQuick (&scr_screenshot_name, gamescreenshotname);
}

/*
================
FS_Shutdown
================
*/
void FS_Shutdown (void)
{
	Mem_FreePool (&fs_mempool);
}

/*
====================
FS_SysOpen

Internal function used to create a qfile_t and open the relevant non-packed file on disk
====================
*/
static qfile_t* FS_SysOpen (const char* filepath, const char* mode, qboolean nonblocking)
{
	qfile_t* file;
	int mod, opt;
	unsigned int ind;

	// Parse the mode string
	switch (mode[0])
	{
		case 'r':
			mod = O_RDONLY;
			opt = 0;
			break;
		case 'w':
			mod = O_WRONLY;
			opt = O_CREAT | O_TRUNC;
			break;
		case 'a':
			mod = O_WRONLY;
			opt = O_CREAT | O_APPEND;
			break;
		default:
			Con_Printf ("FS_SysOpen(%s, %s): invalid mode\n", filepath, mode);
			return NULL;
	}
	for (ind = 1; mode[ind] != '\0'; ind++)
	{
		switch (mode[ind])
		{
			case '+':
				mod = O_RDWR;
				break;
			case 'b':
				opt |= O_BINARY;
				break;
			default:
				Con_Printf ("FS_SysOpen(%s, %s): unknown character in mode (%c)\n",
							filepath, mode, mode[ind]);
		}
	}

#ifndef WIN32
	if (nonblocking)
		opt |= O_NONBLOCK;
#endif

	file = Mem_Alloc (fs_mempool, sizeof (*file));
	memset (file, 0, sizeof (*file));
	file->ungetc = EOF;

	file->handle = open (filepath, mod | opt, 0666);
	if (file->handle < 0)
	{
		Mem_Free (file);
		return NULL;
	}

	file->real_length = lseek (file->handle, 0, SEEK_END);

	// For files opened in append mode, we start at the end of the file
	if (mod & O_APPEND)
		file->position = file->real_length;
	else
		lseek (file->handle, 0, SEEK_SET);

	return file;
}


/*
===========
FS_OpenPackedFile

Open a packed file using its package file descriptor
===========
*/
qfile_t *FS_OpenPackedFile (pack_t* pack, int pack_ind)
{
	packfile_t *pfile;
	int dup_handle;
	qfile_t* file;

	pfile = &pack->files[pack_ind];

	// If we don't have the true offset, get it now
	if (! (pfile->flags & PACKFILE_FLAG_TRUEOFFS))
		PK3_GetTrueFileOffset (pfile, pack);

	// No Zlib DLL = no compressed files
	if (!zlib_dll && (pfile->flags & PACKFILE_FLAG_DEFLATED))
	{
		Con_Printf("WARNING: can't open the compressed file %s\n"
					"You need the Zlib DLL to use compressed files\n",
					pfile->name);
		fs_filesize = -1;
		return NULL;
	}

	dup_handle = dup (pack->handle);
	if (dup_handle < 0)
		Sys_Error ("FS_OpenPackedFile: can't dup package's handle (pack: %s)", pack->filename);

	file = Mem_Alloc (fs_mempool, sizeof (*file));
	memset (file, 0, sizeof (*file));
	file->handle = dup_handle;
	file->flags = QFILE_FLAG_PACKED;
	file->real_length = pfile->realsize;
	file->offset = pfile->offset;
	file->position = 0;
	file->ungetc = EOF;

	if (lseek (file->handle, file->offset, SEEK_SET) == -1)
		Sys_Error ("FS_OpenPackedFile: can't lseek to %s in %s (offset: %d)",
					pfile->name, pack->filename, file->offset);

	if (pfile->flags & PACKFILE_FLAG_DEFLATED)
	{
		ztoolkit_t *ztk;

		file->flags |= QFILE_FLAG_DEFLATED;

		// We need some more variables
		ztk = Mem_Alloc (fs_mempool, sizeof (*ztk));

		ztk->comp_length = pfile->packsize;

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
			Sys_Error ("FS_OpenPackedFile: inflate init error (file: %s)", pfile->name);

		ztk->zstream.next_out = file->buff;
		ztk->zstream.avail_out = sizeof (file->buff);

		file->ztk = ztk;
	}

	fs_filesize = pfile->realsize;

	return file;
}

/*
====================
FS_CheckNastyPath

Return true if the path should be rejected due to one of the following:
1: path elements that are non-portable
2: path elements that would allow access to files outside the game directory,
   or are just not a good idea for a mod to be using.
====================
*/
int FS_CheckNastyPath (const char *path)
{
	// Windows: don't allow \ in filenames (windows-only), period.
	// (on Windows \ is a directory separator, but / is also supported)
	if (strstr(path, "\\"))
		return 1; // non-portable

	// Mac: don't allow Mac-only filenames - : is a directory separator
	// instead of /, but we rely on / working already, so there's no reason to
	// support a Mac-only path
	// Amiga and Windows: : tries to go to root of drive
	if (strstr(path, ":"))
		return 1; // non-portable attempt to go to root of drive

	// Amiga: // is parent directory
	if (strstr(path, "//"))
		return 1; // non-portable attempt to go to parent directory

	// all: don't allow going to current directory (./) or parent directory (../ or /../)
	if (strstr(path, "./"))
		return 2; // attempt to go outside the game directory

	// Windows and UNIXes: don't allow absolute paths
	if (path[0] == '/')
		return 2; // attempt to go outside the game directory

	// after all these checks we're pretty sure it's a / separated filename
	// and won't do much if any harm
	return false;
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

	// search through the path, one element at a time
	for (search = fs_searchpaths;search;search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			int (*strcmp_funct) (const char* str1, const char* str2);
			int left, right, middle;

			pak = search->pack;
			strcmp_funct = pak->ignorecase ? strcasecmp : strcmp;

			// Look for the file (binary search)
			left = 0;
			right = pak->numfiles - 1;
			while (left <= right)
			{
				int diff;

				middle = (left + right) / 2;
				diff = strcmp_funct (pak->files[middle].name, name);

				// Found it
				if (!diff)
				{
					if (!quiet)
						Con_DPrintf("FS_FindFile: %s in %s\n",
									pak->files[middle].name, pak->filename);

					if (index != NULL)
						*index = middle;
					return search;
				}

				// If we're too far in the list
				if (diff > 0)
					right = middle - 1;
				else
					left = middle + 1;
			}
		}
		else
		{
			char netpath[MAX_OSPATH];
			dpsnprintf(netpath, sizeof(netpath), "%s/%s", search->filename, name);
			if (FS_SysFileExists (netpath))
			{
				if (!quiet)
					Con_DPrintf("FS_FindFile: %s\n", netpath);

				if (index != NULL)
					*index = -1;
				return search;
			}
		}
	}

	if (!quiet)
		Con_DPrintf("FS_FindFile: can't find %s\n", name);

	if (index != NULL)
		*index = -1;
	return NULL;
}


/*
===========
FS_OpenReadFile

Look for a file in the search paths and open it in read-only mode

Sets fs_filesize
===========
*/
qfile_t *FS_OpenReadFile (const char *filename, qboolean quiet, qboolean nonblocking)
{
	searchpath_t *search;
	int pack_ind;

	search = FS_FindFile (filename, &pack_ind, quiet);

	// Not found?
	if (search == NULL)
	{
		fs_filesize = -1;
		return NULL;
	}

	// Found in the filesystem?
	if (pack_ind < 0)
	{
		char path [MAX_OSPATH];
		dpsnprintf (path, sizeof (path), "%s/%s", search->filename, filename);
		return FS_SysOpen (path, "rb", nonblocking);
	}

	// So, we found it in a package...
	return FS_OpenPackedFile (search->pack, pack_ind);
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
qfile_t* FS_Open (const char* filepath, const char* mode, qboolean quiet, qboolean nonblocking)
{
	qfile_t* file;

	if (FS_CheckNastyPath(filepath))
	{
		Con_Printf("FS_Open(\"%s\", \"%s\", %s): nasty filename rejected\n", filepath, mode, quiet ? "true" : "false");
		return NULL;
	}

	// If the file is opened in "write", "append", or "read/write" mode
	if (mode[0] == 'w' || mode[0] == 'a' || strchr (mode, '+'))
	{
		char real_path [MAX_OSPATH];

		// Open the file on disk directly
		dpsnprintf (real_path, sizeof (real_path), "%s/%s", fs_gamedir, filepath);

		// Create directories up to the file
		FS_CreatePath (real_path);

		return FS_SysOpen (real_path, mode, nonblocking);
	}

	// Else, we look at the various search paths and open the file in read-only mode
	file = FS_OpenReadFile (filepath, quiet, nonblocking);
	if (file != NULL)
		fs_filesize = file->real_length;

	return file;
}


/*
====================
FS_Close

Close a file
====================
*/
int FS_Close (qfile_t* file)
{
	if (close (file->handle))
		return EOF;

	if (file->ztk)
	{
		qz_inflateEnd (&file->ztk->zstream);
		Mem_Free (file->ztk);
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
	ssize_t result;

	// If necessary, seek to the exact file position we're supposed to be
	if (file->buff_ind != file->buff_len)
		lseek (file->handle, file->buff_ind - file->buff_len, SEEK_CUR);

	// Purge cached data
	FS_Purge (file);

	// Write the buffer and update the position
	result = write (file->handle, data, datasize);
	file->position = lseek (file->handle, 0, SEEK_CUR);
	if (file->real_length < file->position)
		file->real_length = file->position;

	if (result < 0)
		return 0;

	return result;
}


/*
====================
FS_Read

Read up to "buffersize" bytes from a file
====================
*/
size_t FS_Read (qfile_t* file, void* buffer, size_t buffersize)
{
	size_t count, done;

	if (buffersize == 0)
		return 0;

	// Get rid of the ungetc character
	if (file->ungetc != EOF)
	{
		((char*)buffer)[0] = file->ungetc;
		buffersize--;
		file->ungetc = EOF;
		done = 1;
	}
	else
		done = 0;

	// First, we copy as many bytes as we can from "buff"
	if (file->buff_ind < file->buff_len)
	{
		count = file->buff_len - file->buff_ind;

		done += (buffersize > count) ? count : buffersize;
		memcpy (buffer, &file->buff[file->buff_ind], done);
		file->buff_ind += done;

		buffersize -= done;
		if (buffersize == 0)
			return done;
	}

	// NOTE: at this point, the read buffer is always empty

	// If the file isn't compressed
	if (! (file->flags & QFILE_FLAG_DEFLATED))
	{
		int nb;

		// We must take care to not read after the end of the file
		count = file->real_length - file->position;

		// If we have a lot of data to get, put them directly into "buffer"
		if (buffersize > sizeof (file->buff) / 2)
		{
			if (count > buffersize)
				count = buffersize;
			lseek (file->handle, file->offset + file->position, SEEK_SET);
			nb = read (file->handle, &((qbyte*)buffer)[done], count);
			if (nb > 0)
			{
				done += nb;
				file->position += nb;

				// Purge cached data
				FS_Purge (file);
			}
		}
		else
		{
			if (count > sizeof (file->buff))
				count = sizeof (file->buff);
			lseek (file->handle, file->offset + file->position, SEEK_SET);
			nb = read (file->handle, file->buff, count);
			if (nb > 0)
			{
				file->buff_len = nb;
				file->position += nb;

				// Copy the requested data in "buffer" (as much as we can)
				count = (buffersize > file->buff_len) ? file->buff_len : buffersize;
				memcpy (&((qbyte*)buffer)[done], file->buff, count);
				file->buff_ind = count;
				done += count;
			}
		}

		return done;
	}

	// If the file is compressed, it's more complicated...
	// We cycle through a few operations until we have read enough data
	while (buffersize > 0)
	{
		ztoolkit_t *ztk = file->ztk;
		int error;

		// NOTE: at this point, the read buffer is always empty

		// If "input" is also empty, we need to refill it
		if (ztk->in_ind == ztk->in_len)
		{
			// If we are at the end of the file
			if (file->position == file->real_length)
				return done;

			count = ztk->comp_length - ztk->in_position;
			if (count > sizeof (ztk->input))
				count = sizeof (ztk->input);
			lseek (file->handle, file->offset + ztk->in_position, SEEK_SET);
			if (read (file->handle, ztk->input, count) != (ssize_t)count)
				Sys_Error ("FS_Read: unexpected end of file");

			ztk->in_ind = 0;
			ztk->in_len = count;
			ztk->in_position += count;
		}

		ztk->zstream.next_in = &ztk->input[ztk->in_ind];
		ztk->zstream.avail_in = ztk->in_len - ztk->in_ind;

		// Now that we are sure we have compressed data available, we need to determine
		// if it's better to inflate it in "file->buff" or directly in "buffer"

		// Inflate the data in "file->buff"
		if (buffersize < sizeof (file->buff) / 2)
		{
			ztk->zstream.next_out = file->buff;
			ztk->zstream.avail_out = sizeof (file->buff);
			error = qz_inflate (&ztk->zstream, Z_SYNC_FLUSH);
			if (error != Z_OK && error != Z_STREAM_END)
				Sys_Error ("Can't inflate file");
			ztk->in_ind = ztk->in_len - ztk->zstream.avail_in;

			file->buff_len = sizeof (file->buff) - ztk->zstream.avail_out;
			file->position += file->buff_len;

			// Copy the requested data in "buffer" (as much as we can)
			count = (buffersize > file->buff_len) ? file->buff_len : buffersize;
			memcpy (&((qbyte*)buffer)[done], file->buff, count);
			file->buff_ind = count;
		}

		// Else, we inflate directly in "buffer"
		else
		{
			ztk->zstream.next_out = &((qbyte*)buffer)[done];
			ztk->zstream.avail_out = buffersize;
			error = qz_inflate (&ztk->zstream, Z_SYNC_FLUSH);
			if (error != Z_OK && error != Z_STREAM_END)
				Sys_Error ("Can't inflate file");
			ztk->in_ind = ztk->in_len - ztk->zstream.avail_in;

			// How much data did it inflate?
			count = buffersize - ztk->zstream.avail_out;
			file->position += count;

			// Purge cached data
			FS_Purge (file);
		}

		done += count;
		buffersize -= count;
	}

	return done;
}


/*
====================
FS_Print

Print a string into a file
====================
*/
int FS_Print (qfile_t* file, const char *msg)
{
	return FS_Write (file, msg, strlen (msg));
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
	result = FS_VPrintf (file, format, args);
	va_end (args);

	return result;
}


/*
====================
FS_VPrintf

Print a string into a file
====================
*/
int FS_VPrintf (qfile_t* file, const char* format, va_list ap)
{
	int len;
	size_t buff_size;
	char *tempbuff = NULL;

	buff_size = 1024;
	tempbuff = Mem_Alloc (tempmempool, buff_size);
	len = dpvsnprintf (tempbuff, buff_size, format, ap);
	while (len < 0)
	{
		Mem_Free (tempbuff);
		buff_size *= 2;
		tempbuff = Mem_Alloc (tempmempool, buff_size);
		len = dpvsnprintf (tempbuff, buff_size, format, ap);
	}

	len = write (file->handle, tempbuff, len);
	Mem_Free (tempbuff);

	return len;
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
FS_UnGetc

Put a character back into the read buffer (only supports one character!)
====================
*/
int FS_UnGetc (qfile_t* file, unsigned char c)
{
	// If there's already a character waiting to be read
	if (file->ungetc != EOF)
		return EOF;

	file->ungetc = c;
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
	ztoolkit_t *ztk;
	qbyte* buffer;
	size_t buffersize;

	// Compute the file offset
	switch (whence)
	{
		case SEEK_CUR:
			offset += file->position - file->buff_len + file->buff_ind;
			break;

		case SEEK_SET:
			break;

		case SEEK_END:
			offset += file->real_length;
			break;

		default:
			return -1;
	}
	if (offset < 0 || offset > (long) file->real_length)
		return -1;

	// If we have the data in our read buffer, we don't need to actually seek
	if (file->position - file->buff_len <= (size_t)offset &&
		(size_t)offset <= file->position)
	{
		file->buff_ind = offset + file->buff_len - file->position;
		return 0;
	}

	// Purge cached data
	FS_Purge (file);

	// Unpacked or uncompressed files can seek directly
	if (! (file->flags & QFILE_FLAG_DEFLATED))
	{
		if (lseek (file->handle, file->offset + offset, SEEK_SET) == -1)
			return -1;
		file->position = offset;
		return 0;
	}

	// Seeking in compressed files is more a hack than anything else,
	// but we need to support it, so here we go.
	ztk = file->ztk;

	// If we have to go back in the file, we need to restart from the beginning
	if ((size_t)offset <= file->position)
	{
		ztk->in_ind = 0;
		ztk->in_len = 0;
		ztk->in_position = 0;
		file->position = 0;
		lseek (file->handle, file->offset, SEEK_SET);

		// Reset the Zlib stream
		ztk->zstream.next_in = ztk->input;
		ztk->zstream.avail_in = 0;
		qz_inflateReset (&ztk->zstream);
	}

	// We need a big buffer to force inflating into it directly
	buffersize = 2 * sizeof (file->buff);
	buffer = Mem_Alloc (tempmempool, buffersize);

	// Skip all data until we reach the requested offset
	while ((size_t)offset > file->position)
	{
		size_t diff = offset - file->position;
		size_t count, len;

		count = (diff > buffersize) ? buffersize : diff;
		len = FS_Read (file, buffer, count);
		if (len != count)
		{
			Mem_Free (buffer);
			return -1;
		}
	}

	Mem_Free (buffer);
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
	return file->position - file->buff_len + file->buff_ind;
}


/*
====================
FS_Purge

Erases any buffered input or output data
====================
*/
void FS_Purge (qfile_t* file)
{
	file->buff_len = 0;
	file->buff_ind = 0;
	file->ungetc = EOF;
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
	qfile_t *file;
	qbyte *buf;

	file = FS_Open (path, "rb", quiet, false);
	if (!file)
		return NULL;

	buf = Mem_Alloc (pool, fs_filesize + 1);
	buf[fs_filesize] = '\0';

	FS_Read (file, buf, fs_filesize);
	FS_Close (file);

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
	qfile_t *file;

	file = FS_Open (filename, "wb", false, false);
	if (!file)
	{
		Con_Printf("FS_WriteFile: failed on %s\n", filename);
		return false;
	}

	Con_DPrintf("FS_WriteFile: %s\n", filename);
	FS_Write (file, data, len);
	FS_Close (file);
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
	int desc;

	// TODO: use another function instead, to avoid opening the file
	desc = open (path, O_RDONLY | O_BINARY);
	if (desc < 0)
		return false;

	close (desc);
	return true;
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
								Con_DPrintf("SearchPackFile: %s : %s\n", pak->filename, temp);
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
			dpsnprintf(netpath, sizeof (netpath), "%s/%s", searchpath->filename, basepath);
			if ((dir = listdirectory(netpath)))
			{
				for (dirfile = dir;dirfile;dirfile = dirfile->next)
				{
					dpsnprintf(temp, sizeof(temp), "%s/%s", basepath, dirfile->text);
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
								Con_DPrintf("SearchDirFile: %s\n", temp);
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

