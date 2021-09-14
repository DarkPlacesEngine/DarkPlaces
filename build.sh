#!/bin/bash
#------------------------------------------------------------------------------#
#   build.sh (DarkPlaces Build Script)                                         #
#                                                                              #
#   Copyright (c) 2019-2020 David Knapp                                        #
#                                                                              #
# Permission is hereby granted, free of charge, to any person obtaining a copy #
# of this software and associated documentation files (the "Software"), to     #
# deal in the Software without restriction, including without limitation the   #
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or  #
# sell copies of the Software, and to permit persons to whom the Software is   #
# furnished to do so, subject to the following conditions:                     #
#                                                                              #
# The above copyright notice and this permission notice shall be included in   #
# all copies or substantial portions of the Software.                          #
#                                                                              #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR   #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,     #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE  #
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER       #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING      #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS #
# IN THE SOFTWARE.                                                             #
#------------------------------------------------------------------------------#
if [ -z "${BASH_VERSION:-}" ]; then
	echo "This script requires bash."
	exit 1
fi
#------------------------------------------------------------------------------#
### HELPER FUNCTIONS ###
perror() {
	printf -- "\e[31m%b\e[0m" "$1"
}

pwarn() {
	printf -- "\e[33m%b\e[0m" "$1"
}

psuccess() {
	printf -- "\e[32m%b\e[0m" "$1"
}

phelp() {
	local full=$2
	printf "
Usage: %s [OPTIONS] PROJECT" "$me"
	if (( ! full )); then
		printf "

For more information, run again with --help.

"

	else printf "
Options
    --cc=               specify a C compiler
    --cxx=              specify a C++ compiler
    --threads= | --jN   set how many threads to compile with
    --generator=        cmake generator to use. Run 'cmake --help' for a list
    --cmake-options=    pass additional options to cmake
    --config-dir=       override the location of the config.cmake file
    --build-dir=        override the location cmake will write build files
    --reset-build       delete build files of PROJECT
    --reset-cache       delete the cache
    --new-options       specify a new set of options
    --nocache           do not read from, or write to the cache
    --auto              do not display any prompts, even with --reset
    --help              print this message then exit

Unless you set --config-dir, the PROJECT you specify is assumed to be a
correspondingly named subdirectory of the 'game' directory. The build directory
is also relative to the config directory by default, unless --build-dir is set.
If the directory (assumed or specified) doesn't exist, a new project can be
created for you from a template.

The script maintains a cache of your build settings so you don't have to input
the same settings for a specified PROJECT more than once. You can simply run
the script with PROJECT, and optionally --build, and it will configure and/or
build PROJECT with the cached settings automatically.

This script will run in auto mode if ran from a non-interactive shell.
"; fi
	exit "$1"
}

check_empty()
{
	if [ ! -d "$1" ] ||
	! command -v ls -A "$1" >/dev/null; then return 0; fi
	return 1
}
#------------------------------------------------------------------------------#
check_env() { # Make sure the environment is sane before continuing.
	local failed=0

	if ! (( "$(id -u)" )); then
		if (( ! option_asroot )); then
			perror "*** This script cannot be run as root. Use --jackass to override\n\n"
			failed+=1
		else pwarn "* Running as root as you requested. Welcome to Jackass!\n\n"; fi
	fi

	if [[ $- == *i* ]]; then
		option_auto=1
		pwarn "* Shell is non-interactive. Prompts are impossible/pointless. --auto enabled.\n\n"
	fi


	if [ ! -d "$(pwd)/engine" ]; then perror "*** Required directory '\"$(pwd)\"/engine' was not found.\n" ; failed+=1; fi
	if [ ! -d "$(pwd)/game" ] || [ ! -w "$(pwd)/game" ]; then perror "*** Required directory '\"$(pwd)\"/game' was not found or is not writable.\n" ; failed+=1; fi
	if [ ! -d "$(pwd)/game/default" ]; then perror "*** Required directory '\"$(pwd)\"/game/default' was not found.\n"; failed+=1; fi
	if [ ! -f "$(pwd)/CMakeLists.txt" ]; then perror "*** Required file '\"$(pwd)\"/engine/CMakeLists.txt' was not found.\n"; failed+=1; fi
	if [ ! -f "$(pwd)/game/default/dpconfig.cmake" ]; then perror "*** Required file '\"$(pwd)\"/game/default/config.cmake' was not found.\n"; failed+=1; fi

	if ! command -v cmake >/dev/null; then perror "*** Could not find cmake. Please install it and make sure it's in your PATH.\n"; failed+=1; fi

	if (( failed )); then
		perror "*** The script failed to initialize. Please check the output for more information.\n\n"
		phelp 1 0
	fi
}
#------------------------------------------------------------------------------#
option_cache_read() {
	cache_file="${cache_dir}/${option_project}"

	if [ -f "${cache_file}" ]; then
		if grep -q "cache_" "${cache_file}" ; then
			source "$cache_file"
			psuccess "* Loaded cached settings for '$option_project'\n\n"
			return
		else
			perror "* Could not load '$option_project' from the cache. Please check if this is a\nvalid cache file.\n\n"
		fi
	fi
	cache_changed=1
	option_new=1
	cache_new=1
}

option_cache_write() {
	if [ ! -d "${cache_dir}" ]; then 
		if [ -f "${cache_dir}" ]; then
			perror "* The cache directory cannot be created because a file of the same name exists.\nThe cache cannot function. Please rename or delete this file.\n\n"
			return
		fi
		mkdir "${cache_dir}"
	fi

	if (( cache_changed )); then
		if (( ! option_cache_off )); then
			echo "#!/bin/bash

cache_project_dir=\"${option_project_dir}\"
cache_build_dir=\"${option_build_dir}\"
cache_build_cc=\"${option_build_cc}\"
cache_build_cxx=\"${option_build_cxx}\"
cache_build_threads=\"${option_build_threads}\"
cache_build_cmake_generator=\"${option_build_cmake_generator}\"
cache_build_cmake_options=\"${option_build_cmake_options}\"
" > "$cache_file"

			printf "\
Your build options for \"%s\" has been written to the cache. You only have to
run '%s %s' to build the same project again.

If you wish to change any setting later, you can pass --new-options to the
script.

" "$option_project" "$me" "$option_project"
		else pwarn "* The cache is disabled. Skipping write.\n\n"; fi
	fi
}

option_cache_compare() {
	local option=$1; local cache=$2

	if ! [ "${option}" = "${cache}" ]; then cache_changed=1; return 1; fi
	return 0
}

option_cache_list() {
	local cache_list; cache_list="$(pwd)/.temp"
	local cache_select

	if (( option_auto )) || check_empty "${cache_dir}" || [ "$option_project" ]; then
		return; fi

	printf "Please select a project."

	if (( ! option_run_reset_build )); then
		printf " Leave blank to create a new one."
	fi

	printf "\n\n"
	
	while [ ! "$option_project" ]; do
		local j=1
		
		rm -f "$cache_list"
		for i in "${cache_dir}"/*; do
			printf "%s. %s\n" "$j" "$(basename "$i")" >> "$cache_list"
			j=$((j+1))
		done
		cat "$cache_list"
		printf "\nChoose: "
		read -r cache_select
		if [ ! "$cache_select" ]; then break; fi
		option_project="$(sed -n -e "s/^.*$cache_select. //p" "$cache_list")"
		printf "\n"
	done
}
#------------------------------------------------------------------------------#
option_get_prompt() { # Generic prompt function
	local -n option=$1;		local default=$2
	local message=$3;		local required=$4
	local error=$5;			local default_text="[$default]"

	if ! [ "$default" ]; then default_text=""; fi

	if (( option_auto )); then # No prompts in auto mode.
		if (( required )); then
			perror "$error\n"
			phelp 1 0
		else
			option="${default}"
			return
		fi
	fi

	printf -- "%b\n" "$message"
	read -rp "$default_text: " option
	option=${option:-$default}
	printf "\n"
	return
}

option_get_cmdline() { 	# Iterate over any args.
	for (( i=0; i<${#args[@]}; i++)); do
		if [[ "${args[$i]}" == "-"* ]]; then
			case "${args[$i]}" in
				"--new-options" )
					option_new=1 ;;
				"--reset-build" )
					option_run_reset_build=1 ;;
				"--reset-cache" )
					option_run_reset_cache=1 ;;
				"--auto" )
					option_auto=1 ;;
				"--build-dir="* )
					option_build_dir=${args[$i]##--build-dir=} ;;
				"--config-dir="* )
					option_project_dir=${args[$i]##--config-dir=} ;;
				"--cc="* )
					option_build_cc=${args[$i]##--cc=} ;;
				"--cxx="* )
					option_build_cxx=${args[$i]##--cxx=} ;;
				"--threads="*[0-9] )
					option_build_threads=${args[$i]##--threads=} ;;				
				"--j"*[0-9] )
					option_build_threads=${args[$i]##--j} ;;
				"-j"*[0-9] )
					option_build_threads=${args[$i]##-j} ;;
				"--generator="* )
					option_build_cmake_generator=${args[$i]##--generator=} ;;
				"--cmake-options="* )
					option_build_cmake_options=${args[$i]##--cmake-options=} ;;
				"--nocache" )
					option_cache_off=1 ;;
				"--from-cmake" )
					option_from_cmake=1 ;;
				"--jackass" )
					option_asroot=1 ;;
				"--help" )
					phelp 0 1 ;;
				* )
					pwarn "Unknown option '${args[$i]}'\n"
					phelp 1 0 ;;
			esac
		# Last arg should be the build config, but not the first arg.
		else option_project=${args[$i]}; fi
	done
}
#------------------------------------------------------------------------------#
option_get_check() {
	if (( option_run_reset_cache )); then reset_cache; fi

	if (( option_auto )); then
		pwarn "* --auto is set. Prompts will not appear.\n\n"
		if (( option_new )); then
			option_new=0
			pwarn "* --new-options is useless in auto mode. Ignoring.\n\n"
		fi
	fi

	option_cache_list
	option_get_check_config

	if ! (( option_cache_off )); then option_cache_read
	else pwarn "* The cache is disabled. Skipping read.\n\n"; fi

	if (( option_run_reset_build )); then reset_build; exit 0; fi

	option_get_check_config_dir
	option_get_check_build_dir
	if (( ! option_from_cmake )); then
		option_get_check_build_cc
		option_get_check_build_cxx
		option_get_check_build_threads
		option_get_check_build_cmake_generator
		option_get_check_build_cmake_options
	fi
}

option_get_check_config() { # If the user didn't give us anything, ask.
	if (( option_run_reset_build )); then pwarn "* Resetting build files...\n\n"; fi
	
	while true; do
		if [ "$option_project" ]; then
			if [ "${option_project}" == "default" ]; then
				pwarn "* H-hey...! Get your own project! That's the template. You can't use that!\n\n"
				option_project=""
			else break; fi
		else option_get_prompt \
			option_project \
			"$cache_project" \
			"Please specify the name of your project" \
			"" \
			""
		fi
	done
}

option_get_check_config_dir() {
	local new_config

	if [ ! "$cache_project_dir" ]; then # Set the default
		cache_project_dir="$(pwd)/game/$option_project"; fi

	# Don't prompt for this unless something is wrong. Assume the default.
	if [ ! "$option_project_dir" ]; then
		if (( cache_new )) || (( ! option_new )); then
			option_project_dir="$cache_project_dir"
		fi
	fi

	while true; do
		if [ "$option_project_dir" ]; then
			if [ -d "${option_project_dir}" ]; then
				option_cache_compare "$option_project_dir" "$cache_project_dir"
				return	# We're good. Proceed.
			else printf "* The directory of the specified project does not exist.\n\n"
				if [ -w "$(dirname "$option_project_dir")" ]; then
					option_get_prompt \
						new_config \
						"Y" \
						"Would you like to create a new project from the template in:\n$option_project_dir?" \
						"" \
						""
					if [[ "$new_config" =~ ^(Y|y)$ ]]; then
						cp -rv "$config_template" "$option_project_dir"
						continue
					fi
				elif [ -f "${option_project_dir}" ]; then
					pwarn "* The directory of the specified project is a file. Cannot create a new project\nfrom the template here.\n\n"
				else
					pwarn "* The parent directory is also not writable or doesn't exist. Cannot create a new\nproject from the template here.\n\n"
				fi
			fi
		elif (( ! option_new )); then
			pwarn "* No config directory has been specified and --new-options isn't set. Something is wrong with your configuration, or there's\na bug in the script.\n\n"
		fi
		
		# Get our answer unless --auto is set.
		option_get_prompt \
			option_project_dir \
			"$cache_project_dir" \
			"Specify the location of '${option_project}'. If it doesn't exist, it can be created\nfrom a template."	\
			1 \
			"You must provide a valid project directory with --auto set\n\n"
	done
}

option_get_check_build_dir() {
	local finished=0; local force=0; local ask=0

	if [ ! "$cache_build_dir" ]; then
		cache_build_dir="$(pwd)/output/$option_project/"; fi

	# Don't prompt for this unless something is wrong. Assume the default.
	if [ ! "$option_build_dir" ]; then
		if (( cache_new )) || (( ! option_new )); then
			option_build_dir="$cache_build_dir"
		fi
	fi

	while ! (( finished )); do
		if [ "$option_build_dir" ]; then
			# Check if it even exists first.
			if [ -d "$option_build_dir" ]; then # Exists
				# Check if writable. Permissions change.
				if ! [ -w "$option_build_dir" ]; then
					pwarn "* The directory '$option_build_dir' is NOT writable.\n\n"
					ask=1
				elif ! check_empty "$option_build_dir" &&
				[[ $cache_build_dir != "$option_build_dir" ]]; then
					if (( ! option_from_cmake )) && [ ! -f "$option_build_dir/CMakeCache.txt" ]; then
						pwarn "* The directory '$option_build_dir' is NOT empty.\n\n"
						option_get_prompt \
							force \
							"y/N" \
							"Would you like to build here anyway?" \
							1 \
							"*** You must specify an empty directory when --auto is set."
						if [[ "$force" =~ ^(Y|y)$ ]]; then ask=1; fi
					fi
				fi
			else
				pwarn "* Build directory doesn't exist. CMake will create the directory for you.\n\n"
			fi
		else ask=1; fi

		if (( ask )); then
			option_get_prompt \
				option_build_dir \
				"$cache_build_dir" \
				"Please provide an empty and writable directory for the build files" \
				1 \
				"*** You must provide a valid build directory with --auto set\n\n"
			ask=0
		else finished=1; fi
	done
}

option_get_check_build_cc() {
	if [ ! "$option_build_cc" ] && (( option_new )); then
		option_get_prompt \
			option_build_cc \
			"$cache_build_cc" \
			"Which C compiler would you like to use? Leave this default to use the compiler\nspecified in the cache, or if blank, for CMake to autodetect the compiler,\nunless you have something specific in mind (like clang)" \
			"" \
			""
	fi

	export CC="$option_build_cc"

	if ! option_cache_compare "$option_build_cc" "$cache_build_cc" && (( ! cache_new )); then
		pwarn "* The cmake cache must be deleted and regenerated when changing compilers.\n\n"
		reset_build
	fi
}

option_get_check_build_cxx() {
	if [ ! "$option_build_cxx" ] && (( option_new )); then
		option_get_prompt \
			option_build_cxx \
			"$cache_build_cxx" \
			"Which C++ compiler would you like to use? Leave this default to use the compiler\nspecified in the cache, or if blank, for CMake to autodetect the compiler,\nunless you have something specific in mind (like clang++)" \
			"" \
			""
	fi

	export CXX="$option_build_cxx"

	if ! option_cache_compare "$option_build_cxx" "$cache_build_cxx" && (( ! cache_new )); then
		pwarn "* The cmake cache must be deleted and regenerated when changing compilers.\n\n"
		reset_build
	fi
}

option_get_check_build_threads() {
	if [ ! "$option_build_threads" ] && (( ! option_new )); then
		option_build_threads="$cache_build_threads"
	fi

	while true; do
		if [ "$option_build_threads" ]; then
			if (( ! option_build_threads )); then
				pwarn "* Threads must be a number, silly!\n\n"
			elif (( option_build_threads < 0 )); then
				pwarn "* Threads can't be a negative number, silly!\n\n"
			else break; fi
		fi
		option_get_prompt \
			option_build_threads \
			"$cache_build_threads" \
			"How many threads would you like to compile with? Enter 0 to run the configure\nstep only." \
			"" \
			""
	done
	option_cache_compare "$option_build_threads" "$cache_build_threads"
}

option_get_check_build_cmake_generator() {
	if [ ! "$option_build_cmake_generator" ]; then
		if (( ! option_new )); then
			option_build_cmake_generator="$cache_build_cmake_generator"
		else
			option_get_prompt \
				option_build_cmake_generator \
				"$cache_build_cmake_generator" \
				"What CMake generator would you like to use?" \
				"" \
				""
		fi
	fi
	if ! option_cache_compare "$option_build_cmake_generator" "$cache_build_cmake_generator" && (( ! cache_new )); then
		pwarn "* The cmake cache must be deleted and regenerated when changing generators.\n\n"
		reset_build
	fi
}

option_get_check_build_cmake_options() {
	if (( option_new )); then
		option_get_prompt \
			option_build_cmake_options \
			"$cache_build_cmake_options" \
			"Specify additional command-line options for CMake" \
			"" \
			""
	else
		option_build_cmake_options="$cache_build_cmake_options"
	fi
	option_cache_compare "$option_build_cmake_options" "$cache_build_cmake_options"
}
#------------------------------------------------------------------------------#
build_start_config() {
	local cmd_cmake_config="cmake -G\"${option_build_cmake_generator}\" -B$option_build_dir -DGAME_PROJECT=\"${option_project}\" -DGAME_PROJECT_DIR=\"${option_project_dir}\" $option_build_cmake_options"

	printf "* Running CMake...\n\n"	
	printf "* Using \"%s\" build config.\n\n" "$option_project"

	# Try to configure. If that's successful, go ahead and build.
	printf "* CMake config commandline: %s\n\n" "$cmd_cmake_config"
	if ! eval "$cmd_cmake_config"; then
		perror "*** Configure failed. Please check the output for more information.\n\n"
		exit 1
	fi
	psuccess "* Configure completed successfully.\n\n"
}

build_start_compile() {
	local cmd_cmake_build="cmake --build $option_build_dir -- -j$option_build_threads"

	printf "* CMake build commandline: %s\n\n" "$cmd_cmake_build"
	if ! eval "$cmd_cmake_build"; then
		pwarn "* Build failed, but configure was successful. Please check the output for more information.\n\n"
		return
	fi
	psuccess "* Build completed successfully.\n\n"
}

build_start() {
		build_start_config
		build_start_compile
}
#------------------------------------------------------------------------------#
reset_build() {
	local reset="Y" # Defaults to Y in case --auto is set
	if ! [ "$cache_build_dir" ]; then # Not set?
		pwarn "* --reset-build: No build directory for '$option_project' was specified or\nfound in the cache, or the cache is disabled.\n\n"
	elif ! [ -d "$cache_build_dir" ]; then # Doesn't exist?
		pwarn "* --reset-build: The build directory of '$option_project' doesn't exist.\nNothing to delete.\n\n"
	else
		option_get_prompt \
			reset \
			"Y" \
			"Do you wish to delete all build files under\n'$cache_build_dir'?"
		if [[ "$reset" =~ ^(Y|y)$ ]]; then
			if ! rm -rfv "$cache_build_dir" ; then # Can't delete?
				perror "*** --reset-build: Failed to delete build files under '$cache_build_dir'\n\n"
			else
				pwarn "* --reset-build: Deleted the build directory of '$option_project'.\n\n"
			fi
		else return; fi
	fi
}

reset_cache() { # It resets the cache.
	local reset="Y" # Defaults to Y in case --auto is set

	if [ -d "$cache_dir" ]; then
		option_get_prompt \
			reset \
			"Y" \
			"Do you wish to delete the entire build cache?"
		if [[ "$reset" =~ ^(Y|y)$ ]]; then
			rm -rfv "$cache_dir"
			pwarn "* --reset-cache: Deleted the build cache.\n\n"
		else return; fi
	else
		pwarn "* --reset-cache: The build cache doesn't exist. Nothing to delete.\n\n"
	fi
	exit 0
}
#------------------------------------------------------------------------------#
printf "\n\e[1;34m---Darkplaces Build Wizard---\e[0m\n\n"

declare -g args=("$@") # Put cmdline in separate array to be read in functions
declare -g me=$0 # Script can refer to itself regardless of filename in global scope
declare -g cache_dir; cache_dir="./.cache"
declare -g cache_file # Current cache file handle.
declare -g config_template; config_template="$(pwd)/game/default"

### Default options ###
# These are changed by the cache (if it exists) and compared with user input.
declare -g cache_project="darkplaces"
declare -g cache_project_dir="" # Defined later
declare -g cache_build_dir="" # Defined later
declare -g cache_build_cc=""
declare -g cache_build_cxx=""
declare -g cache_build_threads=1
declare -g cache_build_cmake_options=""
declare -g cache_build_cmake_generator="Unix Makefiles"

### User options ###
# If any of these don't match the cache, write to it.
declare -g option_project
declare -g option_project_dir
declare -g option_build_dir
declare -g option_build_cc
declare -g option_build_cxx
declare -g option_build_threads
declare -g option_build_cmake_options
declare -g option_build_cmake_generator
# Per-build options
declare -g option_auto=0
declare -g option_new=0
declare -g option_asroot=0
declare -g option_run_reset_build=0
declare -g option_run_reset_cache=0
declare -g option_cache_off=0
declare -g option_from_cmake=0

# State tracking variables
declare -g cache_new=0
declare -g cache_loaded=0
declare -g cache_changed=0 # Set to 1 if any options don't match the cache.

check_env				# Make sure the environment is sane first
option_get_cmdline		# Get input from command-line
option_get_check		# Check that input and ask for new input
build_start				# Start build with options
option_cache_write		# Write to cache

psuccess "* The Darkplaces Build Wizard has completed the requested operations\n successfully.\n\n"
