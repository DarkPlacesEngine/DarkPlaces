void Curl_Run();
qboolean Curl_Running();
void Curl_Begin(const char *URL, const char *name, qboolean ispak, qboolean forthismap);
void Curl_Init();
void Curl_Init_Commands();
void Curl_Shutdown();
void Curl_CancelAll();
void Curl_Clear_forthismap();
qboolean Curl_Have_forthismap();
void Curl_Register_predownload();

void Curl_ClearRequirements();
void Curl_RequireFile(const char *filename);
void Curl_SendRequirements();

typedef struct Curl_downloadinfo_s
{
	char filename[MAX_QPATH];
	double progress;
	double speed;
	qboolean queued;
}
Curl_downloadinfo_t;
Curl_downloadinfo_t *Curl_GetDownloadInfo(int *nDownloads, const char **additional_info);
	// this may and should be Z_Free()ed
	// the result is actually an array
	// an additional info string may be returned in additional_info as a
	// pointer to a static string (but the argument may be NULL if the caller
	// does not care)
