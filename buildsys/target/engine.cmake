add_library(common INTERFACE)
target_compile_definitions(common INTERFACE -D_FILE_OFFSET_BITS=64 -D__KERNEL_STRICT_NAMES)

set(ENV{TZ} "UTC")

# Build a version string for the engine.
execute_process(
	COMMAND git rev-parse --short HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE revision
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND "git show -s --format=%ad --date='format-local:%a %b %d %Y %H:%I:%S UTC'"
	OUTPUT_VARIABLE timestamp
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(DP_BUILD_REVISION "${timestamp} - ${revision}")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x|i[36])86|AMD64")
	option(ENGINE_CONFIG_SSE "Build with SSE support (x86 and x86_64 only)" ON)
else()
	set(ENGINE_CONFIG_SSE OFF)
endif()

if(NOT MSVC)
	# Flags for gcc, clang, tcc...

	# NOTE: *never* *ever* use the -ffast-math or -funsafe-math-optimizations flag
	# Also, since gcc 5, -ffinite-math-only makes NaN and zero compare equal inside engine code but not inside QC, which causes error spam for seemingly valid QC code like if (x != 0) return 1 / x;

	target_compile_options(common INTERFACE
		-Wall -Winline -Werror=c++-compat -Wwrite-strings -Wshadow -Wold-style-definition -Wstrict-prototypes -Wsign-compare -Wdeclaration-after-statement -Wmissing-prototypes
		$<$<CONFIG:RELEASE>:-O3 -fno-strict-aliasing -fno-math-errno -fno-rounding-math -fno-trapping-math>
		$<$<CONFIG:DEBUG>:-ggdb>
		$<$<CONFIG:PROFILE>:-g -pg -ggdb -fprofile-arcs>
		$<$<CONFIG:PROFILERELEASE>:-fbranch-probabilities>
	)

	if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
		target_compile_options(common INTERFACE $<$<CONFIG:RELEASE>:-fno-signaling-nans>)
	endif()

	target_link_libraries(common INTERFACE -lm)

	if(CMAKE_C_COMPILER_ID STREQUAL "TinyCC")
		target_compile_definitions(common INTERFACE -DSDL_DISABLE_IMMINTRIN_H)
	endif()

	if(ENGINE_CONFIG_PEDANTIC)
		target_compile_options(common INTERFACE -std=c11 -pedantic)
		target_compile_definitions(common INTERFACE -D_POSIX_C_SOURCE=200809L -DCONFIG_PEDANTIC)
	endif()

	if(ENGINE_CONFIG_SSE)
		target_compile_options(common INTERFACE -msse)
	endif()

	if(WIN32) # Windows MinGW
		if(CMAKE_SYSTEM_PROCESSOR MATCHES "i[36]86")
			target_link_options(common INTERFACE --large-address-aware)
		endif()
		target_link_libraries(common INTERFACE -lwinmm -limm32 -lversion -lwsock32 -lws2_32)
	elseif(APPLE) # MacOS, iOS
		target_link_libraries(common INTERFACE -ldl -framework IOKit -framework CoreFoundation)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS") # SunOS, Solaris, OpenSolaris, illumos (OpenIndiana and friends)
		target_link_libraries(common INTERFACE -lrt -ldl -lsocket -lnsl)
	else() # Linux, the BSDs, and probably everything else
		target_link_libraries(common INTERFACE -lrt -ldl)
	endif()
else()
	# Flags for Visual Studio
	target_compile_options(common INTERFACE
		/W4 /Wd4706;4127;4100;4055;4054;4244;4305;4702;4201 /MP
		$<$<CONFIG:RELEASE>:/MD /Gy /O2 /Oi /Zi>
		$<$<CONFIG:DEBUG>:/Od /RTC1 /MDd /Gm>
	)

	target_link_options(common INTERFACE
		/SUBSYSTEM:WINDOWS
		$<$<CONFIG:RELEASE>:/OPT:REF /OPT:ICF>
		$<$<CONFIG:DEBUG>:/DEBUG /NODEFAULTLIB:msvcrt.lib>
	)

	if(CMAKE_SYSTEM_PROCESOR STREQUAL "AMD64")
		target_compile_options(client INTERFACE /Zi)
	else()
		target_compile_options(client INTERFACE /ZI)
		target_link_options(common INTERFACE /LARGEADDRESSAWARE)
	endif()
endif()

if(ENGINE_VERSION)
	target_compile_definitions(common INTERFACE -DSVNREVISION=${ENGINE_VERSION})
endif()

if(CMAKE_BUILD_TYPE)
	target_compile_definitions(common INTERFACE -DBUILDTYPE=${CMAKE_BUILD_TYPE})
endif()

if(ENGINE_NO_BUILD_TIMESTAMPS)
	target_compile_definitions(common INTERFACE -DNO_BUILD_TIMESTAMPS)
endif()

if(ENGINE_BUILD_CLIENT)
	include(buildsys/target/engine/client.cmake)
endif()

if(ENGINE_BUILD_SERVER)
	include(buildsys/target/engine/server.cmake)
endif()
