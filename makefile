TARGET = darkplacesglx

OBJECTS = cd_linux.o chase.o cl_demo.o cl_input.o cl_main.o cl_parse.o cl_tent.o cmd.o common.o console.o cpu_noasm.o crc.o cvar.o fractalnoise.o gl_draw.o gl_poly.o gl_refrag.o gl_rmain.o gl_rmisc.o gl_rsurf.o gl_screen.o gl_warp.o host.o host_cmd.o image.o keys.o mathlib.o menu.o model_alias.o model_brush.o model_shared.o model_sprite.o net_bsd.o net_udp.o net_dgrm.o net_loop.o net_main.o net_vcr.o pr_cmds.o pr_edict.o pr_exec.o r_light.o r_part.o sbar.o snd_dma.o snd_mem.o snd_mix.o snd_linux.o sv_main.o sv_move.o sv_phys.o sv_user.o sys_linux.o transform.o vid_shared.o vid_glx.o view.o wad.o world.o zone.o

OPTIMIZATIONS = -O6 -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations
CFLAGS = -Wall -Werror -I/usr/X11/include $(OPTIMIZATIONS)
#CFLAGS = -Wall -Werror -I/usr/X11/include -ggdb $(OPTIMIZATIONS)
LIBS = -L/usr/X11/lib -lc -lm -lXext -lXxf86dga -lXxf86vm -lGL

#quick:
#	gcc -o $(TARGET) $(CFLAGS) cd_linux.c chase.c cl_demo.c cl_input.c cl_main.c cl_parse.c cl_tent.c cmd.c common.c console.c cpu_noasm.c crc.c cvar.c fractalnoise.c gl_draw.c gl_poly.c gl_refrag.c gl_rmain.c gl_rmisc.c gl_rsurf.c gl_screen.c gl_warp.c host.c host_cmd.c image.c keys.c mathlib.c menu.c model_alias.c model_brush.c model_shared.c model_sprite.c net_bsd.c net_udp.c net_dgrm.c net_loop.c net_main.c net_vcr.c pr_cmds.c pr_edict.c pr_exec.c r_light.c r_part.c sbar.c snd_dma.c snd_mem.c snd_mix.c snd_linux.c sv_main.c sv_move.c sv_phys.c sv_user.c sys_linux.c transform.c vid_shared.c vid_glx.c view.c wad.c world.c zone.c $(LIBS)
.c.o:
	gcc $(CFLAGS) -c $*.c

$(TARGET) : $(OBJECTS)
	gcc $(LIBS) -o $(TARGET) $(OBJECTS) $(LIBS)

