
#ifndef WAVEFILE_H
#define WAVEFILE_H

#include "quakedef.h"

typedef struct wavefile_s
{
	// file this is reading from
	qfile_t *file;

	// these settings are read directly from the wave format
	// 1 is uncompressed PCM
	unsigned int info_format;
	// how many samples per second
	unsigned int info_rate;
	// how many channels (1 = mono, 2 = stereo, 6 = 5.1 audio?)
	unsigned int info_channels;
	// how many bits per channel (8 or 16)
	unsigned int info_bits;

	// these settings are generated from the wave format
	// how many bytes in a sample (which may be one or two channels, thus 1 or 2 or 2 or 4, depending on info_bytesperchannel)
	unsigned int info_bytespersample;
	// how many bytes in channel (1 for 8bit, or 2 for 16bit)
	unsigned int info_bytesperchannel;

	// how many samples in the wave file
	unsigned int length;

	// how large the data chunk is
	unsigned int datalength;
	// beginning of data in data chunk
	unsigned int dataposition;

	// current position in stream (in samples)
	unsigned int position;

	// these are private to the wave file functions, just used for processing
	// size of *buffer
	unsigned int bufferlength;
	// buffer is reallocated if caller asks for more than fits
	void *buffer;

}
wavefile_t;

// opens a wave file, if an error occurs and errorstring is not NULL,
// *errorstring will be set to a message describing the error
wavefile_t *waveopen(char *filename, char **errorstring);
// closes a wave file
void waveclose(wavefile_t *f);

// reads some data from the file as 16bit stereo (converting if necessary)
// returns number of samples read (may be less than requested)
// if not all samples could be read, the remaining buffer will be unmodified
unsigned int waveread16stereo(wavefile_t *f, short *soundbuffer, unsigned int samples);

// seeks to a desired position in the wave
// returns 0 if successful, 1 if not successful
unsigned int waveseek(wavefile_t *f, unsigned int samples);

#endif
