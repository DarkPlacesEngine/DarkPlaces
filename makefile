#uncomment one of these according to your sound driver
#if you use ALSA version 0.9.x
#SND=snd_alsa_0_9.o
#SOUNDLIB=-lasound
#if you use ALSA version 0.5.x
SND=snd_alsa_0_5.o
SOUNDLIB=-lasound
#if you use the kernel sound driver or OSS
#SND=snd_oss.o
#SOUNDLIB=

OBJECTS= builddate.o cd_linux.o chase.o cl_demo.o cl_input.o cl_main.o cl_parse.o cl_tent.o cmd.o common.o console.o crc.o cvar.o fractalnoise.o gl_draw.o r_sky.o gl_rmain.o gl_rsurf.o host.o host_cmd.o image.o keys.o mathlib.o menu.o model_alias.o model_brush.o model_shared.o model_sprite.o net_bsd.o net_udp.o net_dgrm.o net_loop.o net_main.o pr_cmds.o pr_edict.o pr_exec.o r_light.o r_explosion.o sbar.o snd_dma.o snd_mem.o snd_mix.o $(SND) sv_main.o sv_move.o sv_phys.o sv_user.o sv_light.o sys_linux.o transform.o view.o wad.o world.o zone.o vid_shared.o palette.o r_crosshairs.o gl_textures.o gl_models.o r_sprites.o r_modules.o r_explosion.o r_lerpanim.o protocol.o quakeio.o r_clip.o ui.o portals.o sys_shared.o cl_light.o gl_backend.o cl_particles.o cl_screen.o cgamevm.o cgame.o

#K6/athlon optimizations
CPUOPTIMIZATIONS=-march=k6
#athlon optimizations (only for gcc 2.96 and up)
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
OPTIMIZATIONS= -O6 -ffast-math -funroll-loops $(NOPROFILEOPTIMIZATIONS) -fexpensive-optimizations $(CPUOPTIMIZATIONS)
CFLAGS= -MD -Wall -Werror -I/usr/X11R6/include -I/usr/include/glide $(OPTIMIZATIONS) $(PROFILEOPTION)
#debug compile
#OPTIMIZATIONS=
#CFLAGS= -MD -Wall -Werror -I/usr/X11R6/include -ggdb $(OPTIMIZATIONS) $(PROFILEOPTION)

LDFLAGS= -L/usr/X11R6/lib -lm -lX11 -lXext -lXIE -lXxf86dga -lXxf86vm -lGL -ldl $(SOUNDLIB) $(PROFILEOPTION)

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
