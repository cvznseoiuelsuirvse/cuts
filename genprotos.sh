#!/bin/env bash
py scripts/wayland-codegen.py protocols/xdg-shell.xml wayland/types/xdg-shell
py scripts/wayland-codegen.py protocols/linux-dmabuf-v1.xml wayland/types/linux-dmabuf-v1
py scripts/wayland-codegen.py protocols/wayland.xml wayland/types/wayland

