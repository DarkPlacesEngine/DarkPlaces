
#include "quakedef.h"
#include "cl_dyntexture.h"
#include "cl_video.h"
#include "dpvsimpledecode.h"

// VorteX: JAM video module used by Blood Omnicide
#define USEJAM
#ifdef USEJAM
  #include "cl_video_jamdecode.c"
#endif

// cvars
cvar_t cl_video_subtitles = {CVAR_SAVE, "cl_video_subtitles", "0", "show subtitles for videos (if they are present)"};
cvar_t cl_video_subtitles_lines = {CVAR_SAVE, "cl_video_subtitles_lines", "4", "how many lines to occupy for subtitles"};
cvar_t cl_video_subtitles_textsize = {CVAR_SAVE, "cl_video_subtitles_textsize", "16", "textsize for subtitles"};
cvar_t cl_video_scale = {CVAR_SAVE, "cl_video_scale", "1", "scale of video, 1 = fullscreen, 0.75 - 3/4 of screen etc."};
cvar_t cl_video_scale_vpos = {CVAR_SAVE, "cl_video_scale_vpos", "0", "vertical align of scaled video, -1 is top, 1 is bottom"};
cvar_t cl_video_stipple = {CVAR_SAVE, "cl_video_stipple", "0", "draw interlacing-like effect on videos, similar to scr_stipple but static and used only with video playing."};
cvar_t cl_video_brightness = {CVAR_SAVE, "cl_video_brightness", "1", "brightness of video, 1 = fullbright, 0.75 - 3/4 etc."};
cvar_t cl_video_keepaspectratio = {CVAR_SAVE, "cl_video_keepaspectratio", "0", "keeps aspect ratio of fullscreen videos, leaving black color on unfilled areas, a value of 2 let video to be stretched horizontally with top & bottom being sliced out"};
cvar_t cl_video_fadein = {CVAR_SAVE, "cl_video_fadein", "0", "fading-from-black effect once video is started, in seconds"};
cvar_t cl_video_fadeout = {CVAR_SAVE, "cl_video_fadeout", "0", "fading-to-black effect once video is ended, in seconds"};

// constants (and semi-constants)
static int  cl_videormask;
static int  cl_videobmask;
static int  cl_videogmask;
static int	cl_videobytesperpixel;

static int cl_num_videos;
static clvideo_t cl_videos[ MAXCLVIDEOS ];
static rtexturepool_t *cl_videotexturepool;

static clvideo_t *FindUnusedVid( void )
{
	int i;
	for( i = 1 ; i < MAXCLVIDEOS ; i++ )
		if( cl_videos[ i ].state == CLVIDEO_UNUSED )
			return &cl_videos[ i ];
	return NULL;
}

static qboolean OpenStream( clvideo_t * video )
{
	const char *errorstring;
	video->stream = dpvsimpledecode_open( video, video->filename, &errorstring);
	if (!video->stream )
	{
#ifdef USEJAM
		video->stream = jam_open( video, video->filename, &errorstring);
		if (video->stream)
			return true;
#endif
		Con_Printf("unable to open \"%s\", error: %s\n", video->filename, errorstring);
		return false;
	}
	return true;
}

static void VideoUpdateCallback(rtexture_t *rt, void *data)
{
	clvideo_t *video = (clvideo_t *) data;
	R_UpdateTexture( video->cpif.tex, (unsigned char *)video->imagedata, 0, 0, 0, video->cpif.width, video->cpif.height, 1 );
}

static void LinkVideoTexture( clvideo_t *video )
{
	video->cpif.tex = R_LoadTexture2D( cl_videotexturepool, video->cpif.name, video->cpif.width, video->cpif.height, NULL, TEXTYPE_BGRA, TEXF_PERSISTENT | TEXF_CLAMP, -1, NULL );
	R_MakeTextureDynamic( video->cpif.tex, VideoUpdateCallback, video );
	CL_LinkDynTexture( video->cpif.name, video->cpif.tex );
}

static void UnlinkVideoTexture( clvideo_t *video )
{
	CL_UnlinkDynTexture( video->cpif.name );
	// free the texture
	R_FreeTexture( video->cpif.tex );
	video->cpif.tex = NULL;
	// free the image data
	Mem_Free( video->imagedata );
}

static void SuspendVideo( clvideo_t * video )
{
	if (video->suspended)
		return;
	video->suspended = true;
	UnlinkVideoTexture(video);
	// if we are in firstframe mode, also close the stream
	if (video->state == CLVIDEO_FIRSTFRAME)
		video->close(video->stream);
}

static qboolean WakeVideo( clvideo_t * video )
{
	if( !video->suspended )
		return true;
	video->suspended = false;

	if( video->state == CLVIDEO_FIRSTFRAME )
		if( !OpenStream( video ) ) {
			video->state = CLVIDEO_UNUSED;
			return false;
		}

	video->imagedata = Mem_Alloc( cls.permanentmempool, video->cpif.width * video->cpif.height * cl_videobytesperpixel );
	LinkVideoTexture( video );

	// update starttime
	video->starttime += realtime - video->lasttime;

	return true;
}

static void LoadSubtitles( clvideo_t *video, const char *subtitlesfile )
{
	char *subtitle_text;
	const char *data;
	float subtime, sublen;
	int numsubs = 0;

	if (gamemode == GAME_BLOODOMNICIDE)
	{
		char overridename[MAX_QPATH];
		cvar_t *langcvar;

		langcvar = Cvar_FindVar("language");
		subtitle_text = NULL;
		if (langcvar)
		{
			dpsnprintf(overridename, sizeof(overridename), "script/locale/%s/%s", langcvar->string, subtitlesfile);
			subtitle_text = (char *)FS_LoadFile(overridename, cls.permanentmempool, false, NULL);
		}
		if (!subtitle_text)
			subtitle_text = (char *)FS_LoadFile(subtitlesfile, cls.permanentmempool, false, NULL);
	}
	else
	{
		subtitle_text = (char *)FS_LoadFile(subtitlesfile, cls.permanentmempool, false, NULL);
	}
	if (!subtitle_text)
	{
		Con_DPrintf( "LoadSubtitles: can't open subtitle file '%s'!\n", subtitlesfile );
		return;
	}

	// parse subtitle_text
	// line is: x y "text" where
	//    x - start time
	//    y - seconds last (if 0 - last thru next sub, if negative - last to next sub - this amount of seconds)

	data = subtitle_text;
	for (;;)
	{
		if (!COM_ParseToken_QuakeC(&data, false))
			break;
		subtime = atof( com_token );
		if (!COM_ParseToken_QuakeC(&data, false))
			break;
		sublen = atof( com_token );
		if (!COM_ParseToken_QuakeC(&data, false))
			break;
		if (!com_token[0])
			continue;
		// check limits
		if (video->subtitles == CLVIDEO_MAX_SUBTITLES)
		{
			Con_Printf("WARNING: CLVIDEO_MAX_SUBTITLES = %i reached when reading subtitles from '%s'\n", CLVIDEO_MAX_SUBTITLES, subtitlesfile);
			break;	
		}
		// add a sub
		video->subtitle_text[numsubs] = (char *) Mem_Alloc(cls.permanentmempool, strlen(com_token) + 1);
		memcpy(video->subtitle_text[numsubs], com_token, strlen(com_token) + 1);
		video->subtitle_start[numsubs] = subtime;
		video->subtitle_end[numsubs] = sublen;
		if (numsubs > 0) // make true len for prev sub, autofix overlapping subtitles
		{
			if (video->subtitle_end[numsubs-1] <= 0)
				video->subtitle_end[numsubs-1] = max(video->subtitle_start[numsubs-1], video->subtitle_start[numsubs] + video->subtitle_end[numsubs-1]);
			else
				video->subtitle_end[numsubs-1] = min(video->subtitle_start[numsubs-1] + video->subtitle_end[numsubs-1], video->subtitle_start[numsubs]);
		}
		numsubs++;
		// todo: check timing for consistency?
	}
	if (numsubs > 0) // make true len for prev sub, autofix overlapping subtitles
	{
		if (video->subtitle_end[numsubs-1] <= 0)
			video->subtitle_end[numsubs-1] = 99999999; // fixme: make it end when video ends?
		else
			video->subtitle_end[numsubs-1] = video->subtitle_start[numsubs-1] + video->subtitle_end[numsubs-1];
	}
	Z_Free( subtitle_text );
	video->subtitles = numsubs;
/*
	Con_Printf( "video->subtitles: %i\n", video->subtitles );
	for (numsubs = 0; numsubs < video->subtitles; numsubs++)
		Con_Printf( "  %03.2f %03.2f : %s\n", video->subtitle_start[numsubs], video->subtitle_end[numsubs], video->subtitle_text[numsubs] );
*/
}

static clvideo_t* OpenVideo( clvideo_t *video, const char *filename, const char *name, int owner, const char *subtitlesfile )
{
	strlcpy( video->filename, filename, sizeof(video->filename) );
	video->ownertag = owner;
	if( strncmp( name, CLVIDEOPREFIX, sizeof( CLVIDEOPREFIX ) - 1 ) )
		return NULL;
	strlcpy( video->cpif.name, name, sizeof(video->cpif.name) );

	if( !OpenStream( video ) )
		return NULL;

	video->state = CLVIDEO_FIRSTFRAME;
	video->framenum = -1;
	video->framerate = video->getframerate( video->stream );
	video->lasttime = realtime;
	video->subtitles = 0;

	video->cpif.width = video->getwidth( video->stream );
	video->cpif.height = video->getheight( video->stream );
	video->imagedata = Mem_Alloc( cls.permanentmempool, video->cpif.width * video->cpif.height * cl_videobytesperpixel );
	LinkVideoTexture( video );

	// VorteX: load simple subtitle_text file
	if (subtitlesfile[0])
		LoadSubtitles( video, subtitlesfile );

	return video;
}

clvideo_t* CL_OpenVideo( const char *filename, const char *name, int owner, const char *subtitlesfile )
{
	clvideo_t *video;
	// sanity check
	if( !name || !*name || strncmp( name, CLVIDEOPREFIX, sizeof( CLVIDEOPREFIX ) - 1 ) != 0 ) {
		Con_DPrintf( "CL_OpenVideo: Bad video texture name '%s'!\n", name );
		return NULL;
	}

	video = FindUnusedVid();
	if( !video ) {
		Con_Printf( "CL_OpenVideo: unable to open video \"%s\" - video limit reached\n", filename );
		return NULL;
	}
	video = OpenVideo( video, filename, name, owner, subtitlesfile );
	// expand the active range to include the new entry
	if (video) {
		cl_num_videos = max(cl_num_videos, (int)(video - cl_videos) + 1);
	}
	return video;
}

static clvideo_t* CL_GetVideoBySlot( int slot )
{
	clvideo_t *video = &cl_videos[ slot ];

	if( video->suspended )
	{
		if( !WakeVideo( video ) )
			return NULL;
		else if( video->state == CLVIDEO_RESETONWAKEUP )
			video->framenum = -1;
	}

	video->lasttime = realtime;

	return video;
}

clvideo_t *CL_GetVideoByName( const char *name )
{
	int i;

	for( i = 0 ; i < cl_num_videos ; i++ )
		if( cl_videos[ i ].state != CLVIDEO_UNUSED
			&&	!strcmp( cl_videos[ i ].cpif.name , name ) )
			break;
	if( i != cl_num_videos )
		return CL_GetVideoBySlot( i );
	else
		return NULL;
}

void CL_SetVideoState(clvideo_t *video, clvideostate_t state)
{
	if (!video)
		return;

	video->lasttime = realtime;
	video->state = state;
	if (state == CLVIDEO_FIRSTFRAME)
		CL_RestartVideo(video);
}

void CL_RestartVideo(clvideo_t *video)
{
	if (!video)
		return;

	// reset time
	video->starttime = video->lasttime = realtime;
	video->framenum = -1;

	// reopen stream
	video->close(video->stream);
	if (!OpenStream(video))
		video->state = CLVIDEO_UNUSED;
}

// close video
void CL_CloseVideo(clvideo_t * video)
{
	int i;

	if (!video || video->state == CLVIDEO_UNUSED)
		return;

	// close stream
	if (!video->suspended || video->state != CLVIDEO_FIRSTFRAME)
		video->close(video->stream);
	// unlink texture
	if (!video->suspended)
		UnlinkVideoTexture(video);
	// purge subtitles
	if (video->subtitles)
	{
		for (i = 0; i < video->subtitles; i++)
			Z_Free( video->subtitle_text[i] );
		video->subtitles = 0;
	}
	video->state = CLVIDEO_UNUSED;
}

// update all videos
void CL_Video_Frame(void) 
{
	clvideo_t *video;
	int destframe;
	int i;

	if (!cl_num_videos)
		return;
	for (video = cl_videos, i = 0 ; i < cl_num_videos ; video++, i++)
	{
		if (video->state != CLVIDEO_UNUSED && !video->suspended)
		{
			if (realtime - video->lasttime > CLTHRESHOLD)
			{
				SuspendVideo(video);
				continue;
			}
			if (video->state == CLVIDEO_PAUSE)
			{
				video->starttime = realtime - video->framenum * video->framerate;
				continue;
			}
			// read video frame from stream if time has come
			if (video->state == CLVIDEO_FIRSTFRAME )
				destframe = 0;
			else
				destframe = (int)((realtime - video->starttime) * video->framerate);
			if (destframe < 0)
				destframe = 0;
			if (video->framenum < destframe)
			{
				do {
					video->framenum++;
					if (video->decodeframe(video->stream, video->imagedata, cl_videormask, cl_videogmask, cl_videobmask, cl_videobytesperpixel, cl_videobytesperpixel * video->cpif.width))
					{ 
						// finished?
						CL_RestartVideo(video);
						if (video->state == CLVIDEO_PLAY)
							video->state = CLVIDEO_FIRSTFRAME;
						return;
					}
				} while(video->framenum < destframe);
				R_MarkDirtyTexture(video->cpif.tex);
			}
		}
	}

	// stop main video
	if (cl_videos->state == CLVIDEO_FIRSTFRAME)
		CL_VideoStop();

	// reduce range to exclude unnecessary entries
	while(cl_num_videos > 0 && cl_videos[cl_num_videos-1].state == CLVIDEO_UNUSED)
		cl_num_videos--;
}

void CL_Video_Shutdown( void )
{
	int i;
	for (i = 0 ; i < cl_num_videos ; i++)
		CL_CloseVideo(&cl_videos[ i ]);
}

void CL_PurgeOwner( int owner )
{
	int i;

	for (i = 0 ; i < cl_num_videos ; i++)
		if (cl_videos[i].ownertag == owner)
			CL_CloseVideo(&cl_videos[i]);
}

typedef struct
{
	dp_font_t *font;
	float x;
	float y;
	float width;
	float height;
	float alignment; // 0 = left, 0.5 = center, 1 = right
	float fontsize;
	float textalpha;
}
cl_video_subtitle_info_t;

float CL_DrawVideo_WordWidthFunc(void *passthrough, const char *w, size_t *length, float maxWidth)
{
	cl_video_subtitle_info_t *si = (cl_video_subtitle_info_t *) passthrough;

	if(w == NULL)
		return si->fontsize * si->font->maxwidth;
	if(maxWidth >= 0)
		return DrawQ_TextWidth_UntilWidth(w, length, si->fontsize, si->fontsize, false, si->font, -maxWidth); // -maxWidth: we want at least one char
	else if(maxWidth == -1)
		return DrawQ_TextWidth(w, *length, si->fontsize, si->fontsize, false, si->font);
	else
		return 0;
}

int CL_DrawVideo_DisplaySubtitleLine(void *passthrough, const char *line, size_t length, float width, qboolean isContinuation)
{
	cl_video_subtitle_info_t *si = (cl_video_subtitle_info_t *) passthrough;

	int x = (int) (si->x + (si->width - width) * si->alignment);
	if (length > 0)
		DrawQ_String(x, si->y, line, length, si->fontsize, si->fontsize, 1.0, 1.0, 1.0, si->textalpha, 0, NULL, false, si->font);
	si->y += si->fontsize;
	return 1;
}

int cl_videoplaying = false; // old, but still supported

void CL_DrawVideo(void)
{
	clvideo_t *video;
	float videotime, px, py, sx, sy, st[8], b;
	cl_video_subtitle_info_t si;
	int i;

	if (!cl_videoplaying)
		return;

	video = CL_GetVideoBySlot( 0 );

	// fix cvars
	if (cl_video_scale.value <= 0 || cl_video_scale.value > 1)
		Cvar_SetValueQuick( &cl_video_scale, 1);
	if (cl_video_brightness.value <= 0 || cl_video_brightness.value > 10)
		Cvar_SetValueQuick( &cl_video_brightness, 1);

	// calc video proportions
	px = 0;
	py = 0;
	sx = vid_conwidth.integer;
	sy = vid_conheight.integer;
	st[0] = 0.0; st[1] = 0.0; 
	st[2] = 1.0; st[3] = 0.0; 
	st[4] = 0.0; st[5] = 1.0; 
	st[6] = 1.0; st[7] = 1.0; 
	if (cl_video_keepaspectratio.integer)
	{
		float a = ((float)video->cpif.width / (float)video->cpif.height) / ((float)vid.width / (float)vid.height);
		if (cl_video_keepaspectratio.integer >= 2)
		{
			// clip instead of scale
			if (a < 1.0) // clip horizontally
			{
				st[1] = st[3] = (1 - a)*0.5;
				st[5] = st[7] = 1 - (1 - a)*0.5;
			}
			else if (a > 1.0) // clip vertically
			{
				st[0] = st[4] = (1 - 1/a)*0.5;
				st[2] = st[6] = (1/a)*0.5;
			}
		}
		else if (a < 1.0) // scale horizontally
		{
			px += sx * (1 - a) * 0.5;
			sx *= a;
		}
		else if (a > 1.0) // scale vertically
		{
			a = 1 / a;
			py += sy * (1 - a);
			sy *= a;
		}
	}

	if (cl_video_scale.value != 1)
	{
		px += sx * (1 - cl_video_scale.value) * 0.5;
		py += sy * (1 - cl_video_scale.value) * ((bound(-1, cl_video_scale_vpos.value, 1) + 1) / 2);
		sx *= cl_video_scale.value;
		sy *= cl_video_scale.value;
	}

	// calc brightness for fadein and fadeout effects
	b = cl_video_brightness.value;
	if (cl_video_fadein.value && (realtime - video->starttime) < cl_video_fadein.value)
		b = pow((realtime - video->starttime)/cl_video_fadein.value, 2);
	else if (cl_video_fadeout.value && ((video->starttime + video->framenum * video->framerate) - realtime) < cl_video_fadeout.value)
		b = pow(((video->starttime + video->framenum * video->framerate) - realtime)/cl_video_fadeout.value, 2);

	// draw black bg in case stipple is active or video is scaled
	if (cl_video_stipple.integer || px != 0 || py != 0 || sx != vid_conwidth.integer || sy != vid_conheight.integer)
		DrawQ_Fill(0, 0, vid_conwidth.integer, vid_conheight.integer, 0, 0, 0, 1, 0);

	// enable video-only polygon stipple (of global stipple is not active)
	if (qglPolygonStipple && !scr_stipple.integer && cl_video_stipple.integer)
	{
		GLubyte stipple[128];
		int i, s, width, parts;
	
		s = cl_video_stipple.integer;
		parts = (s & 007);
		width = (s & 070) >> 3;
		qglEnable(GL_POLYGON_STIPPLE);CHECKGLERROR // 0x0B42
		for(i = 0; i < 128; ++i)
		{
			int line = i/4;
			stipple[i] = ((line >> width) & ((1 << parts) - 1)) ? 0x00 : 0xFF;
		}
		qglPolygonStipple(stipple);CHECKGLERROR
	}

	// draw video
	DrawQ_SuperPic(px, py, &video->cpif, sx, sy, st[0], st[1], b, b, b, 1, st[2], st[3], b, b, b, 1, st[4], st[5], b, b, b, 1, st[6], st[7], b, b, b, 1, 0);

	// disable video-only stipple
	if (qglPolygonStipple && !scr_stipple.integer && cl_video_stipple.integer)
		qglDisable(GL_POLYGON_STIPPLE);CHECKGLERROR

	// VorteX: draw subtitle_text
	if (!video->subtitles || !cl_video_subtitles.integer)
		return;

	// find current subtitle
	videotime = realtime - video->starttime;
	for (i = 0; i < video->subtitles; i++)
	{
		if (videotime >= video->subtitle_start[i] && videotime <= video->subtitle_end[i])
		{
			// found, draw it
			si.font = FONT_NOTIFY;
			si.x = vid_conwidth.integer * 0.1;
			si.y = vid_conheight.integer - (max(1, cl_video_subtitles_lines.value) * cl_video_subtitles_textsize.value);
			si.width = vid_conwidth.integer * 0.8;
			si.height = max(1, cl_video_subtitles_lines.integer) * cl_video_subtitles_textsize.value;
			si.alignment = 0.5;
			si.fontsize = cl_video_subtitles_textsize.value;
			si.textalpha = min(1, (videotime - video->subtitle_start[i])/0.5) * min(1, ((video->subtitle_end[i] - videotime)/0.3)); // fade in and fade out
			COM_Wordwrap(video->subtitle_text[i], strlen(video->subtitle_text[i]), 0, si.width, CL_DrawVideo_WordWidthFunc, &si, CL_DrawVideo_DisplaySubtitleLine, &si);
			break;
		}
	}
}

void CL_VideoStart(char *filename, const char *subtitlesfile)
{
	Host_StartVideo();

	if( cl_videos->state != CLVIDEO_UNUSED )
		CL_CloseVideo( cl_videos );
	// already contains video/
	if( !OpenVideo( cl_videos, filename, va( CLDYNTEXTUREPREFIX "%s", filename ), 0, subtitlesfile ) )
		return;
	// expand the active range to include the new entry
	cl_num_videos = max(cl_num_videos, 1);

	cl_videoplaying = true;

	CL_SetVideoState( cl_videos, CLVIDEO_PLAY );
	CL_RestartVideo( cl_videos );
}

void CL_Video_KeyEvent( int key, int ascii, qboolean down ) 
{
	// only react to up events, to allow the user to delay the abortion point if it suddenly becomes interesting..
	if( !down ) {
		if( key == K_ESCAPE || key == K_ENTER || key == K_SPACE ) {
			CL_VideoStop();
		}
	}
}

void CL_VideoStop(void)
{
	cl_videoplaying = false;

	CL_CloseVideo( cl_videos );
}

static void CL_PlayVideo_f(void)
{
	char name[MAX_QPATH], subtitlesfile[MAX_QPATH];
	const char *extension;

	Host_StartVideo();

	if (COM_CheckParm("-benchmark"))
		return;

	if (Cmd_Argc() < 2)
	{
		Con_Print("usage: playvideo <videoname> [custom_subtitles_file]\nplays video named video/<videoname>.dpv\nif custom subtitles file is not presented\nit tries video/<videoname>.sub");
		return;
	}

	extension = FS_FileExtension(Cmd_Argv(1));
	if (extension[0])
		dpsnprintf(name, sizeof(name), "video/%s", Cmd_Argv(1));
	else
		dpsnprintf(name, sizeof(name), "video/%s.dpv", Cmd_Argv(1));
	if ( Cmd_Argc() > 2)
		CL_VideoStart(name, Cmd_Argv(2));
	else
	{
		dpsnprintf(subtitlesfile, sizeof(subtitlesfile), "video/%s.dpsubs", Cmd_Argv(1));
		CL_VideoStart(name, subtitlesfile);
	}
}

static void CL_StopVideo_f(void)
{
	CL_VideoStop();
}

static void cl_video_start( void )
{
	int i;
	clvideo_t *video;

	cl_videotexturepool = R_AllocTexturePool();

	for( video = cl_videos, i = 0 ; i < cl_num_videos ; i++, video++ )
		if( video->state != CLVIDEO_UNUSED && !video->suspended )
			LinkVideoTexture( video );
}

static void cl_video_shutdown( void )
{
	int i;
	clvideo_t *video;

	for( video = cl_videos, i = 0 ; i < cl_num_videos ; i++, video++ )
		if( video->state != CLVIDEO_UNUSED && !video->suspended )
			SuspendVideo( video );
	R_FreeTexturePool( &cl_videotexturepool );
}

static void cl_video_newmap( void )
{
}

void CL_Video_Init( void )
{
	union
	{
		unsigned char b[4];
		unsigned int i;
	}
	bgra;

	cl_num_videos = 0;
	cl_videobytesperpixel = 4;

	// set masks in an endian-independent way (as they really represent bytes)
	bgra.i = 0;bgra.b[0] = 0xFF;cl_videobmask = bgra.i;
	bgra.i = 0;bgra.b[1] = 0xFF;cl_videogmask = bgra.i;
	bgra.i = 0;bgra.b[2] = 0xFF;cl_videormask = bgra.i;

	Cmd_AddCommand( "playvideo", CL_PlayVideo_f, "play a .dpv video file" );
	Cmd_AddCommand( "stopvideo", CL_StopVideo_f, "stop playing a .dpv video file" );

	Cvar_RegisterVariable(&cl_video_subtitles);
	Cvar_RegisterVariable(&cl_video_subtitles_lines);
	Cvar_RegisterVariable(&cl_video_subtitles_textsize);
	Cvar_RegisterVariable(&cl_video_scale);
	Cvar_RegisterVariable(&cl_video_scale_vpos);
	Cvar_RegisterVariable(&cl_video_brightness);
	Cvar_RegisterVariable(&cl_video_stipple);
	Cvar_RegisterVariable(&cl_video_keepaspectratio);
	Cvar_RegisterVariable(&cl_video_fadein);
	Cvar_RegisterVariable(&cl_video_fadeout);

	R_RegisterModule( "CL_Video", cl_video_start, cl_video_shutdown, cl_video_newmap, NULL, NULL );
}