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
r_crosshairs.obj :  r_crosshairs.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_darkplacesdexe) $(CompInheritOptsAt_darkplacesdexe) -o$@ r_crosshairs.c
|

r_modules.obj :  r_modules.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_darkplacesdexe) $(CompInheritOptsAt_darkplacesdexe) -o$@ r_modules.c
|

gl_textures.obj :  gl_textures.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_darkplacesdexe) $(CompInheritOptsAt_darkplacesdexe) -o$@ gl_textures.c
|

gl_models.obj :  gl_models.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_darkplacesdexe) $(CompInheritOptsAt_darkplacesdexe) -o$@ gl_models.c
|

buildnumber.obj :  buildnumber.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_darkplacesdexe) $(CompInheritOptsAt_darkplacesdexe) -o$@ buildnumber.c
|

cpu_noasm.obj :  cpu_noasm.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_darkplacesdexe) $(CompInheritOptsAt_darkplacesdexe) -o$@ cpu_noasm.c

|
$(OBJS)\cl_main.obj :  cl_main.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ cl_main.c
|

$(OBJS)\net_wipx.obj :  net_wipx.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ net_wipx.c
|

$(OBJS)\net_wins.obj :  net_wins.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ net_wins.c
|

$(OBJS)\cd_win.obj :  cd_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ cd_win.c
|

$(OBJS)\r_part.obj :  r_part.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ r_part.c
|

$(OBJS)\world.obj :  world.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ world.c
|

$(OBJS)\view.obj :  view.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ view.c
|

$(OBJS)\vid_wgl.obj :  vid_wgl.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ vid_wgl.c
|

$(OBJS)\vid_shared.obj :  vid_shared.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ vid_shared.c
|

$(OBJS)\wad.obj :  wad.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ wad.c
|

$(OBJS)\transform.obj :  transform.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ transform.c
|

$(OBJS)\sys_win.obj :  sys_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ sys_win.c
|

$(OBJS)\sv_user.obj :  sv_user.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ sv_user.c
|

$(OBJS)\sv_phys.obj :  sv_phys.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ sv_phys.c
|

$(OBJS)\sv_move.obj :  sv_move.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ sv_move.c
|

$(OBJS)\sv_main.obj :  sv_main.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ sv_main.c
|

$(OBJS)\snd_win.obj :  snd_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ snd_win.c
|

$(OBJS)\snd_mix.obj :  snd_mix.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ snd_mix.c
|

$(OBJS)\snd_mem.obj :  snd_mem.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ snd_mem.c
|

$(OBJS)\snd_dma.obj :  snd_dma.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ snd_dma.c
|

$(OBJS)\sbar.obj :  sbar.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ sbar.c
|

$(OBJS)\zone.obj :  zone.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ zone.c
|

$(OBJS)\model_sprite.obj :  model_sprite.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ model_sprite.c
|

$(OBJS)\pr_exec.obj :  pr_exec.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ pr_exec.c
|

$(OBJS)\pr_edict.obj :  pr_edict.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ pr_edict.c
|

$(OBJS)\pr_cmds.obj :  pr_cmds.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ pr_cmds.c
|

$(OBJS)\net_win.obj :  net_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ net_win.c
|

$(OBJS)\net_vcr.obj :  net_vcr.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ net_vcr.c
|

$(OBJS)\net_main.obj :  net_main.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ net_main.c
|

$(OBJS)\net_loop.obj :  net_loop.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ net_loop.c
|

$(OBJS)\net_dgrm.obj :  net_dgrm.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ net_dgrm.c
|

$(OBJS)\r_light.obj :  r_light.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ r_light.c
|

$(OBJS)\in_win.obj :  in_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ in_win.c
|

$(OBJS)\model_brush.obj :  model_brush.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ model_brush.c
|

$(OBJS)\model_alias.obj :  model_alias.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ model_alias.c
|

$(OBJS)\menu.obj :  menu.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ menu.c
|

$(OBJS)\mathlib.obj :  mathlib.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ mathlib.c
|

$(OBJS)\keys.obj :  keys.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ keys.c
|

$(OBJS)\model_shared.obj :  model_shared.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ model_shared.c
|

$(OBJS)\gl_screen.obj :  gl_screen.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ gl_screen.c
|

$(OBJS)\image.obj :  image.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ image.c
|

$(OBJS)\host_cmd.obj :  host_cmd.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ host_cmd.c
|

$(OBJS)\host.obj :  host.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ host.c
|

$(OBJS)\hcompress.obj :  hcompress.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ hcompress.c
|

$(OBJS)\gl_warp.obj :  gl_warp.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ gl_warp.c
|

$(OBJS)\fractalnoise.obj :  fractalnoise.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ fractalnoise.c
|

$(OBJS)\gl_rmisc.obj :  gl_rmisc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ gl_rmisc.c
|

$(OBJS)\gl_rmain.obj :  gl_rmain.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ gl_rmain.c
|

$(OBJS)\gl_refrag.obj :  gl_refrag.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ gl_refrag.c
|

$(OBJS)\gl_poly.obj :  gl_poly.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ gl_poly.c
|

$(OBJS)\gl_draw.obj :  gl_draw.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ gl_draw.c
|

$(OBJS)\gl_rsurf.obj :  gl_rsurf.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ gl_rsurf.c
|

$(OBJS)\cl_tent.obj :  cl_tent.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ cl_tent.c
|

$(OBJS)\crc.obj :  crc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ crc.c
|

$(OBJS)\console.obj :  console.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ console.c
|

$(OBJS)\conproc.obj :  conproc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ conproc.c
|

$(OBJS)\common.obj :  common.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ common.c
|

$(OBJS)\cmd.obj :  cmd.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ cmd.c
|

$(OBJS)\cvar.obj :  cvar.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ cvar.c
|

$(OBJS)\chase.obj :  chase.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ chase.c
|

$(OBJS)\cl_input.obj :  cl_input.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ cl_input.c
|

$(OBJS)\cl_demo.obj :  cl_demo.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ cl_demo.c
|

$(OBJS)\cl_parse.obj :  cl_parse.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ cl_parse.c
|



