OBJECTS = cd_linux.o chase.o cl_demo.o cl_input.o cl_main.o cl_parse.o cl_tent.o cmd.o common.o console.o cpu_noasm.o crc.o cvar.o fractalnoise.o gl_draw.o gl_poly.o gl_refrag.o gl_rmain.o gl_rmisc.o gl_rsurf.o gl_screen.o gl_warp.o host.o host_cmd.o image.o keys.o mathlib.o menu.o model_alias.o model_brush.o model_shared.o model_sprite.o net_bsd.o net_udp.o net_dgrm.o net_loop.o net_main.o net_vcr.o pr_cmds.o pr_edict.o pr_exec.o r_light.o r_part.o sbar.o snd_dma.o snd_mem.o snd_mix.o snd_linux.o sv_main.o sv_move.o sv_phys.o sv_user.o sys_linux.o transform.o view.o wad.o world.o zone.o vid_shared.o

OPTIMIZATIONS = -O6 -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations
CFLAGS = -Wall -Werror -I/usr/X11R6/include -I/usr/include/glide $(OPTIMIZATIONS)
#CFLAGS = -Wall -Werror -I/usr/X11/include -ggdb $(OPTIMIZATIONS)
LIBS = -L/usr/X11R6/lib -lc -lm -lXext -lXxf86dga -lXxf86vm -lGL -ldl

all: darkplaces-glx darkplaces-3dfx

.c.o:
	gcc $(CFLAGS) -c $*.c

darkplaces-glx: $(OBJECTS) vid_glx.o
	gcc -o $@ $^ $(LIBS)

darkplaces-3dfx: $(OBJECTS) in_svgalib.o vid_3dfxsvga.o
	gcc -o $@ $^ $(LIBS)


clean:
	rm -f darkplaces-glx $(OBJECTS)
