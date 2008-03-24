//BSD

#ifndef SND_3DRAS_H
#define SND_3DRAS_H

#include "sound.h"

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

#define CHANNELFLAG_NONE		0
#define CHANNELFLAG_FORCELOOP	(1 << 0) // force looping even if the sound is not looped
#define CHANNELFLAG_LOCALSOUND	(1 << 1) // INTERNAL USE. Not settable by S_SetChannelFlag
#define CHANNELFLAG_PAUSED		(1 << 2)
#define CHANNELFLAG_FULLVOLUME	(1 << 3) // isn't affected by the general volume

#define SFXFLAG_NONE		0
//#define SFXFLAG_FILEMISSING	(1 << 0) // wasn't able to load the associated sound file
#define SFXFLAG_SERVERSOUND	(1 << 1) // the sfx is part of the server precache list
//#define SFXFLAG_STREAMED		(1 << 2) // informative only. You shouldn't need to know that
#define SFXFLAG_PERMANENTLOCK	(1 << 3) // can never be freed (ex: used by the client code)

typedef struct channel_s{
	struct channel_s* next;
	void* rasptr;//Sound Event // This is also used to indicate a unused slot (when it's pointing to 0)
	int   entnum;// to allow overriding a specific sound
	int   entchannel;
	unsigned int   id;
} channel_t;

typedef struct entnum_s{
	struct entnum_s *next;
	int       entnum;
	vec3_t    lastloc; //Since DP has no way of tracking the deletion, we will use this instead (great jumps indicate teleport or new ent
	void     *rasptr;//Sound Source // This is also used to indicate a unused slot (when it's pointing to 0)
} entnum_t;

struct sfx_s{
	struct sfx_s *next;
	char  name[MAX_QPATH];
	void* rasptr; //Sound Data// The sound data allocated in the lib
	
	int locks;
	unsigned int flags;       	// cf SFXFLAG_* defines
	//unsigned int loopstart;   	// in sample frames. equals total_length if not looped
	//unsigned int total_length;	// in sample frames
};

#endif
