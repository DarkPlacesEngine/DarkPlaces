# Choose the compiler you want to use
CC=gcc

# athlon optimizations
#CPUOPTIMIZATIONS=-march=athlon
# athlon xp optimizations
#CPUOPTIMIZATIONS=-march=athlon-xp
# Pentium 3 optimizations
#CPUOPTIMIZATIONS=-march=pentium3
# Pentium 4 optimizations
#CPUOPTIMIZATIONS=-march=pentium4
# 686 (Pentium Pro/II) optimizations
#CPUOPTIMIZATIONS=-march=i686
# No specific CPU (386 compatible)
CPUOPTIMIZATIONS=


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
LIB_SND_COREAUDIO=

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
	cgame.o \
	cgamevm.o \
	cl_collision.o \
	cl_demo.o \
	cl_input.o \
	cl_main.o \
	cl_parse.o \
	cl_particles.o \
	cl_screen.o \
	cl_video.o \
	cmd.o \
	collision.o \
	common.o \
	console.o \
	curves.o \
	cvar.o \
	dpvsimpledecode.o \
	filematch.o \
	fractalnoise.o \
	fs.o \
	gl_backend.o \
	gl_draw.o \
	gl_models.o \
	gl_rmain.o \
	gl_rsurf.o \
	gl_textures.o \
	host.o \
	host_cmd.o \
	image.o \
	jpeg.o \
	keys.o \
	lhnet.o \
	mathlib.o \
	matrixlib.o \
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
	r_crosshairs.o \
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
	svvm_cmds.o \
	sys_shared.o \
	ui.o \
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
# CFLAGS_COMMON=-MD -Wall -Werror
CFLAGS_COMMON=-MD -Wall
CFLAGS_DEBUG=-ggdb
CFLAGS_PROFILE=-g -pg -ggdb
CFLAGS_RELEASE=

OPTIM_DEBUG=$(CPUOPTIMIZATIONS)
OPTIM_RELEASE=-O2 -fno-strict-aliasing -ffast-math -funroll-loops $(CPUOPTIMIZATIONS)

DO_CC=$(CC) $(CFLAGS) -c $< -o $@


# Link
LDFLAGS_DEBUG=-g -ggdb $(OPTIM_DEBUG)
LDFLAGS_PROFILE=-g -pg $(OPTIM_RELEASE)
LDFLAGS_RELEASE=$(OPTIM_RELEASE)


##### UNIX specific variables #####

OBJ_GLX= builddate.c sys_linux.o vid_glx.o $(OBJ_SOUND) $(OBJ_CD) $(OBJ_COMMON)

LDFLAGS_UNIXCOMMON=-lm
#LDFLAGS_UNIXCL=-L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lXxf86vm $(LIB_SOUND)
LDFLAGS_UNIXCL=$(UNIX_X11LIBPATH) -lX11 -lXext -lXxf86dga -lXxf86vm $(LIB_SOUND)
LDFLAGS_UNIXSDL=`sdl-config --libs`
EXE_UNIXCL=darkplaces-glx
EXE_UNIXSV=darkplaces-dedicated
EXE_UNIXSDL=darkplaces-sdl

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

# If you want CD sound in Mac OS X
#OBJ_MACOSXCD=cd_macosx.o
# If you want no CD audio
OBJ_MACOSXCD=$(OBJ_NOCD)

# Link
LDFLAGS_MACOSXCL=$(LDFLAGS_UNIXCOMMON) -ldl $(LDFLAGS_UNIXCL)
LDFLAGS_MACOSXSV=$(LDFLAGS_UNIXCOMMON) -ldl
LDFLAGS_MACOSXSDL=$(LDFLAGS_UNIXCOMMON) -ldl `sdl-config --static-libs`

OBJ_AGL= builddate.c sys_linux.o vid_agl.o $(OBJ_SOUND) $(OBJ_CD) $(OBJ_COMMON)

##### SunOS specific variables #####

# No CD support available
OBJ_SUNOSCD=$(OBJ_NOCD)

CFLAGS_SUNOS=-I/usr/lib/oss/include -DBSD_COMP

# Link
LDFLAGS_SUNOSCL=$(LDFLAGS_UNIXCOMMON) -ldl -lsocket -lnsl $(LDFLAGS_UNIXCL)
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

OBJ_WGL= builddate.c darkplaces.o sys_win.o vid_wgl.o conproc.o $(OBJ_SND_WIN) $(OBJ_WINCD) $(OBJ_COMMON)

# Link
LDFLAGS_WINCOMMON=-lwinmm -lwsock32
LDFLAGS_WINCL=$(LDFLAGS_WINCOMMON) -mwindows -luser32 -lgdi32 -ldxguid -ldinput -lcomctl32
LDFLAGS_WINSV=$(LDFLAGS_WINCOMMON) -mconsole
LDFLAGS_WINSDL=$(LDFLAGS_WINCOMMON) `sdl-config --libs`
EXE_WINCL=darkplaces.exe
EXE_WINSV=darkplaces-dedicated.exe
EXE_WINSDL=darkplaces-sdl.exe


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
	@echo "* $(MAKE) cl-debug    : make client (debug version)"
	@echo "* $(MAKE) cl-profile  : make client (profile version)"
	@echo "* $(MAKE) cl-release  : make client (release version)"
	@echo "* $(MAKE) sv-debug    : make dedicated server (debug version)"
	@echo "* $(MAKE) sv-profile  : make dedicated server (profile version)"
	@echo "* $(MAKE) sv-release  : make dedicated server (release version)"
	@echo "* $(MAKE) sdl-debug   : make SDL client (debug version)"
	@echo "* $(MAKE) sdl-profile : make SDL client (profile version)"
	@echo "* $(MAKE) sdl-release : make SDL client (release version)"
	@echo

debug :
	$(MAKE) sv-debug cl-debug sdl-debug

profile :
	$(MAKE) sv-profile cl-profile sdl-profile

release :
	$(MAKE) sv-release cl-release sdl-release

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
	$(DO_CC) `sdl-config --cflags`

sys_sdl.o: sys_sdl.c
	$(DO_CC) `sdl-config --cflags`

snd_sdl.o: snd_sdl.c
	$(DO_CC) `sdl-config --cflags`

cd_sdl.o: cd_sdl.c
	$(DO_CC) `sdl-config --cflags`

darkplaces.o: %.o : %.rc
	windres -o $@ $<

.c.o:
	$(DO_CC)

$(EXE_CL): $(OBJ_CL)
	$(DO_LD)

$(EXE_SV): $(OBJ_SV)
	$(DO_LD)

$(EXE_SDL): $(OBJ_SDL)
	$(DO_LD)

clean:
	-$(CMD_RM) $(EXE_CL)
	-$(CMD_RM) $(EXE_SV)
	-$(CMD_RM) $(EXE_SDL)
	-$(CMD_RM) *.o
	-$(CMD_RM) *.d
