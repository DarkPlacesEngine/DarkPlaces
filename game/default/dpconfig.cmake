option(
ENGINE_BUILD_CLIENT
"Build the client"
ON)

option(
ENGINE_BUILD_SERVER
"Build the server"
ON)

option_string(
ENGINE_BUILD_NAME
"Engine binary name"
"darkplaces")

option_string(
ENGINE_BUILD_WINRC
"Location of Windows .rc, usually for icon"
"${GAME_PROJECT_DIR}/res/darkplaces.rc")

option_string(
GAME_BUILD_CUSTOM_COMMAND
"Custom tool to bootstrap. Enter what you would with execute_process"
"")

option_string(
GAME_BUILD_EXTERNAL_PROJECT
"External cmake project to add. Enter what you would with ExternalProject_Add"
"")

option(
ENGINE_CONFIG_MENU
"Enable menu support"
ON)

option(
ENGINE_CONFIG_VIDEO_CAPTURE
"Enable video capture support"
ON)

option(
ENGINE_CONFIG_BUILD_TIMESTAMP
"Add the git commit timestamp to the version string"
ON)

option(
ENGINE_CONFIG_BUILD_REVISION
"Add the git revision to the version string"
ON)
