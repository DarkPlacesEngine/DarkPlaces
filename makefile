SND=snd_alsa_0_5.o
OBJECTS= buildnumber.o cd_linux.o chase.o cl_demo.o cl_input.o cl_main.o cl_parse.o cl_tent.o cmd.o common.o console.o cpu_noasm.o crc.o cvar.o fractalnoise.o gl_draw.o gl_poly.o gl_refrag.o gl_rmain.o gl_rmisc.o gl_rsurf.o gl_screen.o gl_warp.o host.o host_cmd.o image.o keys.o mathlib.o menu.o model_alias.o model_brush.o model_shared.o model_sprite.o net_bsd.o net_udp.o net_dgrm.o net_loop.o net_main.o net_vcr.o pr_cmds.o pr_edict.o pr_exec.o r_light.o r_part.o sbar.o snd_dma.o snd_mem.o snd_mix.o $(SND) sv_main.o sv_move.o sv_phys.o sv_user.o sys_linux.o transform.o view.o wad.o world.o zone.o vid_shared.o palette.o r_crosshairs.o gl_textures.o gl_models.o r_sprites.o r_modules.o

OPTIMIZATIONS= -O6 -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations

CFLAGS= -Wall -Werror -I/usr/X11R6/include -I/usr/include/glide $(OPTIMIZATIONS)
#CFLAGS= -Wall -Werror -I/usr/X11/include -ggdb $(OPTIMIZATIONS)
#LDFLAGS= -L/usr/X11R6/lib -lm -lX11 -lXext -lXIE -lXxf86dga -lXxf86vm -lGL -ldl
LDFLAGS= -L/usr/X11R6/lib -lm -lX11 -lXext -lXIE -lXxf86dga -lXxf86vm -lGL -ldl -lasound

all: darkplaces-glx darkplaces-3dfx

.c.o:
	gcc $(CFLAGS) -c $*.c

darkplaces-glx: $(OBJECTS) vid_glx.o
	gcc -o $@ $^ $(LDFLAGS)

darkplaces-3dfx: $(OBJECTS) in_svgalib.o vid_3dfxsvga.o
	gcc -o $@ $^ $(LDFLAGS)


clean:
	rm -f darkplaces-glx darkplaces-3dfx $(OBJECTS)
