macro(option_string OPTION DESC VALUE)
	set(${OPTION} ${VALUE} CACHE STRING "${DESC}")
endmacro()

function(dp_build arg_project arg_path)
	if(NOT arg_path)
		if(arg_project)
			set(arg_path ${CMAKE_SOURCE_DIR}/game/${arg_project})
		endif()
	endif()

	if(arg_path)
		include(${arg_path}/dpconfig.cmake)
	endif()

	if(ENGINE_EXE_NAME STREQUAL "") # Cannot be empty
		message(FATAL_ERROR "You must give the executable a name.")
	endif()

	if(ENGINE_EXE_NAME MATCHES "[* *]") # Cannot contain spaces.
		message(FATAL_ERROR "The executable name must not contain spaces.")
	endif()

	if(NOT ENGINE_BUILD_CLIENT AND NOT ENGINE_BUILD_SERVER)
		message(FATAL_ERROR "You must build at least one target.")
	endif()

	if(GAME_BUILD_EXTERNAL_PROJECT)
		include(ExternalProject)
		ExternalProject_Add(${GAME_BUILD_EXTERNAL_PROJECT})
	endif()

	include(buildsys/target/engine.cmake)

endfunction()
