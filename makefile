##### Variables that you may want to modify #####

#choose the compiler you want to use
CC=gcc

#recommended for: anyone not using ALSA 0.5
OBJ_LINUXSOUND=snd_oss.o snd_dma.o snd_mix.o snd_mem.o ogg.o
LINUXSOUNDLIB=
#recommended for: anyone using ALSA 0.5
#OBJ_LINUXSOUND=snd_alsa_0_5.o snd_dma.o snd_mix.o snd_mem.o ogg.o
#LINUXSOUNDLIB=-lasound
#recommended for: no one (this driver needs to be updated, it doesn't compile anymore)
#OBJ_LINUXSOUND=snd_alsa_0_9.o snd_dma.o snd_mix.o snd_mem.o ogg.o
#LINUXSOUNDLIB=-lasound
#recommended for: anyone who can't use the above drivers
#OBJ_LINUXSOUND=snd_null.o
#LINUXSOUNDLIB=

#if you want CD sound in Linux
OBJ_LINUXCD=cd_shared.o cd_linux.o
#if you want no CD audio
#OBJ_LINUXCD=cd_null.o

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

ifdef windir
CMD_RM=del
else
CMD_RM=rm -f
endif

# Objects
CLIENTOBJECTS=	cgame.o cgamevm.o cl_collision.o cl_demo.o cl_input.o \
		cl_main.o cl_parse.o cl_particles.o cl_screen.o cl_video.o \
		console.o dpvsimpledecode.o fractalnoise.o gl_backend.o \
		gl_draw.o gl_models.o gl_rmain.o gl_rsurf.o gl_textures.o \
		jpeg.o keys.o menu.o meshqueue.o r_crosshairs.o r_explosion.o \
		r_lerpanim.o r_light.o r_lightning.o r_modules.o r_sky.o \
		r_sprites.o sbar.o ui.o vid_shared.o view.o wavefile.o \
                r_shadow.o prvm_exec.o prvm_edict.o prvm_cmds.o
SERVEROBJECTS=	pr_cmds.o pr_edict.o pr_exec.o sv_main.o sv_move.o \
		sv_phys.o sv_user.o
SHAREDOBJECTS=	cmd.o collision.o common.o crc.o cvar.o \
		filematch.o host.o host_cmd.o image.o mathlib.o matrixlib.o \
		model_alias.o model_brush.o model_shared.o model_sprite.o \
		netconn.o lhnet.o palette.o portals.o protocol.o fs.o \
		sys_shared.o winding.o world.o wad.o zone.o curves.o
COMMONOBJECTS= $(CLIENTOBJECTS) $(SERVEROBJECTS) $(SHAREDOBJECTS)

# note that builddate.c is very intentionally not compiled to a .o before
# being linked, because it should be recompiled every time an executable is
# built to give the executable a proper date string
OBJ_GLX= builddate.c sys_linux.o vid_glx.o $(OBJ_LINUXCD) $(OBJ_LINUXSOUND) $(COMMONOBJECTS)
OBJ_DED= builddate.c sys_linux.o vid_null.o cd_null.o snd_null.o $(COMMONOBJECTS)
OBJ_WGL_EXE= builddate.c sys_win.o vid_wgl.o conproc.o cd_shared.o cd_win.o snd_win.o snd_dma.o snd_mix.o snd_mem.o $(COMMONOBJECTS)
OBJ_DED_EXE= builddate.c sys_linux.o vid_null.o cd_null.o snd_null.o $(COMMONOBJECTS)


# Compilation
# CFLAGS_NONEXECOMMON=-MD -Wall -Werror
CFLAGS_NONEXECOMMON=-MD -Wall
CFLAGS_EXECOMMON=-MD -Wall
CFLAGS_DEBUG=-ggdb
CFLAGS_PROFILE=-g -pg -ggdb
CFLAGS_RELEASE=

OPTIM_DEBUG=
OPTIM_RELEASE=	-O2 -fno-strict-aliasing -ffast-math -fexpensive-optimizations $(CPUOPTIMIZATIONS)

DO_CC=$(CC) $(CFLAGS) -c $< -o $@


# Link
# LordHavoc note: I have been informed that system libraries must come last
# on the linker line, and that -lm must always be last
LDFLAGS_GLX=-ldl -lm
LDFLAGS_DED=-ldl -lm
LDFLAGS_WGL_EXE=-mwindows -lwinmm -lwsock32 -luser32 -lgdi32 -ldxguid -ldinput -lcomctl32
LDFLAGS_DED_EXE=-mconsole -lwinmm -lwsock32
LDFLAGS_DEBUG=-g -ggdb
LDFLAGS_PROFILE=-g -pg
LDFLAGS_RELEASE=

EXE_GLX=darkplaces-glx
EXE_DED=darkplaces-dedicated
EXE_WGL_EXE=darkplaces.exe
EXE_DED_EXE=darkplaces-dedicated.exe

DO_LD=$(CC) -o $@ $^ $(LDFLAGS)


##### Commands #####

.PHONY : clean help \
	 debug profile release \
	 glx-debug glx-profile glx-release \
	 ded-debug ded-profile ded-release \
	 exedebug exeprofile exerelease \
	 exewgl-debug exewgl-profile exewgl-release \
	 exeded-debug exeded-profile exeded-release \

help:
	@echo
	@echo "===== Choose one ====="
	@echo "* $(MAKE) clean          : delete the binaries, and .o and .d files"
	@echo "* $(MAKE) help           : this help"
	@echo "* $(MAKE) debug          : make GLX and dedicated binaries (debug versions)"
	@echo "* $(MAKE) profile        : make GLX and dedicated binaries (profile versions)"
	@echo "* $(MAKE) release        : make GLX and dedicated binaries (release versions)"
	@echo "* $(MAKE) glx-debug      : make GLX client (debug version)"
	@echo "* $(MAKE) glx-profile    : make GLX client (profile version)"
	@echo "* $(MAKE) glx-release    : make GLX client (release version)"
	@echo "* $(MAKE) ded-debug      : make dedicated server (debug version)"
	@echo "* $(MAKE) ded-profile    : make dedicated server (profile version)"
	@echo "* $(MAKE) ded-release    : make dedicated server (release version)"
	@echo "* $(MAKE) exedebug       : make WGL and dedicated binaries (debug versions)"
	@echo "* $(MAKE) exeprofile     : make WGL and dedicated binaries (profile versions)"
	@echo "* $(MAKE) exerelease     : make WGL and dedicated binaries (release versions)"
	@echo "* $(MAKE) exewgl-debug   : make WGL client (debug version)"
	@echo "* $(MAKE) exewgl-profile : make WGL client (profile version)"
	@echo "* $(MAKE) exewgl-release : make WGL client (release version)"
	@echo "* $(MAKE) exeded-debug   : make dedicated server (debug version)"
	@echo "* $(MAKE) exeded-profile : make dedicated server (profile version)"
	@echo "* $(MAKE) exeded-release : make dedicated server (release version)"
	@echo

debug :
	$(MAKE) glx-debug ded-debug

profile :
	$(MAKE) glx-profile ded-profile

release :
	$(MAKE) glx-release ded-release

exedebug :
	$(MAKE) wglexe-debug dedexe-debug

exeprofile :
	$(MAKE) wglexe-profile dedexe-profile

exerelease :
	$(MAKE) wglexe-release dedexe-release

glx-debug :
	$(MAKE) bin-debug EXE="$(EXE_GLX)" LDFLAGS_COMMON="$(LDFLAGS_GLX)" CFLAGS_COMMON="$(CFLAGS_NONEXECOMMON)"

glx-profile :
	$(MAKE) bin-profile EXE="$(EXE_GLX)" LDFLAGS_COMMON="$(LDFLAGS_GLX)" CFLAGS_COMMON="$(CFLAGS_NONEXECOMMON)"

glx-release :
	$(MAKE) bin-release EXE="$(EXE_GLX)" LDFLAGS_COMMON="$(LDFLAGS_GLX)" CFLAGS_COMMON="$(CFLAGS_NONEXECOMMON)"

ded-debug :
	$(MAKE) bin-debug EXE="$(EXE_DED)" LDFLAGS_COMMON="$(LDFLAGS_DED)" CFLAGS_COMMON="$(CFLAGS_NONEXECOMMON)"

ded-profile :
	$(MAKE) bin-profile EXE="$(EXE_DED)" LDFLAGS_COMMON="$(LDFLAGS_DED)" CFLAGS_COMMON="$(CFLAGS_NONEXECOMMON)"

ded-release :
	$(MAKE) bin-release EXE="$(EXE_DED)" LDFLAGS_COMMON="$(LDFLAGS_DED)" CFLAGS_COMMON="$(CFLAGS_NONEXECOMMON)"

wglexe-debug :
	$(MAKE) bin-debug EXE="$(EXE_WGL_EXE)" LDFLAGS_COMMON="$(LDFLAGS_WGL_EXE)" CFLAGS_COMMON="$(CFLAGS_EXECOMMON)"

wglexe-profile :
	$(MAKE) bin-profile EXE="$(EXE_WGL_EXE)" LDFLAGS_COMMON="$(LDFLAGS_WGL_EXE)" CFLAGS_COMMON="$(CFLAGS_EXECOMMON)"

wglexe-release :
	$(MAKE) bin-release EXE="$(EXE_WGL_EXE)" LDFLAGS_COMMON="$(LDFLAGS_WGL_EXE)" CFLAGS_COMMON="$(CFLAGS_EXECOMMON)"

dedexe-debug :
	$(MAKE) bin-debug EXE="$(EXE_DED_EXE)" LDFLAGS_COMMON="$(LDFLAGS_DED_EXE)" CFLAGS_COMMON="$(CFLAGS_EXECOMMON)"

dedexe-profile :
	$(MAKE) bin-profile EXE="$(EXE_DED_EXE)" LDFLAGS_COMMON="$(LDFLAGS_DED_EXE)" CFLAGS_COMMON="$(CFLAGS_EXECOMMON)"

dedexe-release :
	$(MAKE) bin-release EXE="$(EXE_DED_EXE)" LDFLAGS_COMMON="$(LDFLAGS_DED_EXE)" CFLAGS_COMMON="$(CFLAGS_EXECOMMON)"

bin-debug :
	@echo
	@echo "========== $(EXE) (debug) =========="
	$(MAKE) $(EXE) \
		CFLAGS="$(CFLAGS_COMMON) $(CFLAGS_DEBUG) $(OPTIM_DEBUG)"\
		LDFLAGS="$(LDFLAGS_DEBUG) $(LDFLAGS_COMMON)"

bin-profile :
	@echo
	@echo "========== $(EXE) (profile) =========="
	$(MAKE) $(EXE) \
		CFLAGS="$(CFLAGS_COMMON) $(CFLAGS_PROFILE) $(OPTIM_RELEASE)"\
		LDFLAGS="$(LDFLAGS_PROFILE) $(LDFLAGS_COMMON)"

bin-release :
	@echo
	@echo "========== $(EXE) (release) =========="
	$(MAKE) $(EXE) \
		CFLAGS="$(CFLAGS_COMMON) $(CFLAGS_RELEASE) $(OPTIM_RELEASE)"\
		LDFLAGS="$(LDFLAGS_RELEASE) $(LDFLAGS_COMMON)"
	strip $(EXE)

vid_glx.o: vid_glx.c
	$(DO_CC) -I/usr/X11R6/include

.c.o:
	$(DO_CC)

$(EXE_GLX):  $(OBJ_GLX)
	$(DO_LD) -L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lXxf86vm $(LINUXSOUNDLIB)

$(EXE_DED): $(OBJ_DED)
	$(DO_LD)

$(EXE_WGL_EXE):  $(OBJ_WGL_EXE)
	$(DO_LD)

$(EXE_DED_EXE): $(OBJ_DED_EXE)
	$(DO_LD)


clean:
	-$(CMD_RM) $(EXE_GLX)
	-$(CMD_RM) $(EXE_DED)
	-$(CMD_RM) $(EXE_WGL_EXE)
	-$(CMD_RM) $(EXE_DED_EXE)
	-$(CMD_RM) *.o
	-$(CMD_RM) *.d

-include *.d

