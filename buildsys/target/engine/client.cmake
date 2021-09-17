add_executable(client ${OBJ_CL} ${OBJ_MENU} ${OBJ_SND_COMMON} ${OBJ_CD_COMMON} ${OBJ_VIDEO_CAPTURE})
target_link_libraries(client common)
set_target_properties(client PROPERTIES OUTPUT_NAME "${ENGINE_BUILD_NAME}")

if(WIN32)
	target_sources(client PRIVATE ${ENGINE_BUILD_WINRC})
endif()

option_dependency(SDL2 ENGINE_LINK_TO_SDL2 "shared" "${DP_LINK_OPTIONS_REQUIRED}")
option_dependency(PNG ENGINE_LINK_TO_LIBPNG "static" "${DP_LINK_OPTIONS_REQUIRED}")
option_dependency(zlib ENGINE_LINK_TO_ZLIB "shared" "${DP_LINK_OPTIONS_REQUIRED}")
option_dependency(d0_blind_id ENGINE_LINK_TO_CRYPTO "dlopen" "${DP_LINK_OPTIONS_STANDARD}")
option_dependency(d0_blind_id_rijndael ENGINE_LINK_TO_CRYPTO_RIJNDAEL "dlopen" "${DP_LINK_OPTIONS_STANDARD}")
option_dependency(ODE ENGINE_LINK_TO_LIBODE "none" "${DP_LINK_OPTIONS_STANDARD}")
option_dependency(XMP ENGINE_LINK_TO_XMP "dlopen" "${DP_LINK_OPTIONS_STANDARD}")
option_dependency(Vorbis ENGINE_LINK_TO_VORBIS "shared" "${DP_LINK_OPTIONS_STANDARD}")

find_package(SDL2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(CURL REQUIRED)

target_link_libraries(client SDL2::Main ZLIB::ZLIB PNG::PNG CURL::libcurl)

if(ENGINE_CONFIG_MENU)
	target_compile_definitions(client PRIVATE -DCONFIG_MENU)
endif()

if(ENGINE_LINK_TO_CRYPTO OR ENGINE_LINK_TO_CRYPTO_RIJNDAEL)
	find_package(Crypto)
	if(CRYPTO_INCLUDE_DIR AND d0_blind_id_LIBRARY AND d0_rijndael_LIBRARY)
		target_include_directories(client PRIVATE ${CRYPTO_INCLUDE_DIR})

		if(ENGINE_LINK_TO_CRYPTO)
			target_compile_definitions(client PRIVATE -DLINK_TO_CRYPTO)
			target_link_libraries(client ${d0_blind_id_LIBRARY})
		endif()

		if(ENGINE_LINK_TO_CRYPTO_RIJNDAEL)
			target_compile_definitions(client PRIVATE -DLINK_TO_CRYPTO_RIJNDAEL)
			target_link_libraries(client ${d0_rijndael_LIBRARY})
		endif()
	endif()
endif()

if(ENGINE_LINK_TO_LIBJPEG)
	find_package(JPEG REQUIRED)
	add_dependencies(client JPEG::JPEG)
endif()

if(ENGINE_LINK_TO_LIBVORBIS)
	find_package(Vorbis REQUIRED)
	target_include_directories(client PRIVATE ${VORBIS_INCLUDE_DIRS})
	target_compile_definitions(client PRIVATE -DLINK_TO_LIBVORBIS)
	target_link_libraries(client ${VORBIS_LIBRARIES})
endif()

if(ENGINE_LINK_TO_LIBODE)
	target_compile_definitions(client PRIVATE -DUSEODE)
endif()

if(ENGINE_CONFIG_VIDEO_CAPTURE)
	target_compile_definitions(client PRIVATE -DCONFIG_VIDEO_CAPTURE)
endif()
