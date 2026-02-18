project('cuts', 'c',
  version: '0.1.0',
  default_options: [
    'warning_level=2',
    'c_std=c11',
    'optimization=0',
    'debug=true',
  ],
)

# Find dependencies
libdrm_dep = dependency('libdrm')

# Include directories
inc = include_directories('.', '..', '/usr/include/libdrm', './x86_64-linux-musl-cross/include/')

# Source files
sources = files(
  'src/main.c',
  'server/server.c',
  'server/handlers.c',
  'server/wayland.c',
  'server/error.c',
  'util/helpers.c',
  'util/map.c',
  'util/list.c',
  'util/bitmap.c',
  'backend/util.c',
  'backend/drm.c',
)

# Compiler arguments
c_args = [
  '-Wno-unused-parameter',
]

# Build the executable
executable('main',
  sources,
  include_directories: inc,
  c_args: c_args,
  link_args: ['-lm'],
  dependencies: libdrm_dep,
  install: true,
)
