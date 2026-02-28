#!/bin/bash
if [[ "$1" == "g" ]]; then
	ninja -C build && rm -f /tmp/wayland-* && XDG_RUNTIME_DIR=/tmp gdb build/main
else
	ninja -C build && rm -f /tmp/wayland-* && XDG_RUNTIME_DIR=/tmp build/main
fi
