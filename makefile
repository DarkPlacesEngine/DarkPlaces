

#uncomment these according to your sound driver

#recommended for: anyone not using ALSA 0.5
SND=snd_oss.o snd_dma.o snd_mix.o snd_mem.o
SOUNDLIB=
#recommended for: anyone using ALSA 0.5
#SND=snd_alsa_0_5.o snd_dma.o snd_mix.o snd_mem.o
#SOUNDLIB=-lasound
#recommended for: no one (this driver needs to be updated, it doesn't compile anymore)
#SND=snd_alsa_0_9.o snd_dma.o snd_mix.o snd_mem.o
#SOUNDLIB=-lasound
#recommended for: anyone who can't use the above drivers
#SND=snd_null.o
#SOUNDLIB=


#uncomment your preference
#if you want CD sound in Linux
#CD=cd_linux.o
#if you want no CD audio
CD=cd_null.o

CLIENTOBJECTS= cgame.o cgamevm.o chase.o cl_collision.o cl_demo.o cl_input.o cl_main.o cl_parse.o cl_particles.o cl_screen.o cl_video.o console.o dpvsimpledecode.o fractalnoise.o gl_backend.o gl_draw.o gl_models.o gl_rmain.o gl_rsurf.o gl_textures.o keys.o menu.o meshqueue.o r_crosshairs.o r_explosion.o r_explosion.o r_lerpanim.o r_light.o r_modules.o r_sky.o r_sprites.o sbar.o ui.o vid_shared.o view.o wavefile.o r_shadow.c
SERVEROBJECTS= pr_cmds.o pr_edict.o pr_exec.o sv_light.o sv_main.o sv_move.o sv_phys.o sv_user.o
SHAREDOBJECTS= builddate.o cmd.o collision.o common.o crc.o cvar.o filematch.o host.o host_cmd.o image.o mathlib.o matrixlib.o model_alias.o model_brush.o model_shared.o model_sprite.o net_bsd.o net_dgrm.o net_loop.o net_main.o net_master.o net_udp.o palette.o portals.o protocol.o quakeio.o sys_linux.o sys_shared.o transform.o world.o wad.o zone.o $(NETOBJECTS) $(SERVEROBJECTS)


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

#use this line for profiling
PROFILEOPTION=-pg -g
#use this line for no profiling
#PROFILEOPTION=

#note:
#the -Werror can be removed to compile even if there are warnings,
#this is used to ensure that all released versions are free of warnings.

#normal compile
OPTIMIZATIONS= -O6 -fno-strict-aliasing -ffast-math -funroll-loops -fexpensive-optimizations $(CPUOPTIMIZATIONS)
CFLAGS= -MD -Wall -Werror $(OPTIMIZATIONS) $(PROFILEOPTION)
#debug compile
#OPTIMIZATIONS=
#CFLAGS= -MD -Wall -Werror -ggdb $(OPTIMIZATIONS) $(PROFILEOPTION)

LDFLAGS= $(PROFILEOPTION) -lm -ldl

all: builddate darkplaces-dedicated darkplaces-glx

builddate:
	touch builddate.c

vid_glx.o: vid_glx.c
	gcc $(CFLAGS) -c vid_glx.c -I/usr/X11R6/include

.c.o:
	gcc $(CFLAGS) -c $*.c

darkplaces-glx:  $(SHAREDOBJECTS) $(CLIENTOBJECTS) $(SERVEROBJECTS) vid_glx.o $(CD) $(SND)
	gcc -o $@ $^ $(LDFLAGS) -L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lXxf86vm $(SOUNDLIB)

darkplaces-dedicated: $(SHAREDOBJECTS) $(CLIENTOBJECTS) $(SERVEROBJECTS) vid_null.o cd_null.o snd_null.o
	gcc -o $@ $^ $(LDFLAGS)

clean:
	-rm -f darkplaces-glx darkplaces-dedicated *.o *.d

.PHONY: clean builddate

-include *.d

