

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

OBJECTS= builddate.o $(CD) $(SND) chase.o cl_demo.o cl_input.o cl_main.o cl_parse.o cl_tent.o cmd.o common.o console.o crc.o cvar.o fractalnoise.o gl_draw.o r_sky.o gl_rmain.o gl_rsurf.o host.o host_cmd.o image.o keys.o mathlib.o menu.o model_alias.o model_brush.o model_shared.o model_sprite.o net_bsd.o net_udp.o net_dgrm.o net_loop.o net_main.o pr_cmds.o pr_edict.o pr_exec.o r_light.o r_explosion.o sbar.o sv_main.o sv_move.o sv_phys.o sv_user.o sv_light.o sys_linux.o transform.o view.o wad.o world.o zone.o vid_shared.o palette.o r_crosshairs.o gl_textures.o gl_models.o r_sprites.o r_modules.o r_explosion.o r_lerpanim.o protocol.o quakeio.o r_clip.o ui.o portals.o sys_shared.o cl_light.o gl_backend.o cl_particles.o cl_screen.o cgamevm.o cgame.o filematch.o collision.o cl_collision.o matrixlib.o cl_video.o dpvsimpledecode.o wavefile.o

#K6/athlon optimizations
CPUOPTIMIZATIONS=-march=k6
#note: don't use -march=athlon, every gcc which has it currently (2.96-3.1)
#have optimizer bugs (like entities disappearing randomly - a bug with
#compiling BOX_ON_PLANE_SIDE in mathlib.h)
#CPUOPTIMIZATIONS=-march=athlon
#686 optimizations
#CPUOPTIMIZATIONS=-march=i686

#use this line for profiling
PROFILEOPTION=-pg -g
NOPROFILEOPTIMIZATIONS=
#use this line for no profiling
#PROFILEOPTION=
#NOPROFILEOPTIMIZATIONS=-fomit-frame-pointer
#use these lines for debugging without profiling
#PROFILEOPTION=
#NOPROFILEOPTIMIZATIONS=

#note:
#the -Werror can be removed to compile even if there are warnings,
#this is used to ensure that all released versions are free of warnings.

#normal compile
OPTIMIZATIONS= -O6 -fno-strict-aliasing -ffast-math -funroll-loops $(NOPROFILEOPTIMIZATIONS) -fexpensive-optimizations $(CPUOPTIMIZATIONS)
CFLAGS= -MD -Wall -Werror -I/usr/X11R6/include -I/usr/include/glide $(OPTIMIZATIONS) $(PROFILEOPTION)
#debug compile
#OPTIMIZATIONS=
#CFLAGS= -MD -Wall -Werror -I/usr/X11R6/include -ggdb $(OPTIMIZATIONS) $(PROFILEOPTION)

#LordHavoc: what is XIE?  XFree 4.1.0 doesn't need it and 4.2.0 seems to be missing it entirely
#LDFLAGS= -L/usr/X11R6/lib -lm -lX11 -lXext -lXIE -lXxf86dga -lXxf86vm -lGL -ldl $(SOUNDLIB) $(PROFILEOPTION)
LDFLAGS= -L/usr/X11R6/lib -lm -lX11 -lXext -lXxf86dga -lXxf86vm -lGL -ldl $(SOUNDLIB) $(PROFILEOPTION)

#if you don't need the -3dfx version, use this line
all: builddate darkplaces-glx
#all: builddate darkplaces-glx darkplaces-3dfx

builddate:
	touch builddate.c

.c.o:
	gcc $(CFLAGS) -c $*.c

darkplaces-glx: $(OBJECTS) vid_glx.o
	gcc -o $@ $^ $(LDFLAGS)

darkplaces-3dfx: $(OBJECTS) in_svgalib.o vid_3dfxsvga.o
	gcc -o $@ $^ $(LDFLAGS)


clean:
	-rm -f darkplaces-glx darkplaces-3dfx *.o *.d

.PHONY: clean builddate

-include *.d

