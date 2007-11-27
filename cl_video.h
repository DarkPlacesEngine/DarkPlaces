
#ifndef CL_VIDEO_H
#define CL_VIDEO_H

#define MAXCLVIDEOS		64 + 1 // 1 video is reserved for the cinematic mode
// yields DYNAMIC_TEXTURE_PATH_PREFIX CLVIDEOPREFIX video name for a path
#define CLVIDEOPREFIX	CLDYNTEXTUREPREFIX "video/"
#define CLTHRESHOLD		2.0

#define MENUOWNER		1

typedef enum clvideostate_e
{
	CLVIDEO_UNUSED,
	CLVIDEO_PLAY,
	CLVIDEO_LOOP,
	CLVIDEO_PAUSE,
	CLVIDEO_FIRSTFRAME,
	CLVIDEO_RESETONWAKEUP,
	CLVIDEO_STATECOUNT
} clvideostate_t;

typedef struct clvideo_s
{
	int		ownertag;
	clvideostate_t state;

	// private stuff
	void	*stream;

	double	starttime;
	int		framenum;
	double	framerate;

	void	*imagedata;

	cachepic_t cpif;

	// if a video is suspended, it is automatically paused (else we'd still have to process the frames)

	// used to determine whether the video's resources should be freed or not
    double  lasttime;
	// when lasttime - realtime > THRESHOLD, all but the stream is freed
	qboolean suspended;

	char	filename[MAX_QPATH];
} clvideo_t;

clvideo_t*	CL_OpenVideo( const char *filename, const char *name, int owner );
clvideo_t*	CL_GetVideoByName( const char *name );
void		CL_SetVideoState( clvideo_t *video, clvideostate_t state );
void		CL_RestartVideo( clvideo_t *video );

void		CL_CloseVideo( clvideo_t * video );
void		CL_PurgeOwner( int owner );

void		CL_Video_Frame( void ); // update all videos
void		CL_Video_Init( void );
void		CL_Video_Shutdown( void );

// old interface
extern int cl_videoplaying;

void CL_DrawVideo( void );
void CL_VideoStart( char *filename );
void CL_VideoStop( void );

// new function used for fullscreen videos
// TODO: Andreas Kirsch: move this subsystem somewhere else (preferably host) since the cl_video system shouldnt do such work like managing key events..
void CL_Video_KeyEvent( int key, int ascii, qboolean down );

#endif
