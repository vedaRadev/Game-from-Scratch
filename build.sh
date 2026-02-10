#!/bin/bash

# If this gets reasonably complex, maybe port to a Makefile.
# Probably will be at that point if I have to keep track of more wayland protocols, or if I also decide to support X11

SCRIPT_DIR_REL=$(dirname "$0")
cd "$SCRIPT_DIR_REL"

SRC_DIR=$(realpath ./src)

build_game() {
	echo "Building game..."

	mkdir -p build
	cd build

	DEBUG_COMPILER_FLAGS="-O0 -g3 -DASSERTIONS_ENABLED"
	COMMON_COMPILER_FLAGS="-std=c17 $DEBUG_COMPILER_FLAGS -ffast-math\
		-Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable"
	# --gc-sections = dead code elimination
	COMMON_LINKER_FLAGS=-Wl,--gc-sections

	echo "creating game dll lock file"
	echo "game build in progress" > game.lock
	gcc -shared "$SRC_DIR"/game.c -o game.so\
		$COMMON_COMPILER_FLAGS $COMMON_LINKER_FLAGS
	rm game.lock
	echo "game dll lock file deleted"

	gcc "$SRC_DIR"/platform_linux_wayland.c "$SRC_DIR"/xdg_shell_protocol.c "$SRC_DIR"/xdg_decoration_protocol.c\
		-o platform_linux_wayland\
		-ldl -lwayland-client -lxkbcommon\
		$COMMON_COMPILER_FLAGS $COMMON_LINKER_FLAGS
}

generate_wayland() {
	echo "Generating wayland protocol files..."

	cd "$SRC_DIR"

	WAYLAND_PROTOCOLS=/usr/share/wayland-protocols
	XDG_SHELL_PROTOCOL="$WAYLAND_PROTOCOLS"/stable/xdg-shell/xdg-shell.xml
	XDG_DECORATION_PROTOCOL="$WAYLAND_PROTOCOLS"/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml

	wayland-scanner client-header "$XDG_SHELL_PROTOCOL" xdg_shell_client_protocol.h
	wayland-scanner private-code  "$XDG_SHELL_PROTOCOL" xdg_shell_protocol.c
	wayland-scanner client-header "$XDG_DECORATION_PROTOCOL" xdg_decoration_client_protocol.h
	wayland-scanner private-code  "$XDG_DECORATION_PROTOCOL" xdg_decoration_protocol.c
}

clean_wayland() {
	echo "Cleaning generated wayland files..."

	cd "$SRC_DIR"

	rm xdg_shell_client_protocol.h
	rm xdg_shell_protocol.c
	rm xdg_decoration_client_protocol.h
	rm xdg_decoration_protocol.c
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
