#####  DP_MAKE_TARGET autodetection and arch specific variables #####

.ifndef DP_MAKE_TARGET

DP_MAKE_TARGET=bsd

.endif
DP_ARCH != uname

# Command used to delete files
CMD_RM=$(CMD_UNIXRM)

# default targets
TARGETS_DEBUG=sv-debug cl-debug sdl-debug
TARGETS_PROFILE=sv-profile cl-profile sdl-profile
TARGETS_RELEASE=sv-release cl-release sdl-release
TARGETS_RELEASE_PROFILE=sv-release-profile cl-release-profile sdl-release-profile
TARGETS_NEXUIZ=sv-nexuiz cl-nexuiz sdl-nexuiz

# X11 libs
UNIX_X11LIBPATH=/usr/X11R6/lib

# BSD configuration
.if $(DP_MAKE_TARGET) == "bsd"

# FreeBSD uses OSS
.if $(DP_ARCH) == "FreeBSD"
DEFAULT_SNDAPI=OSS
.else
DEFAULT_SNDAPI=BSD
.endif
OBJ_CD=$(OBJ_BSDCD)

OBJ_CL=$(OBJ_GLX)
OBJ_ICON=
OBJ_ICON_NEXUIZ=

LDFLAGS_CL=$(LDFLAGS_BSDCL)
LDFLAGS_SV=$(LDFLAGS_BSDSV)
LDFLAGS_SDL=$(LDFLAGS_BSDSDL)

SDLCONFIG_CFLAGS=$(SDLCONFIG_UNIXCFLAGS) $(SDLCONFIG_UNIXCFLAGS_X11)
SDLCONFIG_LIBS=$(SDLCONFIG_UNIXLIBS) $(SDLCONFIG_UNIXLIBS_X11)
SDLCONFIG_STATICLIBS=$(SDLCONFIG_UNIXSTATICLIBS) $(SDLCONFIG_UNIXSTATICLIBS_X11)

EXE_CL=$(EXE_UNIXCL)
EXE_SV=$(EXE_UNIXSV)
EXE_SDL=$(EXE_UNIXSDL)
EXE_CLNEXUIZ=$(EXE_UNIXCLNEXUIZ)
EXE_SVNEXUIZ=$(EXE_UNIXSVNEXUIZ)
EXE_SDLNEXUIZ=$(EXE_UNIXSDLNEXUIZ)

# libjpeg dependency (set these to "" if you want to use dynamic loading instead)
CFLAGS_LIBJPEG=-DLINK_TO_LIBJPEG
LIB_JPEG=-ljpeg

.endif


##### Sound configuration #####

.ifndef DP_SOUND_API
DP_SOUND_API=$(DEFAULT_SNDAPI)
.endif

# NULL: no sound
.if $(DP_SOUND_API) == "NULL"
OBJ_SOUND=$(OBJ_SND_NULL)
LIB_SOUND=$(LIB_SND_NULL)
.endif

# OSS: Open Sound System
.if $(DP_SOUND_API) == "OSS"
OBJ_SOUND=$(OBJ_SND_OSS)
LIB_SOUND=$(LIB_SND_OSS)
.endif

# BSD: BSD / Sun audio API
.if $(DP_SOUND_API) == "BSD"
OBJ_SOUND=$(OBJ_SND_BSD)
LIB_SOUND=$(LIB_SND_BSD)
.endif


##### Extra CFLAGS #####

CFLAGS_MAKEDEP=-MD
.ifdef DP_FS_BASEDIR
CFLAGS_FS=-DDP_FS_BASEDIR='\"$(DP_FS_BASEDIR)\"'
.else
CFLAGS_FS=
.endif

CFLAGS_PRELOAD=
.ifdef DP_PRELOAD_DEPENDENCIES
LDFLAGS_CL+=$(LDFLAGS_UNIXCL_PRELOAD)
LDFLAGS_SV+=$(LDFLAGS_UNIXSV_PRELOAD)
LDFLAGS_SDL+=$(LDFLAGS_UNIXSDL_PRELOAD)
CFLAGS_PRELOAD=$(CFLAGS_UNIX_PRELOAD)
.endif


##### BSD Make specific definitions #####

MAKE:=$(MAKE) -f BSDmakefile

DO_LD=$(CC) -o $@ $> $(LDFLAGS)


##### Definitions shared by all makefiles #####
.include "makefile.inc"
