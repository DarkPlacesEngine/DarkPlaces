#include "quakedef.h"
#include "fs.h"
#include "libcurl.h"

static cvar_t cl_curl_maxdownloads = {CVAR_SAVE, "cl_curl_maxdownloads","1", "maximum number of concurrent HTTP/FTP downloads"};
static cvar_t cl_curl_maxspeed = {CVAR_SAVE, "cl_curl_maxspeed","100", "maximum download speed (KiB/s)"};
static cvar_t sv_curl_defaulturl = {CVAR_SAVE, "sv_curl_defaulturl","", "default autodownload source URL"};
static cvar_t sv_curl_serverpackages = {CVAR_SAVE, "sv_curl_serverpackages","", "list of required files for the clients, separated by spaces"};
static cvar_t cl_curl_enabled = {CVAR_SAVE, "cl_curl_enabled","1", "whether client's download support is enabled"};

/*
=================================================================

  Minimal set of definitions from libcurl

  WARNING: for a matter of simplicity, several pointer types are
  casted to "void*", and most enumerated values are not included

=================================================================
*/

typedef struct CURL_s CURL;
typedef struct CURLM_s CURLM;
typedef enum
{
	CURLE_OK = 0
}
CURLcode;
typedef enum
{
	CURLM_CALL_MULTI_PERFORM=-1, /* please call curl_multi_perform() soon */
	CURLM_OK = 0
}
CURLMcode;
#define CURL_GLOBAL_NOTHING 0
#define CURL_GLOBAL_SSL 1
#define CURL_GLOBAL_WIN32 2
#define CURLOPTTYPE_LONG          0
#define CURLOPTTYPE_OBJECTPOINT   10000
#define CURLOPTTYPE_FUNCTIONPOINT 20000
#define CURLOPTTYPE_OFF_T         30000
#define CINIT(name,type,number) CURLOPT_ ## name = CURLOPTTYPE_ ## type + number
typedef enum
{
	CINIT(WRITEDATA, OBJECTPOINT, 1),
	CINIT(URL,  OBJECTPOINT, 2),
	CINIT(ERRORBUFFER, OBJECTPOINT, 10),
	CINIT(WRITEFUNCTION, FUNCTIONPOINT, 11),
	CINIT(REFERER, OBJECTPOINT, 16),
	CINIT(USERAGENT, OBJECTPOINT, 18),
	CINIT(RESUME_FROM, LONG, 21),
	CINIT(FOLLOWLOCATION, LONG, 52),  /* use Location: Luke! */
	CINIT(PRIVATE, OBJECTPOINT, 103),
	CINIT(LOW_SPEED_LIMIT, LONG , 19),
	CINIT(LOW_SPEED_TIME, LONG, 20),
}
CURLoption;
typedef enum
{
	CURLINFO_TEXT = 0,
	CURLINFO_HEADER_IN,    /* 1 */
	CURLINFO_HEADER_OUT,   /* 2 */
	CURLINFO_DATA_IN,      /* 3 */
	CURLINFO_DATA_OUT,     /* 4 */
	CURLINFO_SSL_DATA_IN,  /* 5 */
	CURLINFO_SSL_DATA_OUT, /* 6 */
	CURLINFO_END
}
curl_infotype;
#define CURLINFO_STRING   0x100000
#define CURLINFO_LONG     0x200000
#define CURLINFO_DOUBLE   0x300000
#define CURLINFO_SLIST    0x400000
#define CURLINFO_MASK     0x0fffff
#define CURLINFO_TYPEMASK 0xf00000
typedef enum
{
	CURLINFO_NONE, /* first, never use this */
	CURLINFO_EFFECTIVE_URL    = CURLINFO_STRING + 1,
	CURLINFO_RESPONSE_CODE    = CURLINFO_LONG   + 2,
	CURLINFO_TOTAL_TIME       = CURLINFO_DOUBLE + 3,
	CURLINFO_NAMELOOKUP_TIME  = CURLINFO_DOUBLE + 4,
	CURLINFO_CONNECT_TIME     = CURLINFO_DOUBLE + 5,
	CURLINFO_PRETRANSFER_TIME = CURLINFO_DOUBLE + 6,
	CURLINFO_SIZE_UPLOAD      = CURLINFO_DOUBLE + 7,
	CURLINFO_SIZE_DOWNLOAD    = CURLINFO_DOUBLE + 8,
	CURLINFO_SPEED_DOWNLOAD   = CURLINFO_DOUBLE + 9,
	CURLINFO_SPEED_UPLOAD     = CURLINFO_DOUBLE + 10,
	CURLINFO_HEADER_SIZE      = CURLINFO_LONG   + 11,
	CURLINFO_REQUEST_SIZE     = CURLINFO_LONG   + 12,
	CURLINFO_SSL_VERIFYRESULT = CURLINFO_LONG   + 13,
	CURLINFO_FILETIME         = CURLINFO_LONG   + 14,
	CURLINFO_CONTENT_LENGTH_DOWNLOAD   = CURLINFO_DOUBLE + 15,
	CURLINFO_CONTENT_LENGTH_UPLOAD     = CURLINFO_DOUBLE + 16,
	CURLINFO_STARTTRANSFER_TIME = CURLINFO_DOUBLE + 17,
	CURLINFO_CONTENT_TYPE     = CURLINFO_STRING + 18,
	CURLINFO_REDIRECT_TIME    = CURLINFO_DOUBLE + 19,
	CURLINFO_REDIRECT_COUNT   = CURLINFO_LONG   + 20,
	CURLINFO_PRIVATE          = CURLINFO_STRING + 21,
	CURLINFO_HTTP_CONNECTCODE = CURLINFO_LONG   + 22,
	CURLINFO_HTTPAUTH_AVAIL   = CURLINFO_LONG   + 23,
	CURLINFO_PROXYAUTH_AVAIL  = CURLINFO_LONG   + 24,
	CURLINFO_OS_ERRNO         = CURLINFO_LONG   + 25,
	CURLINFO_NUM_CONNECTS     = CURLINFO_LONG   + 26,
	CURLINFO_SSL_ENGINES      = CURLINFO_SLIST  + 27,
}
CURLINFO;

typedef enum
{
	CURLMSG_NONE, /* first, not used */
	CURLMSG_DONE, /* This easy handle has completed. 'result' contains
					 the CURLcode of the transfer */
	CURLMSG_LAST
}
CURLMSG;
typedef struct
{
	CURLMSG msg;       /* what this message means */
	CURL *easy_handle; /* the handle it concerns */
	union
	{
		void *whatever;    /* message-specific data */
		CURLcode result;   /* return code for transfer */
	}
	data;
}
CURLMsg;

static void (*qcurl_global_init) (long flags);
static void (*qcurl_global_cleanup) (void);

static CURL * (*qcurl_easy_init) (void);
static void (*qcurl_easy_cleanup) (CURL *handle);
static CURLcode (*qcurl_easy_setopt) (CURL *handle, CURLoption option, ...);
static CURLcode (*qcurl_easy_getinfo) (CURL *handle, CURLINFO info, ...);
static const char * (*qcurl_easy_strerror) (CURLcode);

static CURLM * (*qcurl_multi_init) (void);
static CURLMcode (*qcurl_multi_perform) (CURLM *multi_handle, int *running_handles);
static CURLMcode (*qcurl_multi_add_handle) (CURLM *multi_handle, CURL *easy_handle);
static CURLMcode (*qcurl_multi_remove_handle) (CURLM *multi_handle, CURL *easy_handle);
static CURLMsg * (*qcurl_multi_info_read) (CURLM *multi_handle, int *msgs_in_queue);
static void (*qcurl_multi_cleanup) (CURLM *);
static const char * (*qcurl_multi_strerror) (CURLcode);

static dllfunction_t curlfuncs[] =
{
	{"curl_global_init",		(void **) &qcurl_global_init},
	{"curl_global_cleanup",		(void **) &qcurl_global_cleanup},
	{"curl_easy_init",			(void **) &qcurl_easy_init},
	{"curl_easy_cleanup",		(void **) &qcurl_easy_cleanup},
	{"curl_easy_setopt",		(void **) &qcurl_easy_setopt},
	{"curl_easy_strerror",		(void **) &qcurl_easy_strerror},
	{"curl_easy_getinfo",		(void **) &qcurl_easy_getinfo},
	{"curl_multi_init",			(void **) &qcurl_multi_init},
	{"curl_multi_perform",		(void **) &qcurl_multi_perform},
	{"curl_multi_add_handle",	(void **) &qcurl_multi_add_handle},
	{"curl_multi_remove_handle",(void **) &qcurl_multi_remove_handle},
	{"curl_multi_info_read",	(void **) &qcurl_multi_info_read},
	{"curl_multi_cleanup",		(void **) &qcurl_multi_cleanup},
	{"curl_multi_strerror",		(void **) &qcurl_multi_strerror},
	{NULL, NULL}
};

// Handle for CURL DLL
static dllhandle_t curl_dll = NULL;
// will be checked at many places to find out if qcurl calls are allowed

typedef struct downloadinfo_s
{
	char filename[MAX_QPATH];
	char url[1024];
	char referer[256];
	qfile_t *stream;
	fs_offset_t startpos;
	CURL *curle;
	qboolean started;
	qboolean ispak;
	unsigned long bytes_received;
	struct downloadinfo_s *next, *prev;
	qboolean forthismap;

	unsigned char *buffer;
	size_t buffersize;
	curl_callback_t callback;
	void *callback_data;
}
downloadinfo;
static downloadinfo *downloads = NULL;
static int numdownloads = 0;

static qboolean noclear = FALSE;

static int numdownloads_fail = 0;
static int numdownloads_success = 0;
static int numdownloads_added = 0;
static char command_when_done[256] = "";
static char command_when_error[256] = "";

/*
====================
Curl_CommandWhenDone

Sets the command which is to be executed when the last download completes AND
all downloads since last server connect ended with a successful status.
Setting the command to NULL clears it.
====================
*/
void Curl_CommandWhenDone(const char *cmd)
{
	if(!curl_dll)
		return;
	if(cmd)
		strlcpy(command_when_done, cmd, sizeof(command_when_done));
	else
		*command_when_done = 0;
}

/*
FIXME
Do not use yet. Not complete.
Problem: what counts as an error?
*/

void Curl_CommandWhenError(const char *cmd)
{
	if(!curl_dll)
		return;
	if(cmd)
		strlcpy(command_when_error, cmd, sizeof(command_when_error));
	else
		*command_when_error = 0;
}

/*
====================
Curl_Clear_forthismap

Clears the "will disconnect on failure" flags.
====================
*/
void Curl_Clear_forthismap(void)
{
	downloadinfo *di;
	if(noclear)
		return;
	for(di = downloads; di; di = di->next)
		di->forthismap = false;
	Curl_CommandWhenError(NULL);
	Curl_CommandWhenDone(NULL);
	numdownloads_fail = 0;
	numdownloads_success = 0;
	numdownloads_added = 0;
}

/*
====================
Curl_Have_forthismap

Returns true if a download needed for the current game is running.
====================
*/
qboolean Curl_Have_forthismap(void)
{
	return numdownloads_added;
}

void Curl_Register_predownload(void)
{
	Curl_CommandWhenDone("cl_begindownloads");
	Curl_CommandWhenError("cl_begindownloads");
}

/*
====================
Curl_CheckCommandWhenDone

Checks if a "done command" is to be executed.
All downloads finished, at least one success since connect, no single failure
-> execute the command.
*/
static void Curl_CheckCommandWhenDone(void)
{
	if(!curl_dll)
		return;
	if(numdownloads_added && (numdownloads_success == numdownloads_added) && *command_when_done)
	{
		Con_DPrintf("cURL downloads occurred, executing %s\n", command_when_done);
		Cbuf_AddText("\n");
		Cbuf_AddText(command_when_done);
		Cbuf_AddText("\n");
		Curl_Clear_forthismap();
	}
	else if(numdownloads_added && numdownloads_fail && *command_when_error)
	{
		Con_DPrintf("cURL downloads FAILED, executing %s\n", command_when_error);
		Cbuf_AddText("\n");
		Cbuf_AddText(command_when_error);
		Cbuf_AddText("\n");
		Curl_Clear_forthismap();
	}
}

/*
====================
CURL_CloseLibrary

Load the cURL DLL
====================
*/
static qboolean CURL_OpenLibrary (void)
{
	const char* dllnames [] =
	{
#if defined(WIN64)
		"libcurl64.dll",
#elif defined(WIN32)
		"libcurl-4.dll",
		"libcurl-3.dll",
#elif defined(MACOSX)
		"libcurl.4.dylib", // Mac OS X Notyetreleased
		"libcurl.3.dylib", // Mac OS X Tiger
		"libcurl.2.dylib", // Mac OS X Panther
#else
		"libcurl.so.4",
		"libcurl.so.3",
		"libcurl.so", // FreeBSD
#endif
		NULL
	};

	// Already loaded?
	if (curl_dll)
		return true;

	// Load the DLL
	return Sys_LoadLibrary (dllnames, &curl_dll, curlfuncs);
}


/*
====================
CURL_CloseLibrary

Unload the cURL DLL
====================
*/
static void CURL_CloseLibrary (void)
{
	Sys_UnloadLibrary (&curl_dll);
}


static CURLM *curlm = NULL;
static unsigned long bytes_received = 0; // used for bandwidth throttling
static double curltime = 0;

/*
====================
CURL_fwrite

fwrite-compatible function that writes the data to a file. libcurl can call
this.
====================
*/
static size_t CURL_fwrite(void *data, size_t size, size_t nmemb, void *vdi)
{
	fs_offset_t ret = -1;
	size_t bytes = size * nmemb;
	downloadinfo *di = (downloadinfo *) vdi;

	if(di->buffer)
	{
		if(di->bytes_received + bytes <= di->buffersize)
		{
			memcpy(di->buffer + di->bytes_received, data, bytes);
			ret = bytes;
		}
		// otherwise: buffer overrun, ret stays -1
	}

	if(di->stream)
	{
		ret = FS_Write(di->stream, data, bytes);
	}

	bytes_received += bytes;
	di->bytes_received += bytes;

	return ret; // why not ret / nmemb?
}

typedef enum
{
	CURL_DOWNLOAD_SUCCESS = 0,
	CURL_DOWNLOAD_FAILED,
	CURL_DOWNLOAD_ABORTED,
	CURL_DOWNLOAD_SERVERERROR
}
CurlStatus;

static void curl_default_callback(int status, size_t length_received, unsigned char *buffer, void *cbdata)
{
	downloadinfo *di = (downloadinfo *) cbdata;
	switch(status)
	{
		case CURLCBSTATUS_OK:
			Con_Printf("Download of %s: OK\n", di->filename);
			break;
		case CURLCBSTATUS_FAILED:
			Con_Printf("Download of %s: FAILED\n", di->filename);
			break;
		case CURLCBSTATUS_ABORTED:
			Con_Printf("Download of %s: ABORTED\n", di->filename);
			break;
		case CURLCBSTATUS_SERVERERROR:
			Con_Printf("Download of %s: (unknown server error)\n", di->filename);
			break;
		case CURLCBSTATUS_UNKNOWN:
			Con_Printf("Download of %s: (unknown client error)\n", di->filename);
			break;
		default:
			Con_Printf("Download of %s: %d\n", di->filename, status);
			break;
	}
}

static void curl_quiet_callback(int status, size_t length_received, unsigned char *buffer, void *cbdata)
{
	if(developer.integer)
		curl_default_callback(status, length_received, buffer, cbdata);
}

/*
====================
Curl_EndDownload

stops a download. It receives a status (CURL_DOWNLOAD_SUCCESS,
CURL_DOWNLOAD_FAILED or CURL_DOWNLOAD_ABORTED) and in the second case the error
code from libcurl, or 0, if another error has occurred.
====================
*/
static qboolean Curl_Begin(const char *URL, const char *name, qboolean ispak, qboolean forthismap, unsigned char *buf, size_t bufsize, curl_callback_t callback, void *cbdata);
static void Curl_EndDownload(downloadinfo *di, CurlStatus status, CURLcode error)
{
	qboolean ok = false;
	if(!curl_dll)
		return;
	switch(status)
	{
		case CURL_DOWNLOAD_SUCCESS:
			ok = true;
			di->callback(CURLCBSTATUS_OK, di->bytes_received, di->buffer, di->callback_data);
			break;
		case CURL_DOWNLOAD_FAILED:
			di->callback(CURLCBSTATUS_FAILED, di->bytes_received, di->buffer, di->callback_data);
			break;
		case CURL_DOWNLOAD_ABORTED:
			di->callback(CURLCBSTATUS_ABORTED, di->bytes_received, di->buffer, di->callback_data);
			break;
		case CURL_DOWNLOAD_SERVERERROR:
			// reopen to enforce it to have zero bytes again
			if(di->stream)
			{
				FS_Close(di->stream);
				di->stream = FS_OpenRealFile(di->filename, "wb", false);
			}

			if(di->callback)
				di->callback(error ? (int) error : CURLCBSTATUS_SERVERERROR, di->bytes_received, di->buffer, di->callback_data);
			break;
		default:
			if(di->callback)
				di->callback(CURLCBSTATUS_UNKNOWN, di->bytes_received, di->buffer, di->callback_data);
			break;
	}

	if(di->curle)
	{
		qcurl_multi_remove_handle(curlm, di->curle);
		qcurl_easy_cleanup(di->curle);
	}

	if(!di->callback && ok && !di->bytes_received)
	{
		Con_Printf("ERROR: empty file\n");
		ok = false;
	}

	if(di->stream)
		FS_Close(di->stream);

	if(ok && di->ispak)
	{
		ok = FS_AddPack(di->filename, NULL, true);
		if(!ok)
		{
			// pack loading failed?
			// this is critical
			// better clear the file again...
			di->stream = FS_OpenRealFile(di->filename, "wb", false);
			FS_Close(di->stream);

			if(di->startpos && !di->callback)
			{
				// this was a resume?
				// then try to redownload it without reporting the error
				Curl_Begin(di->url, di->filename, di->ispak, di->forthismap, NULL, 0, NULL, NULL);
				di->forthismap = false; // don't count the error
			}
		}
	}

	if(di->prev)
		di->prev->next = di->next;
	else
		downloads = di->next;
	if(di->next)
		di->next->prev = di->prev;

	--numdownloads;
	if(di->forthismap)
	{
		if(ok)
			++numdownloads_success;
		else
			++numdownloads_fail;
	}
	Z_Free(di);
}

/*
====================
CheckPendingDownloads

checks if there are free download slots to start new downloads in.
To not start too many downloads at once, only one download is added at a time,
up to a maximum number of cl_curl_maxdownloads are running.
====================
*/
static void CheckPendingDownloads(void)
{
	if(!curl_dll)
		return;
	if(numdownloads < cl_curl_maxdownloads.integer)
	{
		downloadinfo *di;
		for(di = downloads; di; di = di->next)
		{
			if(!di->started)
			{
				if(!di->buffer)
				{
					Con_Printf("Downloading %s -> %s", di->url, di->filename);

					di->stream = FS_OpenRealFile(di->filename, "ab", false);
					if(!di->stream)
					{
						Con_Printf("\nFAILED: Could not open output file %s\n", di->filename);
						Curl_EndDownload(di, CURL_DOWNLOAD_FAILED, CURLE_OK);
						return;
					}
					FS_Seek(di->stream, 0, SEEK_END);
					di->startpos = FS_Tell(di->stream);

					if(di->startpos > 0)
						Con_Printf(", resuming from position %ld", (long) di->startpos);
					Con_Print("...\n");
				}
				else
				{
					Con_DPrintf("Downloading %s -> memory\n", di->url);
					di->startpos = 0;
				}

				di->curle = qcurl_easy_init();
				qcurl_easy_setopt(di->curle, CURLOPT_URL, di->url);
				qcurl_easy_setopt(di->curle, CURLOPT_USERAGENT, engineversion);
				qcurl_easy_setopt(di->curle, CURLOPT_REFERER, di->referer);
				qcurl_easy_setopt(di->curle, CURLOPT_RESUME_FROM, (long) di->startpos);
				qcurl_easy_setopt(di->curle, CURLOPT_FOLLOWLOCATION, 1);
				qcurl_easy_setopt(di->curle, CURLOPT_WRITEFUNCTION, CURL_fwrite);
				qcurl_easy_setopt(di->curle, CURLOPT_LOW_SPEED_LIMIT, (long) 256);
				qcurl_easy_setopt(di->curle, CURLOPT_LOW_SPEED_TIME, (long) 45);
				qcurl_easy_setopt(di->curle, CURLOPT_WRITEDATA, (void *) di);
				qcurl_easy_setopt(di->curle, CURLOPT_PRIVATE, (void *) di);
				qcurl_multi_add_handle(curlm, di->curle);
				di->started = true;
				++numdownloads;
				if(numdownloads >= cl_curl_maxdownloads.integer)
					break;
			}
		}
	}
}

/*
====================
Curl_Init

this function MUST be called before using anything else in this file.
On Win32, this must be called AFTER WSAStartup has been done!
====================
*/
void Curl_Init(void)
{
	CURL_OpenLibrary();
	if(!curl_dll)
		return;
	qcurl_global_init(CURL_GLOBAL_NOTHING);
	curlm = qcurl_multi_init();
}

/*
====================
Curl_Shutdown

Surprise... closes all the stuff. Please do this BEFORE shutting down LHNET.
====================
*/
void Curl_ClearRequirements(void);
void Curl_Shutdown(void)
{
	if(!curl_dll)
		return;
	Curl_ClearRequirements();
	Curl_CancelAll();
	CURL_CloseLibrary();
	curl_dll = NULL;
}

/*
====================
Curl_Find

Finds the internal information block for a download given by file name.
====================
*/
static downloadinfo *Curl_Find(const char *filename)
{
	downloadinfo *di;
	if(!curl_dll)
		return NULL;
	for(di = downloads; di; di = di->next)
		if(!strcasecmp(di->filename, filename))
			return di;
	return NULL;
}

void Curl_Cancel_ToMemory(curl_callback_t callback, void *cbdata)
{
	downloadinfo *di;
	if(!curl_dll)
		return;
	for(di = downloads; di; )
	{
		if(di->callback == callback && di->callback_data == cbdata)
		{
			di->callback = curl_quiet_callback; // do NOT call the callback
			Curl_EndDownload(di, CURL_DOWNLOAD_ABORTED, CURLE_OK);
			di = downloads;
		}
		else
			di = di->next;
	}
}

/*
====================
Curl_Begin

Starts a download of a given URL to the file name portion of this URL (or name
if given) in the "dlcache/" folder.
====================
*/
static qboolean Curl_Begin(const char *URL, const char *name, qboolean ispak, qboolean forthismap, unsigned char *buf, size_t bufsize, curl_callback_t callback, void *cbdata)
{
	if(!curl_dll)
	{
		return false;
	}
	else
	{
		char fn[MAX_QPATH];
		const char *p, *q;
		size_t length;
		downloadinfo *di;

		// Note: This extraction of the file name portion is NOT entirely correct.
		//
		// It does the following:
		//
		//   http://host/some/script.cgi/SomeFile.pk3?uid=ABCDE -> SomeFile.pk3
		//   http://host/some/script.php?uid=ABCDE&file=/SomeFile.pk3 -> SomeFile.pk3
		//   http://host/some/script.php?uid=ABCDE&file=SomeFile.pk3 -> script.php
		//
		// However, I'd like to keep this "buggy" behavior so that PHP script
		// authors can write download scripts without having to enable
		// AcceptPathInfo on Apache. They just have to ensure that their script
		// can be called with such a "fake" path name like
		// http://host/some/script.php?uid=ABCDE&file=/SomeFile.pk3
		//
		// By the way, such PHP scripts should either send the file or a
		// "Location:" redirect; PHP code example:
		//
		//   header("Location: http://www.example.com/");
		//
		// By the way, this will set User-Agent to something like
		// "Nexuiz build 22:27:55 Mar 17 2006" (engineversion) and Referer to
		// dp://serverhost:serverport/ so you can filter on this; an example
		// httpd log file line might be:
		//
		//   141.2.16.3 - - [17/Mar/2006:22:32:43 +0100] "GET /maps/tznex07.pk3 HTTP/1.1" 200 1077455 "dp://141.2.16.7:26000/" "Nexuiz Linux 22:07:43 Mar 17 2006"

		if(!name)
			name = URL;

		if(!buf)
		{
			p = strrchr(name, '/');
			p = p ? (p+1) : name;
			q = strchr(p, '?');
			length = q ? (size_t)(q - p) : strlen(p);
			dpsnprintf(fn, sizeof(fn), "dlcache/%.*s", (int)length, p);

			name = fn; // make it point back

			// already downloading the file?
			{
				downloadinfo *di = Curl_Find(fn);
				if(di)
				{
					Con_Printf("Can't download %s, already getting it from %s!\n", fn, di->url);

					// however, if it was not for this map yet...
					if(forthismap && !di->forthismap)
					{
						di->forthismap = true;
						// this "fakes" a download attempt so the client will wait for
						// the download to finish and then reconnect
						++numdownloads_added;
					}

					return false;
				}
			}

			if(ispak && FS_FileExists(fn))
			{
				qboolean already_loaded;
				if(FS_AddPack(fn, &already_loaded, true))
				{
					Con_DPrintf("%s already exists, not downloading!\n", fn);
					if(already_loaded)
						Con_DPrintf("(pak was already loaded)\n");
					else
					{
						if(forthismap)
						{
							++numdownloads_added;
							++numdownloads_success;
						}
					}

					return false;
				}
				else
				{
					qfile_t *f = FS_OpenRealFile(fn, "rb", false);
					if(f)
					{
						char buf[4] = {0};
						FS_Read(f, buf, sizeof(buf)); // no "-1", I will use memcmp

						if(memcmp(buf, "PK\x03\x04", 4) && memcmp(buf, "PACK", 4))
						{
							Con_DPrintf("Detected non-PAK %s, clearing and NOT resuming.\n", fn);
							FS_Close(f);
							f = FS_OpenRealFile(fn, "wb", false);
							if(f)
								FS_Close(f);
						}
						else
						{
							// OK
							FS_Close(f);
						}
					}
				}
			}
		}

		// if we get here, we actually want to download... so first verify the
		// URL scheme (so one can't read local files using file://)
		if(strncmp(URL, "http://", 7) && strncmp(URL, "ftp://", 6) && strncmp(URL, "https://", 8))
		{
			Con_Printf("Curl_Begin(\"%s\"): nasty URL scheme rejected\n", URL);
			return false;
		}

		if(forthismap)
			++numdownloads_added;
		di = (downloadinfo *) Z_Malloc(sizeof(*di));
		strlcpy(di->filename, name, sizeof(di->filename));
		strlcpy(di->url, URL, sizeof(di->url));
		dpsnprintf(di->referer, sizeof(di->referer), "dp://%s/", cls.netcon ? cls.netcon->address : "notconnected.invalid");
		di->forthismap = forthismap;
		di->stream = NULL;
		di->startpos = 0;
		di->curle = NULL;
		di->started = false;
		di->ispak = (ispak && !buf);
		di->bytes_received = 0;
		di->next = downloads;
		di->prev = NULL;
		if(di->next)
			di->next->prev = di;

		di->buffer = buf;
		di->buffersize = bufsize;
		if(callback == NULL)
		{
			di->callback = curl_default_callback;
			di->callback_data = di;
		}
		else
		{
			di->callback = callback;
			di->callback_data = cbdata;
		}

		downloads = di;
		return true;
	}
}

qboolean Curl_Begin_ToFile(const char *URL, const char *name, qboolean ispak, qboolean forthismap)
{
	return Curl_Begin(URL, name, ispak, forthismap, NULL, 0, NULL, NULL);
}
qboolean Curl_Begin_ToMemory(const char *URL, unsigned char *buf, size_t bufsize, curl_callback_t callback, void *cbdata)
{
	return Curl_Begin(URL, NULL, false, false, buf, bufsize, callback, cbdata);
}

/*
====================
Curl_Run

call this regularily as this will always download as much as possible without
blocking.
====================
*/
void Curl_Run(void)
{
	noclear = FALSE;

	if(!cl_curl_enabled.integer)
		return;

	if(!curl_dll)
		return;

	Curl_CheckCommandWhenDone();

	if(!downloads)
		return;

	if(realtime < curltime) // throttle
		return;

	{
		int remaining;
		CURLMcode mc;

		do
		{
			mc = qcurl_multi_perform(curlm, &remaining);
		}
		while(mc == CURLM_CALL_MULTI_PERFORM);

		for(;;)
		{
			CURLMsg *msg = qcurl_multi_info_read(curlm, &remaining);
			if(!msg)
				break;
			if(msg->msg == CURLMSG_DONE)
			{
				downloadinfo *di;
				CurlStatus failed = CURL_DOWNLOAD_SUCCESS;
				CURLcode result;
				qcurl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &di);
				result = msg->data.result;
				if(result)
				{
					failed = CURL_DOWNLOAD_FAILED;
				}
				else
				{
					long code;
					qcurl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &code);
					switch(code / 100)
					{
						case 4: // e.g. 404?
						case 5: // e.g. 500?
							failed = CURL_DOWNLOAD_SERVERERROR;
							result = (CURLcode) code;
							break;
					}
				}

				Curl_EndDownload(di, failed, result);
			}
		}
	}

	CheckPendingDownloads();

	// when will we curl the next time?
	// we will wait a bit to ensure our download rate is kept.
	// we now know that realtime >= curltime... so set up a new curltime
	if(cl_curl_maxspeed.value > 0)
	{
		unsigned long bytes = bytes_received; // maybe smoothen a bit?
		curltime = realtime + bytes / (cl_curl_maxspeed.value * 1024.0);
		bytes_received -= bytes;
	}
	else
		curltime = realtime;
}

/*
====================
Curl_CancelAll

Stops ALL downloads.
====================
*/
void Curl_CancelAll(void)
{
	if(!curl_dll)
		return;

	while(downloads)
	{
		Curl_EndDownload(downloads, CURL_DOWNLOAD_ABORTED, CURLE_OK);
		// INVARIANT: downloads will point to the next download after that!
	}
}

/*
====================
Curl_Running

returns true iff there is a download running.
====================
*/
qboolean Curl_Running(void)
{
	if(!curl_dll)
		return false;

	return downloads != NULL;
}

/*
====================
Curl_GetDownloadAmount

returns a value from 0.0 to 1.0 which represents the downloaded amount of data
for the given download.
====================
*/
static double Curl_GetDownloadAmount(downloadinfo *di)
{
	if(!curl_dll)
		return -2;
	if(di->curle)
	{
		double length;
		qcurl_easy_getinfo(di->curle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &length);
		if(length > 0)
			return (di->startpos + di->bytes_received) / (di->startpos + length);
		else
			return 0;
	}
	else
		return -1;
}

/*
====================
Curl_GetDownloadSpeed

returns the speed of the given download in bytes per second
====================
*/
static double Curl_GetDownloadSpeed(downloadinfo *di)
{
	if(!curl_dll)
		return -2;
	if(di->curle)
	{
		double speed;
		qcurl_easy_getinfo(di->curle, CURLINFO_SPEED_DOWNLOAD, &speed);
		return speed;
	}
	else
		return -1;
}

/*
====================
Curl_Info_f

prints the download list
====================
*/
// TODO rewrite using Curl_GetDownloadInfo?
static void Curl_Info_f(void)
{
	downloadinfo *di;
	if(!curl_dll)
		return;
	if(Curl_Running())
	{
		Con_Print("Currently running downloads:\n");
		for(di = downloads; di; di = di->next)
		{
			double speed, percent;
			Con_Printf("  %s -> %s ",  di->url, di->filename);
			percent = 100.0 * Curl_GetDownloadAmount(di);
			speed = Curl_GetDownloadSpeed(di);
			if(percent >= 0)
				Con_Printf("(%.1f%% @ %.1f KiB/s)\n", percent, speed / 1024.0);
			else
				Con_Print("(queued)\n");
		}
	}
	else
	{
		Con_Print("No downloads running.\n");
	}
}

/*
====================
Curl_Curl_f

implements the "curl" console command

curl --info
curl --cancel
curl --cancel filename
curl url

For internal use:

curl [--pak] [--forthismap] [--for filename filename...] url
	--pak: after downloading, load the package into the virtual file system
	--for filename...: only download of at least one of the named files is missing
	--forthismap: don't reconnect on failure

curl --clear_autodownload
	clears the download success/failure counters

curl --finish_autodownload
	if at least one download has been started, disconnect and drop to the menu
	once the last download completes successfully, reconnect to the current server
====================
*/
void Curl_Curl_f(void)
{
	int i;
	int end;
	qboolean pak = false;
	qboolean forthismap = false;
	const char *url;
	const char *name = 0;

	if(!curl_dll)
	{
		Con_Print("libcurl DLL not found, this command is inactive.\n");
		return;
	}

	if(!cl_curl_enabled.integer)
	{
		Con_Print("curl support not enabled. Set cl_curl_enabled to 1 to enable.\n");
		return;
	}

	for(i = 0; i != Cmd_Argc(); ++i)
		Con_DPrintf("%s ", Cmd_Argv(i));
	Con_DPrint("\n");

	if(Cmd_Argc() < 2)
	{
		Con_Print("usage:\ncurl --info, curl --cancel [filename], curl url\n");
		return;
	}

	url = Cmd_Argv(Cmd_Argc() - 1);
	end = Cmd_Argc();

	for(i = 1; i != end; ++i)
	{
		const char *a = Cmd_Argv(i);
		if(!strcmp(a, "--info"))
		{
			Curl_Info_f();
			return;
		}
		else if(!strcmp(a, "--cancel"))
		{
			if(i == end - 1) // last argument
				Curl_CancelAll();
			else
			{
				downloadinfo *di = Curl_Find(url);
				if(di)
					Curl_EndDownload(di, CURL_DOWNLOAD_ABORTED, CURLE_OK);
				else
					Con_Print("download not found\n");
			}
			return;
		}
		else if(!strcmp(a, "--pak"))
		{
			pak = true;
		}
		else if(!strcmp(a, "--for"))
		{
			for(i = i + 1; i != end - 1; ++i)
			{
				if(!FS_FileExists(Cmd_Argv(i)))
					goto needthefile; // why can't I have a "double break"?
			}
			// if we get here, we have all the files...
			return;
		}
		else if(!strcmp(a, "--forthismap"))
		{
			forthismap = true;
		}
		else if(!strcmp(a, "--as"))
		{
			if(i < end - 1)
			{
				++i;
				name = Cmd_Argv(i);
			}
		}
		else if(!strcmp(a, "--clear_autodownload"))
		{
			// mark all running downloads as "not for this map", so if they
			// fail, it does not matter
			Curl_Clear_forthismap();
			return;
		}
		else if(!strcmp(a, "--finish_autodownload"))
		{
			if(numdownloads_added)
			{
				char donecommand[256];
				if(cls.netcon)
				{
					if(cl.loadbegun) // curling won't inhibit loading the map any more when at this stage, so bail out and force a reconnect
					{
						dpsnprintf(donecommand, sizeof(donecommand), "connect %s", cls.netcon->address);
						Curl_CommandWhenDone(donecommand);
						noclear = TRUE;
						CL_Disconnect();
						noclear = FALSE;
						Curl_CheckCommandWhenDone();
					}
					else
						Curl_Register_predownload();
				}
			}
			return;
		}
		else if(*a == '-')
		{
			Con_Printf("invalid option %s\n", a);
			return;
		}
	}

needthefile:
	Curl_Begin_ToFile(url, name, pak, forthismap);
}

/*
static void curl_curlcat_callback(int code, size_t length_received, unsigned char *buffer, void *cbdata)
{
	Con_Printf("Received %d bytes (status %d):\n%.*s\n", (int) length_received, code, (int) length_received, buffer);
	Z_Free(buffer);
}

void Curl_CurlCat_f(void)
{
	unsigned char *buf;
	const char *url = Cmd_Argv(1);
	buf = Z_Malloc(16384);
	Curl_Begin_ToMemory(url, buf, 16384, curl_curlcat_callback, NULL);
}
*/

/*
====================
Curl_Init_Commands

loads the commands and cvars this library uses
====================
*/
void Curl_Init_Commands(void)
{
	Cvar_RegisterVariable (&cl_curl_enabled);
	Cvar_RegisterVariable (&cl_curl_maxdownloads);
	Cvar_RegisterVariable (&cl_curl_maxspeed);
	Cvar_RegisterVariable (&sv_curl_defaulturl);
	Cvar_RegisterVariable (&sv_curl_serverpackages);
	Cmd_AddCommand ("curl", Curl_Curl_f, "download data from an URL and add to search path");
	//Cmd_AddCommand ("curlcat", Curl_CurlCat_f, "display data from an URL (debugging command)");
}

/*
====================
Curl_GetDownloadInfo

returns an array of Curl_downloadinfo_t structs for usage by GUIs.
The number of elements in the array is returned in int *nDownloads.
const char **additional_info may be set to a string of additional user
information, or to NULL if no such display shall occur. The returned
array must be freed later using Z_Free.
====================
*/
Curl_downloadinfo_t *Curl_GetDownloadInfo(int *nDownloads, const char **additional_info)
{
	int i;
	downloadinfo *di;
	Curl_downloadinfo_t *downinfo;
	static char addinfo[128];

	if(!curl_dll)
	{
		*nDownloads = 0;
		if(additional_info)
			*additional_info = NULL;
		return NULL;
	}

	i = 0;
	for(di = downloads; di; di = di->next)
		++i;

	downinfo = (Curl_downloadinfo_t *) Z_Malloc(sizeof(*downinfo) * i);
	i = 0;
	for(di = downloads; di; di = di->next)
	{
		// do not show infobars for background downloads
		if(!developer.integer)
			if(di->buffer)
				continue;
		strlcpy(downinfo[i].filename, di->filename, sizeof(downinfo[i].filename));
		if(di->curle)
		{
			downinfo[i].progress = Curl_GetDownloadAmount(di);
			downinfo[i].speed = Curl_GetDownloadSpeed(di);
			downinfo[i].queued = false;
		}
		else
		{
			downinfo[i].queued = true;
		}
		++i;
	}

	if(additional_info)
	{
		// TODO: can I clear command_when_done as soon as the first download fails?
		if(*command_when_done && !numdownloads_fail && numdownloads_added)
		{
			if(!strncmp(command_when_done, "connect ", 8))
				dpsnprintf(addinfo, sizeof(addinfo), "(will join %s when done)", command_when_done + 8);
			else if(!strcmp(command_when_done, "cl_begindownloads"))
				dpsnprintf(addinfo, sizeof(addinfo), "(will enter the game when done)");
			else
				dpsnprintf(addinfo, sizeof(addinfo), "(will do '%s' when done)", command_when_done);
			*additional_info = addinfo;
		}
		else
			*additional_info = NULL;
	}

	*nDownloads = i;
	return downinfo;
}


/*
====================
Curl_FindPackURL

finds the URL where to find a given package.

For this, it reads a file "curl_urls.txt" of the following format:

	data*.pk3	-
	revdm*.pk3	http://revdm/downloads/are/here/
	*			http://any/other/stuff/is/here/

The URLs should end in /. If not, downloads will still work, but the cached files
can't be just put into the data directory with the same download configuration
(you might want to do this if you want to tag downloaded files from your
server, but you should not). "-" means "don't download".

If no single pattern matched, the cvar sv_curl_defaulturl is used as download
location instead.

Note: pak1.pak and data*.pk3 are excluded from autodownload at another point in
this file for obvious reasons.
====================
*/
static const char *Curl_FindPackURL(const char *filename)
{
	static char foundurl[1024];
	fs_offset_t filesize;
	char *buf = (char *) FS_LoadFile("curl_urls.txt", tempmempool, true, &filesize);
	if(buf && filesize)
	{
		// read lines of format "pattern url"
		char *p = buf;
		char *pattern = NULL, *patternend = NULL, *url = NULL, *urlend = NULL;
		qboolean eof = false;

		pattern = p;
		while(!eof)
		{
			switch(*p)
			{
				case 0:
					eof = true;
					// fallthrough
				case '\n':
				case '\r':
					if(pattern && url && patternend)
					{
						if(!urlend)
							urlend = p;
						*patternend = 0;
						*urlend = 0;
						if(matchpattern(filename, pattern, true))
						{
							strlcpy(foundurl, url, sizeof(foundurl));
							Z_Free(buf);
							return foundurl;
						}
					}
					pattern = NULL;
					patternend = NULL;
					url = NULL;
					urlend = NULL;
					break;
				case ' ':
				case '\t':
					if(pattern && !patternend)
						patternend = p;
					else if(url && !urlend)
						urlend = p;
					break;
				default:
					if(!pattern)
						pattern = p;
					else if(pattern && patternend && !url)
						url = p;
					break;
			}
			++p;
		}
	}
	if(buf)
		Z_Free(buf);
	return sv_curl_defaulturl.string;
}

typedef struct requirement_s
{
	struct requirement_s *next;
	char filename[MAX_QPATH];
}
requirement;
static requirement *requirements = NULL;


/*
====================
Curl_RequireFile

Adds the given file to the list of requirements.
====================
*/
void Curl_RequireFile(const char *filename)
{
	requirement *req = (requirement *) Z_Malloc(sizeof(*requirements));
	req->next = requirements;
	strlcpy(req->filename, filename, sizeof(req->filename));
	requirements = req;
}

/*
====================
Curl_ClearRequirements

Clears the list of required files for playing on the current map.
This should be called at every map change.
====================
*/
void Curl_ClearRequirements(void)
{
	const char *p;
	while(requirements)
	{
		requirement *req = requirements;
		requirements = requirements->next;
		Z_Free(req);
	}
	p = sv_curl_serverpackages.string;
	Con_DPrintf("Require all of: %s\n", p);
	while(COM_ParseToken_Simple(&p, false, false))
	{
		Con_DPrintf("Require: %s\n", com_token);
		Curl_RequireFile(com_token);
	}
}

/*
====================
Curl_SendRequirements

Makes the current host_clients download all files he needs.
This is done by sending him the following console commands:

	curl --clear_autodownload
	curl --pak --for maps/pushmoddm1.bsp --forthismap http://where/this/darn/map/is/pushmoddm1.pk3
	curl --finish_autodownload
====================
*/
void Curl_SendRequirements(void)
{
	// for each requirement, find the pack name
	char sendbuffer[4096] = "";
	requirement *req;
	qboolean foundone = false;

	for(req = requirements; req; req = req->next)
	{
		const char *p;
		const char *thispack = FS_WhichPack(req->filename);
		const char *packurl;

		if(!thispack)
			continue;

		p = strrchr(thispack, '/');
		if(p)
			thispack = p + 1;

		packurl = Curl_FindPackURL(thispack);

		if(packurl && *packurl && strcmp(packurl, "-"))
		{
			if(!foundone)
				strlcat(sendbuffer, "curl --clear_autodownload\n", sizeof(sendbuffer));

			strlcat(sendbuffer, "curl --pak --forthismap --as ", sizeof(sendbuffer));
			strlcat(sendbuffer, thispack, sizeof(sendbuffer));
			strlcat(sendbuffer, " --for ", sizeof(sendbuffer));
			strlcat(sendbuffer, req->filename, sizeof(sendbuffer));
			strlcat(sendbuffer, " ", sizeof(sendbuffer));
			strlcat(sendbuffer, packurl, sizeof(sendbuffer));
			strlcat(sendbuffer, thispack, sizeof(sendbuffer));
			strlcat(sendbuffer, "\n", sizeof(sendbuffer));

			foundone = true;
		}
	}

	if(foundone)
		strlcat(sendbuffer, "curl --finish_autodownload\n", sizeof(sendbuffer));

	if(strlen(sendbuffer) + 1 < sizeof(sendbuffer))
		Host_ClientCommands("%s", sendbuffer);
	else
		Con_Printf("Could not initiate autodownload due to URL buffer overflow\n");
}
