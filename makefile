##### Variables that you may want to modify #####

#choose the compiler you want to use
CC=gcc-cvs

#recommended for: anyone not using ALSA 0.5
OBJ_SND=snd_oss.o snd_dma.o snd_mix.o snd_mem.o
SOUNDLIB=
#recommended for: anyone using ALSA 0.5
#OBJ_SND=snd_alsa_0_5.o snd_dma.o snd_mix.o snd_mem.o
#SOUNDLIB=-lasound
#recommended for: no one (this driver needs to be updated, it doesn't compile anymore)
#OBJ_SND=snd_alsa_0_9.o snd_dma.o snd_mix.o snd_mem.o
#SOUNDLIB=-lasound
#recommended for: anyone who can't use the above drivers
#OBJ_SND=snd_null.o
#SOUNDLIB=

#if you want CD sound in Linux
OBJ_CD=cd_linux.o
#if you want no CD audio
#OBJ_CD=cd_null.o

#K6/athlon optimizations
#CPUOPTIMIZATIONS=-march=k6
#note: don't use -march=athlon, every gcc which has it currently (2.96-3.1)
#have optimizer bugs (like entities disappearing randomly - a bug with
#compiling BOX_ON_PLANE_SIDE in mathlib.h)
#CPUOPTIMIZATIONS=-march=athlon
#686 optimizations
#CPUOPTIMIZATIONS=-march=i686
#no specific CPU
CPUOPTIMIZATIONS=


##### Variables that you shouldn't care about #####

# Objects
CLIENTOBJECTS=	cgame.o cgamevm.o chase.o cl_collision.o cl_demo.o cl_input.o \
		cl_main.o cl_parse.o cl_particles.o cl_screen.o cl_video.o \
		console.o dpvsimpledecode.o fractalnoise.o gl_backend.o \
		gl_draw.o gl_models.o gl_rmain.o gl_rsurf.o gl_textures.o \
		jpeg.o keys.o menu.o meshqueue.o r_crosshairs.o r_explosion.o \
		r_explosion.o r_lerpanim.o r_light.o r_modules.o r_sky.o \
		r_sprites.o sbar.o ui.o vid_shared.o view.o wavefile.o \
		r_shadow.c
SERVEROBJECTS=	pr_cmds.o pr_edict.o pr_exec.o sv_light.o sv_main.o sv_move.o \
		sv_phys.o sv_user.o
SHAREDOBJECTS=	builddate.o cmd.o collision.o common.o crc.o cvar.o \
		filematch.o host.o host_cmd.o image.o mathlib.o matrixlib.o \
		model_alias.o model_brush.o model_shared.o model_sprite.o \
		net_bsd.o net_dgrm.o net_loop.o net_main.o net_master.o \
		net_udp.o palette.o portals.o protocol.o quakeio.o sys_linux.o \
		sys_shared.o transform.o world.o wad.o zone.o

OBJ_COMMON= $(CLIENTOBJECTS) $(SERVEROBJECTS) $(SHAREDOBJECTS)
OBJ_GLX= vid_glx.c $(OBJ_CD) $(OBJ_SND)
OBJ_DED= vid_null.o cd_null.o snd_null.o


# Compilation
CFLAGS_COMMON=-MD -Wall -Werror
CFLAGS_DEBUG=-ggdb
CFLAGS_PROFILE=-g -pg -ggdb
CFLAGS_RELEASE=

OPTIM_DEBUG=
OPTIM_RELEASE=	-O6 -fno-strict-aliasing -ffast-math -funroll-loops \
		-fexpensive-optimizations $(CPUOPTIMIZATIONS)

DO_CC=$(CC) $(CFLAGS) -c $< -o $@


# Link
LDFLAGS_COMMON=-lm -ldl
LDFLAGS_DEBUG=-g -ggdb
LDFLAGS_PROFILE=-g -pg
LDFLAGS_RELEASE=

EXE_GLX=darkplaces-glx
EXE_DED=darkplaces-dedicated

GLX_LIB=-L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lXxf86vm $(SOUNDLIB)

DO_LD=$(CC) -o $@ $^ $(LDFLAGS)


##### Commands #####

.PHONY : clean help \
	 debug profile release \
	 glx-debug glx-profile glx-release \
	 ded-debug ded-profile ded-release \

help:
	@echo
	@echo "===== Choose one ====="
	@echo "* $(MAKE) clean       : delete the binaries, and .o and .d files"
	@echo "* $(MAKE) help        : this help"
	@echo "* $(MAKE) debug       : make GLX and dedicated binaries (debug versions)"
	@echo "* $(MAKE) profile     : make GLX and dedicated binaries (profile versions)"
	@echo "* $(MAKE) release     : make GLX and dedicated binaries (release versions)"
	@echo "* $(MAKE) glx-debug   : make GLX binary (debug version)"
	@echo "* $(MAKE) glx-profile : make GLX binary (profile version)"
	@echo "* $(MAKE) glx-release : make GLX binary (release version)"
	@echo "* $(MAKE) ded-debug   : make dedicated server (debug version)"
	@echo "* $(MAKE) ded-profile : make dedicated server (profile version)"
	@echo "* $(MAKE) ded-release : make dedicated server (release version)"
	@echo

debug :
	$(MAKE) glx-debug ded-debug

profile :
	$(MAKE) glx-profile ded-profile

release :
	$(MAKE) glx-release ded-release

glx-debug :
	$(MAKE) bin-debug EXE="$(EXE_GLX)"

glx-profile :
	$(MAKE) bin-profile EXE="$(EXE_GLX)"

glx-release :
	$(MAKE) bin-release EXE="$(EXE_GLX)"

ded-debug :
	$(MAKE) bin-debug EXE="$(EXE_DED)"

ded-profile :
	$(MAKE) bin-profile EXE="$(EXE_DED)"

ded-release :
	$(MAKE) bin-release EXE="$(EXE_DED)"

bin-debug :
	@echo
	@echo "========== $(EXE) (debug) =========="
#	@echo Using compiler $(CC)
#	@echo Compiling with flags: $(CFLAGS_COMMON) $(CFLAGS_DEBUG) \
#                                    $(OPTIM_DEBUG)
#	@echo
	$(MAKE) $(EXE) \
		CFLAGS="$(CFLAGS_COMMON) $(CFLAGS_DEBUG) $(OPTIM_DEBUG)"\
		LDFLAGS="$(LDFLAGS_COMMON) $(LDFLAGS_DEBUG)"

bin-profile :
	@echo
	@echo "========== $(EXE) (profile) =========="
#	@echo Using compiler $(CC)
#	@echo Compiling with flags: $(CFLAGS_COMMON) $(CFLAGS_PROFILE) \
#                                    $(OPTIM_RELEASE)
#	@echo
	$(MAKE) $(EXE) \
		CFLAGS="$(CFLAGS_COMMON) $(CFLAGS_PROFILE) $(OPTIM_RELEASE)"\
		LDFLAGS="$(LDFLAGS_COMMON) $(LDFLAGS_PROFILE)"

bin-release :
	@echo
	@echo "========== $(EXE) (release) =========="
#	@echo Using compiler $(CC)
#	@echo Compiling with flags: $(CFLAGS_COMMON) $(CFLAGS_RELEASE) \
#                                    $(OPTIM_RELEASE)
#	@echo
	$(MAKE) $(EXE) \
		CFLAGS="$(CFLAGS_COMMON) $(CFLAGS_RELEASE) $(OPTIM_RELEASE)"\
		LDFLAGS="$(LDFLAGS_COMMON) $(LDFLAGS_RELEASE)"

builddate:
	touch builddate.c

vid_glx.o: vid_glx.c
#	@echo "   Compiling" $<
	$(DO_CC) -I/usr/X11R6/include

.c.o:
#	@echo "   Compiling" $<
	$(DO_CC)

$(EXE_GLX):  $(OBJ_COMMON) $(OBJ_GLX)
#	@echo "   Linking  " $@
	$(DO_LD) $(GLX_LIB)

$(EXE_DED): $(OBJ_COMMON) $(OBJ_DED)
#	@echo "   Linking  " $@
	$(DO_LD)

clean:
	rm -f $(EXE_GLX) $(EXE_DED) *.o *.d

-include *.d
