#!/bin/bash

# If this gets reasonably complex, maybe port to a Makefile.
# Probably will be at that point if I have to keep track of more wayland protocols, or if I also decide to support X11

# TODO(mal): Probably want to refactor this file a little to kind of link all the generated wayland files and whatnot together?
# We have to make sure that they're all synced up in:
# - Assembling their object files during build
# - Generating the actual headers and implementation with wayland scanner during waygen
# - Removing all of them during wayclean

SCRIPT_DIR_REL=$(dirname "$0")
cd "$SCRIPT_DIR_REL"

SRC_DIR=$(realpath ./src)
SRC_WAY_DIR="$SRC_DIR"/wayland

build_game() {
	echo "Building game..."

	mkdir -p build
	cd build

	DEBUG_COMPILER_FLAGS="-O0 -g3 -DASSERTIONS_ENABLED"
	COMMON_COMPILER_FLAGS="-std=c17 $DEBUG_COMPILER_FLAGS -ffast-math\
		-Wall -Wextra -Werror\
		-Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare"
	# --gc-sections = dead code elimination
	COMMON_LINKER_FLAGS=-Wl,--gc-sections,--no-undefined

	#############
	# -l link options
	#############
	# dl             = dynamic linking
	# m              = math
	# wayland_client = core wayland functions
	# xkbcommon      = keyboard stuff

	# TODO(mal): Move this note about hot reloading into the readme?
	# NOTE(mal): We create a dummy game.lock file here before compiling/linking
	# then delete it when we're done. In our actual platform layers for hot
	# reloading we perform the following:
	#
	# 1. Stat our game library.
	# 2. If the modified time is greater than our stored modified time obtained
	#    from the last load...
	# 3. Check if the game.lock exists. If not, then...
	# 4. Unload the game shared library.
	# 5. Load the updated game shared library.
	# 6. Store the updated last modified time.
	# 
	# Because the shared library's last modified time is updated before the
	# linker is actually done doing its thing and closes the file, without this
	# game.lock we could attempt to open a partially-written (i.e. corrupted)
	# shared library.
	echo "creating game lib lock file"
	echo "game build in progress" > game.lock
	gcc -shared -fPIC "$SRC_DIR"/game.c -o game.so \
		-lm \
		$COMMON_COMPILER_FLAGS $COMMON_LINKER_FLAGS
	rm game.lock
	echo "game lib lock file deleted"

	gcc "$SRC_WAY_DIR"/platform_linux_wayland.c "$SRC_WAY_DIR"/xdg_shell_protocol.c "$SRC_WAY_DIR"/xdg_decoration_protocol.c \
		"$SRC_WAY_DIR"/wp_viewporter_protocol.c \
		-o platform_linux_wayland \
		-ldl -lwayland-client -lxkbcommon \
		$COMMON_COMPILER_FLAGS $COMMON_LINKER_FLAGS
}

generate_wayland() {
	echo "Generating wayland protocol files..."

	mkdir -p "$SRC_WAY_DIR"
	cd "$SRC_WAY_DIR"

	WAYLAND_PROTOCOLS=/usr/share/wayland-protocols
	XDG_SHELL_PROTOCOL="$WAYLAND_PROTOCOLS"/stable/xdg-shell/xdg-shell.xml
	XDG_DECORATION_PROTOCOL="$WAYLAND_PROTOCOLS"/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
	WP_VIEWPORTER_PROTOCOL="$WAYLAND_PROTOCOLS"/stable/viewporter/viewporter.xml

	wayland-scanner client-header "$XDG_SHELL_PROTOCOL" xdg_shell_client_protocol.h
	wayland-scanner private-code  "$XDG_SHELL_PROTOCOL" xdg_shell_protocol.c

	wayland-scanner client-header "$XDG_DECORATION_PROTOCOL" xdg_decoration_client_protocol.h
	wayland-scanner private-code  "$XDG_DECORATION_PROTOCOL" xdg_decoration_protocol.c

	wayland-scanner client-header "$WP_VIEWPORTER_PROTOCOL" wp_viewporter_client_protocol.h
	wayland-scanner private-code  "$WP_VIEWPORTER_PROTOCOL" wp_viewporter_protocol.c

}

clean_wayland() {
	echo "Cleaning generated wayland files..."

	rm -rf "$SRC_WAY_DIR"
}

if [ -z "$1" ]; then
	build_game
elif [[ "$1" = "waygen" ]]; then
	generate_wayland
elif [[ "$1" = "wayclean" ]]; then
	clean_wayland
else
	echo "Unrecognized build command"
	exit 2
fi
