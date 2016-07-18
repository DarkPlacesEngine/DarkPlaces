#####  DP_MAKE_TARGET autodetection and arch specific variables #####

ifndef DP_MAKE_TARGET

# Win32
ifdef WINDIR
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

# If we're targeting an x86 CPU we want to enable DP_SSE (CFLAGS_SSE and SSE2)
ifeq ($(DP_MAKE_TARGET), mingw)
	DP_SSE:=1
else
	DP_MACHINE:=$(shell uname -m)
	ifeq ($(DP_MACHINE),x86_64)
		DP_SSE:=1
	else
	ifeq ($(DP_MACHINE),i686)
		DP_SSE:=1
	else
	ifeq ($(DP_MACHINE),i386)
		DP_SSE:=1
	else
		DP_SSE:=0
	endif # ifeq ($(DP_MACHINE),i386)
	endif # ifeq ($(DP_MACHINE),i686)
	endif # ifeq ($(DP_MACHINE),x86_64)
endif

# Makefile name
MAKEFILE=makefile

# Commands
ifdef windir
	CMD_RM=del
	CMD_CP=copy /y
	CMD_MKDIR=mkdir
else
	CMD_RM=$(CMD_UNIXRM)
	CMD_CP=$(CMD_UNIXCP)
	CMD_MKDIR=$(CMD_UNIXMKDIR)
endif

# 64bits AMD CPUs use another lib directory
ifeq ($(DP_MACHINE),x86_64)
	UNIX_X11LIBPATH:=/usr/X11R6/lib64
else
	UNIX_X11LIBPATH:=/usr/X11R6/lib
endif

# default targets
TARGETS_DEBUG=sv-debug cl-debug sdl-debug
TARGETS_PROFILE=sv-profile cl-profile sdl-profile
TARGETS_RELEASE=sv-release cl-release sdl-release
TARGETS_RELEASE_PROFILE=sv-release-profile cl-release-profile sdl-release-profile
TARGETS_NEXUIZ=sv-nexuiz cl-nexuiz sdl-nexuiz

###### Optional features #####
DP_CDDA?=enabled
ifeq ($(DP_CDDA), enabled)
	OBJ_SDLCD=$(OBJ_CD_COMMON) cd_sdl.o
	OBJ_LINUXCD=$(OBJ_CD_COMMON) cd_linux.o
	OBJ_BSDCD=$(OBJ_CD_COMMON) cd_bsd.o
	OBJ_WINCD=$(OBJ_CD_COMMON) cd_win.o
else
	OBJ_SDLCD=$(OBJ_CD_COMMON) $(OBJ_NOCD)
	OBJ_LINUXCD=$(OBJ_CD_COMMON) $(OBJ_NOCD)
	OBJ_BSDCD=$(OBJ_CD_COMMON) $(OBJ_NOCD)
	OBJ_WINCD=$(OBJ_CD_COMMON) $(OBJ_NOCD)
endif

DP_VIDEO_CAPTURE?=enabled
ifeq ($(DP_VIDEO_CAPTURE), enabled)
	CFLAGS_VIDEO_CAPTURE=-DCONFIG_VIDEO_CAPTURE
	OBJ_VIDEO_CAPTURE= cap_avi.o cap_ogg.o
else
	CFLAGS_VIDEO_CAPTURE=
	OBJ_VIDEO_CAPTURE=
endif

# Linux configuration
ifeq ($(DP_MAKE_TARGET), linux)
	DEFAULT_SNDAPI=ALSA
	OBJ_CD=$(OBJ_LINUXCD)

	OBJ_CL=$(OBJ_GLX)
	OBJ_ICON=
	OBJ_ICON_NEXUIZ=

	LDFLAGS_CL=$(LDFLAGS_LINUXCL)
	LDFLAGS_SV=$(LDFLAGS_LINUXSV)
	LDFLAGS_SDL=$(LDFLAGS_LINUXSDL)

	SDLCONFIG_CFLAGS=$(SDLCONFIG_UNIXCFLAGS) $(SDLCONFIG_UNIXCFLAGS_X11)
	SDLCONFIG_LIBS=$(SDLCONFIG_UNIXLIBS) $(SDLCONFIG_UNIXLIBS_X11)
	SDLCONFIG_STATICLIBS=$(SDLCONFIG_UNIXSTATICLIBS) $(SDLCONFIG_UNIXSTATICLIBS_X11)

	EXE_CL=$(EXE_UNIXCL)
	EXE_SV=$(EXE_UNIXSV)
	EXE_SDL=$(EXE_UNIXSDL)
	EXE_CLNEXUIZ=$(EXE_UNIXCLNEXUIZ)
	EXE_SVNEXUIZ=$(EXE_UNIXSVNEXUIZ)
	EXE_SDLNEXUIZ=$(EXE_UNIXSDLNEXUIZ)

	DP_LINK_ZLIB?=shared
	DP_LINK_JPEG?=shared
	DP_LINK_ODE?=dlopen
	DP_LINK_CRYPTO?=dlopen
	DP_LINK_CRYPTO_RIJNDAEL?=dlopen
endif

# Mac OS X configuration
ifeq ($(DP_MAKE_TARGET), macosx)
	DEFAULT_SNDAPI=COREAUDIO
	OBJ_CD=$(OBJ_MACOSXCD)

	OBJ_CL=$(OBJ_AGL)
	OBJ_ICON=
	OBJ_ICON_NEXUIZ=

	LDFLAGS_CL=$(LDFLAGS_MACOSXCL)
	LDFLAGS_SV=$(LDFLAGS_MACOSXSV)
	LDFLAGS_SDL=$(LDFLAGS_MACOSXSDL)

	SDLCONFIG_CFLAGS=$(SDLCONFIG_MACOSXCFLAGS)
	SDLCONFIG_LIBS=$(SDLCONFIG_MACOSXLIBS)
	SDLCONFIG_STATICLIBS=$(SDLCONFIG_MACOSXSTATICLIBS)

	EXE_CL=$(EXE_MACOSXCL)
	EXE_SV=$(EXE_UNIXSV)
	EXE_SDL=$(EXE_UNIXSDL)
	EXE_CLNEXUIZ=$(EXE_MACOSXCLNEXUIZ)
	EXE_SVNEXUIZ=$(EXE_UNIXSVNEXUIZ)
	EXE_SDLNEXUIZ=$(EXE_UNIXSDLNEXUIZ)

	ifeq ($(word 2, $(filter -arch, $(CC))), -arch)
		CFLAGS_MAKEDEP=
	endif

	DP_LINK_ZLIB?=shared
	DP_LINK_JPEG?=dlopen
	DP_LINK_ODE?=dlopen
	DP_LINK_CRYPTO?=dlopen
	DP_LINK_CRYPTO_RIJNDAEL?=dlopen

	# on OS X, we don't build the CL by default because it uses deprecated
	# and not-implemented-in-64bit Carbon
	TARGETS_DEBUG=sv-debug sdl-debug
	TARGETS_PROFILE=sv-profile sdl-profile
	TARGETS_RELEASE=sv-release sdl-release
	TARGETS_RELEASE_PROFILE=sv-release-profile sdl-release-profile
	TARGETS_NEXUIZ=sv-nexuiz sdl-nexuiz
endif

# SunOS configuration (Solaris)
ifeq ($(DP_MAKE_TARGET), sunos)
	DEFAULT_SNDAPI=BSD
	OBJ_CD=$(OBJ_SUNOSCD)

	OBJ_CL=$(OBJ_GLX)
	OBJ_ICON=
	OBJ_ICON_NEXUIZ=

	CFLAGS_EXTRA=$(CFLAGS_SUNOS)

	LDFLAGS_CL=$(LDFLAGS_SUNOSCL)
	LDFLAGS_SV=$(LDFLAGS_SUNOSSV)
	LDFLAGS_SDL=$(LDFLAGS_SUNOSSDL)

	SDLCONFIG_CFLAGS=$(SDLCONFIG_UNIXCFLAGS) $(SDLCONFIG_UNIXCFLAGS_X11)
	SDLCONFIG_LIBS=$(SDLCONFIG_UNIXLIBS) $(SDLCONFIG_UNIXLIBS_X11)
	SDLCONFIG_STATICLIBS=$(SDLCONFIG_UNIXSTATICLIBS) $(SDLCONFIG_UNIXSTATICLIBS_X11)

	EXE_CL=$(EXE_UNIXCL)
	EXE_SV=$(EXE_UNIXSV)
	EXE_SDL=$(EXE_UNIXSDL)
	EXE_CLNEXUIZ=$(EXE_UNIXCLNEXUIZ)
	EXE_SVNEXUIZ=$(EXE_UNIXSVNEXUIZ)
	EXE_SDLNEXUIZ=$(EXE_UNIXSDLNEXUIZ)

	DP_LINK_ZLIB?=shared
	DP_LINK_JPEG?=shared
	DP_LINK_ODE?=dlopen
	DP_LINK_CRYPTO?=dlopen
	DP_LINK_CRYPTO_RIJNDAEL?=dlopen
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

	DP_LINK_ZLIB?=shared
	DP_LINK_JPEG?=shared
	DP_LINK_ODE?=dlopen
	DP_LINK_CRYPTO?=dlopen
	DP_LINK_CRYPTO_RIJNDAEL?=dlopen
endif

# Win32 configuration
ifeq ($(WIN32RELEASE), 1)
#	TARGET=i686-pc-mingw32
#	CC=$(TARGET)-g++
#	WINDRES=$(TARGET)-windres
	CPUOPTIMIZATIONS=-march=pentium3 -mfpmath=sse -fno-math-errno -ffinite-math-only -fno-rounding-math -fno-signaling-nans -fno-trapping-math
#       CPUOPTIMIZATIONS+=-DUSE_WSPIAPI_H -DSUPPORTIPV6
	LDFLAGS_WINCOMMON=-Wl,--large-address-aware
else
	LDFLAGS_WINCOMMON=
endif

ifeq ($(WIN64RELEASE), 1)
#	TARGET=x86_64-pc-mingw32
#	CC=$(TARGET)-g++
#	WINDRES=$(TARGET)-windres
endif

ifeq ($(D3D), 1)
	CFLAGS_D3D=-DSUPPORTD3D -DSUPPORTDIRECTX
	CFLAGS_WARNINGS=-Wall
	LDFLAGS_D3D=-ld3d9
else
	CFLAGS_D3D=
	CFLAGS_WARNINGS=-Wall -Wold-style-definition -Wstrict-prototypes -Wsign-compare -Wdeclaration-after-statement -Wmissing-prototypes
	LDFLAGS_D3D=
endif


ifeq ($(DP_MAKE_TARGET), mingw)
	DEFAULT_SNDAPI=WIN
	OBJ_CD=$(OBJ_WINCD)

	OBJ_CL=$(OBJ_WGL)
	OBJ_ICON=darkplaces.o
	OBJ_ICON_NEXUIZ=nexuiz.o

	LDFLAGS_CL=$(LDFLAGS_WINCL)
	LDFLAGS_SV=$(LDFLAGS_WINSV)
	LDFLAGS_SDL=$(LDFLAGS_WINSDL)

	SDLCONFIG_CFLAGS=$(SDLCONFIG_UNIXCFLAGS)
	SDLCONFIG_LIBS=$(SDLCONFIG_UNIXLIBS)
	SDLCONFIG_STATICLIBS=$(SDLCONFIG_UNIXSTATICLIBS)

	EXE_CL=$(EXE_WINCL)
	EXE_SV=$(EXE_WINSV)
	EXE_SDL=$(EXE_WINSDL)
	EXE_CLNEXUIZ=$(EXE_WINCLNEXUIZ)
	EXE_SVNEXUIZ=$(EXE_WINSVNEXUIZ)
	EXE_SDLNEXUIZ=$(EXE_WINSDLNEXUIZ)

	DP_LINK_ZLIB?=dlopen
	DP_LINK_JPEG?=shared
	DP_LINK_ODE?=dlopen
	DP_LINK_CRYPTO?=dlopen
	DP_LINK_CRYPTO_RIJNDAEL?=dlopen
endif

# set these to "" if you want to use dynamic loading instead
# zlib
ifeq ($(DP_LINK_ZLIB), shared)
	CFLAGS_LIBZ=-DLINK_TO_ZLIB
	LIB_Z=-lz
endif
ifeq ($(DP_LINK_ZLIB), dlopen)
	CFLAGS_LIBZ=
	LIB_Z=
endif

# jpeg
ifeq ($(DP_LINK_JPEG), shared)
	CFLAGS_LIBJPEG=-DLINK_TO_LIBJPEG
	LIB_JPEG=-ljpeg
endif
ifeq ($(DP_LINK_JPEG), dlopen)
	CFLAGS_LIBJPEG=
	LIB_JPEG=
endif

# ode
ifeq ($(DP_LINK_ODE), shared)
	ODE_CONFIG?=ode-config
	LIB_ODE=`$(ODE_CONFIG) --libs`
	CFLAGS_ODE=`$(ODE_CONFIG) --cflags` -DUSEODE -DLINK_TO_LIBODE
endif
ifeq ($(DP_LINK_ODE), dlopen)
	LIB_ODE=
	CFLAGS_ODE=-DUSEODE
endif

# d0_blind_id
ifeq ($(DP_LINK_CRYPTO), shared)
	LIB_CRYPTO=-ld0_blind_id
	CFLAGS_CRYPTO=-DLINK_TO_CRYPTO
endif
ifeq ($(DP_LINK_CRYPTO), dlopen)
	LIB_CRYPTO=
	CFLAGS_CRYPTO=
endif
ifeq ($(DP_LINK_CRYPTO_RIJNDAEL), shared)
	LIB_CRYPTO_RIJNDAEL=-ld0_rijndael
	CFLAGS_CRYPTO_RIJNDAEL=-DLINK_TO_CRYPTO_RIJNDAEL
endif
ifeq ($(DP_LINK_CRYPTO_RIJNDAEL), dlopen)
	LIB_CRYPTO_RIJNDAEL=
	CFLAGS_CRYPTO_RIJNDAEL=
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

ifeq ($(DP_SOUND_API),3DRAS)
	OBJ_SOUND=$(OBJ_SND_3DRAS)
	LIB_SOUND=$(LIB_SND_3DRAS)
endif

##### Extra CFLAGS #####

CFLAGS_MAKEDEP?=-MMD
ifdef DP_FS_BASEDIR
	CFLAGS_FS=-DDP_FS_BASEDIR=\"$(DP_FS_BASEDIR)\"
else
	CFLAGS_FS=
endif

CFLAGS_PRELOAD=
ifneq ($(DP_MAKE_TARGET), mingw)
ifdef DP_PRELOAD_DEPENDENCIES
# DP_PRELOAD_DEPENDENCIES: when set, link against the libraries needed using -l
# dynamically so they won't get loaded at runtime using dlopen
	LDFLAGS_CL+=$(LDFLAGS_UNIXCL_PRELOAD)
	LDFLAGS_SV+=$(LDFLAGS_UNIXSV_PRELOAD)
	LDFLAGS_SDL+=$(LDFLAGS_UNIXSDL_PRELOAD)
	CFLAGS_PRELOAD=$(CFLAGS_UNIX_PRELOAD)
endif
endif

CFLAGS_NET=
# Systems without IPv6 support should uncomment this:
#CFLAGS_NET+=-DNOSUPPORTIPV6

##### GNU Make specific definitions #####

DO_LD=$(CC) -o ../../../$@ $^ $(LDFLAGS)


##### Definitions shared by all makefiles #####
include makefile.inc


##### Dependency files #####

-include *.d

# hack to deal with no-longer-needed .h files
%.h:
	@echo
	@echo "NOTE: file $@ mentioned in dependencies missing, continuing..."
	@echo "HINT: consider 'make clean'"
	@echo
