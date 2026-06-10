wayland_sources = files(
  'server.c',
  'display.c',
  'types.c',
  'util.c',

  'proto/wayland.c',
  'proto/xdg-shell.c',
  'proto/linux-dmabuf-v1.c',
  'proto/xdg-decoration-unstable-v1.c',

  'impl/wayland.c',
  'impl/xdg-shell.c',
  'impl/linux-dmabuf-v1.c',
  'impl/xdg-decoration-unstable-v1.c',
)

wayland_dep = declare_dependency(
  sources: wayland_sources,
)
