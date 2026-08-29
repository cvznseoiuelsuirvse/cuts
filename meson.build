project('cuts', 'c',
  version: '0.1.0',
  default_options: [
    'warning_level=2',
    'c_std=c11',
  ],
  meson_version: '>=1.10.0',
)

cc = meson.get_compiler('c')

c_args = [
  '-Wno-unused-parameter',
  '-D_GNU_SOURCE',
]

if get_option('logo_key')
  c_args += '-DUSE_LOGO_KEY'
endif

subdir('util')
subdir('compositor')
subdir('seat')
subdir('wayland')
subdir('render')
subdir('output')
subdir('cursor')

fs = import('fs')

freetype_dep = dependency('freetype2')
sources = [
  'cuts.c',
  'fontenum.c',
]

if not fs.exists('config.h')
  run_command('cp', 'config.def.h', 'config.h', check: true)
endif


executable('cuts',
  sources,
  c_args: c_args,
  dependencies: [
    cur_dep,
    freetype_dep,
    output_dep,
    seat_dep,
    compositor_dep,
    util_dep,
    wayland_dep,
    render_dep,
  ],
  install: true,
)
