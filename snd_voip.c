#include <opus.h>
#include <stdint.h>

#include "quakedef.h"
#include "sound.h"
#include "snd_voip.h"

#define VOIP_OPUS_BITRATE 48000

static void Buffer_Shift(char *buffer, int *buffer_begin, int *buffer_filled, int buffer_size)
{
	if (*buffer_begin >= buffer_size / 2 + 64) {
		memcpy(buffer, buffer + *buffer_begin - 64, *buffer_filled - *buffer_begin + 64);
		*buffer_filled -= (*buffer_begin - 64);
		*buffer_begin = 64;
	}
}

static void Buffer_Shift_Force(char *buffer, int *buffer_begin, int *buffer_filled, int buffer_size, int *buffer_free)
{
	if (*buffer_filled > buffer_size / 2 + 64)
	{
		*buffer_begin = buffer_size / 2 + 64;
		Buffer_Shift(buffer, &*buffer_begin, &*buffer_filled, buffer_size);
		*buffer_free = buffer_size - *buffer_filled;
	}
}

#define CAPTURE_BUFFER_SIZE 16384
#define CAPTURE_BUFFER_ECHO_SIZE 65536
static char *capture_buffer;
static int capture_buffer_begin;
static int capture_buffer_filled;
static long int capture_buffer_pos;
static char *capture_buffer_echo;
static int capture_buffer_echo_begin;
static int capture_buffer_echo_filled;
static long int capture_buffer_echo_pos;
static qbool snd_voip_active;
static sfx_t *snd_echo;
static qbool snd_echo_active;
static OpusEncoder *opus_encoder;
// kept for whoever wires up the netmessage path (see TODO in
// S_VOIP_Capture_Callback below); unused until then.
static char opus_encoder_id;
static unsigned int opus_encoder_seq;
extern cvar_t snd_input_boost;
extern cvar_t snd_input_boost_auto;
void S_VOIP_Capture_Callback (unsigned char *stream, int len)
{
	if (snd_input_boost_auto.integer)
	{
		int len16 = len / 2;
		int i;
		int16_t sample;
		long unsigned int mediumlevel = 0;
		static float boost = 1;
		for (i = 0; i < len16; i++)
		{
			sample = ((int16_t*)stream)[i];
			if (sample > 0)
				mediumlevel += (int)(sample * boost);
			else
				mediumlevel += (int)(-sample * boost);

		}
		mediumlevel /= len16;
		if (mediumlevel > 128)
		{
			if (mediumlevel > 8192 || mediumlevel < 1024)
				boost = (4096.0f / mediumlevel);
			else
				boost = boost * 0.8 + (4096.0f / mediumlevel) * 0.2;

			boost = bound(0.5, boost, 10);
		}
		for (i = 0; i < len16; i++)
		{
			((int16_t*)stream)[i] = ((int16_t*)stream)[i] * boost;
		}
	}
	else if (snd_input_boost.value > 0 && snd_input_boost.value != 1)
	{
		int len16 = len / 2;
		int i;
		for (i = 0; i < len16; i++)
		{
			((int16_t*)stream)[i] = ((int16_t*)stream)[i] * snd_input_boost.value;
		}
	}
	if (snd_voip_active)
	{
		int buffer_free = CAPTURE_BUFFER_SIZE - capture_buffer_filled;
		if (len > buffer_free)
		{
			Buffer_Shift_Force(capture_buffer, &capture_buffer_begin, &capture_buffer_filled, CAPTURE_BUFFER_SIZE, &buffer_free);
			if (len > buffer_free)
				len = buffer_free;

			Con_DPrintf("Capture sound buffer truncated\n");
		}
		memcpy(capture_buffer + capture_buffer_filled, stream, len);
		capture_buffer_filled += len;
		if (cls.state == ca_connected)
		{
			// TODO VOIP: once frames are handed off
			// via a proper netmessage (see matching TODO in netconn.c),
			// re-add a loop here that pulls 960-sample-frame chunks out of
			// capture_buffer, opus_encodes them (encoder id/seq bookkeeping
			// below is still useful for reordering on the receive side),
			// and hands the encoded bytes to that netmessage path instead
			// of building a raw packet by hand
		}
	}
	if (snd_echo_active)
	{
		int buffer_echo_free = CAPTURE_BUFFER_ECHO_SIZE - capture_buffer_echo_filled;
		if (len > buffer_echo_free)
		{
			Buffer_Shift_Force(capture_buffer_echo, &capture_buffer_echo_begin, &capture_buffer_echo_filled, CAPTURE_BUFFER_ECHO_SIZE, &buffer_echo_free);
			if (len > buffer_echo_free)
				len = buffer_echo_free;

			Con_DPrintf("Capture sound buffer truncated\n");
		}
		memcpy(capture_buffer_echo + capture_buffer_echo_filled, stream, len);
		capture_buffer_echo_filled += len;
	}
}

static void Echo_GetSamplesFloat(channel_t *ch, sfx_t *sfx, int firstsampleframe, int numsampleframes, float *outsamplesfloat)
{
	int i, len;
	int bytesrequired;
	int shift;
	shift = firstsampleframe - capture_buffer_echo_pos;
	if (-shift * sfx->format.width * sfx->format.channels > capture_buffer_echo_begin )
	{
		//too far in past
		capture_buffer_echo_pos = firstsampleframe;
		Con_DPrintf("Captured sound samples missed (too far in past)\n");
		return;
	}
	SndSys_LockCapture();
	capture_buffer_echo_begin += shift * sfx->format.width * sfx->format.channels;
	capture_buffer_echo_pos += shift;
	if (capture_buffer_echo_begin >= capture_buffer_echo_filled)
	{
		capture_buffer_echo_begin = capture_buffer_echo_filled;
		capture_buffer_echo_pos = firstsampleframe + numsampleframes;
		SndSys_UnlockCapture();
		return;
	}
	len = numsampleframes * sfx->format.channels;
	bytesrequired = len * sfx->format.width;
	if (bytesrequired > capture_buffer_echo_filled - capture_buffer_echo_begin) {
		bytesrequired = capture_buffer_echo_filled - capture_buffer_echo_begin;
		len = bytesrequired / sfx->format.width;
		numsampleframes = len / sfx->format.channels;
		Con_DPrintf("Captured sound samples missed (too far in future)\n");
	}
	if (sfx->format.width == 2)
	{
		const short *bufs = (const short *)capture_buffer_echo + (capture_buffer_echo_begin / 2);
		for (i = 0;i < len;i++)
			outsamplesfloat[i] = bufs[i] * (1.0f / 32768.0f);
	}
	else
	{
		const signed char *bufb = (const signed char *)capture_buffer_echo + capture_buffer_echo_begin;
		for (i = 0;i < len;i++)
			outsamplesfloat[i] = bufb[i] * (1.0f / 128.0f);
	}
	capture_buffer_echo_begin += bytesrequired;
	capture_buffer_echo_pos += numsampleframes;
	Buffer_Shift(capture_buffer_echo, &capture_buffer_echo_begin, &capture_buffer_echo_filled, CAPTURE_BUFFER_ECHO_SIZE);
	SndSys_UnlockCapture();
}

static void Echo_FreeSfx(sfx_t *sfx)
{
	snd_echo = NULL;
}

static const snd_fetcher_t echo_fetcher = { Echo_GetSamplesFloat, NULL, Echo_FreeSfx };

void S_Echo_Start(struct cmd_state_s *state)
{
	prvm_prog_t *prog = CLVM_prog;
	if (!SndSys_CaptureAvailable())
	{
		Con_Print("Audio capture device not available\n");
		return;
	}
	if (snd_echo_active) return;
	SndSys_LockRenderBuffer();
	snd_echo = S_FindName("*echostream");
	snd_echo->format.speed = VOIP_FREQ;
	snd_echo->format.width = VOIP_WIDTH;
	snd_echo->format.channels = VOIP_CHANNELS;
	snd_echo->fetcher = &echo_fetcher;
	snd_echo->total_length = 99999999;
	snd_echo->volume_mult = 1;
	snd_echo->flags = SFXFLAG_VOIP;
	if (!capture_buffer_echo)
		capture_buffer_echo = (char *)Mem_Alloc(snd_mempool, CAPTURE_BUFFER_ECHO_SIZE);
	memset(capture_buffer_echo, VOIP_WIDTH == 1 ? 0x80 : 0, CAPTURE_BUFFER_ECHO_SIZE);
	capture_buffer_echo_filled = 32768;
	capture_buffer_echo_begin = 0;
	capture_buffer_echo_pos = 0;
	S_StartSound_StartPosition_Flags(-2, 0, snd_echo, vec3_origin, 1, 0, 0, CHANNELFLAG_FULLVOLUME | CHANNELFLAG_LOCALSOUND, 1.0f);
	snd_echo->loopstart = 100000000;
	SndSys_UnlockRenderBuffer();
	SndSys_UnpauseCapture();
	if (PRVM_clientfunction(voip_echo_test))
	{
		PRVM_G_FLOAT(OFS_PARM0) = true;
		prog->ExecuteProgram(prog, PRVM_clientfunction(voip_echo_test), "QC function voip_echo_test is missing");
	}
	prog = MVM_prog;
	if (PRVM_menufunction(voip_echo_test))
	{
		PRVM_G_FLOAT(OFS_PARM0) = true;
		prog->ExecuteProgram(prog, PRVM_menufunction(voip_echo_test), "QC function voip_echo_test is missing");
	}
	SndSys_LockCapture();
	snd_echo_active = true;
	SndSys_UnlockCapture();
}

void S_FreeSfx (sfx_t *sfx, qbool force);
void S_Echo_Stop(struct cmd_state_s *state)
{
	prvm_prog_t *prog = CLVM_prog;
	if (!snd_echo_active) return;
	if (!snd_voip_active)
		SndSys_PauseCapture();

	SndSys_LockRenderBuffer();
	if (snd_echo)
	{
		S_FreeSfx(snd_echo, 1);
		snd_echo = NULL;
	}
	SndSys_UnlockRenderBuffer();
	if (PRVM_clientfunction(voip_echo_test))
	{
		PRVM_G_FLOAT(OFS_PARM0) = false;
		prog->ExecuteProgram(prog, PRVM_clientfunction(voip_echo_test), "QC function voip_echo_test is missing");
	}
	prog = MVM_prog;
	if (PRVM_menufunction(voip_echo_test))
	{
		PRVM_G_FLOAT(OFS_PARM0) = false;
		prog->ExecuteProgram(prog, PRVM_menufunction(voip_echo_test), "QC function voip_echo_test is missing");
	}
	SndSys_LockCapture();
	snd_echo_active = false;
	SndSys_UnlockCapture();
}

void S_VOIP_Start(struct cmd_state_s *state)
{
	prvm_prog_t *prog = CLVM_prog;
	if (!SndSys_CaptureAvailable())
	{
		Con_Print("Audio capture device not available\n");
		return;
	}
	if (snd_voip_active) return;
	if (!capture_buffer)
		capture_buffer = (char *)Mem_Alloc(snd_mempool, CAPTURE_BUFFER_SIZE);

	if (!opus_encoder)
	{
		int err;
		opus_encoder = opus_encoder_create(VOIP_FREQ, VOIP_CHANNELS, OPUS_APPLICATION_VOIP, &err);
		opus_encoder_id++;
		opus_encoder_seq = 0;
		if (opus_encoder)
		{
			err = opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(VOIP_OPUS_BITRATE));
			if (err)
			{
				Con_Printf("Opus encoder bitrate setup failed\n");
				opus_encoder_destroy(opus_encoder);
				opus_encoder = NULL;
				return;
			}
		}
		else
		{
			Con_Printf("Opus encoder creation failed\n");
			return;
		}
	}
	capture_buffer_begin = 0;
	capture_buffer_filled = 0;
	capture_buffer_pos = 0;
	SndSys_LockCapture();
	snd_voip_active = true;
	SndSys_UnlockCapture();
	SndSys_UnpauseCapture();
	if (PRVM_clientfunction(voip_talk))
	{
		PRVM_G_FLOAT(OFS_PARM0) = true;
		prog->ExecuteProgram(prog, PRVM_clientfunction(voip_talk), "QC function voip_talk is missing");
	}
}

void S_VOIP_Stop(struct cmd_state_s *state)
{
	prvm_prog_t *prog = CLVM_prog;
	if (!snd_voip_active) return;
	if (!snd_echo_active)
		SndSys_PauseCapture();

	SndSys_LockCapture();
	snd_voip_active = false;
	SndSys_UnlockCapture();
	if (opus_encoder)
	{
		opus_encoder_destroy(opus_encoder);
		opus_encoder = NULL;
	}
	if (PRVM_clientfunction(voip_talk))
	{
		PRVM_G_FLOAT(OFS_PARM0) = false;
		prog->ExecuteProgram(prog, PRVM_clientfunction(voip_talk), "QC function voip_talk is missing");
	}
}

static sfx_t *snd_voip_speakers[128];
static char *voip_buffer[128];
static int voip_buffer_begin[128];
static int voip_buffer_filled[128];
static long int voip_buffer_pos[128];

static void VOIP_GetSamplesFloat(channel_t *ch, sfx_t *sfx, int firstsampleframe, int numsampleframes, float *outsamplesfloat)
{
	int client = (sfx_t**)sfx->fetcher_data - snd_voip_speakers;
	char *buffer = voip_buffer[client];
	int *buffer_begin = &voip_buffer_begin[client];
	int *buffer_filled = &voip_buffer_filled[client];
	long int *buffer_pos = &voip_buffer_pos[client];
	int i, len;
	int bytesrequired;
	int shift;
	shift = firstsampleframe - *buffer_pos;
	if (-shift * sfx->format.width * sfx->format.channels > *buffer_begin )
	{
		//too far in past
		*buffer_pos = firstsampleframe;
		Con_DPrintf("VOIP sound samples missed (too far in past)\n");
		return;
	}
	*buffer_begin += shift * sfx->format.width * sfx->format.channels;
	if (*buffer_begin >= *buffer_filled)
	{
		*buffer_begin = *buffer_filled;
		*buffer_pos = firstsampleframe + numsampleframes;
		if (sfx->total_length == 99999999)
		{
			Con_DPrintf("Schedule voip sfx remove\n");
			sfx->total_length = firstsampleframe;
		}
		return;
	}
	*buffer_pos += shift;
	len = numsampleframes * sfx->format.channels;
	bytesrequired = len * sfx->format.width;
	if (bytesrequired > *buffer_filled - *buffer_begin) {
		bytesrequired = *buffer_filled - *buffer_begin;
		len = bytesrequired / sfx->format.width;
		numsampleframes = len / sfx->format.channels;
		Con_DPrintf("VOIP sound samples missed (too far in future)\n");
	}
	if (sfx->format.width == 2)
	{
		const short *bufs = (const short *)buffer + (*buffer_begin / 2);
		for (i = 0;i < len;i++)
			outsamplesfloat[i] = bufs[i] * (1.0f / 32768.0f);
	}
	else
	{
		const signed char *bufb = (const signed char *)buffer + *buffer_begin;
		for (i = 0;i < len;i++)
			outsamplesfloat[i] = bufb[i] * (1.0f / 128.0f);
	}
	*buffer_begin += bytesrequired;
	*buffer_pos += numsampleframes;
	Buffer_Shift(buffer, buffer_begin, buffer_filled, CAPTURE_BUFFER_SIZE);
}

static OpusDecoder *opus_decoder[128];
static void VOIP_FreeSfx(sfx_t *sfx)
{
	int client = (sfx_t**)sfx->fetcher_data - snd_voip_speakers;
	*((sfx_t**)sfx->fetcher_data) = NULL;
	if (opus_decoder[client])
	{
		opus_decoder_destroy(opus_decoder[client]);
		opus_decoder[client] = NULL;
	}
}

static snd_fetcher_t voip_fetcher = { VOIP_GetSamplesFloat, NULL, VOIP_FreeSfx };

static unsigned int opus_decoder_seq[128];
static unsigned char opus_decoder_id[128];

void S_VOIP_Received(unsigned char *packet, int len, int client)
{
	int buffer_free;
	char *buffer;
	int *buffer_filled;
	char packet_decoded[1920];
	unsigned int seq;
	prvm_prog_t *prog = CLVM_prog;
	if (client < 0 || client > 127)
	{
		Con_Printf("Bad VOIP packet\n");
		return;
	}
	if (len <= 8)
		return;
	
	if (PRVM_clientfunction(voip_event))
	{
		PRVM_G_FLOAT(OFS_PARM0) = (int)client;
		prog->ExecuteProgram(prog, PRVM_clientfunction(voip_event), "QC function voip_event is missing");
		if (!PRVM_G_FLOAT(OFS_RETURN)) return;
	}
	if (!voip_buffer[client]) voip_buffer[client] = (char *)Mem_Alloc(snd_mempool, CAPTURE_BUFFER_SIZE);
	if (opus_decoder_id[client] != packet[0])
	{
		if (opus_decoder[client])
		{
			opus_decoder_destroy(opus_decoder[client]);
			opus_decoder[client] = NULL;
		}
		opus_decoder_id[client] = packet[0];
	}
	if (!opus_decoder[client])
	{
		int err;
		opus_decoder[client] = opus_decoder_create(VOIP_FREQ, VOIP_CHANNELS, &err);
		Con_DPrintf("Creating opus decoder for client %i\n", client);
		if (!opus_decoder[client])
		{
			Con_Printf("Opus decoder creation failed\n");
			return;
		}
		opus_decoder_seq[client] = 0;
	}
	SndSys_LockRenderBuffer();
	if (!snd_voip_speakers[client])
	{
		char stream_name[32];
		sfx_t *sfx;
		dpsnprintf(stream_name, 32, "*voipstream/%i", client);
		sfx = S_FindName(stream_name);
		sfx->format.speed = VOIP_FREQ;
		sfx->format.width = VOIP_WIDTH;
		sfx->format.channels = VOIP_CHANNELS;
		sfx->fetcher = &voip_fetcher;
		sfx->fetcher_data = &snd_voip_speakers[client];
		sfx->total_length = 99999999;
		sfx->volume_mult = 1;
		sfx->flags = SFXFLAG_VOIP;
		snd_voip_speakers[client] = sfx;
		S_StartSound_StartPosition_Flags(-3 - client, 0, sfx, vec3_origin, 1, 0, 0, CHANNELFLAG_FULLVOLUME | CHANNELFLAG_LOCALSOUND, 1.0f);
		sfx->loopstart = 100000000;
	}
	else
	{
		snd_voip_speakers[client]->total_length = 99999999;
	}
	buffer = voip_buffer[client];
	buffer_filled = &voip_buffer_filled[client];
	buffer_free = CAPTURE_BUFFER_SIZE - *buffer_filled;
	seq = packet[2] + packet[3] * 256 + packet[4] * 65536 + packet[5] * 16777216;
	if (seq < opus_decoder_seq[client])
	{
		Con_DPrintf("Wrong ordered packet dropped (got %i but expected %i) for stream id %i\n", seq, opus_decoder_seq[client], opus_decoder_id[client]);
		SndSys_UnlockRenderBuffer();
		return;
	}
	if (seq > opus_decoder_seq[client])
	{
		int len_decoded;
		Con_DPrintf("Opus frame missed (got %i but expected %i) for stream id %i\n", seq, opus_decoder_seq[client], opus_decoder_id[client]);
		if (seq - opus_decoder_seq[client] > 10)
			opus_decoder_seq[client] = seq - 10;
		while (seq > opus_decoder_seq[client])
		{
			len_decoded = opus_decode(opus_decoder[client], NULL, 0, (opus_int16*)packet_decoded, 960, 0);
			if (len_decoded > 0)
			{
				len_decoded *= 2; //16 bit
				if (len_decoded > buffer_free)
					len_decoded = buffer_free;
				memcpy(buffer + *buffer_filled, packet_decoded, len_decoded);
				*buffer_filled += len_decoded;
				buffer_free -= len_decoded;
			}
			opus_decoder_seq[client]++;
		}
	}
	opus_decoder_seq[client]++;
	len -= 6;
	packet += 6;
	len = opus_decode(opus_decoder[client], (unsigned char*)packet, len, (opus_int16*)packet_decoded, 960, 0);
	if (len < 0)
	{
		Con_Printf("Opus decode failed (sequence = %i)\n", opus_decoder_seq[client]);
		SndSys_UnlockRenderBuffer();
		return;
	}
	len *= 2;
	if (len > buffer_free)
	{
		len = buffer_free;
		Con_DPrintf("VOIP sound buffer truncated\n");
	}
	memcpy(buffer + *buffer_filled, packet_decoded, len);
	*buffer_filled += len;
	SndSys_UnlockRenderBuffer();
}

void S_VOIP_Shutdown(void)
{
	int i;
	SndSys_PauseCapture();
	S_VOIP_Stop(NULL);
	S_Echo_Stop(NULL);
	capture_buffer = NULL;
	capture_buffer_echo = NULL;
	for (i = 0; i < 128; i++)
	{
		voip_buffer[i] = NULL;
	}
}
