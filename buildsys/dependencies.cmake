set(DP_LINK_OPTIONS_STANDARD "none;dlopen;shared;static" CACHE INTERNAL "" FORCE)
set(DP_LINK_OPTIONS_NOSTATIC "none;dlopen;shared" CACHE INTERNAL "" FORCE)
set(DP_LINK_OPTIONS_REQUIRED "shared;static" CACHE INTERNAL "" FORCE)

macro(option_dependency arg_name arg_option arg_linkage arg_allowed_linkage)
	option_string(${arg_option} "Link to ${arg_name}" "${arg_linkage}")
	set_property(CACHE ${arg_option} PROPERTY STRINGS ${arg_allowed_linkage})
endmacro()
