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

OBJECTS= buildnumber.o cd_linux.o chase.o cl_demo.o cl_input.o cl_main.o cl_parse.o cl_tent.o cmd.o common.o console.o crc.o cvar.o fractalnoise.o gl_draw.o r_sky.o gl_rmain.o gl_rsurf.o gl_screen.o host.o host_cmd.o image.o keys.o mathlib.o menu.o model_alias.o model_brush.o model_shared.o model_sprite.o net_bsd.o net_udp.o net_dgrm.o net_loop.o net_main.o pr_cmds.o pr_edict.o pr_exec.o r_light.o r_particles.o r_explosion.o sbar.o snd_dma.o snd_mem.o snd_mix.o $(SND) sv_main.o sv_move.o sv_phys.o sv_user.o sv_light.o sys_linux.o transform.o view.o wad.o world.o zone.o vid_shared.o palette.o r_crosshairs.o gl_textures.o gl_models.o r_sprites.o r_modules.o r_explosion.o r_lerpanim.o r_decals.o protocol.o quakeio.o r_clip.o ui.o portals.o sys_shared.o cl_light.o gl_backend.o cl_particles.o cl_decals.o cl_screen.o

#K6/athlon optimizations
CPUOPTIMIZATIONS=-march=k6
#686 optimizations
#CPUOPTIMIZATIONS=-march=i686

#use this line for profiling
PROFILEOPTION=-pg -g
NOPROFILEOPTIMIZATIONS=
#use this line for no profiling
#PROFILEOPTION=
#NOPROFILEOPTIMIZATIONS=-fomit-frame-pointer

#note:
#the -Werror can be removed to compile even if there are warnings,
#this is used to ensure that all released versions are free of warnings.

#normal compile
OPTIMIZATIONS= -O6 -ffast-math -funroll-loops $(NOPROFILEOPTIMIZATIONS) -fexpensive-optimizations $(CPUOPTIMIZATIONS)
CFLAGS= -MD -Wall -Werror -I/usr/X11R6/include -I/usr/include/glide $(OPTIMIZATIONS) $(PROFILEOPTION)
#debug compile
#OPTIMIZATIONS= -O -g
#CFLAGS= -MD -Wall -Werror -I/usr/X11R6/include -ggdb $(OPTIMIZATIONS) $(PROFILEOPTION)

LDFLAGS= -L/usr/X11R6/lib -lm -lX11 -lXext -lXIE -lXxf86dga -lXxf86vm -lGL -ldl $(SOUNDLIB) $(PROFILEOPTION)

#most people can't build the -3dfx version (-3dfx version needs some updates for new mesa)
all: buildnum darkplaces-glx
#all: darkplaces-glx darkplaces-3dfx

buildnum:
	make -C buildnum
	buildnum/buildnum buildnumber.c

.c.o:
	gcc $(CFLAGS) -c $*.c

darkplaces-glx: $(OBJECTS) vid_glx.o
	gcc -o $@ $^ $(LDFLAGS)

darkplaces-3dfx: $(OBJECTS) in_svgalib.o vid_3dfxsvga.o
	gcc -o $@ $^ $(LDFLAGS)


clean:
	-make -C buildnum clean
	-rm -f darkplaces-glx darkplaces-3dfx
	-rm -f vid_glx.o in_svgalib.o vid_3dfxsvga.o $(OBJECTS) *.d

.PHONY: clean buildnum

-include *.d
