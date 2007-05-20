# Choose the compiler you want to use
CC=gcc

# athlon optimizations
#CPUOPTIMIZATIONS=-march=athlon
# athlon xp optimizations
#CPUOPTIMIZATIONS=-march=athlon-xp
# athlon 64 optimizations
#CPUOPTIMIZATIONS=-march=athlon64 -m32
# Pentium 3 optimizations
#CPUOPTIMIZATIONS=-march=pentium3
# Pentium 4 optimizations
#CPUOPTIMIZATIONS=-march=pentium4
# 686 (Pentium Pro/II) optimizations
#CPUOPTIMIZATIONS=-march=i686
# No specific CPU (386 compatible)
CPUOPTIMIZATIONS=

SDL_CONFIG?=sdl-config


###### Sound and audio CD #####

OBJ_SND_COMMON=snd_main.o snd_mem.o snd_mix.o snd_ogg.o snd_wav.o

# No sound
OBJ_SND_NULL=snd_null.o
LIB_SND_NULL=

# Open Sound System (Linux, FreeBSD and Solaris)
OBJ_SND_OSS=$(OBJ_SND_COMMON) snd_oss.o
LIB_SND_OSS=

# Advanced Linux Sound Architecture (Linux)
OBJ_SND_ALSA=$(OBJ_SND_COMMON) snd_alsa.o
LIB_SND_ALSA=-lasound

# Core Audio (Mac OS X)
OBJ_SND_COREAUDIO=$(OBJ_SND_COMMON) snd_coreaudio.c
LIB_SND_COREAUDIO=-framework CoreAudio

# BSD / Sun audio API (NetBSD and OpenBSD)
OBJ_SND_BSD=$(OBJ_SND_COMMON) snd_bsd.o
LIB_SND_BSD=

# DirectX and Win32 WAVE output (Win32)
OBJ_SND_WIN=$(OBJ_SND_COMMON) snd_win.o
LIB_SND_WIN=


# CD objects
OBJ_NOCD=cd_null.o


###### Common objects and flags #####

# Common objects
OBJ_COMMON= \
	cd_shared.o \
	cl_collision.o \
	cl_demo.o \
	cl_input.o \
	cl_main.o \
	cl_parse.o \
	cl_particles.o \
	cl_screen.o \
	cl_video.o \
	clvm_cmds.o \
	cmd.o \
	collision.o \
	common.o \
	console.o \
	csprogs.o \
	curves.o \
	cvar.o \
	dpvsimpledecode.o \
	filematch.o \
	fractalnoise.o \
	fs.o \
	gl_backend.o \
	gl_draw.o \
	gl_rmain.o \
	gl_rsurf.o \
	gl_textures.o \
	host.o \
	host_cmd.o \
	image.o \
	image_png.o \
	jpeg.o \
	keys.o \
	lhnet.o \
	libcurl.o \
	mathlib.o \
	matrixlib.o \
	mdfour.o \
	menu.o \
	meshqueue.o \
	model_alias.o \
	model_brush.o \
	model_shared.o \
	model_sprite.o \
	mvm_cmds.o \
	netconn.o \
	palette.o \
	polygon.o \
	portals.o \
	protocol.o \
	prvm_cmds.o \
	prvm_edict.o \
	prvm_exec.o \
	r_explosion.o \
	r_lerpanim.o \
	r_light.o \
	r_lightning.o \
	r_modules.o \
	r_shadow.o \
	r_sky.o \
	r_sprites.o \
	sbar.o \
	sv_main.o \
	sv_move.o \
	sv_phys.o \
	sv_user.o \
	svbsp.o \
	svvm_cmds.o \
	sys_shared.o \
	vid_shared.o \
	view.o \
	wad.o \
	world.o \
	zone.o

# note that builddate.c is very intentionally not compiled to a .o before
# being linked, because it should be recompiled every time an executable is
# built to give the executable a proper date string
OBJ_SV= builddate.c sys_linux.o vid_null.o $(OBJ_SND_NULL) $(OBJ_NOCD) $(OBJ_COMMON)
OBJ_SDL= builddate.c sys_sdl.o vid_sdl.o $(OBJ_SND_COMMON) snd_sdl.o cd_sdl.o $(OBJ_COMMON)


# Compilation
CFLAGS_COMMON=$(CFLAGS_MAKEDEP) $(CFLAGS_PRELOAD) $(CFLAGS_FS) -Wall -Wsign-compare -Wdeclaration-after-statement
CFLAGS_DEBUG=-ggdb
CFLAGS_PROFILE=-g -pg -ggdb
CFLAGS_RELEASE=
CFLAGS_SDL=`$(SDL_CONFIG) --cflags`

OPTIM_DEBUG=$(CPUOPTIMIZATIONS)
#OPTIM_RELEASE=-O2 -fno-strict-aliasing -ffast-math -funroll-loops $(CPUOPTIMIZATIONS)
#OPTIM_RELEASE=-O2 -fno-strict-aliasing -fno-math-errno -fno-trapping-math -ffinite-math-only -fno-signaling-nans -fcx-limited-range -funroll-loops $(CPUOPTIMIZATIONS)
#OPTIM_RELEASE=-O2 -fno-strict-aliasing -funroll-loops $(CPUOPTIMIZATIONS)
OPTIM_RELEASE=-O2 -fno-strict-aliasing $(CPUOPTIMIZATIONS)

DO_CC=$(CC) $(CFLAGS) -c $< -o $@


# Link
LDFLAGS_DEBUG=-g -ggdb $(OPTIM_DEBUG)
LDFLAGS_PROFILE=-g -pg $(OPTIM_RELEASE)
LDFLAGS_RELEASE=$(OPTIM_RELEASE)


##### UNIX specific variables #####

OBJ_GLX= builddate.c sys_linux.o vid_glx.o $(OBJ_SOUND) $(OBJ_CD) $(OBJ_COMMON)

LDFLAGS_UNIXCOMMON=-lm
LDFLAGS_UNIXCL=-L$(UNIX_X11LIBPATH) -lX11 -lXext -lXxf86dga -lXxf86vm $(LIB_SOUND)
LDFLAGS_UNIXCL_PRELOAD=-lz -ljpeg -lpng -lvorbis -lvorbisfile -lcurl
LDFLAGS_UNIXSV_PRELOAD=-lz -lcurl
LDFLAGS_UNIXSDL_PRELOAD=-lz -ljpeg -lpng -lvorbis -lvorbisfile -lcurl
CFLAGS_UNIX_PRELOAD=-DPREFER_PRELOAD

LDFLAGS_UNIXSDL=`$(SDL_CONFIG) --libs`
EXE_UNIXCL=darkplaces-glx
EXE_UNIXSV=darkplaces-dedicated
EXE_UNIXSDL=darkplaces-sdl
EXE_UNIXCLNEXUIZ=nexuiz-glx
EXE_UNIXSVNEXUIZ=nexuiz-dedicated
EXE_UNIXSDLNEXUIZ=nexuiz-sdl

CMD_UNIXRM=rm -rf


##### Linux specific variables #####

# If you want CD sound in Linux
OBJ_LINUXCD=cd_linux.o
# If you want no CD audio
#OBJ_LINUXCD=$(OBJ_NOCD)

# Link
LDFLAGS_LINUXCL=$(LDFLAGS_UNIXCOMMON) -ldl $(LDFLAGS_UNIXCL)
LDFLAGS_LINUXSV=$(LDFLAGS_UNIXCOMMON) -ldl
LDFLAGS_LINUXSDL=$(LDFLAGS_UNIXCOMMON) -ldl $(LDFLAGS_UNIXSDL)


##### Mac OS X specific variables #####

# No CD support available
OBJ_MACOSXCD=$(OBJ_NOCD)

# Link
LDFLAGS_MACOSXCL=$(LDFLAGS_UNIXCOMMON) -ldl -framework IOKit -framework Carbon $(LIB_SOUND)
LDFLAGS_MACOSXSV=$(LDFLAGS_UNIXCOMMON) -ldl
LDFLAGS_MACOSXSDL=$(LDFLAGS_UNIXCOMMON) -ldl `$(SDL_CONFIG) --static-libs`

OBJ_AGL= builddate.c sys_linux.o vid_agl.o $(OBJ_SOUND) $(OBJ_CD) $(OBJ_COMMON)

EXE_MACOSXCL=darkplaces-agl
EXE_MACOSXCLNEXUIZ=nexuiz-agl


##### SunOS specific variables #####

# No CD support available
OBJ_SUNOSCD=$(OBJ_NOCD)

CFLAGS_SUNOS=-I/usr/lib/oss/include -DBSD_COMP -DSUNOS

# Link
LDFLAGS_SUNOSCL=$(LDFLAGS_UNIXCOMMON) -ldl -lsocket -lnsl -R$(UNIX_X11LIBPATH) -L$(UNIX_X11LIBPATH) -lX11 -lXext -lXxf86vm $(LIB_SOUND)
LDFLAGS_SUNOSSV=$(LDFLAGS_UNIXCOMMON) -ldl -lsocket -lnsl
LDFLAGS_SUNOSSDL=$(LDFLAGS_UNIXCOMMON) -ldl -lsocket -lnsl $(LDFLAGS_UNIXSDL)


##### BSD specific variables #####

#if you want CD sound in BSD
OBJ_BSDCD=cd_bsd.o
#if you want no CD audio
#OBJ_BSDCD=$(OBJ_NOCD)

# Link
LDFLAGS_BSDCL=$(LDFLAGS_UNIXCOMMON) -lutil $(LDFLAGS_UNIXCL)
LDFLAGS_BSDSV=$(LDFLAGS_UNIXCOMMON)
LDFLAGS_BSDSDL=$(LDFLAGS_UNIXCOMMON) $(LDFLAGS_UNIXSDL)


##### Win32 specific variables #####

#if you want CD sound in Win32
OBJ_WINCD=cd_win.o
#if you want no CD audio
#OBJ_WINCD=$(OBJ_NOCD)

OBJ_WGL= builddate.c sys_win.o vid_wgl.o conproc.o $(OBJ_SND_WIN) $(OBJ_WINCD) $(OBJ_COMMON)

# Link
LDFLAGS_WINCOMMON=-lwinmm -lwsock32
LDFLAGS_WINCL=$(LDFLAGS_WINCOMMON) -mwindows -luser32 -lgdi32 -ldxguid -ldinput -lcomctl32
LDFLAGS_WINSV=$(LDFLAGS_WINCOMMON) -mconsole
LDFLAGS_WINSDL=$(LDFLAGS_WINCOMMON) `$(SDL_CONFIG) --libs`
EXE_WINCL=darkplaces.exe
EXE_WINSV=darkplaces-dedicated.exe
EXE_WINSDL=darkplaces-sdl.exe
EXE_WINCLNEXUIZ=nexuiz.exe
EXE_WINSVNEXUIZ=nexuiz-dedicated.exe
EXE_WINSDLNEXUIZ=nexuiz-sdl.exe


##### Commands #####

.PHONY : clean help \
	 debug profile release \
	 cl-debug cl-profile cl-release \
	 sv-debug sv-profile sv-release \
	 sdl-debug sdl-profile sdl-release

help:
	@echo
	@echo "===== Choose one ====="
	@echo "* $(MAKE) clean       : delete all files produced by the build"
	@echo "* $(MAKE) help        : this help"
	@echo "* $(MAKE) debug       : make client and server binaries (debug versions)"
	@echo "* $(MAKE) profile     : make client and server binaries (profile versions)"
	@echo "* $(MAKE) release     : make client and server binaries (release versions)"
	@echo "* $(MAKE) nexuiz      : make client and server binaries with nexuiz icon (release versions)"
	@echo "* $(MAKE) cl-debug    : make client (debug version)"
	@echo "* $(MAKE) cl-profile  : make client (profile version)"
	@echo "* $(MAKE) cl-release  : make client (release version)"
	@echo "* $(MAKE) cl-nexuiz   : make client with nexuiz icon (release version)"
	@echo "* $(MAKE) sv-debug    : make dedicated server (debug version)"
	@echo "* $(MAKE) sv-profile  : make dedicated server (profile version)"
	@echo "* $(MAKE) sv-release  : make dedicated server (release version)"
	@echo "* $(MAKE) sv-nexuiz   : make dedicated server with nexuiz icon (release version)"
	@echo "* $(MAKE) sdl-debug   : make SDL client (debug version)"
	@echo "* $(MAKE) sdl-profile : make SDL client (profile version)"
	@echo "* $(MAKE) sdl-release : make SDL client (release version)"
	@echo "* $(MAKE) sdl-nexuiz  : make SDL client with nexuiz icon (release version)"
	@echo

debug :
	$(MAKE) sv-debug cl-debug sdl-debug

profile :
	$(MAKE) sv-profile cl-profile sdl-profile

release :
	$(MAKE) sv-release cl-release sdl-release

nexuiz :
	$(MAKE) sv-nexuiz cl-nexuiz sdl-nexuiz

cl-debug :
	$(MAKE) bin-debug \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_CL)" LDFLAGS_COMMON="$(LDFLAGS_CL)"

cl-profile :
	$(MAKE) bin-profile \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_CL)" LDFLAGS_COMMON="$(LDFLAGS_CL)"

cl-release :
	$(MAKE) bin-release \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_CL)" LDFLAGS_COMMON="$(LDFLAGS_CL)"

cl-nexuiz :
	$(MAKE) bin-release \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_CLNEXUIZ)" LDFLAGS_COMMON="$(LDFLAGS_CL)"

sv-debug :
	$(MAKE) bin-debug \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_SV)" LDFLAGS_COMMON="$(LDFLAGS_SV)"

sv-profile :
	$(MAKE) bin-profile \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_SV)" LDFLAGS_COMMON="$(LDFLAGS_SV)"

sv-release :
	$(MAKE) bin-release \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_SV)" LDFLAGS_COMMON="$(LDFLAGS_SV)"

sv-nexuiz :
	$(MAKE) bin-release \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_SVNEXUIZ)" LDFLAGS_COMMON="$(LDFLAGS_SV)"

sdl-debug :
	$(MAKE) bin-debug \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_SDL)" LDFLAGS_COMMON="$(LDFLAGS_SDL)"

sdl-profile :
	$(MAKE) bin-profile \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_SDL)" LDFLAGS_COMMON="$(LDFLAGS_SDL)"

sdl-release :
	$(MAKE) bin-release \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_SDL)" LDFLAGS_COMMON="$(LDFLAGS_SDL)"

sdl-nexuiz :
	$(MAKE) bin-release \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		EXE="$(EXE_SDLNEXUIZ)" LDFLAGS_COMMON="$(LDFLAGS_SDL)"

bin-debug :
	@echo
	@echo "========== $(EXE) (debug) =========="
	$(MAKE) $(EXE) \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		CFLAGS="$(CFLAGS_COMMON) $(CFLAGS_EXTRA) $(CFLAGS_DEBUG) $(OPTIM_DEBUG)"\
		LDFLAGS="$(LDFLAGS_DEBUG) $(LDFLAGS_COMMON)"

bin-profile :
	@echo
	@echo "========== $(EXE) (profile) =========="
	$(MAKE) $(EXE) \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		CFLAGS="$(CFLAGS_COMMON) $(CFLAGS_EXTRA) $(CFLAGS_PROFILE) $(OPTIM_RELEASE)"\
		LDFLAGS="$(LDFLAGS_PROFILE) $(LDFLAGS_COMMON)"

bin-release :
	@echo
	@echo "========== $(EXE) (release) =========="
	$(MAKE) $(EXE) \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) DP_SOUND_API=$(DP_SOUND_API) \
		CFLAGS="$(CFLAGS_COMMON) $(CFLAGS_EXTRA) $(CFLAGS_RELEASE) $(OPTIM_RELEASE)"\
		LDFLAGS="$(LDFLAGS_RELEASE) $(LDFLAGS_COMMON)"
	strip $(EXE)

vid_glx.o: vid_glx.c
	$(DO_CC) -I/usr/X11R6/include

vid_sdl.o: vid_sdl.c
	$(DO_CC) $(CFLAGS_SDL)

sys_sdl.o: sys_sdl.c
	$(DO_CC) $(CFLAGS_SDL)

snd_sdl.o: snd_sdl.c
	$(DO_CC) $(CFLAGS_SDL)

cd_sdl.o: cd_sdl.c
	$(DO_CC) $(CFLAGS_SDL)

darkplaces.o: %.o : %.rc
	windres -o $@ $<

nexuiz.o: %.o : %.rc
	windres -o $@ $<

.c.o:
	$(DO_CC)

$(EXE_CL): $(OBJ_CL) $(OBJ_ICON)
	$(DO_LD)

$(EXE_SV): $(OBJ_SV) $(OBJ_ICON)
	$(DO_LD)

$(EXE_SDL): $(OBJ_SDL) $(OBJ_ICON)
	$(DO_LD)

$(EXE_CLNEXUIZ): $(OBJ_CL) $(OBJ_ICON_NEXUIZ)
	$(DO_LD)

$(EXE_SVNEXUIZ): $(OBJ_SV) $(OBJ_ICON_NEXUIZ)
	$(DO_LD)

$(EXE_SDLNEXUIZ): $(OBJ_SDL) $(OBJ_ICON_NEXUIZ)
	$(DO_LD)

clean:
	-$(CMD_RM) $(EXE_CL)
	-$(CMD_RM) $(EXE_SV)
	-$(CMD_RM) $(EXE_SDL)
	-$(CMD_RM) $(EXE_CLNEXUIZ)
	-$(CMD_RM) $(EXE_SVNEXUIZ)
	-$(CMD_RM) $(EXE_SDLNEXUIZ)
	-$(CMD_RM) *.o
	-$(CMD_RM) *.d
