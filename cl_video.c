
#include "quakedef.h"
#include "cl_video.h"
#include "dpvsimpledecode.h"

mempool_t *clvideomempool;

int cl_videoplaying = false;
void *cl_videostream;

double cl_videostarttime;
int cl_videoframenum;
double cl_videoframerate;

int cl_videoimagewidth;
int cl_videoimageheight;
int cl_videoimagedata_rmask;
int cl_videoimagedata_gmask;
int cl_videoimagedata_bmask;
int cl_videoimagedata_bytesperpixel;
void *cl_videoimagedata;

int cl_videosoundrate;
int cl_videosoundlength;
short *cl_videosounddata;
int cl_videosoundresamplelength;
short *cl_videosoundresampledata;

rtexture_t *cl_videotexture;
rtexturepool_t *cl_videotexturepool;

void CL_VideoFrame(void)
{
	int frames, framenum, samples, s;
	if (!cl_videoplaying)
		return;
	framenum = (realtime - cl_videostarttime) * cl_videoframerate;
	//Con_Printf("frame %i\n", framenum);
	if (framenum < 0)
		framenum = 0;
	frames = 0;
	while (cl_videoframenum < framenum)
	{
		frames++;
		cl_videoframenum++;
		if (dpvsimpledecode_video(cl_videostream, cl_videoimagedata, cl_videoimagedata_rmask, cl_videoimagedata_gmask, cl_videoimagedata_bmask, cl_videoimagedata_bytesperpixel, cl_videoimagewidth * cl_videoimagedata_bytesperpixel))
		{
			CL_VideoStop();
			return;
		}
	}
	if (frames)
	{
		R_UpdateTexture(cl_videotexture, cl_videoimagedata);
		//Draw_NewPic("engine_videoframe", cl_videoimagewidth, cl_videoimageheight, false, cl_videoimagedata);
	}
	if (cl_videosoundrate && (samples = S_RawSamples_QueueWantsMore()))
	{
		Con_DPrintf("%i = S_RawSamples_QueueWantsMore()\n", samples);

		// calculate how much source data we need to fill the output...
		s = samples * cl_videosoundrate / S_RawSamples_SampleRate();

		// reallocate processing buffer if needed
		if (cl_videosoundresamplelength < samples)
		{
			cl_videosoundresamplelength = samples + 100;
			if (cl_videosoundresampledata)
				Mem_Free(cl_videosoundresampledata);
			cl_videosoundresampledata = Mem_Alloc(clvideomempool, cl_videosoundresamplelength * sizeof(short[2]));
		}

		// reallocate loading buffer if needed
		if (cl_videosoundlength < s)
		{
			cl_videosoundlength = s + 100;
			if (cl_videosounddata)
				Mem_Free(cl_videosounddata);
			cl_videosounddata = Mem_Alloc(clvideomempool, cl_videosoundlength * sizeof(short[2]));
		}

		dpvsimpledecode_audio(cl_videostream, cl_videosounddata, s);
		S_ResampleBuffer16Stereo(cl_videosounddata, s, cl_videosoundresampledata, samples);
		S_RawSamples_Enqueue(cl_videosoundresampledata, samples);
	}
}

void CL_DrawVideo(void)
{
	if (cl_videoplaying)
	{
		drawqueuemesh_t mesh;
		float vertex3f[12];
		float texcoord2f[8];
		float color4f[16];
		float s1, t1, s2, t2, x1, y1, x2, y2;
		x1 = 0;
		y1 = 0;
		x2 = vid.conwidth;
		y2 = vid.conheight;
		R_FragmentLocation(cl_videotexture, NULL, NULL, &s1, &t1, &s2, &t2);
		texcoord2f[0] = s1;texcoord2f[1] = t1;
		texcoord2f[2] = s2;texcoord2f[3] = t1;
		texcoord2f[4] = s2;texcoord2f[5] = t2;
		texcoord2f[6] = s1;texcoord2f[7] = t2;
		R_FillColors(color4f, 4, 1, 1, 1, 1);
		vertex3f[ 0] = x1;vertex3f[ 1] = y1;vertex3f[ 2] = 0;
		vertex3f[ 3] = x2;vertex3f[ 4] = y1;vertex3f[ 5] = 0;
		vertex3f[ 6] = x2;vertex3f[ 7] = y2;vertex3f[ 8] = 0;
		vertex3f[ 9] = x1;vertex3f[10] = y2;vertex3f[11] = 0;
		mesh.texture = cl_videotexture;
		mesh.num_triangles = 2;
		mesh.num_vertices = 4;
		mesh.data_element3i = polygonelements;
		mesh.data_vertex3f = vertex3f;
		mesh.data_texcoord2f = texcoord2f;
		mesh.data_color4f = color4f;
		DrawQ_Mesh(&mesh, 0);
		//DrawQ_Pic(0, 0, "engine_videoframe", vid.conwidth, vid.conheight, 1, 1, 1, 1, 0);
	}
}

void CL_VideoStart(char *filename)
{
	char *errorstring;
	cl_videostream = dpvsimpledecode_open(filename, &errorstring);
	if (!cl_videostream)
	{
		Con_Printf("unable to open \"%s\", error: %s\n", filename, errorstring);
		return;
	}

	cl_videoplaying = true;
	cl_videostarttime = realtime;
	cl_videoframenum = -1;
	cl_videoframerate = dpvsimpledecode_getframerate(cl_videostream);
	cl_videoimagewidth = dpvsimpledecode_getwidth(cl_videostream);
	cl_videoimageheight = dpvsimpledecode_getheight(cl_videostream);

	// RGBA format
	cl_videoimagedata_bytesperpixel = 4;
	cl_videoimagedata_rmask = BigLong(0xFF000000);
	cl_videoimagedata_gmask = BigLong(0x00FF0000);
	cl_videoimagedata_bmask = BigLong(0x0000FF00);
	cl_videoimagedata = Mem_Alloc(clvideomempool, cl_videoimagewidth * cl_videoimageheight * cl_videoimagedata_bytesperpixel);
	//memset(cl_videoimagedata, 97, cl_videoimagewidth * cl_videoimageheight * cl_videoimagedata_bytesperpixel);

	cl_videosoundrate = dpvsimpledecode_getsoundrate(cl_videostream);
	cl_videosoundlength = 0;
	cl_videosounddata = NULL;
	cl_videosoundresamplelength = 0;
	cl_videosoundresampledata = NULL;

	cl_videotexturepool = R_AllocTexturePool();
	cl_videotexture = R_LoadTexture2D(cl_videotexturepool, "videotexture", cl_videoimagewidth, cl_videoimageheight, NULL, TEXTYPE_RGBA, TEXF_FRAGMENT, NULL);
}

void CL_VideoStop(void)
{
	cl_videoplaying = false;

	S_RawSamples_ClearQueue();

	if (cl_videostream)
		dpvsimpledecode_close(cl_videostream);
	cl_videostream = NULL;

	if (cl_videoimagedata)
		Mem_Free(cl_videoimagedata);
	cl_videoimagedata = NULL;

	if (cl_videosounddata)
		Mem_Free(cl_videosounddata);
	cl_videosounddata = NULL;

	if (cl_videosoundresampledata)
		Mem_Free(cl_videosoundresampledata);
	cl_videosoundresampledata = NULL;

	cl_videotexture = NULL;
	R_FreeTexturePool(&cl_videotexturepool);

	Draw_FreePic("engine_videoframe");
}

static void CL_PlayVideo_f(void)
{
	char name[1024];

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("usage: playvideo <videoname>\nplays video named video/<videoname>.dpv\n");
		return;
	}

	sprintf(name, "video/%s.dpv", Cmd_Argv(1));
	CL_VideoStart(name);
}

static void CL_StopVideo_f(void)
{
	CL_VideoStop();
}

void CL_Video_Init(void)
{
	Cmd_AddCommand("playvideo", CL_PlayVideo_f);
	Cmd_AddCommand("stopvideo", CL_StopVideo_f);

	clvideomempool = Mem_AllocPool("CL_Video");
}
