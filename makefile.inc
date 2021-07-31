# Invalid call detection
CHECKLEVEL1 = @if [ "$(LEVEL)" != 1 ]; then $(MAKE) help; false; fi
CHECKLEVEL2 = @if [ "$(LEVEL)" != 2 ]; then $(MAKE) help; false; fi

# Choose the compiler you want to use
CC?=gcc

# athlon optimizations
#CPUOPTIMIZATIONS?=-march=athlon
# athlon xp optimizations
#CPUOPTIMIZATIONS?=-march=athlon-xp
# athlon 64 optimizations
#CPUOPTIMIZATIONS?=-march=athlon64 -m32
# Pentium 3 optimizations
#CPUOPTIMIZATIONS?=-march=pentium3
# Pentium 4 optimizations
#CPUOPTIMIZATIONS?=-march=pentium4
# 686 (Pentium Pro/II) optimizations
#CPUOPTIMIZATIONS?=-march=i686
# No specific CPU (386 compatible)
#CPUOPTIMIZATIONS?=
# Experimental
#CPUOPTIMIZATIONS?=-fno-math-errno -fno-rounding-math -fno-signaling-nans -fassociative-math -freciprocal-math -fno-signed-zeros -fno-trapping-math
# Normal
ifeq ($(CC), clang)
	CPUOPTIMIZATIONS?=-fno-math-errno -fno-rounding-math -fno-trapping-math
else
	CPUOPTIMIZATIONS?=-fno-math-errno -fno-rounding-math -fno-signaling-nans -fno-trapping-math
endif
# NOTE: *never* *ever* use the -ffast-math or -funsafe-math-optimizations flag
# Also, since gcc 5, -ffinite-math-only makes NaN and zero compare equal inside engine code but not inside QC, which causes error spam for seemingly valid QC code like if (x != 0) return 1 / x;

SDL_CONFIG?=sdl2-config
SDLCONFIG_UNIXCFLAGS?=`$(SDL_CONFIG) --cflags`
SDLCONFIG_UNIXCFLAGS_X11?=
SDLCONFIG_UNIXLIBS?=`$(SDL_CONFIG) --libs`
SDLCONFIG_UNIXLIBS_X11?=-lX11
SDLCONFIG_UNIXSTATICLIBS?=`$(SDL_CONFIG) --static-libs`
SDLCONFIG_UNIXSTATICLIBS_X11?=-lX11
SDLCONFIG_MACOSXCFLAGS=-I/Library/Frameworks/SDL2.framework/Headers -I$(HOME)/Library/Frameworks/SDL2.framework/Headers
SDLCONFIG_MACOSXLIBS=-F$(HOME)/Library/Frameworks/ -framework SDL2 -framework Cocoa $(SDLCONFIG_MACOSXCFLAGS)
SDLCONFIG_MACOSXSTATICLIBS=-F$(HOME)/Library/Frameworks/ -framework SDL2 -framework Cocoa $(SDLCONFIG_MACOSXCFLAGS)
STRIP?=strip


###### Sound #####

OBJ_SND_COMMON=snd_main.o snd_mem.o snd_mix.o snd_ogg.o snd_wav.o $(OBJ_SND_XMP)

# No sound
OBJ_SND_NULL=snd_null.o
LIB_SND_NULL=

###### Common objects and flags #####

# Common objects
OBJ_COMMON= \
	bih.o \
	crypto.o \
	cd_shared.o \
	cl_cmd.o \
	cl_collision.o \
	cl_demo.o \
	cl_ents.o \
	cl_ents4.o \
	cl_ents5.o \
	cl_ents_nq.o \
	cl_ents_qw.o \
	cl_input.o \
	cl_main.o \
	cl_parse.o \
	cl_particles.o \
	cl_screen.o \
	cl_video.o \
	cl_video_libavw.o \
	clvm_cmds.o \
	cmd.o \
	collision.o \
	com_crc16.o \
	com_ents.o \
	com_ents4.o \
	com_game.o \
	com_infostring.o \
	com_msg.o \
	common.o \
	console.o \
	csprogs.o \
	curves.o \
	cvar.o \
	dpvsimpledecode.o \
	filematch.o \
	fractalnoise.o \
	fs.o \
	ft2.o \
	utf8lib.o \
	gl_backend.o \
	gl_draw.o \
	gl_rmain.o \
	gl_rsurf.o \
	gl_textures.o \
	hmac.o \
	host.o \
	image.o \
	image_png.o \
	jpeg.o \
	keys.o \
	lhnet.o \
	libcurl.o \
	mathlib.o \
	matrixlib.o \
	mdfour.o \
	meshqueue.o \
	mod_skeletal_animatevertices_sse.o \
	mod_skeletal_animatevertices_generic.o \
	model_alias.o \
	model_brush.o \
	model_shared.o \
	model_sprite.o \
	netconn.o \
	palette.o \
	polygon.o \
	portals.o \
	protocol.o \
	prvm_cmds.o \
	prvm_edict.o \
	prvm_exec.o \
	r_explosion.o \
	r_lightning.o \
	r_modules.o \
	r_shadow.o \
	r_sky.o \
	r_sprites.o \
	r_stats.o \
	sbar.o \
	sv_ccmds.o \
	sv_demo.o \
	sv_ents.o \
	sv_ents4.o \
	sv_ents5.o \
	sv_ents_csqc.o \
	sv_ents_nq.o \
	sv_main.o \
	sv_move.o \
	sv_phys.o \
	sv_save.o \
	sv_send.o \
	sv_user.o \
	svbsp.o \
	svvm_cmds.o \
	sys_shared.o \
	taskqueue.o \
	vid_shared.o \
	view.o \
	wad.o \
	world.o \
	zone.o

OBJ_MENU= \
	menu.o \
	mvm_cmds.o

# note that builddate.c is very intentionally not compiled to a .o before
# being linked, because it should be recompiled every time an executable is
# built to give the executable a proper date string
OBJ_SV= builddate.c sys_unix.o vid_null.o thread_null.o $(OBJ_SND_NULL) $(OBJ_COMMON)
OBJ_SDL= builddate.c sys_sdl.o vid_sdl.o thread_sdl.o $(OBJ_MENU) $(OBJ_SND_COMMON) $(OBJ_SND_XMP) snd_sdl.o $(OBJ_VIDEO_CAPTURE) $(OBJ_COMMON)


# Compilation
ifeq ($(PEDANTIC),1)
	CFLAGS_STANDARD=-std=c11 -pedantic -D_POSIX_C_SOURCE=200809L -DCONFIG_PEDANTIC
else
	CFLAGS_STANDARD=
endif

CFLAGS_TCC=

ifeq ($(CC), tcc)
	CFLAGS_TCC=-DSDL_DISABLE_IMMINTRIN_H
endif

CFLAGS_COMMON=$(CFLAGS_STANDARD) $(CFLAGS_TCC) $(CFLAGS_MAKEDEP) $(CFLAGS_PRELOAD) $(CFLAGS_FS) $(CFLAGS_WARNINGS) $(CFLAGS_LIBZ) $(CFLAGS_LIBJPEG) $(CFLAGS_SND_XMP) $(CFLAGS_NET) $(CFLAGS_SDL) -D_FILE_OFFSET_BITS=64 -D__KERNEL_STRICT_NAMES -I../../../
CFLAGS_CLIENT=-DCONFIG_MENU $(CFLAGS_VIDEO_CAPTURE)
CFLAGS_SERVER=
CFLAGS_DEBUG=-ggdb
CFLAGS_PROFILE=-g -pg -ggdb -fprofile-arcs
CFLAGS_RELEASE=
CFLAGS_RELEASE_PROFILE=-fbranch-probabilities
CFLAGS_SDL=

ifeq ($(DP_SSE),1)
	CFLAGS_SSE=-msse
	CFLAGS_SSE2=-msse2
else
	CFLAGS_SSE=
	CFLAGS_SSE2=
endif # ifeq ($(DP_SSE),1)

OPTIM_DEBUG=$(CPUOPTIMIZATIONS)
#OPTIM_RELEASE=-O2 -fno-strict-aliasing -ffast-math -funroll-loops $(CPUOPTIMIZATIONS)
#OPTIM_RELEASE=-O2 -fno-strict-aliasing -fno-math-errno -fno-trapping-math -fno-signaling-nans -fcx-limited-range -funroll-loops $(CPUOPTIMIZATIONS)
#OPTIM_RELEASE=-O2 -fno-strict-aliasing -funroll-loops $(CPUOPTIMIZATIONS)
#OPTIM_RELEASE=-O2 -fno-strict-aliasing $(CPUOPTIMIZATIONS)
OPTIM_RELEASE=-O3 -fno-strict-aliasing $(CPUOPTIMIZATIONS)
# NOTE: *never* *ever* use the -ffast-math or -funsafe-math-optimizations flag
# Also, since gcc 5, -ffinite-math-only makes NaN and zero compare equal inside engine code but not inside QC, which causes error spam for seemingly valid QC code like if (x != 0) return 1 / x;

DO_CC=$(CC) $(CFLAGS) -c $< -o $@


# Link
LDFLAGS_DEBUG=-g -ggdb $(OPTIM_DEBUG) -DSVNREVISION=`{ test -d .svn && svnversion; } || { test -d .git && git describe --always; } || echo -` -DBUILDTYPE=debug
LDFLAGS_PROFILE=-g -pg -fprofile-arcs $(OPTIM_RELEASE) -DSVNREVISION=`{ test -d .svn && svnversion; } || { test -d .git && git describe --always; } || echo -` -DBUILDTYPE=profile
LDFLAGS_RELEASE=$(OPTIM_RELEASE) -DSVNREVISION=`{ test -d .svn && svnversion; } || { test -d .git && git describe --always; } || echo -` -DBUILDTYPE=release


##### UNIX specific variables #####

LDFLAGS_UNIXCOMMON=-lm $(LIB_ODE) $(LIB_Z) $(LIB_JPEG) $(LIB_CRYPTO) $(LIB_CRYPTO_RIJNDAEL) $(LIB_SND_XMP)
LDFLAGS_UNIXSV_PRELOAD=-lz -ljpeg -lpng -lcurl
LDFLAGS_UNIXSDL_PRELOAD=-lz -ljpeg -lpng -logg -ltheora -lvorbis -lvorbisenc -lvorbisfile -lcurl -lxmp
CFLAGS_UNIX_PRELOAD=-DPREFER_PRELOAD

LDFLAGS_UNIXSDL=$(SDL_LIBS) #-lGLESv2
EXE_UNIXSV=darkplaces-dedicated
EXE_UNIXSDL=darkplaces-sdl
EXE_UNIXSVNEXUIZ=nexuiz-dedicated
EXE_UNIXSDLNEXUIZ=nexuiz-sdl

CMD_UNIXRM=rm -rf
CMD_UNIXCP=cp -f
CMD_UNIXMKDIR=mkdir -p


##### Linux specific variables #####

# Link
LDFLAGS_LINUXSV=$(LDFLAGS_UNIXCOMMON) -lrt -ldl
LDFLAGS_LINUXSDL=$(LDFLAGS_UNIXCOMMON) -lrt -ldl $(LDFLAGS_UNIXSDL)


##### Mac OS X specific variables #####

# Link
LDFLAGS_MACOSXSV=$(LDFLAGS_UNIXCOMMON) -ldl
LDFLAGS_MACOSXSDL=$(LDFLAGS_UNIXCOMMON) -ldl -framework IOKit $(SDLCONFIG_STATICLIBS) ../../../SDLMain.m


##### SunOS specific variables #####

CFLAGS_SUNOS=-I/usr/lib/oss/include -DBSD_COMP -DSUNOS

# Link
LDFLAGS_SUNOSSV=$(LDFLAGS_UNIXCOMMON) -lrt -ldl -lsocket -lnsl
LDFLAGS_SUNOSSDL=$(LDFLAGS_UNIXCOMMON) -lrt -ldl -lsocket -lnsl $(LDFLAGS_UNIXSDL)


##### BSD specific variables #####

# Link
LDFLAGS_BSDSV=$(LDFLAGS_UNIXCOMMON) 
LDFLAGS_BSDSDL=$(LDFLAGS_UNIXCOMMON) $(LDFLAGS_UNIXSDL)


##### Win32 specific variables #####

WINDRES ?= windres

# Link
# see LDFLAGS_WINCOMMON in makefile
LDFLAGS_WINSV=$(LDFLAGS_WINCOMMON) $(LIB_CRYPTO) $(LIB_CRYPTO_RIJNDAEL) -mconsole -lwinmm -lws2_32 $(LIB_Z) $(LIB_JPEG)
LDFLAGS_WINSDL=$(LDFLAGS_WINCOMMON) $(LIB_CRYPTO) $(LIB_CRYPTO_RIJNDAEL) $(LDFLAGS_UNIXSDL) -lwinmm -lws2_32 $(LIB_Z) $(LIB_JPEG) $(LIB_SND_XMP)
EXE_WINSV=darkplaces-dedicated.exe
EXE_WINSDL=darkplaces-sdl.exe
EXE_WINSVNEXUIZ=nexuiz-dedicated.exe
EXE_WINSDLNEXUIZ=nexuiz-sdl.exe

VPATH := ../../../

##### Commands #####

.PHONY : clean clean-profile help \
	 debug profile release \
	 sv-debug sv-profile sv-release \
	 sdl-debug sdl-profile sdl-release

help:
	@echo
	@echo "===== Choose one ====="
	@echo "* $(MAKE) clean                : delete all files produced by the build except"
	@echo "                              profiling information"
	@echo "* $(MAKE) clean-profile        : delete all files produced by the build, including"
	@echo "                              profiling informaiton"
	@echo "* $(MAKE) help                 : this help"
	@echo "* $(MAKE) debug                : make client and server binaries (debug versions)"
	@echo "* $(MAKE) profile              : make client and server binaries (profile versions)"
	@echo "* $(MAKE) release              : make client and server binaries (release versions)"
	@echo "* $(MAKE) release-profile      : make client and server binaries (release versions)"
	@echo "                              (with profiling optimizations) The profiled"
	@echo "                              version of the program must have been"
	@echo "                              previously compiled"
	@echo "* $(MAKE) nexuiz               : make client and server binaries with nexuiz icon"
	@echo "                              (release versions)"
	@echo "* $(MAKE) sv-debug             : make dedicated server (debug version)"
	@echo "* $(MAKE) sv-profile           : make dedicated server (profile version)"
	@echo "* $(MAKE) sv-release-profile   : make dedicated server (release profile version)"
	@echo "* $(MAKE) sv-release           : make dedicated server (release version)"
	@echo "* $(MAKE) sv-nexuiz            : make dedicated server with nexuiz icon"
	@echo "                              (release version)"
	@echo "* $(MAKE) sdl-debug            : make SDL client (debug version)"
	@echo "* $(MAKE) sdl-profile          : make SDL client (profile version)"
	@echo "* $(MAKE) sdl-release-profile  : make SDL client (release version)"
	@echo "* $(MAKE) sdl-release          : make SDL client (release version)"
	@echo "* $(MAKE) sdl-nexuiz           : make SDL client with nexuiz icon (release version)"
	@echo

debug :
	$(MAKE) $(TARGETS_DEBUG)

profile :
	$(MAKE) $(TARGETS_PROFILE)

release :
	$(MAKE) $(TARGETS_RELEASE)

release-profile :
	$(MAKE) $(TARGETS_RELEASE_PROFILE)

nexuiz :
	$(MAKE) $(TARGETS_NEXUIZ)

sv-debug :
	$(MAKE) bin-debug \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		EXE='$(EXE_SV)' CFLAGS_FEATURES='$(CFLAGS_SERVER)' LDFLAGS_COMMON='$(LDFLAGS_SV)' LEVEL=1

sv-profile :
	$(MAKE) bin-profile \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		EXE='$(EXE_SV)' CFLAGS_FEATURES='$(CFLAGS_SERVER)' LDFLAGS_COMMON='$(LDFLAGS_SV)' LEVEL=1

sv-release :
	$(MAKE) bin-release \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		EXE='$(EXE_SV)' CFLAGS_FEATURES='$(CFLAGS_SERVER)' LDFLAGS_COMMON='$(LDFLAGS_SV)' LEVEL=1

sv-release-profile :
	$(MAKE) bin-release-profile \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		EXE='$(EXE_SV)' CFLAGS_FEATURES='$(CFLAGS_SERVER)' LDFLAGS_COMMON='$(LDFLAGS_SV)' LEVEL=1

sv-nexuiz :
	$(MAKE) bin-release \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		EXE='$(EXE_SVNEXUIZ)' CFLAGS_FEATURES='$(CFLAGS_SERVER)' LDFLAGS_COMMON='$(LDFLAGS_SV)' LEVEL=1

sdl-debug :
	$(MAKE) bin-debug \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		EXE='$(EXE_SDL)' CFLAGS_FEATURES='$(CFLAGS_CLIENT)' CFLAGS_SDL='$(SDLCONFIG_CFLAGS)' LDFLAGS_COMMON='$(LDFLAGS_SDL)' LEVEL=1

sdl-profile :
	$(MAKE) bin-profile \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		EXE='$(EXE_SDL)' CFLAGS_FEATURES='$(CFLAGS_CLIENT)' CFLAGS_SDL='$(SDLCONFIG_CFLAGS)' LDFLAGS_COMMON='$(LDFLAGS_SDL)' LEVEL=1

sdl-release :
	$(MAKE) bin-release \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		EXE='$(EXE_SDL)' CFLAGS_FEATURES='$(CFLAGS_CLIENT)' CFLAGS_SDL='$(SDLCONFIG_CFLAGS)' LDFLAGS_COMMON='$(LDFLAGS_SDL)' LEVEL=1

sdl-release-profile :
	$(MAKE) bin-release-profile \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		EXE='$(EXE_SDL)' CFLAGS_FEATURES='$(CFLAGS_CLIENT)' CFLAGS_SDL='$(SDLCONFIG_CFLAGS)' LDFLAGS_COMMON='$(LDFLAGS_SDL)' LEVEL=1

sdl-nexuiz :
	$(MAKE) bin-release \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		EXE='$(EXE_SDLNEXUIZ)' CFLAGS_FEATURES='$(CFLAGS_CLIENT)' CFLAGS_SDL='$(SDLCONFIG_CFLAGS)' LDFLAGS_COMMON='$(LDFLAGS_SDL)' LEVEL=1

bin-debug :
	$(CHECKLEVEL1)
	@echo
	@echo '========== $(EXE) (debug) =========='
	$(MAKE) prepare BUILD_DIR=build-obj/debug/$(EXE)
	$(MAKE) -C build-obj/debug/$(EXE) $(EXE) \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		CFLAGS='$(CFLAGS_COMMON) $(CFLAGS_FEATURES) $(CFLAGS_EXTRA) $(CFLAGS_DEBUG) $(OPTIM_DEBUG)'\
		LDFLAGS='$(LDFLAGS_DEBUG) $(LDFLAGS_COMMON)' LEVEL=2

bin-profile :
	$(CHECKLEVEL1)
	@echo
	@echo '========== $(EXE) (profile) =========='
	$(MAKE) prepare BUILD_DIR=build-obj/profile/$(EXE)
	$(MAKE) -C build-obj/profile/$(EXE) $(EXE) \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		CFLAGS='$(CFLAGS_COMMON) $(CFLAGS_FEATURES) $(CFLAGS_EXTRA) $(CFLAGS_PROFILE) $(OPTIM_RELEASE)'\
		LDFLAGS='$(LDFLAGS_PROFILE) $(LDFLAGS_COMMON)' LEVEL=2

bin-release :
	$(CHECKLEVEL1)
	@echo
	@echo '========== $(EXE) (release) =========='
	$(MAKE) prepare BUILD_DIR=build-obj/release/$(EXE)
	$(MAKE) -C build-obj/release/$(EXE) $(EXE) \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		CFLAGS='$(CFLAGS_COMMON) $(CFLAGS_FEATURES) $(CFLAGS_EXTRA) $(CFLAGS_RELEASE) $(OPTIM_RELEASE)'\
		LDFLAGS='$(LDFLAGS_RELEASE) $(LDFLAGS_COMMON)' LEVEL=2
	$(STRIP) $(EXE)

bin-release-profile :
	$(CHECKLEVEL1)
	@echo
	@echo '========== $(EXE) (release) =========='
	$(MAKE) prepare BUILD_DIR=build-obj/release-profile/$(EXE)
	$(MAKE) -C build-obj/release-profile/$(EXE) $(EXE) \
		DP_MAKE_TARGET=$(DP_MAKE_TARGET) \
		CFLAGS='$(CFLAGS_COMMON) $(CFLAGS_FEATURES) $(CFLAGS_EXTRA) $(CFLAGS_RELEASE_PROFILE) $(OPTIM_RELEASE)'\
		LDFLAGS='$(LDFLAGS_RELEASE) $(LDFLAGS_COMMON)' LEVEL=2
	$(STRIP) $(EXE)

prepare :
	$(CMD_MKDIR) $(BUILD_DIR)
	$(CMD_CP) makefile.inc $(BUILD_DIR)/
	$(CMD_CP) $(MAKEFILE) $(BUILD_DIR)/


#this checks USEODE when compiling so it needs the ODE flags as well
cl_parse.o: cl_parse.c
	$(CHECKLEVEL2)
	$(DO_CC) $(CFLAGS_ODE)

cs_progs.o: cs_progs.c
	$(CHECKLEVEL2)
	$(DO_CC) $(CFLAGS_ODE)

sv_main.o: sv_main.c
	$(CHECKLEVEL2)
	$(DO_CC) $(CFLAGS_ODE)

sv_phys.o: sv_phys.c
	$(CHECKLEVEL2)
	$(DO_CC) $(CFLAGS_ODE)

prvm_cmds.o: prvm_cmds.c
	$(CHECKLEVEL2)
	$(DO_CC) $(CFLAGS_ODE)

world.o: world.c
	$(CHECKLEVEL2)
	$(DO_CC) $(CFLAGS_ODE)

crypto.o: crypto.c
	$(CHECKLEVEL2)
	$(DO_CC) $(CFLAGS_CRYPTO) $(CFLAGS_CRYPTO_RIJNDAEL)

mod_skeletal_animatevertices_sse.o: mod_skeletal_animatevertices_sse.c
	$(CHECKLEVEL2)
	$(DO_CC) $(CFLAGS_SSE)

snd_xmp.o: snd_xmp.c
	$(CHECKLEVEL2)
	$(DO_CC) $(CFLAGS_SND_XMP)

#this checks USEXMP when compiling so it needs the XMP flags as well
snd_main.o: snd_main.c
	$(CHECKLEVEL2)
	$(DO_CC) $(CFLAGS_SND_XMP)

#this checks USEXMP when compiling so it needs the XMP flags as well
snd_mem.o: snd_mem.c
	$(CHECKLEVEL2)
	$(DO_CC) $(CFLAGS_SND_XMP)

darkplaces.o: %.o : %.rc
	$(CHECKLEVEL2)
	$(WINDRES) -o $@ $<

nexuiz.o: %.o : %.rc
	$(CHECKLEVEL2)
	$(WINDRES) -o $@ $<

.c.o:
	$(CHECKLEVEL2)
	$(DO_CC)

$(EXE_SV): $(OBJ_SV) $(OBJ_ICON)
	$(CHECKLEVEL2)
	$(DO_LD)

$(EXE_SDL): $(OBJ_SDL) $(OBJ_ICON)
	$(CHECKLEVEL2)
	$(DO_LD)

$(EXE_SVNEXUIZ): $(OBJ_SV) $(OBJ_ICON_NEXUIZ)
	$(CHECKLEVEL2)
	$(DO_LD)

$(EXE_SDLNEXUIZ): $(OBJ_SDL) $(OBJ_ICON_NEXUIZ)
	$(CHECKLEVEL2)
	$(DO_LD)

clean:
	-$(CMD_RM) $(EXE_SV)
	-$(CMD_RM) $(EXE_SDL)
	-$(CMD_RM) $(EXE_SVNEXUIZ)
	-$(CMD_RM) $(EXE_SDLNEXUIZ)
	-$(CMD_RM) *.o
	-$(CMD_RM) *.d
	-$(CMD_RM) *.gch
	-$(CMD_RM) build-obj/

clean-profile: clean
	-$(CMD_RM) *.gcda
	-$(CMD_RM) *.gcno
