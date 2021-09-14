cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(darkplaces C)

include(buildsys/build.cmake) # Utility and helper functions and macros
include(buildsys/dependencies.cmake)
include(game/default/dpconfig.cmake) # Initial build options

unset(CMAKE_C_FLAGS_MINSIZEREL CACHE)
unset(CMAKE_C_FLAGS_RELWITHDEBINFO CACHE)

unset(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL CACHE)
unset(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO CACHE)

if(NOT FIRST_CONFIG)
	unset(CMAKE_C_FLAGS CACHE)
	unset(CMAKE_C_FLAGS_RELEASE CACHE)
	unset(CMAKE_C_FLAGS_DEBUG CACHE)

	unset(CMAKE_EXE_LINKER_FLAGS CACHE)
	unset(CMAKE_EXE_LINKER_FLAGS_RELEASE CACHE)
	unset(CMAKE_EXE_LINKER_FLAGS_DEBUG CACHE)

	set(CMAKE_C_FLAGS "" CACHE STRING "")
	set(CMAKE_C_FLAGS_RELEASE "" CACHE STRING "")
	set(CMAKE_C_FLAGS_DEBUG "" CACHE STRING "")
	set(CMAKE_C_FLAGS_PROFILE "" CACHE STRING "")
	set(CMAKE_C_FLAGS_PROFILERELEASE "" CACHE STRING "")

	set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "" CACHE STRING "")
	set(CMAKE_EXE_LINKER_FLAGS_DEBUG "" CACHE STRING "")
	set(CMAKE_EXE_LINKER_FLAGS_PROFILE "" CACHE STRING "")
	set(CMAKE_EXE_LINKER_FLAGS_PROFILERELEASE "" CACHE STRING "")

	set(FIRST_CONFIG TRUE CACHE INTERNAL "")
endif()

if(NOT MSVC)
	set(CMAKE_CONFIGURATION_TYPES "Release;Debug;Profile;ProfileRelease" CACHE STRING "" FORCE)
else()
	set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING "" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "${CMAKE_CONFIGURATION_TYPES}")
mark_as_advanced(CMAKE_CONFIGURATION_TYPES)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/buildsys/module)
set(GAME_PROJECT "" CACHE STRING "Name of game project")
set(GAME_PROJECT_DIR "" CACHE PATH "Location of game project")

set(OBJ_COMMON
	bih.c
	crypto.c
	cd_shared.c
	cl_cmd.c
	cl_collision.c
	cl_demo.c
	cl_ents.c
	cl_ents4.c
	cl_ents5.c
	cl_ents_nq.c
	cl_ents_qw.c
	cl_input.c
	cl_main.c
	cl_parse.c
	cl_particles.c
	cl_screen.c
	cl_video.c
	cl_video_libavw.c
	clvm_cmds.c
	cmd.c
	collision.c
	com_crc16.c
	com_ents.c
	com_ents4.c
	com_game.c
	com_infostring.c
	com_msg.c
	common.c
	console.c
	csprogs.c
	curves.c
	cvar.c
	dpvsimpledecode.c
	filematch.c
	fractalnoise.c
	fs.c
	ft2.c
	utf8lib.c
	gl_backend.c
	gl_draw.c
	gl_rmain.c
	gl_rsurf.c
	gl_textures.c
	hmac.c
	host.c
	image.c
	image_png.c
	jpeg.c
	keys.c
	lhnet.c
	libcurl.c
	mathlib.c
	matrixlib.c
	mdfour.c
	meshqueue.c
	mod_skeletal_animatevertices_sse.c
	mod_skeletal_animatevertices_generic.c
	model_alias.c
	model_brush.c
	model_shared.c
	model_sprite.c
	netconn.c
	palette.c
	polygon.c
	portals.c
	protocol.c
	prvm_cmds.c
	prvm_edict.c
	prvm_exec.c
	r_explosion.c
	r_lightning.c
	r_modules.c
	r_shadow.c
	r_sky.c
	r_sprites.c
	r_stats.c
	sbar.c
	sv_ccmds.c
	sv_demo.c
	sv_ents.c
	sv_ents4.c
	sv_ents5.c
	sv_ents_csqc.c
	sv_ents_nq.c
	sv_main.c
	sv_move.c
	sv_phys.c
	sv_save.c
	sv_send.c
	sv_user.c
	svbsp.c
	svvm_cmds.c
	sys_shared.c
	taskqueue.c
	vid_shared.c
	view.c
	wad.c
	world.c
	zone.c
)
set(OBJ_HEADERS
	bih.h
	bspfile.h
	cap_avi.h
	cap_ogg.h
	cap.h
	cdaudio.h
	cl_collision.h
	cl_parse.h
	cl_particles.h
	cl_screen.h
	cl_video_libavw.h
	cl_video.h
	client.h
	clprogdefs.h
	clvm_cmds.h
	cmd.h
	collision.h
	com_game.h
	com_infostring.h
	com_list.h
	common.h
	console.h
	crypto.h
	csprogs.h
	curves.h
	cvar.h
	darkplaces.h
	dpvsimpledecode.h
	draw.h
	filematch.h
	fs.h
	ft2_defs.h
	ft2_fontdefs.h
	ft2.h
	gl_backend.h
	glquake.h
	hmac.h
	host.h
	image_png.h
	image.h
	input.h
	jpeg.h
	keys.h
	lhfont.h
	lhnet.h
	libcurl.h
	mathlib.h
	matrixlib.h
	mdfour.h
	menu.h
	meshqueue.h
	mod_skeletal_animatevertices_generic.h
	mod_skeletal_animatevertices_sse.h
	model_alias.h
	model_brush.h
	model_dpmodel.h
	model_hlmdl.h
	model_iqm.h
	model_psk.h
	model_q1bsp.h
	model_q2bsp.h
	model_q3bsp.h
	model_shared.h
	model_sprite.h
	model_vbsp.h
	model_zymotic.h
	modelgen.h
	mprogdefs.h
	netconn.h
	palette.h
	pmove.h
	polygon.h
	portals.h
	pr_comp.h
	progdefs.h
	progs.h
	progsvm.h
	protocol.h
	prvm_cmds.h
	prvm_execprogram.h
	prvm_offsets.h
	qdefs.h
	qstats.h
	qtypes.h
	quakedef.h
	r_modules.h
	r_qshader.h
	r_shadow.h
	r_stats.h
	r_textures.h
	render.h
	resource.h
	sbar.h
	screen.h
	server.h
	shader_glsl.h
	snd_main.h
	snd_ogg.h
	snd_wav.h
	snd_xmp.h
	sound.h
	spritegn.h
	sv_demo.h
	svbsp.h
	sys.h
	taskqueue.h
	thread.h
	timing.h
	utf8lib.h
	vid.h
	view.h
	vpk.h
	wad.h
	world.h
	zone.h
)
set(OBJ_SND_COMMON snd_main.c snd_mem.c snd_mix.c snd_ogg.c snd_wav.c)
set(OBJ_VIDEO_CAPTURE cap_avi.c cap_ogg.c)
set(OBJ_MENU menu.c mvm_cmds.c)
set(OBJ_SV builddate.c sys_unix.c vid_null.c snd_null.c thread_null.c)
set(OBJ_CL builddate.c snd_sdl.c sys_sdl.c vid_sdl.c thread_sdl.c)
set(OBJ_RC ${ENGINE_BUILD_WINRC})

# Make the headers show up in IDEs
add_custom_target(headers EXCLUDE_FROM_ALL SOURCES ${OBJ_HEADERS})

dp_build("${GAME_PROJECT}" "${GAME_PROJECT_DIR}")
