
#include "quakedef.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "wavefile.h"

wavefile_t *waveopen(char *filename, char **errorstring)
{
	int validfmt, position, length, l;
	char *error;
	wavefile_t *w;
	qfile_t *file;
	unsigned char buffer[1024];
	error = NULL;
	file = FS_Open (filename, "rb", true);
	if (file)
	{
		w = malloc(sizeof(*w));
		memset(w, 0, sizeof(*w));
		if (w)
		{
			w->file = file;
			if (FS_Read (w->file, buffer, 12))
			{
				if (!memcmp(buffer, "RIFF", 4))
				{
					if (!memcmp(buffer + 8, "WAVE", 4))
					{
						validfmt = 0;
						for(;;)
						{
							if (!FS_Read(w->file, buffer, 8))
							{
								//error = "error reading chunk\n");
								break;
							}
							position = FS_Tell(w->file);
							length = buffer[4] | (buffer[5] << 8) | (buffer[6] << 16) | (buffer[7] << 24);
							if (!memcmp(buffer, "fmt ", 4))
							{
								validfmt = 0;
								l = length;
								if (l > 16)
									l = 16;
								if (!FS_Read(w->file, buffer, l))
								{
									error = "error reading \"fmt \" chunk\n";
									break;
								}
								w->info_format = buffer[0] | (buffer[1] << 8);
								if (w->info_format != 1)
								{
									error = "only PCM format supported\n";
									break;
								}
								w->info_channels = buffer[2] | (buffer[3] << 8);
								if (w->info_channels != 1 && w->info_channels != 2)
								{
									error = "only mono and stereo supported\n";
									break;
								}
								w->info_rate = buffer[4] | (buffer[5] << 8) | (buffer[6] << 16) | (buffer[7] << 24);
								if (w->info_rate < 1)
								{
									error = "only rates 1hz-100khz supported\n";
									break;
								}
								w->info_bits = buffer[14] | (buffer[15] << 8);
								if (w->info_bits != 8 && w->info_bits != 16)
								{
									error = "only 8bit and 16bit supported\n";
									break;
								}
								validfmt = 1;
							}
							else if (!memcmp(buffer, "data", 4))
							{
								if (validfmt)
								{
									w->datalength = length;
									w->dataposition = position;
								}
							}
							// other chunks that might be of interest:
							// "cue " (for looping)
							if (FS_Seek(w->file, position + length, SEEK_SET))
							{
								error = "error seeking to next chunk\n";
								break;
							}
						}
						if (w->datalength && validfmt)
						{
							w->info_bytesperchannel = w->info_bits / 8;
							w->info_bytespersample = w->info_channels * w->info_bytesperchannel;
							w->length = w->datalength / w->info_bytespersample;
							w->position = 0;
							FS_Seek(w->file, w->dataposition, SEEK_SET);
							return w;
						}
					}
					else
						error = "not a RIFF WAVE file\n";
				}
				else
					error = "not a RIFF file\n";
			}
			else
				error = "error reading file\n";
			free(w);
		}
		else
			error = "unable to allocate memory\n";
		FS_Close(file);
	}
	else
		error = "unable to open file\n";
	if (errorstring)
		*errorstring = error;
	return NULL;
}

void waveclose(wavefile_t *f)
{
	if (f)
	{
		FS_Close(f->file);
		free(f);
	}
}

unsigned int waveread16stereo(wavefile_t *w, short *soundbuffer, unsigned int samples)
{
	unsigned int i;
	unsigned int length;
	unsigned char *in;
	short *out;
	length = samples;
	if (length > w->length - w->position)
		length = w->length - w->position;
	if (w->bufferlength < length)
	{
		if (w->buffer)
			free(w->buffer);
		w->bufferlength = length + 100;
		w->buffer = malloc(w->bufferlength * w->info_bytespersample);
	}
	length = FS_Read(w->file, w->buffer, w->info_bytespersample * length) / w->info_bytespersample;
	w->position += length;
	if (length > 0)
	{
		if (w->info_bytesperchannel == 2)
		{
			if (w->info_channels == 2)
			{
				for (i = 0, in = w->buffer, out = soundbuffer;i < length;i++, in += 4, out += 2)
				{
					out[0] = in[0] | (in[1] << 8);
					out[1] = in[2] | (in[3] << 8);
				}
			}
			else
				for (i = 0, in = w->buffer, out = soundbuffer;i < length;i++, in += 2, out += 2)
					out[0] = out[1] = in[0] | (in[1] << 8);
		}
		else
		{
			if (w->info_channels == 2)
			{
				for (i = 0, in = w->buffer, out = soundbuffer;i < length;i++, in += 2, out += 2)
				{
					out[0] = (in[0] - 128) << 8;
					out[1] = (in[1] - 128) << 8;
				}
			}
			else
				for (i = 0, in = w->buffer, out = soundbuffer;i < length;i++, in += 1, out += 2)
					out[0] = out[1] = (in[0] - 128) << 8;
		}
	}
	return length;
}

unsigned int waveseek(wavefile_t *w, unsigned int samples)
{
	if (samples > w->datalength)
		return 1;
	else
	{
		w->position = samples;
		FS_Seek(w->file, w->dataposition + w->position * w->info_bytespersample, SEEK_SET);
		return 0;
	}
}
