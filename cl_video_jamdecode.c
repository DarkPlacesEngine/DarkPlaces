// JAM format decoder, used by Blood Omnicide

typedef struct jamdecodestream_s
{
	int error;

	qfile_t *file;
	double info_framerate;
	unsigned int info_frames;
	unsigned int info_imagewidth;
	unsigned int info_imageheight;
	int doubleres;
	float colorscale;
	unsigned char colorsub;
	float stipple;

	// info used durign decoding
	unsigned char *videopixels;
	unsigned char *compressed;
	unsigned char *framedata;
	unsigned char *prevframedata;
	unsigned char colormap[768];
	unsigned int framesize;
	unsigned int framenum;

	// channel the sound file is being played on
	int sndchan;
}
jamdecodestream_t;

#define JAMDECODEERROR_NONE                0
#define JAMDECODEERROR_EOF                 1
#define JAMDECODEERROR_READERROR           2
#define JAMDECODEERROR_BAD_FRAME_HEADER    3
#define JAMDECODEERROR_BAD_OUTPUT_SIZE     4
#define JAMDECODEERROR_BAD_COLORMAP        5

// opens a stream
void jam_close(void *stream);
unsigned int jam_getwidth(void *stream);
unsigned int jam_getheight(void *stream);
double jam_getframerate(void *stream);
int jam_video(void *stream, void *imagedata, unsigned int Rmask, unsigned int Gmask, unsigned int Bmask, unsigned int bytesperpixel, int imagebytesperrow);
void *jam_open(clvideo_t *video, char *filename, const char **errorstring)
{
	unsigned char jamHead[16];
	char *wavename;
	jamdecodestream_t *s;
	qfile_t *file;

	s = (jamdecodestream_t *)Z_Malloc(sizeof(jamdecodestream_t));
	if (s != NULL)
	{
		if ((file = FS_OpenVirtualFile(filename, false)))
		{
			s->file = file;
			if (FS_Read(s->file, &jamHead, 16))
			{
				if (!memcmp(jamHead, "JAM", 3))
				{
					s->info_imagewidth = LittleLong(*(jamHead + 4));
					s->info_imageheight = LittleLong(*(jamHead + 8));
					s->info_frames = LittleLong(*(jamHead + 12));
					s->info_framerate = 15;
					s->doubleres = 0;
					s->colorscale = 0.70;
					s->colorsub = 8;
					s->stipple = 0.4;
					s->framesize = s->info_imagewidth * s->info_imageheight;
					if (s->framesize > 0)
					{
						s->compressed = (unsigned char *)Z_Malloc(s->framesize);
						s->framedata = (unsigned char *)Z_Malloc(s->framesize * 2);
						s->prevframedata = (unsigned char *)Z_Malloc(s->framesize * 2);
						s->videopixels = (unsigned char *)Z_Malloc(s->framesize * 4); // bgra, doubleres
						if (s->compressed != NULL && s->framedata != NULL && s->prevframedata != NULL && s->videopixels != NULL)
						{
							size_t namelen;

							namelen = strlen(filename) + 10;
							wavename = (char *)Z_Malloc(namelen);
							if (wavename)
							{
								sfx_t* sfx;

								FS_StripExtension(filename, wavename, namelen);
								strlcat(wavename, ".wav", namelen);
								sfx = S_PrecacheSound(wavename, false, false);
								if (sfx != NULL)
									s->sndchan = S_StartSound (-1, 0, sfx, vec3_origin, 1.0f, 0);
								else
									s->sndchan = -1;
								Z_Free(wavename);
							}
							// all is well...
							// set the module functions
							s->framenum = 0;
							video->close = jam_close;
							video->getwidth = jam_getwidth;
							video->getheight = jam_getheight;
							video->getframerate = jam_getframerate;
							video->decodeframe = jam_video;
							return s;
						}
						else if (errorstring != NULL)
							*errorstring = "unable to allocate memory for stream info structure";
						if (s->compressed != NULL)
							Z_Free(s->compressed);
						if (s->framedata != NULL)
							Z_Free(s->framedata);
						if (s->prevframedata != NULL)
							Z_Free(s->prevframedata);
						if (s->videopixels != NULL)
							Z_Free(s->videopixels);
					}
					else if (errorstring != NULL)
						*errorstring = "bad framesize";
				}
				else if (errorstring != NULL)
					*errorstring = "not JAM videofile";
			}
			else if (errorstring != NULL)
				*errorstring = "unexpected EOF";
			FS_Close(file);
		}
		else if (errorstring != NULL)
			*errorstring = "unable to open videofile";
		Z_Free(s);
	}
	else if (errorstring != NULL)
		*errorstring = "unable to allocate memory for stream info structure";
	return NULL;
}

// closes a stream
void jam_close(void *stream)
{
	jamdecodestream_t *s = (jamdecodestream_t *)stream;
	if (s == NULL)
		return;
	Z_Free(s->compressed);
	Z_Free(s->framedata);
	Z_Free(s->prevframedata);
	Z_Free(s->videopixels);
	if (s->sndchan != -1)
		S_StopChannel(s->sndchan, true, true);
	if (s->file)
		FS_Close(s->file);
	Z_Free(s);
}

// returns the width of the image data
unsigned int jam_getwidth(void *stream)
{
	jamdecodestream_t *s = (jamdecodestream_t *)stream;
	if (s->doubleres)
		return s->info_imagewidth * 2;
	return s->info_imagewidth;
}

// returns the height of the image data
unsigned int jam_getheight(void *stream)
{
	jamdecodestream_t *s = (jamdecodestream_t *)stream;
	if (s->doubleres)
		return s->info_imageheight * 2;
	return s->info_imageheight;
}

// returns the framerate of the stream
double jam_getframerate(void *stream)
{
	jamdecodestream_t *s = (jamdecodestream_t *)stream;
	return s->info_framerate;
}


// decode JAM frame
void jam_decodeframe(unsigned char *inbuf, unsigned char *outbuf, unsigned char *prevbuf, int outsize, int frametype)
{
	unsigned char *srcptr, *destptr, *prevptr;
	int bytesleft;
	unsigned int mark;
	unsigned short int bits;
	int rep;
	int backoffs;
	unsigned char *back;
	int i;

	srcptr = inbuf;
	destptr = outbuf;
	prevptr = prevbuf;
	bytesleft = outsize;

	if (frametype == 2)
	{
		memcpy(outbuf, inbuf, outsize);
		return;
	}
	while(bytesleft > 0)
	{
		memcpy(&mark, srcptr, 4);
		srcptr += 4;
		for(i=0; i<32 && bytesleft > 0; i++,mark=mark>>1)
		{
			if(mark & 1)
			{
				*destptr = *srcptr;
				destptr ++;
				prevptr ++;
				srcptr ++;
				bytesleft --;
			}
			else
			{
				bits = srcptr[0] + 256*srcptr[1];
				rep = (bits >> 11) + 3;
				if(frametype == 1)
				{
					backoffs = 0x821 - (bits & 0x7ff);
					back = destptr - backoffs;
				}
				else
				{
					backoffs = 0x400 - (bits & 0x7ff);
					back = prevptr - backoffs;
				}
				srcptr += 2;
				memcpy(destptr, back, rep);
				destptr += rep;
				prevptr += rep;
				bytesleft -= rep;
			}
		}
	}
}

// decodes a video frame to the supplied output pixels
int jam_video(void *stream, void *imagedata, unsigned int Rmask, unsigned int Gmask, unsigned int Bmask, unsigned int bytesperpixel, int imagebytesperrow)
{
	unsigned char frameHead[16], *b;
	unsigned int compsize, outsize, i, j;
	jamdecodestream_t *s = (jamdecodestream_t *)stream;

	s->error = DPVSIMPLEDECODEERROR_NONE;
	if (s->framenum < s->info_frames)
	{
readframe:
		if (FS_Read(s->file, &frameHead, 16))
		{
			compsize = LittleLong(*(frameHead + 8)) - 16;
			outsize = LittleLong(*(frameHead + 12));
			if (compsize > s->framesize || outsize > s->framesize)
				s->error = JAMDECODEERROR_BAD_FRAME_HEADER;
			else if (FS_Read(s->file, s->compressed, compsize))
			{
				// palette goes interleaved with special flag
				if (frameHead[0] == 2)
				{
					if (compsize == 768)
					{
						memcpy(s->colormap, s->compressed, 768);
						for(i = 0; i < 768; i++)
							s->colormap[i] = (unsigned char)(bound(0, (s->colormap[i] * s->colorscale) - s->colorsub, 255));
						goto readframe;
					}
					//else
					//	s->error = JAMDECODEERROR_BAD_COLORMAP;
				}
				else
				{
					// decode frame
					// shift buffers to provide current and previous one, decode
					b = s->prevframedata;
					s->prevframedata = s->framedata;
					s->framedata = b;
					jam_decodeframe(s->compressed, s->framedata, s->prevframedata, outsize, frameHead[4]);
					// make 32bit imagepixels from 8bit palettized frame
					if (s->doubleres)
						b = s->videopixels;
					else
						b = (unsigned char *)imagedata;
					for(i = 0; i < s->framesize; i++)
					{
						// bgra
						*b++ = s->colormap[s->framedata[i]*3 + 2];
						*b++ = s->colormap[s->framedata[i]*3 + 1];
						*b++ = s->colormap[s->framedata[i]*3];
						*b++ = 255;
					}
					// nearest 2x
					if (s->doubleres)
					{
						for (i = 0; i < s->info_imageheight; i++)
						{
							b = (unsigned char *)imagedata + (s->info_imagewidth*2*4)*(i*2);
							for (j = 0; j < s->info_imagewidth; j++)
							{
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 1];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 2];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 3];
								//
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 1];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 2];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 3];
							}
							b = (unsigned char *)imagedata + (s->info_imagewidth*2*4)*(i*2 + 1);
							for (j = 0; j < s->info_imagewidth; j++)
							{
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 1];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 2];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 3];
								//
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 1];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 2];
								*b++ = s->videopixels[i*s->info_imagewidth*4 + j*4 + 3];
							}
						}
						// do stippling
						if (s->stipple)
						{
							for (i = 0; i < s->info_imageheight; i++)
							{
								b = (unsigned char *)imagedata + (s->info_imagewidth * 4 * 2 * 2 * i);
								for (j = 0; j < s->info_imagewidth; j++)
								{
									b[0] = b[0] * s->stipple;
									b[1] = b[1] * s->stipple;
									b[2] = b[2] * s->stipple;
									b += 4;
									b[0] = b[0] * s->stipple;
									b[1] = b[1] * s->stipple;
									b[2] = b[2] * s->stipple;
									b += 4;
								}
							}
						}
					}

				}
			}
			else
				s->error = JAMDECODEERROR_READERROR;
		}
		else
			s->error = JAMDECODEERROR_READERROR;
	}
	else
		s->error = DPVSIMPLEDECODEERROR_EOF;
	return s->error;
}
