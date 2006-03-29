#####  DP_MAKE_TARGET autodetection and arch specific variables #####

.ifndef DP_MAKE_TARGET

DP_MAKE_TARGET=bsd

.endif
DP_ARCH != uname


# Command used to delete files
CMD_RM=$(CMD_UNIXRM)

UNIX_X11LIBPATH=-L/usr/X11R6/lib

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

EXE_CL=$(EXE_UNIXCL)
EXE_SV=$(EXE_UNIXSV)
EXE_SDL=$(EXE_UNIXSDL)
EXE_CLNEXUIZ=$(EXE_UNIXCLNEXUIZ)
EXE_SVNEXUIZ=$(EXE_UNIXSVNEXUIZ)
EXE_SDLNEXUIZ=$(EXE_UNIXSDLNEXUIZ)

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


##### BSD Make specific definitions #####

MAKE:=$(MAKE)

DO_LD=$(CC) -o $@ $> $(LDFLAGS)


##### Definitions shared by all makefiles #####
.include "makefile.inc"


##### Dependency files #####

DEPEND_FILES != ls *.d
.for i in $(DEPEND_FILES)
.	include "$i"
.endfor
