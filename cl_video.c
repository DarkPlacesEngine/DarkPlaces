
#include "quakedef.h"
#include "cl_video.h"
#include "dpvsimpledecode.h"

// constants (and semi-constants)
static int  cl_videormask;
static int  cl_videobmask;
static int  cl_videogmask;
static int	cl_videobytesperpixel;

static clvideo_t videoarray[ MAXCLVIDEOS ];
static mempool_t *cl_videomempool;
static rtexturepool_t *cl_videotexturepool;

static clvideo_t *FindUnusedVid( void )
{
	int i;
	for( i = 1 ; i < MAXCLVIDEOS ; i++ )
		if( videoarray[ i ].state == CLVIDEO_UNUSED )
			return &videoarray[ i ];
	return NULL;
}

static qboolean OpenStream( clvideo_t * video )
{
	char *errorstring;
	video->stream = dpvsimpledecode_open( video->filename, &errorstring);
	if (!video->stream )
	{
		Con_Printf("unable to open \"%s\", error: %s\n", video->filename, errorstring);
		return false;
	}
	return true;
}

static void SuspendVideo( clvideo_t * video )
{
	if( video->suspended )
		return;
	video->suspended = true;
	// free the texture
	R_FreeTexture( video->cpif.tex );
	// free the image data
	Mem_Free( video->imagedata );
	// if we are in firstframe mode, also close the stream
	if( video->state == CLVIDEO_FIRSTFRAME ) 
		dpvsimpledecode_close( video->stream );	
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
		
	video->imagedata = Mem_Alloc( cl_videomempool, video->cpif.width * video->cpif.height * cl_videobytesperpixel );
	video->cpif.tex = R_LoadTexture2D( cl_videotexturepool, video->cpif.name, 
		video->cpif.width, video->cpif.height, NULL, TEXTYPE_RGBA, 0, NULL );    

	// update starttime
	video->starttime += realtime - video->lasttime;
	return true;
}

static clvideo_t* OpenVideo( clvideo_t *video, char *filename, char *name, int owner )
{
	strncpy( video->filename, filename, MAX_QPATH );
	video->ownertag = owner;
	strncpy( video->cpif.name, CLVIDEOPREFIX, MAX_QPATH );
	strncat( video->cpif.name, name, MAX_QPATH - sizeof( CLVIDEOPREFIX ) );

	if( !OpenStream( video ) )
		return NULL;

	video->state = CLVIDEO_FIRSTFRAME;
	video->framenum = -1;
	video->framerate = dpvsimpledecode_getframerate( video->stream );
	video->lasttime = realtime;

	video->cpif.width = dpvsimpledecode_getwidth( video->stream );
	video->cpif.height = dpvsimpledecode_getheight( video->stream );
	video->cpif.tex = R_LoadTexture2D( cl_videotexturepool, video->cpif.name, 
		video->cpif.width, video->cpif.height, NULL, TEXTYPE_RGBA, 0, NULL );

    video->imagedata = Mem_Alloc( cl_videomempool, video->cpif.width * video->cpif.height * cl_videobytesperpixel );

	return video;
}

clvideo_t* CL_OpenVideo( char *filename, char *name, int owner )
{
	clvideo_t *video;

	video = FindUnusedVid();
	if( !video ) {
		Con_Printf( "unable to open video \"%s\" - video limit reached\n", filename );
		return NULL;
	}
	return OpenVideo( video, filename, name, owner );
}

clvideo_t* CL_GetVideo( char *name )
{
	int i;
	clvideo_t *video;

	for( i = 0 ; i < MAXCLVIDEOS ; i++ )
		if( videoarray[ i ].state != CLVIDEO_UNUSED 
			&&	!strcmp( videoarray[ i ].cpif.name , name ) )
			break;
	if( i == MAXCLVIDEOS )
		return NULL;
	video = &videoarray[ i ];

	if( video->suspended )
		if( !WakeVideo( video ) )
			return NULL;
	video->lasttime = realtime;

	return video;
}

void CL_StartVideo( clvideo_t * video )
{
	if( !video )
		return;

	video->starttime = video->lasttime = realtime;
	video->framenum = -1;
	video->state = CLVIDEO_PLAY;
}

void CL_LoopVideo( clvideo_t * video )
{
	if( !video )
		return;

	video->starttime = video->lasttime = realtime;
	video->framenum = -1;
	video->state = CLVIDEO_LOOP;
}

void CL_PauseVideo( clvideo_t * video )
{
	if( !video )
		return;

	video->state = CLVIDEO_PAUSE;
	video->lasttime = realtime;
}

void CL_RestartVideo( clvideo_t *video )
{
	if( !video )
		return;
    
	video->starttime = video->lasttime = realtime;
	video->framenum = -1;
}

void CL_StopVideo( clvideo_t * video )
{
	if( !video )
		return;

	video->lasttime = realtime;
	video->framenum = -1;
	video->state = CLVIDEO_FIRSTFRAME;
}

void CL_CloseVideo( clvideo_t * video )
{
	if( !video || video->state == CLVIDEO_UNUSED )
		return;

	video->state = CLVIDEO_UNUSED;
	
	if( !video->suspended || video->state != CLVIDEO_FIRSTFRAME )
		dpvsimpledecode_close( video->stream );
	if( !video->suspended ) {
		Mem_Free( video->imagedata );
		R_FreeTexture( video->cpif.tex );
	}
}

static void VideoFrame( clvideo_t *video )
{
	int destframe;

	if( video->state == CLVIDEO_FIRSTFRAME )
		destframe = 0;
	else
		destframe = (realtime - video->starttime) * video->framerate;
	if( destframe < 0 )
		destframe = 0;
	if( video->framenum < destframe ) {
		do {
			video->framenum++;
			if( dpvsimpledecode_video( video->stream, video->imagedata, cl_videormask, 
				cl_videogmask, cl_videobmask, cl_videobytesperpixel, 
				cl_videobytesperpixel * video->cpif.width ) 
				) { // finished?
				video->framenum = -1;
				if( video->state == CLVIDEO_LOOP )
						video->starttime = realtime;
				else if( video->state == CLVIDEO_PLAY )
						video->state = CLVIDEO_FIRSTFRAME;
				return;
			}
		} while( video->framenum < destframe );
		R_UpdateTexture( video->cpif.tex, video->imagedata );
	}					
}

void CL_VideoFrame( void ) // update all videos
{
	int i;
	clvideo_t *video;

	for( video = videoarray, i = 0 ; i < MAXCLVIDEOS ; video++, i++ )
		if( video->state != CLVIDEO_UNUSED && !video->suspended )
			if( realtime - video->lasttime > CLTHRESHOLD )
				SuspendVideo( video );
			else if( video->state == CLVIDEO_PAUSE )
				video->starttime = realtime + video->framenum * video->framerate;
			else 
				VideoFrame( video );

	if( videoarray->state == CLVIDEO_FIRSTFRAME )
		CL_VideoStop();
}

void CL_Video_Shutdown( void )
{
	int i;
	for( i = 0 ; i < MAXCLVIDEOS ; i++ )
		CL_CloseVideo( &videoarray[ i ] );

	R_FreeTexturePool( &cl_videotexturepool );
	Mem_FreePool( &cl_videomempool );
}

void CL_PurgeOwner( int owner )
{
	int i;
	for( i = 0 ; i < MAXCLVIDEOS ; i++ )
		if( videoarray[ i ].ownertag == owner )
			CL_CloseVideo( &videoarray[ i ] );
}

int cl_videoplaying = false; // old, but still supported

void CL_DrawVideo(void)
{
	if (cl_videoplaying)
		DrawQ_Pic(0, 0, videoarray->cpif.name, vid.conwidth, vid.conheight, 1, 1, 1, 1, 0);
}

void CL_VideoStart(char *filename)
{
	if( videoarray->state != CLVIDEO_UNUSED )
		CL_CloseVideo( videoarray );
	if( !OpenVideo( videoarray, filename, filename, 0 ) )
		return;

	cl_videoplaying = true;

	CL_StartVideo( videoarray );
}

void CL_VideoStop(void)
{
	cl_videoplaying = false;

	CL_CloseVideo( videoarray );
}

static void CL_PlayVideo_f(void)
{
	char name[1024];

	if (Cmd_Argc() != 2)
	{
		Con_Print("usage: playvideo <videoname>\nplays video named video/<videoname>.dpv\n");
		return;
	}

	sprintf(name, "video/%s.dpv", Cmd_Argv(1));
	CL_VideoStart(name);
}

static void CL_StopVideo_f(void)
{
	CL_VideoStop();
}

void CL_Video_Init( void )
{
	cl_videobytesperpixel = 4;
	cl_videormask = BigLong(0xFF000000);
	cl_videogmask = BigLong(0x00FF0000);
	cl_videobmask = BigLong(0x0000FF00);

	cl_videomempool = Mem_AllocPool( "CL_Video", 0, NULL );
	cl_videotexturepool = R_AllocTexturePool();

	Cmd_AddCommand( "playvideo", CL_PlayVideo_f );
	Cmd_AddCommand( "stopvideo", CL_StopVideo_f );
}
