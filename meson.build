project('cuts', 'c',
  version: '0.1.0',
  default_options: [
    'warning_level=2',
    'c_std=c11',
  ],
  meson_version: '>=1.10.0',
)

fs = import('fs')
python = find_program('python3')
src_root = meson.project_source_root()
cc = meson.get_compiler('c')

c_args = [
  '-Wno-unused-parameter',
  '-D_GNU_SOURCE',
]

if get_option('test')
  c_args += '-DCUTS_TEST'
endif

subdir('util')
subdir('compositor')
subdir('seat')
subdir('wayland')
subdir('render')
subdir('output')
subdir('cursor')

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
