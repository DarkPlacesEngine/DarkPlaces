/*
	DarkPlaces file system

	Copyright (C) 2003-2005 Mathieu Olivier

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

#ifndef FS_H
#define FS_H


// ------ Types ------ //

typedef struct qfile_s qfile_t;

#ifdef WIN32
//typedef long fs_offset_t; // 32bit
typedef __int64 fs_offset_t; ///< 64bit (lots of warnings, and read/write still don't take 64bit on win64)
#else
typedef long long fs_offset_t;
#endif



// ------ Variables ------ //

extern char fs_gamedir [MAX_OSPATH];
extern char fs_basedir [MAX_OSPATH];
extern char fs_userdir [MAX_OSPATH];

// list of active game directories (empty if not running a mod)
#define MAX_GAMEDIRS 16
extern int fs_numgamedirs;
extern char fs_gamedirs[MAX_GAMEDIRS][MAX_QPATH];


// ------ Main functions ------ //

// IMPORTANT: the file path is automatically prefixed by the current game directory for
// each file created by FS_WriteFile, or opened in "write" or "append" mode by FS_OpenRealFile

qboolean FS_AddPack(const char *pakfile, qboolean *already_loaded, qboolean keep_plain_dirs); // already_loaded may be NULL if caller does not care
const char *FS_WhichPack(const char *filename);
void FS_CreatePath (char *path);
int FS_SysOpenFD(const char *filepath, const char *mode, qboolean nonblocking); // uses absolute path
qfile_t* FS_SysOpen (const char* filepath, const char* mode, qboolean nonblocking); // uses absolute path
qfile_t* FS_OpenRealFile (const char* filepath, const char* mode, qboolean quiet);
qfile_t* FS_OpenVirtualFile (const char* filepath, qboolean quiet);
qfile_t* FS_FileFromData (const unsigned char *data, const size_t size, qboolean quiet);
int FS_Close (qfile_t* file);
void FS_RemoveOnClose(qfile_t* file);
fs_offset_t FS_Write (qfile_t* file, const void* data, size_t datasize);
fs_offset_t FS_Read (qfile_t* file, void* buffer, size_t buffersize);
int FS_Print(qfile_t* file, const char *msg);
int FS_Printf(qfile_t* file, const char* format, ...) DP_FUNC_PRINTF(2);
int FS_VPrintf(qfile_t* file, const char* format, va_list ap);
int FS_Getc (qfile_t* file);
int FS_UnGetc (qfile_t* file, unsigned char c);
int FS_Seek (qfile_t* file, fs_offset_t offset, int whence);
fs_offset_t FS_Tell (qfile_t* file);
fs_offset_t FS_FileSize (qfile_t* file);
void FS_Purge (qfile_t* file);
const char *FS_FileWithoutPath (const char *in);
const char *FS_FileExtension (const char *in);
int FS_CheckNastyPath (const char *path, qboolean isgamedir);

extern const char *const fs_checkgamedir_missing; // "(missing)"
const char *FS_CheckGameDir(const char *gamedir); // returns NULL if nasty, fs_checkgamedir_missing (exact pointer) if missing

typedef struct
{
	char name[MAX_OSPATH];
	char description[8192];
}
gamedir_t;
extern gamedir_t *fs_all_gamedirs; // terminated by entry with empty name
extern int fs_all_gamedirs_count;

qboolean FS_ChangeGameDirs(int numgamedirs, char gamedirs[][MAX_QPATH], qboolean complain, qboolean failmissing);
qboolean FS_IsRegisteredQuakePack(const char *name);
int FS_CRCFile(const char *filename, size_t *filesizepointer);
void FS_Rescan(void);

typedef struct fssearch_s
{
	int numfilenames;
	char **filenames;
	// array of filenames
	char *filenamesbuffer;
}
fssearch_t;

fssearch_t *FS_Search(const char *pattern, int caseinsensitive, int quiet);
void FS_FreeSearch(fssearch_t *search);

unsigned char *FS_LoadFile (const char *path, mempool_t *pool, qboolean quiet, fs_offset_t *filesizepointer);
qboolean FS_WriteFileInBlocks (const char *filename, const void *const *data, const fs_offset_t *len, size_t count);
qboolean FS_WriteFile (const char *filename, const void *data, fs_offset_t len);


// ------ Other functions ------ //

void FS_StripExtension (const char *in, char *out, size_t size_out);
void FS_DefaultExtension (char *path, const char *extension, size_t size_path);

#define FS_FILETYPE_NONE 0
#define FS_FILETYPE_FILE 1
#define FS_FILETYPE_DIRECTORY 2
int FS_FileType (const char *filename);		// the file can be into a package
int FS_SysFileType (const char *filename);		// only look for files outside of packages

qboolean FS_FileExists (const char *filename);		// the file can be into a package
qboolean FS_SysFileExists (const char *filename);	// only look for files outside of packages

void FS_mkdir (const char *path);

unsigned char *FS_Deflate(const unsigned char *data, size_t size, size_t *deflated_size, int level, mempool_t *mempool);
unsigned char *FS_Inflate(const unsigned char *data, size_t size, size_t *inflated_size, mempool_t *mempool);

qboolean FS_HasZlib(void);

#endif
