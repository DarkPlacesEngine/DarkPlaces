add_executable(server ${OBJ_COMMON} ${OBJ_SV})
target_link_libraries(server common)
set_target_properties(server PROPERTIES OUTPUT_NAME ${ENGINE_BUILD_NAME}-dedicated)

if(WIN32)
	target_sources(server PRIVATE ${ENGINE_BUILD_WINRC})
endif()
