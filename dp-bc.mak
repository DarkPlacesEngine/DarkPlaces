#
#        Borland C++ makefile for Darkplaces
#
#        Copyright (C) 1999,2000  Jukka Sorjonen.
#        Please see the file "AUTHORS" for a list of contributors
#
#        This program is free software; you can redistribute it and/or
#        modify it under the terms of the GNU General Public License
#        as published by the Free Software Foundation; either version 2
#        of the License, or (at your option) any later version.
#
#        This program is distributed in the hope that it will be useful,
#        but WITHOUT ANY WARRANTY; without even the implied warranty of
#        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#        See the GNU General Public License for more details.
#
#        You should have received a copy of the GNU General Public License
#        along with this program; if not, write to:
#
#                Free Software Foundation, Inc.
#                59 Temple Place - Suite 330
#                Boston, MA  02111-1307, USA
#
#

.AUTODEPEND

#
# Borland C++ tools
#
IMPLIB  = Implib
BCC32   = Bcc32
BCC32I  = Bcc32i
#TLINK32 = TLink32
TLINK32 = Ilink32
ILINK32 = Ilink32
TLIB    = TLib
BRC32   = Brc32
TASM32  = Tasm32

#
# Options
#

# Where quakeforge source is located
DPROOT = D:\PROJECT\QUAKE1\DARKPLACES

# Complier root directory
CROOT = D:\BORLAND\BCC55
# For 5.02
#CROOT = D:\BC5

# Where you want to place those .obj files
#OBJS = $(DPROOT)\TARGETS\GLQW_CLIENT
OBJS = $(DPROOT)
#\SOURCE

# ... and final exe
#EXE = $(DPROOT)\TARGETS
EXE = $(DPROOT)

# Path to your Direct-X libraries and includes
DIRECTXSDK=D:\project\dx7sdk

# end of system dependant stuffs

# for releases
DEBUGOPTS = -k- -vi
# for debugging
#DEBUGOPTS = -y -v
# -xp -xs -o

# no optimizations - for debugging
#OPT = -a -O-S -Od
# for basic optimizations for 386
#OPT = -3 -Oc -Oi -Ov -a4
# -a4 seems to break DP...
OPT = -3 -Oc -Oi -Ov
# for Pentium
#OPT = -5 -Oc -Oi -Ov -OS
# for Pentium Pro and higher
#OPT = -6 -Oc -Oi -Ov -OS
# Testing purposes
#OPT = -6 -Oc -Oi -Ov -Og -Oc -Ol -Ob -Oe -Om -Op

# disable warnings, for cleaner compile
WARNS = -w-
# for debugging
#WARNS = -w

COMPOPTS = $(DEBUGOPTS) $(OPT) $(WARNS) -R -WM -H-

# for normal releases
LINKOPTS = -w-dup -w-dpl -Tpe -aa -V4.0 -c -Gn -Gz -x -L$(LIBS)
# for debugging
#LINKOPTS = -w -v -w-dup -w-dpl -Tpe -aa -V4.0 -c -Gn -Gz -m -M -s -L$(LIBS)

# you shouldn't need to change anything below this line

SYSLIBS = $(CROOT)\LIB
MISCLIBS = $(DIRECTXSDK)\lib\borland
LIBS=$(SYSLIBS);$(MISCLIBS)

SYSINCLUDE = $(CROOT)\INCLUDE
QFINCLUDES = $(DPROOT);$(DIRECTXSDK)\include;
MISCINCLUDES = $(DIRECTXSDK)\include

INCLUDES = $(QFINCLUDES);$(SYSINCLUDE);$(MISCINCLUDES)

DEFINES=_WINDOWS=1;_WIN32=1;WINDOWS=1;WIN32=1


COMPOPTS = $(DEBUGOPTS) $(OPT) $(WARNS) -R -WM -H-
#-He- -f -ff -fp-

# for normal releases
LINKOPTS = -w-dup -w-dpl -Tpe -aa -V4.0 -c -Gn -Gz -x -L$(LIBS)
# for debugging
#LINKOPTS = -w -v -w-dup -w-dpl -Tpe -aa -V4.0 -c -Gn -Gz -m -M -s -L$(LIBS)
# -Gm

# MASM
ASSEMBLER = ML
ASMOUT = $(DPROOT)\source
ASMIN = /Fo$(OBJS)
#ASMOPTS=/nologo /c /Cp /Zi /H64
ASMOPTS=/nologo /c /Cp
#/Cx /Zi /Zd /H64
EXT1=.asm
EXT2=.obj

# TASM32
#ASSEMBLER = $(TASM32)
#ASMIN = $(DPROOT)\common
#ASMOUT = ,
#ASMOPTS = /ml
#EXT1=.obj
#EXT2=.asm

DEPEND = \
   $(OBJS)\r_crosshairs.obj\
   $(OBJS)\r_modules.obj\
   $(OBJS)\gl_textures.obj\
   $(OBJS)\gl_models.obj\
   $(OBJS)\buildnumber.obj\
   $(OBJS)\cpu_noasm.obj\
   $(OBJS)\cl_main.obj\
   $(DIRECTXSDK)\lib\borland\dxguid.lib\
   $(DPROOT)\opengl32.lib\
   $(OBJS)\net_wipx.obj\
   $(OBJS)\net_wins.obj\
   $(OBJS)\cd_win.obj\
   $(OBJS)\r_part.obj\
   $(OBJS)\world.obj\
   $(OBJS)\view.obj\
   $(OBJS)\vid_wgl.obj\
   $(OBJS)\vid_shared.obj\
   $(OBJS)\wad.obj\
   $(OBJS)\transform.obj\
   $(OBJS)\sys_win.obj\
   $(OBJS)\sv_user.obj\
   $(OBJS)\sv_phys.obj\
   $(OBJS)\sv_move.obj\
   $(OBJS)\sv_main.obj\
   $(OBJS)\snd_win.obj\
   $(OBJS)\snd_mix.obj\
   $(OBJS)\snd_mem.obj\
   $(OBJS)\snd_dma.obj\
   $(OBJS)\sbar.obj\
   $(OBJS)\zone.obj\
   $(OBJS)\model_sprite.obj\
   $(OBJS)\pr_exec.obj\
   $(OBJS)\pr_edict.obj\
   $(OBJS)\pr_cmds.obj\
   $(OBJS)\net_win.obj\
   $(OBJS)\net_vcr.obj\
   $(OBJS)\net_main.obj\
   $(OBJS)\net_loop.obj\
   $(OBJS)\net_dgrm.obj\
   $(OBJS)\r_light.obj\
   $(OBJS)\in_win.obj\
   $(OBJS)\model_brush.obj\
   $(OBJS)\model_alias.obj\
   $(OBJS)\menu.obj\
   $(OBJS)\mathlib.obj\
   $(OBJS)\keys.obj\
   $(OBJS)\model_shared.obj\
   $(OBJS)\gl_screen.obj\
   $(OBJS)\image.obj\
   $(OBJS)\host_cmd.obj\
   $(OBJS)\host.obj\
   $(OBJS)\hcompress.obj\
   $(OBJS)\gl_warp.obj\
   $(OBJS)\fractalnoise.obj\
   $(OBJS)\gl_rmisc.obj\
   $(OBJS)\gl_rmain.obj\
   $(OBJS)\gl_refrag.obj\
   $(OBJS)\gl_poly.obj\
   $(OBJS)\gl_draw.obj\
   $(OBJS)\gl_rsurf.obj\
   $(OBJS)\cl_tent.obj\
   $(OBJS)\crc.obj\
   $(OBJS)\console.obj\
   $(OBJS)\conproc.obj\
   $(OBJS)\common.obj\
   $(OBJS)\cmd.obj\
   $(OBJS)\cvar.obj\
   $(OBJS)\chase.obj\
   $(OBJS)\cl_input.obj\
   $(OBJS)\cl_demo.obj\
   $(OBJS)\cl_parse.obj

$(EXE)\darkplaces.exe : $(DEPEND)
  $(TLINK32) @&&|
 /v $(LINKOPTS) +
$(CROOT)\LIB\c0w32.obj+
$(OBJS)\r_crosshairs.obj+
$(OBJS)\r_modules.obj+
$(OBJS)\gl_textures.obj+
$(OBJS)\gl_models.obj+
$(OBJS)\buildnumber.obj+
$(OBJS)\cpu_noasm.obj+
$(OBJS)\cl_main.obj+
$(OBJS)\net_wipx.obj+
$(OBJS)\net_wins.obj+
$(OBJS)\cd_win.obj+
$(OBJS)\r_part.obj+
$(OBJS)\world.obj+
$(OBJS)\view.obj+
$(OBJS)\vid_wgl.obj+
$(OBJS)\vid_shared.obj+
$(OBJS)\wad.obj+
$(OBJS)\transform.obj+
$(OBJS)\sys_win.obj+
$(OBJS)\sv_user.obj+
$(OBJS)\sv_phys.obj+
$(OBJS)\sv_move.obj+
$(OBJS)\sv_main.obj+
$(OBJS)\snd_win.obj+
$(OBJS)\snd_mix.obj+
$(OBJS)\snd_mem.obj+
$(OBJS)\snd_dma.obj+
$(OBJS)\sbar.obj+
$(OBJS)\zone.obj+
$(OBJS)\model_sprite.obj+
$(OBJS)\pr_exec.obj+
$(OBJS)\pr_edict.obj+
$(OBJS)\pr_cmds.obj+
$(OBJS)\net_win.obj+
$(OBJS)\net_vcr.obj+
$(OBJS)\net_main.obj+
$(OBJS)\net_loop.obj+
$(OBJS)\net_dgrm.obj+
$(OBJS)\r_light.obj+
$(OBJS)\in_win.obj+
$(OBJS)\model_brush.obj+
$(OBJS)\model_alias.obj+
$(OBJS)\menu.obj+
$(OBJS)\mathlib.obj+
$(OBJS)\keys.obj+
$(OBJS)\model_shared.obj+
$(OBJS)\gl_screen.obj+
$(OBJS)\image.obj+
$(OBJS)\host_cmd.obj+
$(OBJS)\host.obj+
$(OBJS)\hcompress.obj+
$(OBJS)\gl_warp.obj+
$(OBJS)\fractalnoise.obj+
$(OBJS)\gl_rmisc.obj+
$(OBJS)\gl_rmain.obj+
$(OBJS)\gl_refrag.obj+
$(OBJS)\gl_poly.obj+
$(OBJS)\gl_draw.obj+
$(OBJS)\gl_rsurf.obj+
$(OBJS)\cl_tent.obj+
$(OBJS)\crc.obj+
$(OBJS)\console.obj+
$(OBJS)\conproc.obj+
$(OBJS)\common.obj+
$(OBJS)\cmd.obj+
$(OBJS)\cvar.obj+
$(OBJS)\chase.obj+
$(OBJS)\cl_input.obj+
$(OBJS)\cl_demo.obj+
$(OBJS)\cl_parse.obj
$<,$*
$(DIRECTXSDK)\lib\borland\dxguid.lib+
$(DPROOT)\opengl32.lib+
($CROOT)\LIB\import32.lib+
($CROOT)\LIB\cw32.lib

|
$(OBJS)\r_crosshairs.obj :  $(DPROOT)\r_crosshairs.c
  $(BCC32) -P- -c @&&|
  $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\r_crosshairs.c
|

$(OBJS)\r_modules.obj :  $(DPROOT)\r_modules.c
  $(BCC32) -P- -c @&&|
  $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\r_modules.c
|

$(OBJS)\gl_textures.obj :  $(DPROOT)\gl_textures.c
  $(BCC32) -P- -c @&&|
  $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\gl_textures.c
|

$(OBJS)\gl_models.obj :  $(DPROOT)\gl_models.c
  $(BCC32) -P- -c @&&|
  $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\gl_models.c
|

$(OBJS)\buildnumber.obj :  $(DPROOT)\buildnumber.c
  $(BCC32) -P- -c @&&|
  $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\buildnumber.c
|

$(OBJS)\cpu_noasm.obj :  $(DPROOT)\cpu_noasm.c
  $(BCC32) -P- -c @&&|
  $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\cpu_noasm.c

|
$(OBJS)\cl_main.obj :  $(DPROOT)\cl_main.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\cl_main.c
|

$(OBJS)\net_wipx.obj :  $(DPROOT)\net_wipx.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\net_wipx.c
|

$(OBJS)\net_wins.obj :  $(DPROOT)\net_wins.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\net_wins.c
|

$(OBJS)\cd_win.obj :  $(DPROOT)\cd_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\cd_win.c
|

$(OBJS)\r_part.obj :  $(DPROOT)\r_part.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\r_part.c
|

$(OBJS)\world.obj :  $(DPROOT)\world.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\world.c
|

$(OBJS)\view.obj :  $(DPROOT)\view.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\view.c
|

$(OBJS)\vid_wgl.obj :  $(DPROOT)\vid_wgl.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\vid_wgl.c
|

$(OBJS)\vid_shared.obj :  $(DPROOT)\vid_shared.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\vid_shared.c
|

$(OBJS)\wad.obj :  $(DPROOT)\wad.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\wad.c
|

$(OBJS)\transform.obj :  $(DPROOT)\transform.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\transform.c
|

$(OBJS)\sys_win.obj :  $(DPROOT)\sys_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\sys_win.c
|

$(OBJS)\sv_user.obj :  $(DPROOT)\sv_user.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\sv_user.c
|

$(OBJS)\sv_phys.obj :  $(DPROOT)\sv_phys.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\sv_phys.c
|

$(OBJS)\sv_move.obj :  $(DPROOT)\sv_move.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\sv_move.c
|

$(OBJS)\sv_main.obj :  $(DPROOT)\sv_main.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\sv_main.c
|

$(OBJS)\snd_win.obj :  $(DPROOT)\snd_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\snd_win.c
|

$(OBJS)\snd_mix.obj :  $(DPROOT)\snd_mix.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\snd_mix.c
|

$(OBJS)\snd_mem.obj :  $(DPROOT)\snd_mem.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\snd_mem.c
|

$(OBJS)\snd_dma.obj :  $(DPROOT)\snd_dma.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\snd_dma.c
|

$(OBJS)\sbar.obj :  $(DPROOT)\sbar.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\sbar.c
|

$(OBJS)\zone.obj :  $(DPROOT)\zone.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\zone.c
|

$(OBJS)\model_sprite.obj :  $(DPROOT)\model_sprite.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\model_sprite.c
|

$(OBJS)\pr_exec.obj :  $(DPROOT)\pr_exec.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\pr_exec.c
|

$(OBJS)\pr_edict.obj :  $(DPROOT)\pr_edict.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\pr_edict.c
|

$(OBJS)\pr_cmds.obj :  $(DPROOT)\pr_cmds.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\pr_cmds.c
|

$(OBJS)\net_win.obj :  $(DPROOT)\net_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\net_win.c
|

$(OBJS)\net_vcr.obj :  $(DPROOT)\net_vcr.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\net_vcr.c
|

$(OBJS)\net_main.obj :  $(DPROOT)\net_main.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\net_main.c
|

$(OBJS)\net_loop.obj :  $(DPROOT)\net_loop.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\net_loop.c
|

$(OBJS)\net_dgrm.obj :  $(DPROOT)\net_dgrm.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\net_dgrm.c
|

$(OBJS)\r_light.obj :  $(DPROOT)\r_light.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\r_light.c
|

$(OBJS)\in_win.obj :  $(DPROOT)\in_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\in_win.c
|

$(OBJS)\model_brush.obj :  $(DPROOT)\model_brush.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\model_brush.c
|

$(OBJS)\model_alias.obj :  $(DPROOT)\model_alias.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\model_alias.c
|

$(OBJS)\menu.obj :  $(DPROOT)\menu.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\menu.c
|

$(OBJS)\mathlib.obj :  $(DPROOT)\mathlib.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\mathlib.c
|

$(OBJS)\keys.obj :  $(DPROOT)\keys.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\keys.c
|

$(OBJS)\model_shared.obj :  $(DPROOT)\model_shared.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\model_shared.c
|

$(OBJS)\gl_screen.obj :  $(DPROOT)\gl_screen.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\gl_screen.c
|

$(OBJS)\image.obj :  $(DPROOT)\image.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\image.c
|

$(OBJS)\host_cmd.obj :  $(DPROOT)\host_cmd.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\host_cmd.c
|

$(OBJS)\host.obj :  $(DPROOT)\host.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\host.c
|

$(OBJS)\hcompress.obj :  $(DPROOT)\hcompress.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\hcompress.c
|

$(OBJS)\gl_warp.obj :  $(DPROOT)\gl_warp.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\gl_warp.c
|

$(OBJS)\fractalnoise.obj :  $(DPROOT)\fractalnoise.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\fractalnoise.c
|

$(OBJS)\gl_rmisc.obj :  $(DPROOT)\gl_rmisc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\gl_rmisc.c
|

$(OBJS)\gl_rmain.obj :  $(DPROOT)\gl_rmain.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\gl_rmain.c
|

$(OBJS)\gl_refrag.obj :  $(DPROOT)\gl_refrag.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\gl_refrag.c
|

$(OBJS)\gl_poly.obj :  $(DPROOT)\gl_poly.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\gl_poly.c
|

$(OBJS)\gl_draw.obj :  $(DPROOT)\gl_draw.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\gl_draw.c
|

$(OBJS)\gl_rsurf.obj :  $(DPROOT)\gl_rsurf.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\gl_rsurf.c
|

$(OBJS)\cl_tent.obj :  $(DPROOT)\cl_tent.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\cl_tent.c
|

$(OBJS)\crc.obj :  crc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\crc.c
|

$(OBJS)\console.obj :  $(DPROOT)\console.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\console.c
|

$(OBJS)\conproc.obj :  $(DPROOT)\conproc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\conproc.c
|

$(OBJS)\common.obj :  $(DPROOT)\common.c
  $(BCC32) -P- -c @&&|                       
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\common.c
|

$(OBJS)\cmd.obj :  $(DPROOT)\cmd.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\cmd.c
|

$(OBJS)\cvar.obj :  $(DPROOT)\cvar.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\cvar.c
|

$(OBJS)\chase.obj :  $(DPROOT)\chase.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\chase.c
|

$(OBJS)\cl_input.obj :  $(DPROOT)\cl_input.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\cl_input.c
|

$(OBJS)\cl_demo.obj :  $(DPROOT)\cl_demo.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\cl_demo.c
|

$(OBJS)\cl_parse.obj :  $(DPROOT)\cl_parse.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(DPROOT)\cl_parse.c
|



