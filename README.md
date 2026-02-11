## Linux Version Dev Dependencies
libwayland-dev
wayland-protocols
libxkbcommon-dev

WSL2: May have to set `XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir`
Actually there seem to be a lot of issues to fix when running on WSL2, mostly due to file system permissions and where things are actually pointing.
For example, attempting to mmap the shared mem containing the keyboard map fails with "Operation not permitted"
