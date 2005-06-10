#####  DP_MAKE_TARGET autodetection and arch specific variables ##### 

ifndef DP_MAKE_TARGET

# Win32
ifdef windir
	DP_MAKE_TARGET=mingw
else

# UNIXes
DP_ARCH:=$(shell uname)
ifneq ($(filter %BSD,$(DP_ARCH)),)
	DP_MAKE_TARGET=bsd
else
ifeq ($(DP_ARCH), Darwin)
	DP_MAKE_TARGET=macosx
else
ifeq ($(DP_ARCH), SunOS)
	DP_MAKE_TARGET=sunos
else
	DP_MAKE_TARGET=linux

endif  # ifeq ($(DP_ARCH), SunOS)
endif  # ifeq ($(DP_ARCH), Darwin)
endif  # ifneq ($(filter %BSD,$(DP_ARCH)),)
endif  # ifdef windir
endif  # ifndef DP_MAKE_TARGET

# If we're not on compiling for Win32, we need additional information
ifneq ($(DP_MAKE_TARGET), mingw)
	DP_ARCH:=$(shell uname)
	DP_MACHINE:=$(shell uname -m)
endif


# Command used to delete files
ifdef windir
	CMD_RM=del
else
	CMD_RM=$(CMD_UNIXRM)
endif

# 64bits AMD CPUs use another lib directory
ifeq ($(DP_MACHINE),x86_64)
	UNIX_X11LIBPATH:=-L/usr/X11R6/lib64
else
	UNIX_X11LIBPATH:=-L/usr/X11R6/lib
endif


# Linux configuration
ifeq ($(DP_MAKE_TARGET), linux)
	DEFAULT_SNDAPI=ALSA
	OBJ_CD=$(OBJ_LINUXCD)

	OBJ_CL=$(OBJ_GLX)

	LDFLAGS_CL=$(LDFLAGS_LINUXCL)
	LDFLAGS_SV=$(LDFLAGS_LINUXSV)
	LDFLAGS_SDL=$(LDFLAGS_LINUXSDL)

	EXE_CL=$(EXE_UNIXCL)
	EXE_SV=$(EXE_UNIXSV)
	EXE_SDL=$(EXE_UNIXSDL)
endif

# Mac OS X configuration
ifeq ($(DP_MAKE_TARGET), macosx)
	DEFAULT_SNDAPI=COREAUDIO
	OBJ_CD=$(OBJ_MACOSXCD)

	OBJ_CL=$(OBJ_AGL)

	LDFLAGS_CL=$(LDFLAGS_MACOSXCL)
	LDFLAGS_SV=$(LDFLAGS_MACOSXSV)
	LDFLAGS_SDL=$(LDFLAGS_MACOSXSDL)

	EXE_CL=$(EXE_UNIXCL)
	EXE_SV=$(EXE_UNIXSV)
	EXE_SDL=$(EXE_UNIXSDL)
endif

# SunOS configuration (Solaris)
ifeq ($(DP_MAKE_TARGET), sunos)
	DEFAULT_SNDAPI=OSS
	OBJ_CD=$(OBJ_SUNOSCD)

	OBJ_CL=$(OBJ_GLX)

	CFLAGS_EXTRA=$(CFLAGS_SUNOS)

	LDFLAGS_CL=$(LDFLAGS_SUNOSCL)
	LDFLAGS_SV=$(LDFLAGS_SUNOSSV)
	LDFLAGS_SDL=$(LDFLAGS_SUNOSSDL)

	EXE_CL=$(EXE_UNIXCL)
	EXE_SV=$(EXE_UNIXSV)
	EXE_SDL=$(EXE_UNIXSDL)
endif

# BSD configuration
ifeq ($(DP_MAKE_TARGET), bsd)
ifeq ($(DP_ARCH),FreeBSD)
	DEFAULT_SNDAPI=OSS
else
	DEFAULT_SNDAPI=BSD
endif
	OBJ_CD=$(OBJ_BSDCD)

	OBJ_CL=$(OBJ_GLX)

	LDFLAGS_CL=$(LDFLAGS_BSDCL)
	LDFLAGS_SV=$(LDFLAGS_BSDSV)
	LDFLAGS_SDL=$(LDFLAGS_BSDSDL)

	EXE_CL=$(EXE_UNIXCL)
	EXE_SV=$(EXE_UNIXSV)
	EXE_SDL=$(EXE_UNIXSDL)
endif

# Win32 configuration
ifeq ($(DP_MAKE_TARGET), mingw)
	DEFAULT_SNDAPI=WIN
	OBJ_CD=$(OBJ_WINCD)

	OBJ_CL=$(OBJ_WGL)

	LDFLAGS_CL=$(LDFLAGS_WINCL)
	LDFLAGS_SV=$(LDFLAGS_WINSV)
	LDFLAGS_SDL=$(LDFLAGS_WINSDL)

	EXE_CL=$(EXE_WINCL)
	EXE_SV=$(EXE_WINSV)
	EXE_SDL=$(EXE_WINSDL)
endif


##### Sound configuration #####

ifndef DP_SOUND_API
	DP_SOUND_API=$(DEFAULT_SNDAPI)
endif

# NULL: no sound
ifeq ($(DP_SOUND_API), NULL)
	OBJ_SOUND=$(OBJ_SND_NULL)
	LIB_SOUND=$(LIB_SND_NULL)
endif

# OSS: Open Sound System
ifeq ($(DP_SOUND_API), OSS)
	OBJ_SOUND=$(OBJ_SND_OSS)
	LIB_SOUND=$(LIB_SND_OSS)
endif

# ALSA: Advanced Linux Sound Architecture
ifeq ($(DP_SOUND_API), ALSA)
	OBJ_SOUND=$(OBJ_SND_ALSA)
	LIB_SOUND=$(LIB_SND_ALSA)
endif

# COREAUDIO: Core Audio
ifeq ($(DP_SOUND_API), COREAUDIO)
	OBJ_SOUND=$(OBJ_SND_COREAUDIO)
	LIB_SOUND=$(LIB_SND_COREAUDIO)
endif

# BSD: BSD / Sun audio API
ifeq ($(DP_SOUND_API), BSD)
	OBJ_SOUND=$(OBJ_SND_BSD)
	LIB_SOUND=$(LIB_SND_BSD)
endif

# WIN: DirectX and Win32 WAVE output
ifeq ($(DP_SOUND_API), WIN)
	OBJ_SOUND=$(OBJ_SND_WIN)
	LIB_SOUND=$(LIB_SND_WIN)
endif


##### GNU Make specific definitions #####

DO_LD=$(CC) -o $@ $^ $(LDFLAGS)


##### Definitions shared by all makefiles #####
include makefile.inc


##### Dependency files #####

-include *.d
